#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "history.h"
#include "app.h"

#define HIST_PATH APP_DIR "/history.txt"

HistEntry g_songs[HIST_MAX];
int g_songCount = 0;
int g_songVer = 0;

/* tabs and newlines would break the file format */
static void clean(char* s) { for (; *s; s++) if (*s == '\t' || *s == '\r' || *s == '\n') *s = ' '; }

/* same safe write as config.ini: .tmp first, then replace */
static void hist_save(void) {
    g_songVer++;
    FILE* f = fopen(HIST_PATH ".tmp", "wb");
    if (!f) return;
    for (int i = g_songCount - 1; i >= 0; i--)   /* oldest first, like a log */
        fprintf(f, "%lld\t%d\t%s\t%s\n", (long long)g_songs[i].when, g_songs[i].liked, g_songs[i].station, g_songs[i].title);
    bool ok = fflush(f) == 0;
    ok = (fclose(f) == 0) && ok;
    if (!ok) { remove(HIST_PATH ".tmp"); return; }
    remove(HIST_PATH);
    rename(HIST_PATH ".tmp", HIST_PATH);
}

/* full list: drop the oldest entry that is not liked (if all are liked, the oldest one) */
static void make_room(void) {
    if (g_songCount < HIST_MAX) return;
    int k = g_songCount - 1;
    while (k >= 0 && g_songs[k].liked) k--;
    if (k < 0) k = g_songCount - 1;
    memmove(&g_songs[k], &g_songs[k + 1], sizeof(HistEntry) * (g_songCount - 1 - k));
    g_songCount--;
}

static void push_front(const HistEntry* e) {
    make_room();
    memmove(&g_songs[1], &g_songs[0], sizeof(HistEntry) * g_songCount);
    g_songs[0] = *e;
    g_songCount++;
}

void hist_load(void) {
    g_songCount = 0;
    FILE* f = fopen(HIST_PATH, "rb");
    if (!f) {   /* a cut between remove() and rename() leaves only the .tmp */
        rename(HIST_PATH ".tmp", HIST_PATH);
        f = fopen(HIST_PATH, "rb");
        if (!f) return;
    }
    char line[320];
    while (fgets(line, sizeof(line), f)) {
        line[strcspn(line, "\r\n")] = 0;
        char* t1 = strchr(line, '\t'); if (!t1) continue;
        char* t2 = strchr(t1 + 1, '\t'); if (!t2) continue;
        char* t3 = strchr(t2 + 1, '\t'); if (!t3) continue;
        *t1 = *t2 = *t3 = 0;
        HistEntry e = { (time_t)atoll(line), atoi(t1 + 1) != 0, "", "" };
        snprintf(e.station, sizeof(e.station), "%s", t2 + 1);
        snprintf(e.title, sizeof(e.title), "%s", t3 + 1);
        if (e.title[0]) push_front(&e);
    }
    fclose(f);
}

int hist_find(const char* title) {
    for (int i = 0; i < g_songCount; i++) if (!strcmp(g_songs[i].title, title)) return i;
    return -1;
}

int hist_add(const char* title, const char* station, bool liked) {
    if (g_songCount && !strcmp(g_songs[0].title, title)) {   /* same song still on top */
        if (liked && !g_songs[0].liked) { g_songs[0].liked = true; hist_save(); }
        return 0;
    }
    HistEntry e = { time(NULL), liked, "", "" };
    snprintf(e.station, sizeof(e.station), "%s", station);
    snprintf(e.title, sizeof(e.title), "%s", title);
    clean(e.station); clean(e.title);
    push_front(&e);
    hist_save();
    return 0;
}

void hist_set_liked(int i, bool liked) {
    if (i < 0 || i >= g_songCount || g_songs[i].liked == liked) return;
    g_songs[i].liked = liked;
    hist_save();
}
