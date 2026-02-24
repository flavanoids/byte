#ifndef BYTES_PONG_H
#define BYTES_PONG_H

#include "game.h"

#define PONG_WIN_SCORE   5
#define PONG_PADDLE_LEN  5
#define PONG_HEADER_ROWS 3
#define PONG_BALL_SPEED_INIT 1.0f
#define PONG_BALL_SPEED_INC  0.05f
#define PONG_BALL_MAX_SPEED  2.5f

typedef struct {
    float x, y;
    float vx, vy;
    float prev_x, prev_y;
    float speed;
} pong_ball_t;

typedef struct {
    int x, y;
    int len;
} pong_paddle_t;

typedef struct {
    pong_ball_t   ball;
    pong_paddle_t p1;
    pong_paddle_t p2;
    int           score1;
    int           score2;
    int           rows;
    int           cols;
    int           field_top;
    int           field_bottom;
    bool          scored;
} pong_state_t;

extern const game_def_t pong_game_def;

#endif
