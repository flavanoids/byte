#include "ui.h"

#include <locale.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <wchar.h>

static const char *TITLE_ART[] = {
    " ____        _            ",
    "|  _ \\      | |           ",
    "| |_) |_   _| |_ ___  ___",
    "|  _ <| | | | __/ _ \\/ __|",
    "| |_) | |_| | ||  __/\\__ \\",
    "|____/ \\__, |\\__\\___||___/",
    "        __/ |             ",
    "       |___/              "
};
#define TITLE_LINES 8

static const char *MENU_ITEMS[] = {
    "Host Game",
    "Join Game",
    "Solo Play",
    "Watch Game",
    "Stats",
    "Quit"
};

void ui_init(void)
{
    setlocale(LC_ALL, "");
    initscr();
    ESCDELAY = 25;
    cbreak();
    noecho();
    curs_set(0);
    keypad(stdscr, TRUE);
    nodelay(stdscr, FALSE);

    if (has_colors()) {
        start_color();
        use_default_colors();
        init_pair(COLOR_P1,     COLOR_CYAN,    -1);
        init_pair(COLOR_P2,     COLOR_MAGENTA, -1);
        init_pair(COLOR_BALL,   COLOR_YELLOW,  -1);
        init_pair(COLOR_BORDER, COLOR_WHITE,   -1);
        init_pair(COLOR_MENU,   COLOR_GREEN,   -1);
        init_pair(COLOR_ALERT,  COLOR_RED,     -1);
        init_pair(COLOR_DIM,    COLOR_WHITE,   -1);
    }
}

void ui_cleanup(void)
{
    endwin();
}

void ui_draw_box(int y, int x, int h, int w)
{
    attron(COLOR_PAIR(COLOR_BORDER));
    mvaddstr(y, x, "\u250C");
    for (int i = 1; i < w - 1; i++)
        addstr("\u2500");
    addstr("\u2510");

    for (int i = 1; i < h - 1; i++) {
        mvaddstr(y + i, x, "\u2502");
        mvaddstr(y + i, x + w - 1, "\u2502");
    }

    mvaddstr(y + h - 1, x, "\u2514");
    for (int i = 1; i < w - 1; i++)
        addstr("\u2500");
    addstr("\u2518");
    attroff(COLOR_PAIR(COLOR_BORDER));
}

menu_choice_t ui_main_menu(void)
{
    int selected = 0;
    int rows, cols;

    keypad(stdscr, TRUE);
    nodelay(stdscr, FALSE);

    while (1) {
        getmaxyx(stdscr, rows, cols);
        clear();

        int title_y = rows / 2 - TITLE_LINES - 3;
        if (title_y < 1) title_y = 1;

        attron(COLOR_PAIR(COLOR_MENU) | A_BOLD);
        for (int i = 0; i < TITLE_LINES; i++) {
            int tx = (cols - 28) / 2;
            if (tx < 0) tx = 0;
            mvaddstr(title_y + i, tx, TITLE_ART[i]);
        }
        attroff(COLOR_PAIR(COLOR_MENU) | A_BOLD);

        int menu_y = title_y + TITLE_LINES + 2;
        for (int i = 0; i < MENU_COUNT; i++) {
            int mx = (cols - (int)strlen(MENU_ITEMS[i])) / 2;
            if (mx < 0) mx = 0;
            if (i == selected) {
                attron(COLOR_PAIR(COLOR_MENU) | A_REVERSE | A_BOLD);
                mvprintw(menu_y + i, mx - 2, "  %s  ", MENU_ITEMS[i]);
                attroff(COLOR_PAIR(COLOR_MENU) | A_REVERSE | A_BOLD);
            } else {
                attron(COLOR_PAIR(COLOR_BORDER));
                mvaddstr(menu_y + i, mx, MENU_ITEMS[i]);
                attroff(COLOR_PAIR(COLOR_BORDER));
            }
        }

        attron(COLOR_PAIR(COLOR_DIM) | A_DIM);
        mvaddstr(rows - 2, (cols - 30) / 2, "Use arrows to select, Enter to");
        mvaddstr(rows - 1, (cols - 15) / 2, "confirm, q quit");
        attroff(COLOR_PAIR(COLOR_DIM) | A_DIM);

        refresh();

        int key = getch();
        switch (key) {
        case KEY_UP: case 'k': case 'w': case 'W':
            selected = (selected - 1 + MENU_COUNT) % MENU_COUNT;
            break;
        case KEY_DOWN: case 'j': case 's': case 'S':
            selected = (selected + 1) % MENU_COUNT;
            break;
        case '\n': case '\r':
            return (menu_choice_t)selected;
        case 'q':
            return MENU_QUIT;
        }
    }
}

