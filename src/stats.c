#include "stats.h"
#include "common.h"
#include "ui.h"
#include "platform.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

void stats_init(stats_t *st)
{
    memset(st, 0, sizeof(*st));

    char home[240];
    platform_get_home_dir(home, sizeof(home));
    snprintf(st->path, sizeof(st->path), "%s/.bytes_stats", home);
}

bool stats_load(stats_t *st)
{
    FILE *f = fopen(st->path, "r");
    if (f == NULL)
        return false;

    char line[128];
    while (fgets(line, sizeof(line), f) != NULL && st->count < STATS_MAX_ENTRIES) {
        char *eq = strchr(line, '=');
        if (eq == NULL)
            continue;

        *eq = '\0';
        const char *key = line;
        const char *val = eq + 1;

        char *nl = strchr(val, '\n');
        if (nl != NULL)
            *nl = '\0';

        strncpy(st->entries[st->count].key, key, STATS_KEY_LEN - 1);
        st->entries[st->count].key[STATS_KEY_LEN - 1] = '\0';
        st->entries[st->count].value = atoi(val);
        st->count++;
    }

    fclose(f);
    return true;
}

bool stats_save(const stats_t *st)
{
    FILE *f = fopen(st->path, "w");
    if (f == NULL)
        return false;

    for (int i = 0; i < st->count; i++)
        fprintf(f, "%s=%d\n", st->entries[i].key, st->entries[i].value);

    fclose(f);
    return true;
}

int stats_get(const stats_t *st, const char *key)
{
    for (int i = 0; i < st->count; i++) {
        if (strcmp(st->entries[i].key, key) == 0)
            return st->entries[i].value;
    }
    return 0;
}

void stats_set(stats_t *st, const char *key, int value)
{
    for (int i = 0; i < st->count; i++) {
        if (strcmp(st->entries[i].key, key) == 0) {
            st->entries[i].value = value;
            return;
        }
    }

    if (st->count < STATS_MAX_ENTRIES) {
        strncpy(st->entries[st->count].key, key, STATS_KEY_LEN - 1);
        st->entries[st->count].key[STATS_KEY_LEN - 1] = '\0';
        st->entries[st->count].value = value;
        st->count++;
    }
}

void stats_increment(stats_t *st, const char *key)
{
    stats_set(st, key, stats_get(st, key) + 1);
}

void stats_record_game(stats_t *st, const char *game_name, bool won)
{
    char key[STATS_KEY_LEN];

    snprintf(key, sizeof(key), "%s_played", game_name);
    stats_increment(st, key);

    if (won) {
        snprintf(key, sizeof(key), "%s_won", game_name);
        stats_increment(st, key);
    } else {
        snprintf(key, sizeof(key), "%s_lost", game_name);
        stats_increment(st, key);
    }

    stats_save(st);
}

void stats_display(const stats_t *st)
{
    int rows, cols;
    getmaxyx(stdscr, rows, cols);
    clear();

    attron(COLOR_PAIR(COLOR_MENU) | A_BOLD);
    const char *title = "Game Statistics";
    mvaddstr(2, (cols - (int)strlen(title)) / 2, title);
    attroff(COLOR_PAIR(COLOR_MENU) | A_BOLD);

    int bw = 40;
    int bh = 10;
    int bx = (cols - bw) / 2;
    int by = 4;
    ui_draw_box(by, bx, bh, bw);

    const char *games[] = { "pong" };
    int ngames = 1;
    int line = by + 1;

    for (int g = 0; g < ngames && line < by + bh - 1; g++) {
        char key[STATS_KEY_LEN];

        attron(COLOR_PAIR(COLOR_BORDER) | A_BOLD);
        char game_title[32];
        snprintf(game_title, sizeof(game_title), "  %s", games[g]);
        if (game_title[2] >= 'a' && game_title[2] <= 'z')
            game_title[2] = (char)(game_title[2] - 'a' + 'A');
        mvaddstr(line++, bx + 2, game_title);
        attroff(COLOR_PAIR(COLOR_BORDER) | A_BOLD);

        snprintf(key, sizeof(key), "%s_played", games[g]);
        int played = stats_get(st, key);
        snprintf(key, sizeof(key), "%s_won", games[g]);
        int won = stats_get(st, key);
        snprintf(key, sizeof(key), "%s_lost", games[g]);
        int lost = stats_get(st, key);

        char buf[64];
        attron(COLOR_PAIR(COLOR_BORDER));
        snprintf(buf, sizeof(buf), "    Played: %d", played);
        mvaddstr(line++, bx + 2, buf);

        attron(COLOR_PAIR(COLOR_MENU));
        snprintf(buf, sizeof(buf), "    Won:    %d", won);
        mvaddstr(line++, bx + 2, buf);
        attroff(COLOR_PAIR(COLOR_MENU));

        attron(COLOR_PAIR(COLOR_ALERT));
        snprintf(buf, sizeof(buf), "    Lost:   %d", lost);
        mvaddstr(line++, bx + 2, buf);
        attroff(COLOR_PAIR(COLOR_ALERT));

        if (played > 0) {
            int pct = (won * 100) / played;
            snprintf(buf, sizeof(buf), "    Win %%:  %d%%", pct);
            attron(COLOR_PAIR(COLOR_BORDER));
            mvaddstr(line++, bx + 2, buf);
            attroff(COLOR_PAIR(COLOR_BORDER));
        }

        line++;
    }

    if (st->count == 0) {
        attron(COLOR_PAIR(COLOR_DIM) | A_DIM);
        const char *msg = "No games played yet.";
        mvaddstr(by + 3, bx + (bw - (int)strlen(msg)) / 2, msg);
        attroff(COLOR_PAIR(COLOR_DIM) | A_DIM);
    }

    attron(COLOR_PAIR(COLOR_DIM) | A_DIM);
    const char *hint = "Press any key to return...";
    mvaddstr(rows - 2, (cols - (int)strlen(hint)) / 2, hint);
    attroff(COLOR_PAIR(COLOR_DIM) | A_DIM);

    refresh();
    nodelay(stdscr, FALSE);
    getch();
}
