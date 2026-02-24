#include "network.h"

#include <stdio.h>
#include <string.h>

static int set_tcp_nodelay(bytes_socket_t fd)
{
    int flag = 1;
    return setsockopt(fd, IPPROTO_TCP, TCP_NODELAY,
                      (const char *)&flag, sizeof(flag));
}

static int set_keepalive(bytes_socket_t fd)
{
    int on = 1;
    if (setsockopt(fd, SOL_SOCKET, SO_KEEPALIVE,
                   (const char *)&on, sizeof(on)) < 0)
        return -1;
#ifdef TCP_KEEPIDLE
    int idle = 5;
    setsockopt(fd, IPPROTO_TCP, TCP_KEEPIDLE,
               (const char *)&idle, sizeof(idle));
#elif defined(TCP_KEEPALIVE) && defined(BYTES_MACOS)
    int idle = 5;
    setsockopt(fd, IPPROTO_TCP, TCP_KEEPALIVE,
               (const char *)&idle, sizeof(idle));
#endif
#ifdef TCP_KEEPINTVL
    int intvl = 2;
    setsockopt(fd, IPPROTO_TCP, TCP_KEEPINTVL,
               (const char *)&intvl, sizeof(intvl));
#endif
#ifdef TCP_KEEPCNT
    int cnt = 3;
    setsockopt(fd, IPPROTO_TCP, TCP_KEEPCNT,
               (const char *)&cnt, sizeof(cnt));
#endif
    return 0;
}

static int net_poll_writable(bytes_socket_t fd, int timeout_ms)
{
#ifdef BYTES_WINDOWS
    WSAPOLLFD pfd;
    pfd.fd = fd;
    pfd.events = POLLWRNORM;
    pfd.revents = 0;

    int ret = WSAPoll(&pfd, 1, timeout_ms);

    if (ret > 0 && (pfd.revents & (POLLERR | POLLHUP | POLLNVAL)))
        return -1;
    return ret;
#else
    struct pollfd pfd;
    pfd.fd = fd;
    pfd.events = POLLOUT;
    pfd.revents = 0;

    int ret;
    do {
        ret = poll(&pfd, 1, timeout_ms);
    } while (ret < 0 && errno == EINTR);

    if (ret > 0 && (pfd.revents & (POLLERR | POLLHUP | POLLNVAL)))
        return -1;
    return ret;
#endif
}

int net_server_init(net_server_t *srv, int port)
{
    memset(srv, 0, sizeof(*srv));
    srv->port = port;
    srv->listen_fd = BYTES_INVALID_SOCKET;

    bytes_socket_t fd = socket(AF_INET, SOCK_STREAM, 0);
    if (fd == BYTES_INVALID_SOCKET)
        return -1;

    int opt = 1;
    setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, (const char *)&opt, sizeof(opt));

    struct sockaddr_in addr;
    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = INADDR_ANY;
    addr.sin_port = htons((uint16_t)port);

    if (bind(fd, (struct sockaddr *)&addr, sizeof(addr)) < 0) {
        platform_close_socket(fd);
        return -1;
    }

    if (listen(fd, MAX_CLIENTS) < 0) {
        platform_close_socket(fd);
        return -1;
    }

    platform_set_nonblocking(fd);
    srv->listen_fd = fd;
    srv->running = true;

    for (int i = 0; i < MAX_CLIENTS; i++) {
        srv->clients[i].fd = BYTES_INVALID_SOCKET;
        srv->clients[i].connected = false;
    }

    return 0;
}

