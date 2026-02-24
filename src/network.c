#include "network.h"

#include <arpa/inet.h>
#include <errno.h>
#include <fcntl.h>
#include <ifaddrs.h>
#include <net/if.h>
#include <netdb.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <poll.h>
#include <stdio.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>

static int set_nonblocking(int fd)
{
    int flags = fcntl(fd, F_GETFL, 0);
    if (flags == -1)
        return -1;
    return fcntl(fd, F_SETFL, flags | O_NONBLOCK);
}

static int set_tcp_nodelay(int fd)
{
    int flag = 1;
    return setsockopt(fd, IPPROTO_TCP, TCP_NODELAY, &flag, sizeof(flag));
}

static int set_keepalive(int fd)
{
    int on = 1;
    if (setsockopt(fd, SOL_SOCKET, SO_KEEPALIVE, &on, sizeof(on)) < 0)
        return -1;
#ifdef TCP_KEEPIDLE
    int idle = 5;
    setsockopt(fd, IPPROTO_TCP, TCP_KEEPIDLE, &idle, sizeof(idle));
#endif
#ifdef TCP_KEEPINTVL
    int intvl = 2;
    setsockopt(fd, IPPROTO_TCP, TCP_KEEPINTVL, &intvl, sizeof(intvl));
#endif
#ifdef TCP_KEEPCNT
    int cnt = 3;
    setsockopt(fd, IPPROTO_TCP, TCP_KEEPCNT, &cnt, sizeof(cnt));
#endif
    return 0;
}

static int net_poll_writable(int fd, int timeout_ms)
{
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
}

