#ifndef BYTES_PLATFORM_H
#define BYTES_PLATFORM_H

/* ── Platform detection ─────────────────────────────────────────── */

#if defined(_WIN32) || defined(_WIN64)
    #define BYTES_WINDOWS 1
#elif defined(__APPLE__) && defined(__MACH__)
    #define BYTES_MACOS   1
#else
    #define BYTES_LINUX   1
#endif

/* ── Headers ────────────────────────────────────────────────────── */

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

#ifdef BYTES_WINDOWS

    #ifndef _WIN32_WINNT
        #define _WIN32_WINNT 0x0600
    #endif
    #include <winsock2.h>
    #include <ws2tcpip.h>
    #include <windows.h>
    #include <iphlpapi.h>

    /* Resolve MOUSE_MOVED conflict between wincontypes.h and PDCurses */
    #ifdef MOUSE_MOVED
        #undef MOUSE_MOVED
    #endif
    #include <curses.h>

    typedef SOCKET bytes_socket_t;
    #define BYTES_INVALID_SOCKET INVALID_SOCKET

    #define BYTES_MSG_NOSIGNAL 0
    #define BYTES_EAGAIN       WSAEWOULDBLOCK
    #define BYTES_EWOULDBLOCK  WSAEWOULDBLOCK
    #define BYTES_EINPROGRESS  WSAEWOULDBLOCK
    #define BYTES_EINTR        WSAEINTR

    static inline int bytes_socket_error(void) { return WSAGetLastError(); }

#else /* POSIX (Linux + macOS) */

    #include <arpa/inet.h>
    #include <errno.h>
    #include <fcntl.h>
    #include <ifaddrs.h>
    #include <net/if.h>
    #include <netdb.h>
    #include <netinet/in.h>
    #include <netinet/tcp.h>
    #include <poll.h>
    #include <signal.h>
    #include <sys/socket.h>
    #include <sys/types.h>
    #include <unistd.h>
    #include <ncurses.h>

    typedef int bytes_socket_t;
    #define BYTES_INVALID_SOCKET (-1)

    #ifdef BYTES_MACOS
        #define BYTES_MSG_NOSIGNAL 0
    #else
        #define BYTES_MSG_NOSIGNAL MSG_NOSIGNAL
    #endif

    #define BYTES_EAGAIN      EAGAIN
    #define BYTES_EWOULDBLOCK EWOULDBLOCK
    #define BYTES_EINPROGRESS EINPROGRESS
    #define BYTES_EINTR       EINTR

    static inline int bytes_socket_error(void) { return errno; }

#endif

/* ── Packed struct macros ───────────────────────────────────────── */

#ifdef _MSC_VER
    #define BYTES_PACKED_BEGIN __pragma(pack(push, 1))
    #define BYTES_PACKED_END   __pragma(pack(pop))
    #define BYTES_PACKED_ATTR
#else
    #define BYTES_PACKED_BEGIN
    #define BYTES_PACKED_END
    #define BYTES_PACKED_ATTR __attribute__((packed))
#endif

/* ── Platform functions ─────────────────────────────────────────── */

int      platform_net_init(void);
void     platform_net_cleanup(void);

int      platform_set_nonblocking(bytes_socket_t fd);
int      platform_set_nosigpipe(bytes_socket_t fd);
void     platform_close_socket(bytes_socket_t fd);

int64_t  platform_mono_us(void);
void     platform_usleep(unsigned us);
void     platform_ignore_sigpipe(void);

void     platform_get_home_dir(char *buf, size_t len);
char    *platform_get_local_ip(char *buf, size_t buflen);

#endif