int net_server_accept(net_server_t *srv, int timeout_ms)
{
    if (net_poll_readable(srv->listen_fd, timeout_ms) <= 0)
        return -1;

    struct sockaddr_in peer;
    socklen_t peerlen = sizeof(peer);
    bytes_socket_t cfd = accept(srv->listen_fd,
                                (struct sockaddr *)&peer, &peerlen);
    if (cfd == BYTES_INVALID_SOCKET)
        return -1;

    set_tcp_nodelay(cfd);
    platform_set_nonblocking(cfd);
    platform_set_nosigpipe(cfd);
    set_keepalive(cfd);

    for (int i = 0; i < MAX_CLIENTS; i++) {
        if (!srv->clients[i].connected) {
            srv->clients[i].fd = cfd;
            srv->clients[i].connected = true;
            srv->clients[i].is_player = false;
            srv->clients[i].player_id = 0;
            srv->clients[i].name[0] = '\0';
            srv->client_count++;
            return i;
        }
    }

    platform_close_socket(cfd);
    return -1;
}

void net_server_close_client(net_server_t *srv, int idx)
{
    if (idx < 0 || idx >= MAX_CLIENTS)
        return;
    if (!srv->clients[idx].connected)
        return;

    platform_close_socket(srv->clients[idx].fd);
    srv->clients[idx].fd = BYTES_INVALID_SOCKET;
    srv->clients[idx].connected = false;
    srv->client_count--;
}

void net_server_shutdown(net_server_t *srv)
{
    srv->running = false;
    for (int i = 0; i < MAX_CLIENTS; i++) {
        if (srv->clients[i].connected) {
            platform_close_socket(srv->clients[i].fd);
            srv->clients[i].fd = BYTES_INVALID_SOCKET;
            srv->clients[i].connected = false;
        }
    }
    if (srv->listen_fd != BYTES_INVALID_SOCKET) {
        platform_close_socket(srv->listen_fd);
        srv->listen_fd = BYTES_INVALID_SOCKET;
    }
    srv->client_count = 0;
}

int net_client_connect(net_connection_t *conn, const char *host, int port)
{
    memset(conn, 0, sizeof(*conn));
    conn->fd = BYTES_INVALID_SOCKET;

    struct addrinfo hints, *res, *p;
    memset(&hints, 0, sizeof(hints));
    hints.ai_family = AF_INET;
    hints.ai_socktype = SOCK_STREAM;

    char port_str[8];
    snprintf(port_str, sizeof(port_str), "%d", port);

    if (getaddrinfo(host, port_str, &hints, &res) != 0)
        return -1;

    bytes_socket_t fd = BYTES_INVALID_SOCKET;
    for (p = res; p != NULL; p = p->ai_next) {
        fd = socket(p->ai_family, p->ai_socktype, p->ai_protocol);
        if (fd == BYTES_INVALID_SOCKET)
            continue;

        platform_set_nonblocking(fd);

        int rc = connect(fd, p->ai_addr, (int)p->ai_addrlen);
        if (rc == 0)
            break;

        int err = bytes_socket_error();
        if (err != BYTES_EINPROGRESS
#ifdef BYTES_WINDOWS
            && err != WSAEWOULDBLOCK
#endif
        ) {
            platform_close_socket(fd);
            fd = BYTES_INVALID_SOCKET;
            continue;
        }

        int ready = net_poll_writable(fd, 5000);
        if (ready <= 0) {
            platform_close_socket(fd);
            fd = BYTES_INVALID_SOCKET;
            continue;
        }

        int sockerr = 0;
        socklen_t errlen = sizeof(sockerr);
        getsockopt(fd, SOL_SOCKET, SO_ERROR,
                   (char *)&sockerr, &errlen);
        if (sockerr != 0) {
            platform_close_socket(fd);
            fd = BYTES_INVALID_SOCKET;
            continue;
        }

        break;
    }
    freeaddrinfo(res);

    if (fd == BYTES_INVALID_SOCKET)
        return -1;

    set_tcp_nodelay(fd);
    platform_set_nosigpipe(fd);
    set_keepalive(fd);
    conn->fd = fd;
    conn->connected = true;
    return 0;
}

void net_client_disconnect(net_connection_t *conn)
{
    if (conn->fd != BYTES_INVALID_SOCKET) {
        platform_close_socket(conn->fd);
        conn->fd = BYTES_INVALID_SOCKET;
    }
    conn->connected = false;
}

