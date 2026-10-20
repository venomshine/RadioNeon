#include <3ds.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <sys/stat.h>
#include "stations.h"
#include "app.h"

#define DIR_PATH  APP_DIR
#define FILE_PATH DIR_PATH "/stations.txt"
#define OLD_LIST_PATH DIR_PATH "/stations.old.txt"
#define SET_PATH  DIR_PATH "/settings.txt"

Station g_stations[MAX_STATIONS];
int g_stationCount = 0;

/* Built-in list. Every URL below was checked to answer with real audio (curl + ffprobe) when this list
   was made. Most are run from Japan; a few music streams from abroad complete the genres.
   Format: name | url | genre (genre optional) */
static const char* DEFAULT_LIST =
"# Radio Neon / ラジオネオン - lista de emisoras\n"
"# Formato:  Nombre | URL | Género   (una por línea, '#' = comentario; el género es opcional)\n"
"# Soporta MP3 y AAC/AAC+ (ADTS) por http:// o https://\n"
"LISTEN.moe J-POP | https://listen.moe/fallback | J-POP\n"
"FMかほく 78.7 | http://radio.kahoku.net:8000/ | J-POP\n"
"R/a/dio アニメ | https://relay0.r-a-d.io/main.mp3 | ANIME\n"
"Stereo Anime | https://radio.stereoanime.com/listen/stereoanime/128 | ANIME\n"
"幻想郷ラジオ Gensokyo | https://stream.gensokyoradio.net/1/ | TOUHOU\n"
"Gotanno FM 89.2 同人 | https://radio.gotanno.love/ | DOUJIN\n"
"Retro PC Game Music | http://gyusyabu.ddo.jp:8000/ | GAME\n"
"City Pop One | http://65.21.61.215:8000/citypopone | CITY POP\n"
"FLY FM シティポップ | https://flyfm.stream.laut.fm/flyfm | CITY POP\n"
"Eurobeat.fm 榛名山 | https://eurobeat.stream.laut.fm/eurobeat | EUROBEAT\n"
"ADRN Eurobeat ユーロビート | http://abm22.com.au:8000/CONTAINER26 | EUROBEAT\n"
"WREP ヒップホップ (AAC) | http://c13.radioboss.fm:8200/stream | HIP-HOP\n"
"SOUND UP STATION NFRS | https://stream2.rcast.net/70945 | INDIE\n"
"Radio Hayama 葉山 | https://server10.reliastream.com/proxy/radiohayama | INDIE\n"
"湘南ビーチFM 78.9 | https://shonanbeachfm.out.airtime.pro/shonanbeachfm_a | JAZZ\n"
"Nonstop Casiopea | https://nonstopcasiopea.radioca.st/ | FUSION\n"
"アシヤ・ラヂヲ (AAC) | https://s3.radio.co/sc8d895604/listen | BOSSA NOVA\n"
"OTTAVA クラシック | https://ottava2.out.airtime.pro/ottava2_a | CLASSICAL\n"
"FM世田谷 83.4 | https://fmsetagaya834.out.airtime.pro/fmsetagaya834_a | TALK\n"
"RadiCro レディクロ | https://radicrojapan.out.airtime.pro/radicrojapan_a | TALK\n"
"羊飼いラジオ Hitsujikai | https://streaming.radio.co/se4a8e6a93/listen | TALK\n"
"Replay News 日本語ニュース | https://replaynewsja.ice.infomaniak.ch/replaynewsja-128.mp3 | NEWS\n";

/* URLs of the list shipped with 1.0.0 and 1.1.0 (both the same). A stations.txt that is exactly that
   list was never edited, so it is replaced by the new one (the old file is kept as stations.old.txt). */
static const char* OLD_DEFAULT_URLS[] = {
    "https://listen.moe/fallback",
    "https://relay0.r-a-d.io/main.mp3",
    "https://stream.gensokyoradio.net/1/",
    "https://radio.stereoanime.com/listen/stereoanime/128",
    "http://158.69.227.214:8137/stream",
    "http://65.21.170.149:8000/stream",
    "http://65.21.61.215:8000/citypopone",
    "https://play.streamafrica.net/japancitypop",
    "https://j.onlyhit.us/play",
    "https://shonanbeachfm.out.airtime.pro/shonanbeachfm_a",
    "https://fmsetagaya834.out.airtime.pro/fmsetagaya834_a",
    "https://ottava2.out.airtime.pro/ottava2_a",
    "https://radicrojapan.out.airtime.pro/radicrojapan_a",
    "https://server10.reliastream.com/proxy/radiohayama",
    "https://streaming.radio.co/se4a8e6a93/listen",
    "https://s3.radio.co/sc8d895604/listen",
    "https://stream2.rcast.net/70945",
    "https://s10.webradio-hosting.com/proxy/fredradiojp/stream",
    "https://replaynewsja.ice.infomaniak.ch/replaynewsja-128.mp3",
    "https://nonstopcasiopea.radioca.st/",
};
#define NUM_OLD_DEFAULT (int)(sizeof(OLD_DEFAULT_URLS) / sizeof(OLD_DEFAULT_URLS[0]))
static int g_replacedFrom[MAX_STATIONS];   /* old index -> OLD_DEFAULT_URLS index, while remapping */
static bool g_replaced = false;

static char* trim(char* s) {
    while (*s == ' ' || *s == '\t' || *s == '\r' || *s == '\n') s++;
    char* e = s + strlen(s);
    while (e > s && (e[-1] == ' ' || e[-1] == '\t' || e[-1] == '\r' || e[-1] == '\n')) *--e = 0;
    return s;
}

