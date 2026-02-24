#ifndef BYTES_UI_H
#define BYTES_UI_H

#include "common.h"
#include <ncurses.h>
#include <stdbool.h>

typedef enum {
    MENU_HOST = 0,
    MENU_JOIN,
    MENU_SOLO,
    MENU_WATCH,
    MENU_STATS,
    MENU_QUIT,
    MENU_COUNT
} menu_choice_t;

void ui_init(void);
void ui_cleanup(void);

menu_choice_t ui_main_menu(void);
void ui_get_name(char *name, size_t maxlen);
void ui_get_host_and_port(char *host, size_t hostlen, int *port, int default_port);
void ui_waiting_screen(const char *player_name, const char *ip, int port);
void ui_player_joined(const char *p1_name, const char *p2_name);
void ui_countdown(const char *p1_name, const char *p2_name);
void ui_game_over(const char *winner_name, bool you_won);
void ui_pause_overlay(int seconds_left);
void ui_show_message(const char *msg);

void ui_draw_box(int y, int x, int h, int w);

#endif
