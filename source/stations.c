#include <3ds.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <sys/stat.h>
#include "stations.h"

#define DIR_PATH  "sdmc:/3ds/radio3ds"
#define FILE_PATH DIR_PATH "/stations.txt"
#define SET_PATH  DIR_PATH "/settings.txt"

Station g_stations[MAX_STATIONS];
int g_stationCount = 0;

/* Built-in list. Every URL below was verified to answer with real audio
   (curl + ffprobe) when this build was made. Format: name | url */
static const char* DEFAULT_LIST =
"# Radio Neon / ラジオネオン - lista de emisoras\n"
"# Formato:  Nombre | URL   (una por línea, '#' = comentario)\n"
"# Soporta MP3 y AAC/AAC+ (ADTS) por http:// o https://\n"
"LISTEN.moe J-POP | https://listen.moe/fallback\n"
"R/a/dio アニメ | https://relay0.r-a-d.io/main.mp3\n"
"幻想郷ラジオ Gensokyo | https://stream.gensokyoradio.net/1/\n"
"Stereo Anime | https://radio.stereoanime.com/listen/stereoanime/128\n"
"Kawaii Anime Radio (AAC+) | http://158.69.227.214:8137/stream\n"
"JPopsuki Radio | http://65.21.170.149:8000/stream\n"
"City Pop One | http://65.21.61.215:8000/citypopone\n"
"日本のシティポップ BOX | https://play.streamafrica.net/japancitypop\n"
"Only Hits Japan | https://j.onlyhit.us/play\n"
"湘南ビーチFM 78.9 | https://shonanbeachfm.out.airtime.pro/shonanbeachfm_a\n"
"FM世田谷 83.4 | https://fmsetagaya834.out.airtime.pro/fmsetagaya834_a\n"
"OTTAVA クラシック | https://ottava2.out.airtime.pro/ottava2_a\n"
"RadiCro レディクロ | https://radicrojapan.out.airtime.pro/radicrojapan_a\n"
"Radio Hayama 葉山 | https://server10.reliastream.com/proxy/radiohayama\n"
"羊飼いラジオ Hitsujikai | https://streaming.radio.co/se4a8e6a93/listen\n"
"アシヤ・ラヂヲ (AAC) | https://s3.radio.co/sc8d895604/listen\n"
"SOUND UP STATION NFRS | https://stream2.rcast.net/70945\n"
"Fred Film Radio 日本語 | https://s10.webradio-hosting.com/proxy/fredradiojp/stream\n"
"Replay News 日本語ニュース | https://replaynewsja.ice.infomaniak.ch/replaynewsja-128.mp3\n"
"Nonstop Casiopea | https://nonstopcasiopea.radioca.st/\n";

static char* trim(char* s) {
    while (*s == ' ' || *s == '\t' || *s == '\r' || *s == '\n') s++;
    char* e = s + strlen(s);
    while (e > s && (e[-1] == ' ' || e[-1] == '\t' || e[-1] == '\r' || e[-1] == '\n')) *--e = 0;
    return s;
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
        char* bar = strrchr(l, '|');
        char *name, *url;
        if (bar) { *bar = 0; name = trim(l); url = trim(bar + 1); }
        else { url = l; name = l; }
        if (strncmp(url, "http://", 7) && strncmp(url, "https://", 8)) continue;
        Station* s = &g_stations[g_stationCount++];
        snprintf(s->name, sizeof(s->name), "%s", *name ? name : url);
        snprintf(s->url, sizeof(s->url), "%s", url);
    }
}

int stations_load(void) {
    mkdir("sdmc:/3ds", 0777);
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
    return g_stationCount;
}