/* lists written by older versions have no genre: take it from the built-in list when the URL matches */
static void default_genre(const char* url, char* out, size_t n) {
    out[0] = 0;
    size_t ul = strlen(url);
    for (const char* p = DEFAULT_LIST; (p = strstr(p, url)); p += ul) {
        if (p[-1] != ' ' || (p[ul] != ' ' && p[ul] != '\n')) continue;   /* whole field only */
        const char* e = strchr(p, '\n');
        const char* bar = memchr(p, '|', e ? (size_t)(e - p) : strlen(p));
        if (!bar) return;
        const char* g = bar + 1;
        while (*g == ' ') g++;
        size_t len = e ? (size_t)(e - g) : strlen(g);
        while (len && g[len - 1] == ' ') len--;
        if (len >= n) len = n - 1;
        memcpy(out, g, len); out[len] = 0;
        return;
    }
}

static void parse_list(const char* text) {
    g_stationCount = 0;
    const char* p = text;
    char line[600];
    while (*p && g_stationCount < MAX_STATIONS) {
        const char* nl = strchr(p, '\n');
        size_t len = nl ? (size_t)(nl - p) : strlen(p);
        if (len >= sizeof(line)) len = sizeof(line) - 1;
        memcpy(line, p, len); line[len] = 0;
        p = nl ? nl + 1 : p + strlen(p);
        char* l = trim(line);
        /* skip UTF-8 BOM */
        if ((unsigned char)l[0] == 0xEF && (unsigned char)l[1] == 0xBB && (unsigned char)l[2] == 0xBF) l += 3;
        if (!*l || *l == '#') continue;
        /* name | url [| genre]: the fields are taken from the right, so a name may contain '|' */
        char *name, *url, *genre = NULL;
        char* bar = strrchr(l, '|');
        if (bar) {
            char* last = trim(bar + 1);
            if (strncmp(last, "http://", 7) && strncmp(last, "https://", 8)) {   /* third field: genre */
                *bar = 0; genre = last;
                bar = strrchr(l, '|');
            }
        }
        if (bar) { *bar = 0; name = trim(l); url = trim(bar + 1); }
        else { url = trim(l); name = url; }
        if (strncmp(url, "http://", 7) && strncmp(url, "https://", 8)) continue;
        Station* s = &g_stations[g_stationCount++];
        snprintf(s->name, sizeof(s->name), "%s", *name ? name : url);
        snprintf(s->url, sizeof(s->url), "%s", url);
        if (genre) snprintf(s->genre, sizeof(s->genre), "%s", genre);
        else default_genre(url, s->genre, sizeof(s->genre));
    }
}

static bool is_old_default(void);

int stations_load(void) {
    mkdir("sdmc:/3ds", 0777);
    struct stat st;   /* 1.0.0 used sdmc:/3ds/radio3ds: move it once so nothing is lost */
    if (stat(DIR_PATH, &st) != 0 && stat(APP_OLD_DIR, &st) == 0) rename(APP_OLD_DIR, DIR_PATH);
    mkdir(DIR_PATH, 0777);
    FILE* f = fopen(FILE_PATH, "rb");
    if (!f) {
        f = fopen(FILE_PATH, "wb");
        if (f) { fputs(DEFAULT_LIST, f); fclose(f); }
        parse_list(DEFAULT_LIST);
        return g_stationCount;
    }
    fseek(f, 0, SEEK_END);
    long sz = ftell(f);
    fseek(f, 0, SEEK_SET);
    if (sz <= 0 || sz > 256 * 1024) { fclose(f); parse_list(DEFAULT_LIST); return g_stationCount; }
    char* buf = malloc(sz + 1);
    if (!buf) { fclose(f); parse_list(DEFAULT_LIST); return g_stationCount; }
    fread(buf, 1, sz, f);
    buf[sz] = 0;
    fclose(f);
    parse_list(buf);
    free(buf);
    if (g_stationCount == 0) parse_list(DEFAULT_LIST);
    else if (is_old_default()) {
        remove(OLD_LIST_PATH);
        rename(FILE_PATH, OLD_LIST_PATH);
        f = fopen(FILE_PATH, "wb");
        if (f) { fputs(DEFAULT_LIST, f); fclose(f); }
        parse_list(DEFAULT_LIST);
        g_replaced = true;
    }
    return g_stationCount;
}

/* the untouched old list: same number of stations and every URL one of the old built-in ones, each once */
static bool is_old_default(void) {
    if (g_stationCount != NUM_OLD_DEFAULT) return false;
    bool used[NUM_OLD_DEFAULT] = { false };
    for (int i = 0; i < g_stationCount; i++) {
        int k = 0;
        while (k < NUM_OLD_DEFAULT && (used[k] || strcmp(g_stations[i].url, OLD_DEFAULT_URLS[k]))) k++;
        if (k == NUM_OLD_DEFAULT) return false;
        used[k] = true;
        g_replacedFrom[i] = k;
    }
    return true;
}

int stations_remap(int oldIndex) {
    if (!g_replaced || oldIndex < 0 || oldIndex >= NUM_OLD_DEFAULT) return oldIndex;
    const char* url = OLD_DEFAULT_URLS[g_replacedFrom[oldIndex]];
    for (int i = 0; i < g_stationCount; i++) if (!strcmp(g_stations[i].url, url)) return i;
    return 0;
}

