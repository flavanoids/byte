#ifndef BYTES_COMMON_H
#define BYTES_COMMON_H

#include <stdint.h>
#include <stddef.h>

#define MAX_NAME_LEN     32
#define DEFAULT_PORT     7500
#define MAX_SPECTATORS   8
#define MAX_CLIENTS      (2 + MAX_SPECTATORS)
#define RECONNECT_TIMEOUT_SEC 30
#define TICK_RATE_HZ     30
#define TICK_INTERVAL_US (1000000 / TICK_RATE_HZ)
#define MAX_MSG_PAYLOAD  256
#define MSG_HEADER_SIZE  3

typedef enum {
    ROLE_PLAYER    = 0,
    ROLE_SPECTATOR = 1
} client_role_t;

typedef enum {
    GAME_PONG = 0
} game_type_t;

typedef enum {
    COLOR_P1       = 1,
    COLOR_P2       = 2,
    COLOR_BALL     = 3,
    COLOR_BORDER   = 4,
    COLOR_MENU     = 5,
    COLOR_ALERT    = 6,
    COLOR_DIM      = 7
} color_pair_t;

#endif
