#pragma once
#include <stdbool.h>
#include <time.h>

/* Songs that played (ICY titles), newest first. Liked ones are never dropped.
   Stored in sdmc:/3ds/RadioNeon/history.txt: one "time<TAB>liked<TAB>station<TAB>title" per line. */
#define HIST_MAX 200

typedef struct {
    time_t when;
    bool   liked;
    char   station[64];
    char   title[160];
} HistEntry;

extern HistEntry g_songs[HIST_MAX];
extern int g_songCount;

void hist_load(void);
/* adds a song at the top (or moves it there if it is already the newest); returns its index (0) */
int  hist_add(const char* title, const char* station, bool liked);
int  hist_find(const char* title);          /* index of the newest entry with that title, -1 if none */
void hist_set_liked(int i, bool liked);     /* saves */
