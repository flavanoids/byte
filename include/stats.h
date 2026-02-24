#ifndef BYTES_STATS_H
#define BYTES_STATS_H

#include <stdbool.h>

#define STATS_MAX_ENTRIES 32
#define STATS_KEY_LEN     64

typedef struct {
    char key[STATS_KEY_LEN];
    int  value;
} stats_entry_t;

typedef struct {
    stats_entry_t entries[STATS_MAX_ENTRIES];
    int           count;
    char          path[256];
} stats_t;

void stats_init(stats_t *st);
bool stats_load(stats_t *st);
bool stats_save(const stats_t *st);
int  stats_get(const stats_t *st, const char *key);
void stats_set(stats_t *st, const char *key, int value);
void stats_increment(stats_t *st, const char *key);
void stats_record_game(stats_t *st, const char *game_name, bool won);
void stats_display(const stats_t *st);

#endif
