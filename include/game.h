#ifndef BYTES_GAME_H
#define BYTES_GAME_H

#include "common.h"
#include "network.h"
#include <ncurses.h>
#include <stdbool.h>
#include <stdint.h>

typedef struct game_def game_def_t;

struct game_def {
    const char *name;
    game_type_t type;
    size_t      state_size;

    void (*init)(void *state, int rows, int cols);
    void (*handle_input)(void *state, int player_id, int key);
    void (*update)(void *state);
    void (*render)(void *state, const char *p1_name, const char *p2_name,
                   bool is_spectator, int spectator_count);
    int  (*pack_state)(const void *state, uint8_t *buf, size_t buflen);
    int  (*unpack_state)(void *state, const uint8_t *buf, size_t len);
    bool (*is_over)(const void *state);
    int  (*get_winner)(const void *state);
};

typedef struct {
    const game_def_t *def;
    void             *state;
    char              p1_name[MAX_NAME_LEN];
    char              p2_name[MAX_NAME_LEN];
    bool              paused;
    bool              running;
    bool              is_server;
    bool              is_spectator;
    int               spectator_count;
    uint8_t           local_player_id;
} game_session_t;

void game_session_init(game_session_t *gs, const game_def_t *def,
                       const char *p1, const char *p2,
                       bool is_server, bool is_spectator,
                       uint8_t local_player_id);
void game_session_cleanup(game_session_t *gs);

void game_run_server(game_session_t *gs, net_server_t *srv, int player_client_idx);
void game_run_client(game_session_t *gs, net_connection_t *conn);
void game_run_spectator(game_session_t *gs, net_connection_t *conn);

const game_def_t *game_get_def(game_type_t type);
int game_get_count(void);
const char *game_get_name(int index);

#endif