void ui_get_name(char *name, size_t maxlen)
{
    int rows, cols;
    getmaxyx(stdscr, rows, cols);
    clear();

    int cy = rows / 2 - 1;
    int cx = (cols - 30) / 2;
    if (cx < 2) cx = 2;

    attron(COLOR_PAIR(COLOR_MENU) | A_BOLD);
    mvaddstr(cy, cx, "Enter your name:");
    attroff(COLOR_PAIR(COLOR_MENU) | A_BOLD);

    attron(COLOR_PAIR(COLOR_BORDER));
    mvhline(cy + 2, cx, '_', (int)maxlen - 1);
    attroff(COLOR_PAIR(COLOR_BORDER));

    move(cy + 2, cx);
    curs_set(1);
    echo();
    refresh();

    getnstr(name, (int)maxlen - 1);
    name[maxlen - 1] = '\0';

    if (name[0] == '\0')
        strncpy(name, "Player", maxlen - 1);

    noecho();
    curs_set(0);
    keypad(stdscr, TRUE);
}

void ui_get_host_and_port(char *host, size_t hostlen, int *port, int default_port)
{
    int rows, cols;
    getmaxyx(stdscr, rows, cols);
    clear();

    int cy = rows / 2 - 2;
    int cx = (cols - 30) / 2;
    if (cx < 2) cx = 2;

    attron(COLOR_PAIR(COLOR_MENU) | A_BOLD);
    mvaddstr(cy, cx, "Server address:");
    attroff(COLOR_PAIR(COLOR_MENU) | A_BOLD);

    attron(COLOR_PAIR(COLOR_BORDER));
    mvhline(cy + 1, cx, '_', 20);
    attroff(COLOR_PAIR(COLOR_BORDER));

    move(cy + 1, cx);
    curs_set(1);
    echo();
    refresh();

    char input[64];
    getnstr(input, 63);
    input[63] = '\0';

    /* Parse host:port or just host */
    char *colon = strrchr(input, ':');
    if (colon != NULL) {
        *colon = '\0';
        int p = atoi(colon + 1);
        if (p > 0 && p < 65536)
            *port = p;
        else
            *port = default_port;
    } else {
        *port = default_port;
    }

    if (input[0] == '\0')
        strncpy(host, "127.0.0.1", hostlen - 1);
    else
        strncpy(host, input, hostlen - 1);
    host[hostlen - 1] = '\0';

    noecho();
    curs_set(0);
    keypad(stdscr, TRUE);
}

void ui_waiting_screen(const char *player_name, const char *ip, int port)
{
    int rows, cols;
    getmaxyx(stdscr, rows, cols);
    clear();

    int cy = rows / 2 - 3;
    int cx;

    attron(COLOR_PAIR(COLOR_MENU) | A_BOLD);
    const char *msg = "Waiting for another player...";
    cx = (cols - (int)strlen(msg)) / 2;
    mvaddstr(cy, cx, msg);
    attroff(COLOR_PAIR(COLOR_MENU) | A_BOLD);

    attron(COLOR_PAIR(COLOR_P1) | A_BOLD);
    char namebuf[64];
    snprintf(namebuf, sizeof(namebuf), "You: %s", player_name);
    cx = (cols - (int)strlen(namebuf)) / 2;
    mvaddstr(cy + 2, cx, namebuf);
    attroff(COLOR_PAIR(COLOR_P1) | A_BOLD);

    attron(COLOR_PAIR(COLOR_BORDER));
    char addrbuf[80];
    snprintf(addrbuf, sizeof(addrbuf), "Server: %s:%d", ip, port);
    cx = (cols - (int)strlen(addrbuf)) / 2;
    mvaddstr(cy + 4, cx, addrbuf);
    attroff(COLOR_PAIR(COLOR_BORDER));

    attron(COLOR_PAIR(COLOR_DIM) | A_DIM);
    const char *hint = "Other players connect with this address";
    cx = (cols - (int)strlen(hint)) / 2;
    mvaddstr(cy + 6, cx, hint);
    attroff(COLOR_PAIR(COLOR_DIM) | A_DIM);

    refresh();
}

void ui_player_joined(const char *p1_name, const char *p2_name)
{
    int rows, cols;
    getmaxyx(stdscr, rows, cols);
    clear();

    int cy = rows / 2 - 2;

    attron(COLOR_PAIR(COLOR_MENU) | A_BOLD);
    const char *msg = "Player connected!";
    mvaddstr(cy, (cols - (int)strlen(msg)) / 2, msg);
    attroff(COLOR_PAIR(COLOR_MENU) | A_BOLD);

    attron(COLOR_PAIR(COLOR_P1) | A_BOLD);
    char buf1[64];
    snprintf(buf1, sizeof(buf1), "Player 1: %s", p1_name);
    mvaddstr(cy + 2, (cols - (int)strlen(buf1)) / 2, buf1);
    attroff(COLOR_PAIR(COLOR_P1) | A_BOLD);

    attron(COLOR_PAIR(COLOR_P2) | A_BOLD);
    char buf2[64];
    snprintf(buf2, sizeof(buf2), "Player 2: %s", p2_name);
    mvaddstr(cy + 3, (cols - (int)strlen(buf2)) / 2, buf2);
    attroff(COLOR_PAIR(COLOR_P2) | A_BOLD);

    refresh();
    usleep(1500000);
}

