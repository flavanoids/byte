#include "pong.h"
#include "ui.h"

#include <math.h>
#include <stdlib.h>
#include <string.h>

/* Unicode characters */
#define CH_BALL      "\u25CF"
#define CH_PADDLE    "\u2588"
#define CH_HLINE     "\u2500"
#define CH_VLINE     "\u2502"
#define CH_TL        "\u250C"
#define CH_TR        "\u2510"
#define CH_BL        "\u2514"
#define CH_BR        "\u2518"
#define CH_CENTER    "\u254E"
#define CH_T_RIGHT   "\u251C"
#define CH_T_LEFT    "\u2524"

static void pong_init(void *state, int rows, int cols)
{
    pong_state_t *s = (pong_state_t *)state;
    memset(s, 0, sizeof(*s));

    s->rows = rows;
    s->cols = cols;
    s->field_top = PONG_HEADER_ROWS;
    s->field_bottom = rows - 1;

    int field_h = s->field_bottom - s->field_top;
    int mid_y = s->field_top + field_h / 2;

    s->ball.x = (float)(cols / 2);
    s->ball.y = (float)mid_y;
    s->ball.vx = 1.0f;
    s->ball.vy = 0.5f;
    s->ball.speed = PONG_BALL_SPEED_INIT;
    s->ball.prev_x = s->ball.x;
    s->ball.prev_y = s->ball.y;

    s->p1.x = 2;
    s->p1.y = mid_y - PONG_PADDLE_LEN / 2;
    s->p1.len = PONG_PADDLE_LEN;

    s->p2.x = cols - 3;
    s->p2.y = mid_y - PONG_PADDLE_LEN / 2;
    s->p2.len = PONG_PADDLE_LEN;
}

static void reset_ball(pong_state_t *s)
{
    int field_h = s->field_bottom - s->field_top;
    int mid_y = s->field_top + field_h / 2;

    s->ball.x = (float)(s->cols / 2);
    s->ball.y = (float)mid_y;
    s->ball.speed = PONG_BALL_SPEED_INIT;

    /* Alternate direction based on who scored */
    s->ball.vx = (s->score1 + s->score2) % 2 == 0 ? 1.0f : -1.0f;
    s->ball.vy = ((float)(rand() % 100) / 100.0f) - 0.5f;
}

static void pong_handle_input(void *state, int player_id, int key)
{
    pong_state_t *s = (pong_state_t *)state;

    if (player_id == 1) {
        switch (key) {
        case 'w': case 'W': case KEY_UP:
            if (s->p1.y > s->field_top + 1)
                s->p1.y--;
            break;
        case 's': case 'S': case KEY_DOWN:
            if (s->p1.y + s->p1.len < s->field_bottom - 1)
                s->p1.y++;
            break;
        }
    } else {
        switch (key) {
        case KEY_UP: case 'w': case 'W':
            if (s->p2.y > s->field_top + 1)
                s->p2.y--;
            break;
        case KEY_DOWN: case 's': case 'S':
            if (s->p2.y + s->p2.len < s->field_bottom - 1)
                s->p2.y++;
            break;
        }
    }
}

static void pong_update(void *state)
{
    pong_state_t *s = (pong_state_t *)state;

    s->ball.prev_x = s->ball.x;
    s->ball.prev_y = s->ball.y;
    s->scored = false;

    float nx = s->ball.x + s->ball.vx * s->ball.speed;
    float ny = s->ball.y + s->ball.vy * s->ball.speed;

    /* Top/bottom wall bounce */
    if ((int)ny <= s->field_top) {
        ny = (float)(s->field_top + 1);
        s->ball.vy = -s->ball.vy;
    }
    if ((int)ny >= s->field_bottom - 1) {
        ny = (float)(s->field_bottom - 2);
        s->ball.vy = -s->ball.vy;
    }

    /* Left paddle collision */
    int bx = (int)roundf(nx);
    int by = (int)roundf(ny);

    if (bx <= s->p1.x + 1 && s->ball.vx < 0) {
        if (by >= s->p1.y && by < s->p1.y + s->p1.len) {
            nx = (float)(s->p1.x + 2);
            s->ball.vx = -s->ball.vx;
            /* Angle based on where ball hits paddle */
            float offset = (float)(by - s->p1.y) / (float)s->p1.len - 0.5f;
            s->ball.vy = offset * 2.0f;
            s->ball.speed += PONG_BALL_SPEED_INC;
            if (s->ball.speed > PONG_BALL_MAX_SPEED)
                s->ball.speed = PONG_BALL_MAX_SPEED;
        }
    }

    /* Right paddle collision */
    if (bx >= s->p2.x - 1 && s->ball.vx > 0) {
        if (by >= s->p2.y && by < s->p2.y + s->p2.len) {
            nx = (float)(s->p2.x - 2);
            s->ball.vx = -s->ball.vx;
            float offset = (float)(by - s->p2.y) / (float)s->p2.len - 0.5f;
            s->ball.vy = offset * 2.0f;
            s->ball.speed += PONG_BALL_SPEED_INC;
            if (s->ball.speed > PONG_BALL_MAX_SPEED)
                s->ball.speed = PONG_BALL_MAX_SPEED;
        }
    }

    /* Scoring */
    if (bx <= 0) {
        s->score2++;
        s->scored = true;
        reset_ball(s);
        return;
    }
    if (bx >= s->cols - 1) {
        s->score1++;
        s->scored = true;
        reset_ball(s);
        return;
    }

    s->ball.x = nx;
    s->ball.y = ny;
}

