#include "platform.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

/* ── Network init / cleanup ─────────────────────────────────────── */

#ifdef BYTES_WINDOWS

int platform_net_init(void)
{
    WSADATA wsa;
    return WSAStartup(MAKEWORD(2, 2), &wsa) == 0 ? 0 : -1;
}

void platform_net_cleanup(void)
{
    WSACleanup();
}

#else

int platform_net_init(void)
{
    return 0;
}

void platform_net_cleanup(void)
{
}

#endif

/* ── Socket helpers ─────────────────────────────────────────────── */

#ifdef BYTES_WINDOWS

int platform_set_nonblocking(bytes_socket_t fd)
{
    u_long mode = 1;
    return ioctlsocket(fd, FIONBIO, &mode) == 0 ? 0 : -1;
}

void platform_close_socket(bytes_socket_t fd)
{
    if (fd != BYTES_INVALID_SOCKET)
        closesocket(fd);
}

int platform_set_nosigpipe(bytes_socket_t fd)
{
    (void)fd;
    return 0;
}

#else

int platform_set_nonblocking(bytes_socket_t fd)
{
    int flags = fcntl(fd, F_GETFL, 0);
    if (flags == -1)
        return -1;
    return fcntl(fd, F_SETFL, flags | O_NONBLOCK);
}

void platform_close_socket(bytes_socket_t fd)
{
    if (fd >= 0)
        close(fd);
}

int platform_set_nosigpipe(bytes_socket_t fd)
{
#ifdef BYTES_MACOS
    int on = 1;
    return setsockopt(fd, SOL_SOCKET, SO_NOSIGPIPE, &on, sizeof(on));
#else
    (void)fd;
    return 0;
#endif
}

#endif

/* ── Monotonic clock ────────────────────────────────────────────── */

#ifdef BYTES_WINDOWS

int64_t platform_mono_us(void)
{
    static LARGE_INTEGER freq = {0};
    if (freq.QuadPart == 0)
        QueryPerformanceFrequency(&freq);

    LARGE_INTEGER now;
    QueryPerformanceCounter(&now);
    return (int64_t)(now.QuadPart * 1000000 / freq.QuadPart);
}

#else

int64_t platform_mono_us(void)
{
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (int64_t)ts.tv_sec * 1000000 + ts.tv_nsec / 1000;
}

#endif

/* ── Sleep ──────────────────────────────────────────────────────── */

#ifdef BYTES_WINDOWS

void platform_usleep(unsigned us)
{
    Sleep(us / 1000);
}

#else

void platform_usleep(unsigned us)
{
    usleep(us);
}

#endif

/* ── Signal handling ────────────────────────────────────────────── */

void platform_ignore_sigpipe(void)
{
#ifndef BYTES_WINDOWS
    signal(SIGPIPE, SIG_IGN);
#endif
}

/* ── Home directory ─────────────────────────────────────────────── */

void platform_get_home_dir(char *buf, size_t len)
{
#ifdef BYTES_WINDOWS
    const char *appdata = getenv("APPDATA");
    if (appdata != NULL) {
        snprintf(buf, len, "%s", appdata);
        return;
    }
    const char *userprofile = getenv("USERPROFILE");
    if (userprofile != NULL) {
        snprintf(buf, len, "%s", userprofile);
        return;
    }
    snprintf(buf, len, ".");
#else
    const char *home = getenv("HOME");
    if (home != NULL)
        snprintf(buf, len, "%s", home);
    else
        snprintf(buf, len, ".");
#endif
}

/* ── Local IP discovery ─────────────────────────────────────────── */

#ifdef BYTES_WINDOWS

char *platform_get_local_ip(char *buf, size_t buflen)
{
    PIP_ADAPTER_ADDRESSES addrs = NULL;
    ULONG outlen = 15000;
    DWORD ret;

    addrs = (PIP_ADAPTER_ADDRESSES)malloc(outlen);
    if (addrs == NULL) {
        snprintf(buf, buflen, "127.0.0.1");
        return buf;
    }

    ret = GetAdaptersAddresses(AF_INET, GAA_FLAG_SKIP_ANYCAST |
                               GAA_FLAG_SKIP_MULTICAST |
                               GAA_FLAG_SKIP_DNS_SERVER, NULL, addrs, &outlen);
    if (ret != NO_ERROR) {
        free(addrs);
        snprintf(buf, buflen, "127.0.0.1");
        return buf;
    }

    PIP_ADAPTER_ADDRESSES cur;
    for (cur = addrs; cur != NULL; cur = cur->Next) {
        if (cur->OperStatus != IfOperStatusUp)
            continue;
        if (cur->IfType == IF_TYPE_SOFTWARE_LOOPBACK)
            continue;

        PIP_ADAPTER_UNICAST_ADDRESS ua = cur->FirstUnicastAddress;
        for (; ua != NULL; ua = ua->Next) {
            if (ua->Address.lpSockaddr->sa_family == AF_INET) {
                struct sockaddr_in *sa = (struct sockaddr_in *)ua->Address.lpSockaddr;
                inet_ntop(AF_INET, &sa->sin_addr, buf, (socklen_t)buflen);
                free(addrs);
                return buf;
            }
        }
    }

    free(addrs);
    snprintf(buf, buflen, "127.0.0.1");
    return buf;
}

#else

char *platform_get_local_ip(char *buf, size_t buflen)
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

#endif