void ui_countdown(const char *p1_name, const char *p2_name)
{
    int rows, cols;
    getmaxyx(stdscr, rows, cols);

    for (int i = 3; i >= 1; i--) {
        clear();

        attron(COLOR_PAIR(COLOR_P1) | A_BOLD);
        mvaddstr(1, 2, p1_name);
        attroff(COLOR_PAIR(COLOR_P1) | A_BOLD);

        attron(COLOR_PAIR(COLOR_BORDER));
        addstr(" vs ");
        attroff(COLOR_PAIR(COLOR_BORDER));

        attron(COLOR_PAIR(COLOR_P2) | A_BOLD);
        addstr(p2_name);
        attroff(COLOR_PAIR(COLOR_P2) | A_BOLD);

        char num[4];
        snprintf(num, sizeof(num), "%d", i);
        attron(COLOR_PAIR(COLOR_MENU) | A_BOLD);
        mvaddstr(rows / 2, (cols - 1) / 2, num);
        attroff(COLOR_PAIR(COLOR_MENU) | A_BOLD);

        refresh();
        usleep(1000000);
    }

    clear();
    attron(COLOR_PAIR(COLOR_MENU) | A_BOLD);
    const char *go = "GO!";
    mvaddstr(rows / 2, (cols - (int)strlen(go)) / 2, go);
    attroff(COLOR_PAIR(COLOR_MENU) | A_BOLD);
    refresh();
    usleep(500000);
}

void ui_game_over(const char *winner_name, bool you_won)
{
    int rows, cols;
    getmaxyx(stdscr, rows, cols);
    clear();

    int cy = rows / 2 - 3;

    if (you_won) {
        attron(COLOR_PAIR(COLOR_MENU) | A_BOLD);
        const char *msg = "YOU WIN!";
        mvaddstr(cy, (cols - (int)strlen(msg)) / 2, msg);
        attroff(COLOR_PAIR(COLOR_MENU) | A_BOLD);
    } else {
        attron(COLOR_PAIR(COLOR_ALERT) | A_BOLD);
        const char *msg = "YOU LOSE!";
        mvaddstr(cy, (cols - (int)strlen(msg)) / 2, msg);
        attroff(COLOR_PAIR(COLOR_ALERT) | A_BOLD);
    }

    attron(COLOR_PAIR(COLOR_BORDER) | A_BOLD);
    char buf[64];
    snprintf(buf, sizeof(buf), "Winner: %s", winner_name);
    mvaddstr(cy + 2, (cols - (int)strlen(buf)) / 2, buf);
    attroff(COLOR_PAIR(COLOR_BORDER) | A_BOLD);

    /* Terminal bell */
    putchar('\a');
    fflush(stdout);

    attron(COLOR_PAIR(COLOR_DIM) | A_DIM);
    const char *hint = "Press any key to continue...";
    mvaddstr(cy + 5, (cols - (int)strlen(hint)) / 2, hint);
    attroff(COLOR_PAIR(COLOR_DIM) | A_DIM);

    refresh();
    nodelay(stdscr, FALSE);
    getch();
}

void ui_pause_overlay(int seconds_left)
{
    int rows, cols;
    getmaxyx(stdscr, rows, cols);

    int bw = 40;
    int bh = 5;
    int by = rows / 2 - bh / 2;
    int bx = (cols - bw) / 2;

    attron(COLOR_PAIR(COLOR_ALERT));
    ui_draw_box(by, bx, bh, bw);
    attroff(COLOR_PAIR(COLOR_ALERT));

    attron(COLOR_PAIR(COLOR_ALERT) | A_BOLD);
    const char *msg = "Player disconnected!";
    mvaddstr(by + 1, bx + (bw - (int)strlen(msg)) / 2, msg);
    attroff(COLOR_PAIR(COLOR_ALERT) | A_BOLD);

    char timebuf[40];
    snprintf(timebuf, sizeof(timebuf), "Waiting %ds for reconnect...", seconds_left);
    attron(COLOR_PAIR(COLOR_BORDER));
    mvaddstr(by + 3, bx + (bw - (int)strlen(timebuf)) / 2, timebuf);
    attroff(COLOR_PAIR(COLOR_BORDER));

    refresh();
}

void ui_show_message(const char *msg)
{
    int rows, cols;
    getmaxyx(stdscr, rows, cols);
    clear();

    attron(COLOR_PAIR(COLOR_BORDER) | A_BOLD);
    mvaddstr(rows / 2, (cols - (int)strlen(msg)) / 2, msg);
    attroff(COLOR_PAIR(COLOR_BORDER) | A_BOLD);

    refresh();
}