int net_send(bytes_socket_t fd, const uint8_t *buf, size_t len, int timeout_ms)
{
    if (timeout_ms <= 0)
        timeout_ms = 100;

    size_t sent = 0;
    while (sent < len) {
        int n = send(fd, (const char *)(buf + sent), (int)(len - sent),
                     BYTES_MSG_NOSIGNAL);
        if (n < 0) {
            int err = bytes_socket_error();
            if (err == BYTES_EINTR)
                continue;
            if (err == BYTES_EAGAIN || err == BYTES_EWOULDBLOCK) {
                int ready = net_poll_writable(fd, timeout_ms);
                if (ready <= 0)
                    return -1;
                continue;
            }
            return -1;
        }
        if (n == 0)
            return -1;
        sent += (size_t)n;
    }
    return (int)sent;
}

int net_recv(bytes_socket_t fd, uint8_t *buf, size_t buflen, int timeout_ms)
{
    if (timeout_ms > 0) {
        int ready = net_poll_readable(fd, timeout_ms);
        if (ready <= 0)
            return ready;
    }

    size_t total = 0;

    while (total < 3) {
        int n = recv(fd, (char *)(buf + total), (int)(3 - total), 0);
        if (n < 0) {
            int err = bytes_socket_error();
            if (err == BYTES_EINTR)
                continue;
            if (err == BYTES_EAGAIN || err == BYTES_EWOULDBLOCK)
                return 0;
            return -1;
        }
        if (n == 0)
            return -1;
        total += (size_t)n;
    }

    uint16_t payload_len = (uint16_t)(buf[1] | (buf[2] << 8));
    if ((size_t)(3 + payload_len) > buflen)
        return -1;

    while (total < (size_t)(3 + payload_len)) {
        int n = recv(fd, (char *)(buf + total),
                     (int)((size_t)(3 + payload_len) - total), 0);
        if (n < 0) {
            int err = bytes_socket_error();
            if (err == BYTES_EINTR)
                continue;
            if (err == BYTES_EAGAIN || err == BYTES_EWOULDBLOCK) {
                if (net_poll_readable(fd, 50) <= 0)
                    return -1;
                continue;
            }
            return -1;
        }
        if (n == 0)
            return -1;
        total += (size_t)n;
    }

    return (int)total;
}

int net_send_to_all(net_server_t *srv, const uint8_t *buf, size_t len)
{
    int count = 0;
    for (int i = 0; i < MAX_CLIENTS; i++) {
        if (srv->clients[i].connected) {
            if (net_send(srv->clients[i].fd, buf, len, 100) > 0)
                count++;
            else
                net_server_close_client(srv, i);
        }
    }
    return count;
}

int net_send_to_spectators(net_server_t *srv, const uint8_t *buf, size_t len)
{
    int count = 0;
    for (int i = 0; i < MAX_CLIENTS; i++) {
        if (srv->clients[i].connected && !srv->clients[i].is_player) {
            if (net_send(srv->clients[i].fd, buf, len, 100) > 0)
                count++;
            else
                net_server_close_client(srv, i);
        }
    }
    return count;
}

int net_poll_readable(bytes_socket_t fd, int timeout_ms)
{
#ifdef BYTES_WINDOWS
    WSAPOLLFD pfd;
    pfd.fd = fd;
    pfd.events = POLLRDNORM;
    pfd.revents = 0;

    int ret = WSAPoll(&pfd, 1, timeout_ms);

    if (ret > 0 && (pfd.revents & (POLLERR | POLLHUP | POLLNVAL)))
        return -1;
    return ret;
#else
    struct pollfd pfd;
    pfd.fd = fd;
    pfd.events = POLLIN;
    pfd.revents = 0;

    int ret;
    do {
        ret = poll(&pfd, 1, timeout_ms);
    } while (ret < 0 && errno == EINTR);

    if (ret > 0 && (pfd.revents & (POLLERR | POLLHUP | POLLNVAL)))
        return -1;
    return ret;
#endif
}

char *net_get_local_ip(char *buf, size_t buflen)
{
    return platform_get_local_ip(buf, buflen);
}