int net_server_init(net_server_t *srv, int port)
{
    memset(srv, 0, sizeof(*srv));
    srv->port = port;
    srv->listen_fd = -1;

    int fd = socket(AF_INET, SOCK_STREAM, 0);
    if (fd < 0)
        return -1;

    int opt = 1;
    setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

    struct sockaddr_in addr;
    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = INADDR_ANY;
    addr.sin_port = htons((uint16_t)port);

    if (bind(fd, (struct sockaddr *)&addr, sizeof(addr)) < 0) {
        close(fd);
        return -1;
    }

    if (listen(fd, MAX_CLIENTS) < 0) {
        close(fd);
        return -1;
    }

    set_nonblocking(fd);
    srv->listen_fd = fd;
    srv->running = true;

    for (int i = 0; i < MAX_CLIENTS; i++) {
        srv->clients[i].fd = -1;
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
    int cfd = accept(srv->listen_fd, (struct sockaddr *)&peer, &peerlen);
    if (cfd < 0)
        return -1;

    set_tcp_nodelay(cfd);
    set_nonblocking(cfd);
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

    close(cfd);
    return -1;
}

void net_server_close_client(net_server_t *srv, int idx)
{
    if (idx < 0 || idx >= MAX_CLIENTS)
        return;
    if (!srv->clients[idx].connected)
        return;

    close(srv->clients[idx].fd);
    srv->clients[idx].fd = -1;
    srv->clients[idx].connected = false;
    srv->client_count--;
}

void net_server_shutdown(net_server_t *srv)
{
    srv->running = false;
    for (int i = 0; i < MAX_CLIENTS; i++) {
        if (srv->clients[i].connected) {
            close(srv->clients[i].fd);
            srv->clients[i].fd = -1;
            srv->clients[i].connected = false;
        }
    }
    if (srv->listen_fd >= 0) {
        close(srv->listen_fd);
        srv->listen_fd = -1;
    }
    srv->client_count = 0;
}

int net_client_connect(net_connection_t *conn, const char *host, int port)
{
    memset(conn, 0, sizeof(*conn));
    conn->fd = -1;

    struct addrinfo hints, *res, *p;
    memset(&hints, 0, sizeof(hints));
    hints.ai_family = AF_INET;
    hints.ai_socktype = SOCK_STREAM;

    char port_str[8];
    snprintf(port_str, sizeof(port_str), "%d", port);

    if (getaddrinfo(host, port_str, &hints, &res) != 0)
        return -1;

    int fd = -1;
    for (p = res; p != NULL; p = p->ai_next) {
        fd = socket(p->ai_family, p->ai_socktype, p->ai_protocol);
        if (fd < 0)
            continue;

        set_nonblocking(fd);

        int rc = connect(fd, p->ai_addr, p->ai_addrlen);
        if (rc == 0)
            break;

        if (errno != EINPROGRESS) {
            close(fd);
            fd = -1;
            continue;
        }

        int ready = net_poll_writable(fd, 5000);
        if (ready <= 0) {
            close(fd);
            fd = -1;
            continue;
        }

        int sockerr = 0;
        socklen_t errlen = sizeof(sockerr);
        getsockopt(fd, SOL_SOCKET, SO_ERROR, &sockerr, &errlen);
        if (sockerr != 0) {
            close(fd);
            fd = -1;
            continue;
        }

        break;
    }
    freeaddrinfo(res);

    if (fd < 0)
        return -1;

    set_tcp_nodelay(fd);
    set_keepalive(fd);
    conn->fd = fd;
    conn->connected = true;
    return 0;
}

void net_client_disconnect(net_connection_t *conn)
{
    if (conn->fd >= 0) {
        close(conn->fd);
        conn->fd = -1;
    }
    conn->connected = false;
}

int net_send(int fd, const uint8_t *buf, size_t len, int timeout_ms)
{
    if (timeout_ms <= 0)
        timeout_ms = 100;

    size_t sent = 0;
    while (sent < len) {
        ssize_t n = send(fd, buf + sent, len - sent, MSG_NOSIGNAL);
        if (n < 0) {
            if (errno == EINTR)
                continue;
            if (errno == EAGAIN || errno == EWOULDBLOCK) {
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

int net_recv(int fd, uint8_t *buf, size_t buflen, int timeout_ms)
{
    if (timeout_ms > 0) {
        int ready = net_poll_readable(fd, timeout_ms);
        if (ready <= 0)
            return ready;
    }

    size_t total = 0;

    /* Read header first (3 bytes) */
    while (total < 3) {
        ssize_t n = recv(fd, buf + total, 3 - total, 0);
        if (n < 0) {
            if (errno == EINTR)
                continue;
            if (errno == EAGAIN || errno == EWOULDBLOCK)
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

    /* Read payload */
    while (total < (size_t)(3 + payload_len)) {
        ssize_t n = recv(fd, buf + total, (size_t)(3 + payload_len) - total, 0);
        if (n < 0) {
            if (errno == EINTR)
                continue;
            if (errno == EAGAIN || errno == EWOULDBLOCK) {
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

int net_poll_readable(int fd, int timeout_ms)
{
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
}

char *net_get_local_ip(char *buf, size_t buflen)
{
    struct ifaddrs *ifaddr, *ifa;

    if (getifaddrs(&ifaddr) == -1) {
        snprintf(buf, buflen, "127.0.0.1");
        return buf;
    }

    for (ifa = ifaddr; ifa != NULL; ifa = ifa->ifa_next) {
        if (ifa->ifa_addr == NULL)
            continue;
        if (ifa->ifa_addr->sa_family != AF_INET)
            continue;
        if (ifa->ifa_flags & IFF_LOOPBACK)
            continue;
        if (!(ifa->ifa_flags & IFF_UP))
            continue;

        struct sockaddr_in *sa = (struct sockaddr_in *)ifa->ifa_addr;
        inet_ntop(AF_INET, &sa->sin_addr, buf, (socklen_t)buflen);
        freeifaddrs(ifaddr);
        return buf;
    }

    freeifaddrs(ifaddr);
    snprintf(buf, buflen, "127.0.0.1");
    return buf;
}