static void draw_field(const pong_state_t *s)
{
    int w = s->cols;
    int top = s->field_top - 1;
    int bot = s->field_bottom;

    attron(COLOR_PAIR(COLOR_BORDER));

    /* Top border with title */
    mvaddstr(top, 0, CH_TL);
    int title_pos = (w - 8) / 2;
    for (int i = 1; i < w - 1; i++) {
        if (i == title_pos) {
            attroff(COLOR_PAIR(COLOR_BORDER));
            attron(COLOR_PAIR(COLOR_MENU) | A_BOLD);
            addstr(" PONG ");
            attroff(COLOR_PAIR(COLOR_MENU) | A_BOLD);
            attron(COLOR_PAIR(COLOR_BORDER));
            i += 5;
        } else {
            addstr(CH_HLINE);
        }
    }
    addstr(CH_TR);

    /* Separator below header */
    mvaddstr(s->field_top, 0, CH_T_RIGHT);
    for (int i = 1; i < w - 1; i++)
        addstr(CH_HLINE);
    addstr(CH_T_LEFT);

    /* Side borders and center line */
    for (int y = s->field_top + 1; y < bot; y++) {
        mvaddstr(y, 0, CH_VLINE);
        mvaddstr(y, w - 1, CH_VLINE);

        attron(A_DIM);
        mvaddstr(y, w / 2, CH_CENTER);
        attroff(A_DIM);
    }

    /* Bottom border */
    mvaddstr(bot, 0, CH_BL);
    for (int i = 1; i < w - 1; i++)
        addstr(CH_HLINE);
    addstr(CH_BR);

    attroff(COLOR_PAIR(COLOR_BORDER));
}

static void pong_render(void *state, const char *p1_name, const char *p2_name,
                        bool is_spectator, int spectator_count)
{
    pong_state_t *s = (pong_state_t *)state;
    int w = s->cols;

    draw_field(s);

    /* Score header */
    int header_y = s->field_top - 1;

    attron(COLOR_PAIR(COLOR_P1) | A_BOLD);
    mvprintw(header_y + 1, 2, "%s: %d", p1_name, s->score1);
    attroff(COLOR_PAIR(COLOR_P1) | A_BOLD);

    char p2buf[48];
    snprintf(p2buf, sizeof(p2buf), "%d :%s", s->score2, p2_name);
    attron(COLOR_PAIR(COLOR_P2) | A_BOLD);
    mvaddstr(header_y + 1, w - 2 - (int)strlen(p2buf), p2buf);
    attroff(COLOR_PAIR(COLOR_P2) | A_BOLD);

    /* Ball trail (dim previous position) */
    int px = (int)roundf(s->ball.prev_x);
    int py = (int)roundf(s->ball.prev_y);
    if (px > 0 && px < w - 1 && py > s->field_top && py < s->field_bottom) {
        attron(COLOR_PAIR(COLOR_DIM) | A_DIM);
        mvaddstr(py, px, CH_BALL);
        attroff(COLOR_PAIR(COLOR_DIM) | A_DIM);
    }

    /* Ball */
    int bx = (int)roundf(s->ball.x);
    int by = (int)roundf(s->ball.y);
    if (bx > 0 && bx < w - 1 && by > s->field_top && by < s->field_bottom) {
        attron(COLOR_PAIR(COLOR_BALL) | A_BOLD);
        mvaddstr(by, bx, CH_BALL);
        attroff(COLOR_PAIR(COLOR_BALL) | A_BOLD);
    }

    /* Paddles */
    attron(COLOR_PAIR(COLOR_P1) | A_BOLD);
    for (int i = 0; i < s->p1.len; i++)
        mvaddstr(s->p1.y + i, s->p1.x, CH_PADDLE);
    attroff(COLOR_PAIR(COLOR_P1) | A_BOLD);

    attron(COLOR_PAIR(COLOR_P2) | A_BOLD);
    for (int i = 0; i < s->p2.len; i++)
        mvaddstr(s->p2.y + i, s->p2.x, CH_PADDLE);
    attroff(COLOR_PAIR(COLOR_P2) | A_BOLD);

    /* Spectator label */
    if (is_spectator) {
        attron(COLOR_PAIR(COLOR_DIM) | A_DIM);
        mvprintw(s->field_bottom, (w - 16) / 2, " [SPECTATING] ");
        attroff(COLOR_PAIR(COLOR_DIM) | A_DIM);
    }
    if (spectator_count > 0) {
        attron(COLOR_PAIR(COLOR_DIM) | A_DIM);
        mvprintw(s->field_bottom, w - 18, " %d watching ", spectator_count);
        attroff(COLOR_PAIR(COLOR_DIM) | A_DIM);
    }

    /* Terminal bell on score */
    if (s->scored) {
        putchar('\a');
        fflush(stdout);
    }
}

