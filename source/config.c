#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <citro2d.h>
#include "config.h"
#include "stations.h"
#include "app.h"

#define DIR_PATH APP_DIR
#define CFG_PATH DIR_PATH "/config.ini"
#define OLD_PATH DIR_PATH "/settings.txt"
#define FAV_PATH DIR_PATH "/favorites.txt"

const AccentPreset ACCENTS[NUM_ACCENTS] = {
    { "AMARILLO ACIDO", 252, 238, 10 },
    { "BLANCO",         236, 236, 228 },
    { "CIAN",             0, 240, 255 },
    { "MAGENTA",        255,  43, 214 },
    { "VERDE NEON",      57, 255,  20 },
    { "MORADO UV",      138,  43, 255 },
    { "ROJO",           244,   0,   9 },   /* Coca-Cola red #F40009 */
};

Config g_cfg = { 0, true, true, true, false, 0, 0 };
u32 g_accent = 0;
static bool g_fav[MAX_STATIONS];

void config_apply_accent(void) {
    if (g_cfg.accent < 0 || g_cfg.accent >= NUM_ACCENTS) g_cfg.accent = 0;
    const AccentPreset* a = &ACCENTS[g_cfg.accent];
    g_accent = C2D_Color32(a->r, a->g, a->b, 255);
}

/* write to path.tmp, then replace: a power cut never leaves a half-written file */
static bool commit_file(FILE* f, const char* tmp, const char* path) {
    bool ok = fflush(f) == 0;
    ok = (fclose(f) == 0) && ok;
    if (!ok) { remove(tmp); return false; }
    remove(path);              /* FAT rename does not overwrite */
    return rename(tmp, path) == 0;
}

static bool parse_bool(const char* v) { return *v == '1' || *v == 't' || *v == 'T' || *v == 'o' || *v == 'O' || *v == 'y'; }

static void read_file(const char* path) {
    FILE* f = fopen(path, "rb");
    if (!f) return;
    char line[160];
    while (fgets(line, sizeof(line), f)) {
        char* eq = strchr(line, '=');
        if (!eq || line[0] == '#' || line[0] == ';') continue;
        *eq = 0;
        char* v = eq + 1;
        v[strcspn(v, "\r\n")] = 0;
        if (!strcmp(line, "station")) g_cfg.station = atoi(v);
        else if (!strcmp(line, "accent")) g_cfg.accent = atoi(v);
        else if (!strcmp(line, "lid_play")) g_cfg.lidPlay = parse_bool(v);
        else if (!strcmp(line, "saver")) g_cfg.saver = parse_bool(v);
        else if (!strcmp(line, "scanlines")) g_cfg.scanlines = parse_bool(v);
        else if (!strcmp(line, "glitch")) g_cfg.glitch = parse_bool(v);
        else if (!strcmp(line, "autoplay")) g_cfg.autoplay = parse_bool(v);
        else if (!strcmp(line, "lang")) g_cfg.lang = atoi(v);
        else if (!strcmp(line, "shuffle")) g_cfg.shuffle = parse_bool(v);
        else if (!strcmp(line, "link")) snprintf(g_cfg.link, sizeof(g_cfg.link), "%.5s", v);
    }
    fclose(f);
}

/* commit_file() only removes the old file once the .tmp is complete: if the power went off between
   remove() and rename(), the .tmp is the good copy */
static void recover_tmp(const char* tmp, const char* path) {
    FILE* f = fopen(path, "rb");
    if (f) { fclose(f); return; }
    rename(tmp, path);
}

void config_load(void) {
    recover_tmp(CFG_PATH ".tmp", CFG_PATH);
    FILE* f = fopen(CFG_PATH, "rb");
    if (f) { fclose(f); read_file(CFG_PATH); }
    else read_file(OLD_PATH);          /* older settings.txt (station only) */
    if (g_cfg.lang < 0 || g_cfg.lang > 3) g_cfg.lang = 0;
    config_apply_accent();
}

void config_save(void) {
    FILE* f = fopen(CFG_PATH ".tmp", "wb");
    if (!f) return;
    fprintf(f, "# " APP_NAME " " APP_VERSION " config\n");
    fprintf(f, "station=%d\naccent=%d\nlid_play=%d\nsaver=%d\nscanlines=%d\nglitch=%d\nautoplay=%d\nlang=%d\nshuffle=%d\nlink=%s\n",
            g_cfg.station, g_cfg.accent, g_cfg.lidPlay, g_cfg.saver, g_cfg.scanlines, g_cfg.glitch, g_cfg.autoplay, g_cfg.lang, g_cfg.shuffle, g_cfg.link);
    commit_file(f, CFG_PATH ".tmp", CFG_PATH);
}

void fav_load(void) {
    memset(g_fav, 0, sizeof(g_fav));
    recover_tmp(FAV_PATH ".tmp", FAV_PATH);
    FILE* f = fopen(FAV_PATH, "rb");
    if (!f) return;
    char line[400];
    while (fgets(line, sizeof(line), f)) {
        line[strcspn(line, "\r\n")] = 0;
        if (!line[0] || line[0] == '#') continue;
        for (int i = 0; i < g_stationCount; i++)
            if (!strcmp(g_stations[i].url, line)) g_fav[i] = true;
    }
    fclose(f);
}

static void fav_save(void) {
    FILE* f = fopen(FAV_PATH ".tmp", "wb");
    if (!f) return;
    fprintf(f, "# Radio Neon favoritas (una URL por linea)\n");
    for (int i = 0; i < g_stationCount; i++) if (g_fav[i]) fprintf(f, "%s\n", g_stations[i].url);
    commit_file(f, FAV_PATH ".tmp", FAV_PATH);
}

bool fav_is(int i) { return i >= 0 && i < g_stationCount && g_fav[i]; }
void fav_toggle(int i) { if (i < 0 || i >= g_stationCount) return; g_fav[i] = !g_fav[i]; fav_save(); }
int fav_count(void) { int n = 0; for (int i = 0; i < g_stationCount; i++) n += g_fav[i]; return n; }
