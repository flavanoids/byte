#ifndef BYTES_NETWORK_H
#define BYTES_NETWORK_H

#include "common.h"
#include <stdbool.h>

typedef struct {
    int  fd;
    bool connected;
    bool is_player;
    uint8_t player_id;
    char name[MAX_NAME_LEN];
} net_client_t;

typedef struct {
    int listen_fd;
    int port;
    net_client_t clients[MAX_CLIENTS];
    int client_count;
    bool running;
} net_server_t;

typedef struct {
    int fd;
    bool connected;
    char local_name[MAX_NAME_LEN];
    uint8_t role;
} net_connection_t;

int  net_server_init(net_server_t *srv, int port);
int  net_server_accept(net_server_t *srv, int timeout_ms);
void net_server_close_client(net_server_t *srv, int idx);
void net_server_shutdown(net_server_t *srv);

int  net_client_connect(net_connection_t *conn, const char *host, int port);
void net_client_disconnect(net_connection_t *conn);

int  net_send(int fd, const uint8_t *buf, size_t len, int timeout_ms);
int  net_recv(int fd, uint8_t *buf, size_t buflen, int timeout_ms);
int  net_send_to_all(net_server_t *srv, const uint8_t *buf, size_t len);
int  net_send_to_spectators(net_server_t *srv, const uint8_t *buf, size_t len);

int  net_poll_readable(int fd, int timeout_ms);

char *net_get_local_ip(char *buf, size_t buflen);

#endif