static int pong_pack_state(const void *state, uint8_t *buf, size_t buflen)
{
    const pong_state_t *s = (const pong_state_t *)state;
    size_t need = 10 * sizeof(int16_t) + sizeof(int16_t);
    if (buflen < need)
        return -1;

    int16_t *d = (int16_t *)buf;
    d[0] = (int16_t)(s->ball.x * 100.0f);
    d[1] = (int16_t)(s->ball.y * 100.0f);
    d[2] = (int16_t)(s->ball.vx * 100.0f);
    d[3] = (int16_t)(s->ball.vy * 100.0f);
    d[4] = (int16_t)(s->ball.speed * 100.0f);
    d[5] = (int16_t)s->p1.y;
    d[6] = (int16_t)s->p2.y;
    d[7] = (int16_t)s->score1;
    d[8] = (int16_t)s->score2;
    d[9] = (int16_t)(s->ball.prev_x * 100.0f);
    d[10] = (int16_t)(s->ball.prev_y * 100.0f);

    return (int)need;
}

static int pong_unpack_state(void *state, const uint8_t *buf, size_t len)
{
    pong_state_t *s = (pong_state_t *)state;
    size_t need = 11 * sizeof(int16_t);
    if (len < need)
        return -1;

    const int16_t *d = (const int16_t *)buf;
    s->ball.x = (float)d[0] / 100.0f;
    s->ball.y = (float)d[1] / 100.0f;
    s->ball.vx = (float)d[2] / 100.0f;
    s->ball.vy = (float)d[3] / 100.0f;
    s->ball.speed = (float)d[4] / 100.0f;
    s->p1.y = (int)d[5];
    s->p2.y = (int)d[6];
    s->score1 = (int)d[7];
    s->score2 = (int)d[8];
    s->ball.prev_x = (float)d[9] / 100.0f;
    s->ball.prev_y = (float)d[10] / 100.0f;

    /* Derive field dimensions from terminal if not set */
    if (s->rows == 0) {
        int rows, cols;
        getmaxyx(stdscr, rows, cols);
        s->rows = rows;
        s->cols = cols;
        s->field_top = PONG_HEADER_ROWS;
        s->field_bottom = rows - 1;
        s->p1.x = 2;
        s->p2.x = cols - 3;
        s->p1.len = PONG_PADDLE_LEN;
        s->p2.len = PONG_PADDLE_LEN;
    }

    return 0;
}

static bool pong_is_over(const void *state)
{
    const pong_state_t *s = (const pong_state_t *)state;
    return s->score1 >= PONG_WIN_SCORE || s->score2 >= PONG_WIN_SCORE;
}

static int pong_get_winner(const void *state)
{
    const pong_state_t *s = (const pong_state_t *)state;
    if (s->score1 >= PONG_WIN_SCORE) return 1;
    if (s->score2 >= PONG_WIN_SCORE) return 2;
    return 0;
}

const game_def_t pong_game_def = {
    .name         = "Pong",
    .type         = GAME_PONG,
    .state_size   = sizeof(pong_state_t),
    .init         = pong_init,
    .handle_input = pong_handle_input,
    .update       = pong_update,
    .render       = pong_render,
    .pack_state   = pong_pack_state,
    .unpack_state = pong_unpack_state,
    .is_over      = pong_is_over,
    .get_winner   = pong_get_winner
};
