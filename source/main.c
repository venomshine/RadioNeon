/* Radio Neon / ラジオネオン - Japanese internet radio for Nintendo 3DS
 * Cyberpunk HUD edition: carbon black, single acid-yellow accent (#FCEE0A),
 * small alert red for ON AIR / errors only.
 * Effects re-implemented by hand for citro2d (references, no code reused):
 *   Magic UI   : Hyper Text (decode on station change), Border Beam -> monochrome edge trace,
 *                Flickering Grid -> faint dot matrix, Marquee (ICY title)
 *   motionvideo: kinetic type (title slides in), shape morphing (play <-> stop)
 *   rareui     : Bounce Sidebar (spring list indicator), Hook Sidebar (dashed rail)
 * 1.0.0: HUD menu (all / favourites / options / about), accent presets, exit dialog,
 *     lid-closed playback with headphones, beat-driven smoothed scope.
 */
#include <3ds.h>
#include <citro2d.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include "fx.h"
#include "player.h"
#include "stations.h"
#include "dspfw.h"
#include "config.h"
#include "i18n.h"
#include "netlog.h"
#include "app.h"
#include "qrcodegen.h"
#include "history.h"
#include "web.h"

#define TOP_W 400
#define BOT_W 320
#define SCR_H 240
#define NBANDS 48
#define NWAVE 170

static C3D_RenderTarget *g_top, *g_bot;
static float g_t;
static int   g_sel = 0, g_playing = -1;   /* g_sel = current station (playing, or the last one played) */
enum { M_CTRL = 0, M_LIST, M_MENU, M_OPT, M_ABOUT, M_SLEEP, M_HIST, M_HELP };
static int   g_mode = M_CTRL;
static int   g_view[MAX_STATIONS], g_viewN = 0, g_cur = 0;   /* list view (all or favourites) */
static bool  g_favView = false;
static int   g_listFrom = 0;                 /* screen the list was opened from (B / BACK returns there) */
#define HIST_N 32
static int   g_hist[HIST_N], g_histN = 0;     /* shuffle history, so PREV goes back */
static int   g_menuCur = 0, g_optCur = 0;
static bool  g_dialog = false;
static float g_dialogT = -10;
#define START_HOLD 2.0f            /* START held this long: exit dialog (a short press opens / closes the menu) */
static float g_startDown = -1;     /* when START went down, -1 = not held */
static bool  g_batDialog = false;          /* low battery notice (10 %) */
static float g_batDialogT = -10;
static bool  g_batLock = false;            /* 3 % and no charger: silent until the charger is plugged in */
static bool  g_sleepBlocked = false, g_lcdOff = false;
static int   g_resumeStation = -1;
static bool  g_audioReady = false, g_audioDown = false;   /* hooks only act after boot, and up only after down */
static float g_bassEnv = 0, g_bassAvg = 0, g_bassPeak = 0.05f, g_pulse = 0, g_lastBeat = -1;
static aptHookCookie g_aptCookie;
static bool g_mcuOk = false;               /* mcu::HWC: exact battery %, volume slider fallback */
static float g_savedT = -10;               /* a song was just saved / removed (B): SIGNAL LOCK shows it for 2 s */
static bool  g_savedLike = true;           /* which of the two */
static void heart(float cx, float cy, float r, bool filled, u32 c);
static int  now_song(const PlayerInfo* pi);
static float g_listScroll = 0, g_listVel = 0;
static float g_selY = 0, g_selV = 0;
static float g_changeT = -10;
static char  g_shownName[96] = "";
static float g_morph = 0;
static float g_bands[NBANDS], g_peaks[NBANDS];
static float g_wave[NWAVE];
static float g_glitchUntil = 0, g_nextGlitch = 9;
static float g_modeT = -10;
static FwSource g_fw = FW_NONE;
static char  g_fwDetail[96] = "";
static bool  g_quit = false;
static float g_flicker = 1;

typedef struct { float x, y, t0; bool on; } Lock;
static Lock g_lock[6];

/* --------------------------------------------------------------- helpers */
static void url_host(const char* url, char* out, size_t n) {
    const char* p = strstr(url, "://");
    p = p ? p + 3 : url;
    size_t i = 0;
    while (*p && *p != '/' && *p != ':' && i + 1 < n) { char c = *p++; out[i++] = (c >= 'a' && c <= 'z') ? c - 32 : c; }
    out[i] = 0;
    if (i > 20) { out[18] = '.'; out[19] = '.'; out[20] = 0; }
}

static void add_lock(float x, float y) {
    for (int i = 0; i < 6; i++) if (!g_lock[i].on) { g_lock[i] = (Lock){ x, y, g_t, true }; return; }
}

static float ease_out_cubic(float x) { if (x >= 1) return 1; if (x < 0) return 0; float k = 1 - x; return 1 - k * k * k; }

static void dot_matrix(float w) {
    for (int y = 12; y < SCR_H; y += 16)
        for (int x = 8; x < w; x += 16)
            C2D_DrawRectSolid(x, y, 0, 1, 1, col_a(COL_DIM, 0.35f));
}

static void scanlines(float w) {
    if (!g_cfg.scanlines) return;
    for (int y = 0; y < SCR_H; y += 2) C2D_DrawRectSolid(0, y, 0, w, 1, C2D_Color32(0, 0, 0, 34));
}

static void seg_bar(float x, float y, int n, float segw, float gap, float h, float v, u32 on, u32 off) {
    int k = (int)(v * n + 0.5f);
    for (int i = 0; i < n; i++) C2D_DrawRectSolid(x + i * (segw + gap), y, 0, segw, h, i < k ? on : off);
}

/* heart (saved songs): the classic parametric curve, filled or outline */
static void heart(float cx, float cy, float r, bool filled, u32 c) {
    enum { N = 32 };
    float px[N], py[N];
    for (int i = 0; i < N; i++) {
        float t = i * 2 * (float)M_PI / N, st = sinf(t);
        float x = 16 * st * st * st, y = 13 * cosf(t) - 5 * cosf(2 * t) - 2 * cosf(3 * t) - cosf(4 * t);
        px[i] = cx + x * r / 16; py[i] = cy - y * r / 16;
    }
    for (int i = 0; i < N; i++) {
        int j = (i + 1) % N;
        if (filled) C2D_DrawTriangle(cx, cy, c, px[i], py[i], c, px[j], py[j], c, 0);
        else C2D_DrawLine(px[i], py[i], c, px[j], py[j], c, 1.2f, 0);
    }
}

/* 5-point star: filled (favourite) or outline */
static void star(float cx, float cy, float r, bool filled, u32 c) {
    float px[10], py[10];
    for (int i = 0; i < 10; i++) {
        float a = -(float)M_PI / 2 + i * (float)M_PI / 5, rr = (i & 1) ? r * 0.45f : r;
        px[i] = cx + cosf(a) * rr; py[i] = cy + sinf(a) * rr;
    }
    for (int i = 0; i < 10; i++) {
        int j = (i + 1) % 10;
        if (filled) C2D_DrawTriangle(cx, cy, c, px[i], py[i], c, px[j], py[j], c, 0);
        else C2D_DrawLine(px[i], py[i], c, px[j], py[j], c, 1, 0);
    }
}

/* translatable label: ASCII -> HUD mono font, anything else (accents, kana/kanji) -> system font
   scaled to the same visual size. */
static float lbl(const char* s, float x, float y, int sc, u32 col, int align) {
    if (mono_ok(s)) return mono_draw(s, x, y, sc, col, align);
    return text_draw(s, x, y - (sc == 2 ? 4 : 3), sc == 2 ? 0.6f : 0.42f, col, align);
}

static float lbl_width(const char* s, int sc) { return mono_ok(s) ? mono_width(s, sc) : text_width(s, sc == 2 ? 0.6f : 0.42f); }
static const char* acc_name(int k) { return T((StrId)(S_ACC0 + k)); }

/* toggle switch */
static void hud_switch(float x, float y, bool on) {
    if (on) { ch_fill(x, y, 44, 16, 4, CUT_TL | CUT_BR, COL_ACC); mono_draw("ON", x + 22, y + 4, 1, COL_BLACK, 1); }
    else { ch_outline(x, y, 44, 16, 4, CUT_TL | CUT_BR, COL_LINE, 1); mono_draw("OFF", x + 22, y + 4, 1, COL_DIM, 1); }
}

/* --------------------------------------------------------------- actions */
static void play_station(int i) {
    if (!g_stationCount || g_batLock) return;   /* battery lock: nothing plays, whatever asks for it */
    i = (i % g_stationCount + g_stationCount) % g_stationCount;
    g_sel = i; g_playing = i;
    player_play(g_stations[i].url);
}

static void stop_station(void) { player_stop(); g_playing = -1; }

/* sleep timer: the radio stops by itself, fading out over the last 10 s. With the lid closed the
 * console then goes to sleep on its own (power_update: nothing playing -> sleep allowed). */
#define SLEEP_FADE_MS 10000
static u64 g_sleepEnd = 0;   /* osGetTime() when it stops, 0 = off */
static int g_sleepSet = 0;   /* minutes chosen, to mark the row in the timer screen */

static void sleep_set(int minutes) {
    g_sleepSet = minutes;
    g_sleepEnd = minutes > 0 ? osGetTime() + (u64)minutes * 60000 : 0;
    player_set_volume(1.0f);
    netlog("sleep timer: %d min", minutes);
}

static void sleep_update(void) {
    if (!g_sleepEnd) return;
    u64 now = osGetTime();
    if (now >= g_sleepEnd) {
        netlog("sleep timer: time is up%s", g_playing >= 0 ? ", stopping" : "");
        stop_station();
        g_sleepEnd = 0; g_sleepSet = 0;
        player_set_volume(1.0f);
        return;
    }
    u64 left = g_sleepEnd - now;
    if (left < SLEEP_FADE_MS) player_set_volume((float)left / SLEEP_FADE_MS);
}

static void sleep_left_str(char* out, size_t n) {
    u64 now = osGetTime();
    u64 left = g_sleepEnd > now ? (g_sleepEnd - now + 999) / 1000 : 0;
    if (left >= 3600) snprintf(out, n, "%d:%02d:%02d", (int)(left / 3600), (int)(left / 60 % 60), (int)(left % 60));
    else snprintf(out, n, "%02d:%02d", (int)(left / 60), (int)(left % 60));
}

static bool in_recent(int st, int depth) {
    for (int k = g_histN - 1; k >= 0 && k >= g_histN - depth; k--) if (g_hist[k] == st) return true;
    return false;
}

/* PREV / NEXT. Shuffle: NEXT picks a random station not heard recently, PREV walks back the history. */
static void step_station(int dir) {
    int n = g_stationCount;
    if (!n) return;
    if (g_cfg.shuffle && n > 2) {
        if (dir < 0) {
            if (g_histN > 0) { play_station(g_hist[--g_histN]); return; }
        } else {
            int depth = n / 2 < 8 ? n / 2 : 8, nxt = g_sel;
            for (int t = 0; t < 24 && (nxt == g_sel || in_recent(nxt, depth)); t++) nxt = rand() % n;
            if (nxt == g_sel) nxt = (g_sel + 1 + rand() % (n - 1)) % n;
            if (g_histN == HIST_N) { memmove(g_hist, g_hist + 1, sizeof(int) * (HIST_N - 1)); g_histN--; }
            g_hist[g_histN++] = g_sel;
            play_station(nxt);
            return;
        }
    }
    play_station(g_sel + dir);
}

/* --------------------------------------------------------------- top screen */
static void status_tag(const PlayerInfo* pi, float right, float y) {
    const char* lab; u32 fg, bg = 0; bool fill = false;
    switch (pi->state) {
        case PS_PLAYING:      lab = "ON AIR"; fill = true; bg = COL_RED; fg = COL_TEXT; break;
        case PS_CONNECTING:   lab = "LINK..."; fg = COL_ACC; break;
        case PS_BUFFERING:    lab = "BUFFER"; fg = COL_ACC; break;
        case PS_RECONNECTING: lab = "RELINK"; fg = COL_ACC; break;
        case PS_ERROR:        lab = "ERROR"; fg = COL_RED; break;
        default:              lab = "STANDBY"; fg = COL_DIM; break;
    }
    float w = mono_width(lab, 1) + 18, x = right - w;
    if (fill) ch_fill(x, y, w, 12, 4, CUT_TL | CUT_BR, bg);
    else ch_outline(x, y, w, 12, 4, CUT_TL | CUT_BR, fg, 1);
    bool blink = pi->state == PS_PLAYING ? fmodf(g_t, 1.0f) < 0.6f : (pi->state == PS_STOPPED ? true : fmodf(g_t * 3, 1.0f) < 0.5f);
    if (blink) C2D_DrawRectSolid(x + 5, y + 4, 0, 4, 4, fill ? COL_TEXT : fg);
    mono_draw(lab, x + 12, y + 2, 1, fg, 0);
}

static void draw_name(const PlayerInfo* pi, int idx) {
    const char* name = g_stationCount ? g_stations[idx].name : "NO STATIONS";
    if (strcmp(name, g_shownName)) { snprintf(g_shownName, sizeof(g_shownName), "%s", name); g_changeT = g_t; }
    float dt = g_t - g_changeT;
    char buf[192];
    if (dt < 0.5f) hyper_text(name, dt / 0.5f, buf, sizeof(buf), (unsigned)(g_t * 40));
    else snprintf(buf, sizeof(buf), "%s", name);
    float sc = text_fit(name, 1.0f, 340);
    float slide = (1 - ease_out_cubic(dt / 0.35f)) * 24;       /* kinetic slide-in */
    float x = 30 + slide, y = 54;
    if (g_cfg.glitch && g_t > g_nextGlitch) { g_glitchUntil = g_t + 0.09f; g_nextGlitch = g_t + 8 + frand() * 6; }
    bool glitch = g_cfg.glitch && (dt < 0.35f || g_t < g_glitchUntil);
    if (glitch) {
        float o = 2 + frand() * 3;
        text_draw(buf, x - o, y, sc, col_a(COL_RED, 0.8f), 0);
        text_draw(buf, x + o, y, sc, col_a(C2D_Color32(0, 230, 255, 255), 0.7f), 0);
        /* sliced displacement */
        for (int i = 0; i < 4; i++) {
            float sy = y + i * 8, off = (frand() - 0.5f) * 14;
            scissor_begin(true, 0, sy, TOP_W, 8);
            text_draw(buf, x + off, y, sc, COL_TEXT, 0);
            scissor_end();
        }
    } else {
        text_draw(buf, x, y, sc, COL_TEXT, 0);
    }
    C2D_DrawRectSolid(30, 88.5f, 0, 340, 1, COL_LINE);
    if (g_t - g_savedT < 2) {   /* song just saved: in place of SIGNAL LOCK, same size, never over anything */
        u32 c = g_savedLike ? COL_ACC : COL_DIM;
        float w = lbl(T(g_savedLike ? S_SAVED : S_REMOVED), 370, 92, 1, c, 2);
        heart(370 - w - 9, 95.5f, 4.5f, g_savedLike, c);
    } else mono_draw(pi->state == PS_PLAYING ? "SIGNAL LOCK" : "NO SIGNAL", 370, 92, 1, pi->state == PS_PLAYING ? COL_ACC : COL_DIM, 2);
    if (fav_is(idx)) { star(35, 96, 4.5f, true, COL_ACC); mono_draw("FAV", 43, 92, 1, COL_ACC, 0); }
}

/* player errors are produced in Spanish by the engine; translate the known ones */
static void tr_error(const char* e, char* out, size_t n) {
    static const struct { const char* es; StrId id; } MAP[] = {
        { "Formato no soportado", S_ERR_FORMAT }, { "Stream finalizado", S_ERR_END },
        { "Sin firmware DSP", S_ERR_NODSP }, { "No se pudo crear hilo", S_ERR_THREAD } };
    for (size_t i = 0; i < sizeof(MAP) / sizeof(MAP[0]); i++)
        if (!strncmp(e, MAP[i].es, strlen(MAP[i].es))) { snprintf(out, n, "%s", T(MAP[i].id)); return; }
    if (!strncmp(e, "Red: ", 5)) { snprintf(out, n, T(S_ERR_NET), e + 5); return; }
    snprintf(out, n, "%s", e);
}

/* network problem in the user's language, with the retry countdown */
static void net_message(const PlayerInfo* pi, char* out, size_t n) {
    char msg[160];
    snprintf(msg, sizeof(msg), T((StrId)(S_NE_SOC + pi->netErr - NE_SOC)), pi->netArg);
    if (pi->state == PS_ERROR) { snprintf(out, n, "%s", msg); return; }
    if (pi->netErr == NE_SOC || pi->netErr == NE_NOWIFI || pi->netErr == NE_NOIP) { snprintf(out, n, T(S_NET_WAITING), msg); return; }
    u64 now = osGetTime();
    if (pi->retryAt > now) snprintf(out, n, T(S_NET_RETRY_IN), msg, (int)((pi->retryAt - now + 999) / 1000));
    else snprintf(out, n, T(S_NET_RETRYING), msg);
}

static void draw_icy(const PlayerInfo* pi) {
    char line[300]; u32 lc = COL_TEXT;
    bool net = pi->netErr != NE_NONE && pi->state != PS_PLAYING && pi->state != PS_STOPPED;
    if (g_batLock) { snprintf(line, sizeof(line), "%s", T(S_BATL_NOW)); lc = COL_RED; }
    else if (net) { net_message(pi, line, sizeof(line)); lc = COL_RED; }
    else if (pi->state == PS_ERROR) { tr_error(pi->error, line, sizeof(line)); lc = COL_RED; }
    else if (pi->title[0]) snprintf(line, sizeof(line), "%s", pi->title);
    else if (pi->state == PS_STOPPED) { snprintf(line, sizeof(line), "%s", T(S_IDLE)); lc = COL_DIM; }
    else if (pi->icyName[0]) snprintf(line, sizeof(line), "%s", pi->icyName);
    else { snprintf(line, sizeof(line), "%s", T(S_NO_ICY)); lc = COL_DIM; }
    mono_draw(g_batLock ? "BAT>" : pi->state == PS_ERROR ? "ERR>" : net ? "NET>" : "NOW>", 30, 106, 1, g_batLock || pi->state == PS_ERROR || net ? COL_RED : COL_ACC, 0);
    float x0 = 60, x1 = 370;
    if (pi->title[0] && !net && pi->state != PS_ERROR) {   /* B saves the song that is playing */
        int k = now_song(pi);
        bool saved = k >= 0 && g_songs[k].liked;
        mono_draw("[B]", 352, 106, 1, saved ? COL_ACC : COL_DIM, 2);
        heart(363, 109.5f, 5, saved, saved ? COL_ACC : COL_DIM);
        x1 = 330;
    }
    float tw = text_width(line, 0.5f);
    scissor_begin(true, x0, 100, x1 - x0, 18);
    if (tw <= x1 - x0) text_draw(line, x0, 101, 0.5f, lc, 0);
    else {
        float span = tw + 50, off = fmodf(g_t * 32, span);
        text_draw(line, x0 - off, 101, 0.5f, lc, 0);
        text_draw(line, x0 - off + span, 101, 0.5f, lc, 0);
    }
    scissor_end();
}

/* volume: the console's slider (0-63), shown in the scope header. Dim at rest; lit with the % while
 * it moves and for 1.5 s after (then it fades for 0.5 s). */
static int   g_vol = -1;
static float g_volT = -10;   /* when it last moved */

static void vol_update(void) {
    u8 v = 0;
    if (R_FAILED(HIDUSER_GetSoundVolume(&v)) && !(g_mcuOk && R_SUCCEEDED(MCUHWC_GetSoundSliderLevel(&v)))) return;
    if (v > 63) v = 63;
    if (g_vol >= 0 && v != g_vol) g_volT = g_t;
    g_vol = v;
}

static void draw_vol(void) {
    if (g_vol < 0) return;
    float d = g_t - g_volT, f = d < 1.5f ? 1 : d < 2.0f ? (2.0f - d) / 0.5f : 0;
    float x = 186, y = 128;
    mono_draw("VOL", x, y, 1, f > 0 ? COL_ACC : COL_DIM, 0);
    seg_bar(x + 22, y + 1, 12, 5, 1, 6, g_vol / 63.0f, f > 0 ? COL_ACC : col_a(COL_ACC, 0.55f), COL_LINE);
    if (f > 0) {
        char pc[12];
        snprintf(pc, sizeof(pc), "%d%%", (g_vol * 100 + 31) / 63);
        mono_draw(pc, x + 100, y, 1, col_a(COL_ACC, f), 0);
    }
}

static void draw_scope(const PlayerInfo* pi) {
    float bx = 30, by = 124, bw = 340, bh = 78;
    ch_fill(bx, by, bw, bh, 7, CUT_TL | CUT_BR, col_a(COL_PANEL, 0.9f));
    ch_outline(bx, by, bw, bh, 7, CUT_TL | CUT_BR, COL_LINE, 1);
    dashed_hline(bx + 6, bx + bw - 6, by + bh / 2, 3, 3, col_a(COL_LINE, 0.9f));
    for (int i = 1; i < 8; i++) C2D_DrawRectSolid(bx + bw * i / 8, by + bh - 4, 0, 1, 4, COL_LINE);
    reticle(bx + bw / 2, by + bh / 2, 9 + g_pulse * 5, col_a(g_pulse > 0.3f ? COL_ACC : COL_DIM, 0.6f));
    mono_draw("SCOPE", bx + 10, by + 4, 1, COL_DIM, 0);
    mono_draw("BEAT", bx + 46, by + 4, 1, g_pulse > 0.35f ? COL_ACC : col_a(COL_DIM, 0.4f), 0);
    mono_draw("FFT/48", bx + bw - 8, by + 4, 1, COL_DIM, 2);
    /* data bars (monochrome, behind the trace) */
    float barw = (bw - 20) / NBANDS;
    for (int i = 0; i < NBANDS; i++) {
        float h = g_bands[i] * (bh - 18);
        float x = bx + 10 + i * barw;
        C2D_DrawRectSolid(x, by + bh - 3 - h, 0, barw - 2, h, col_a(COL_ACC, 0.22f));
        float py = by + bh - 4 - g_peaks[i] * (bh - 18);
        C2D_DrawRectSolid(x, py, 0, barw - 2, 1, col_a(COL_ACC, 0.65f));
    }
    /* oscilloscope trace */
    float cy = by + bh / 2, amp = bh * 0.42f;
    float px0 = bx + 8, py0 = cy - g_wave[0] * amp;
    for (int i = 1; i < NWAVE; i++) {
        float px = bx + 8 + (bw - 16) * i / (NWAVE - 1);
        float v = g_wave[i];
        if (v > 1) v = 1;
        if (v < -1) v = -1;
        float py = cy - v * amp;
        C2D_DrawLine(px0, py0, COL_ACC, px, py, COL_ACC, 1.6f, 0);
        px0 = px; py0 = py;
    }
    /* sweep marker */
    float sx = bx + 8 + fmodf(g_t * 120, bw - 16);
    C2D_DrawRectSolid(sx, by + 14, 0, 1, bh - 20, col_a(COL_ACC, 0.18f));
}

static void draw_footer(const PlayerInfo* pi) {
    char a[64], b[64];
    if (pi->state == PS_STOPPED || !pi->codec[0]) snprintf(a, sizeof(a), "CODEC ---");
    else {
        char br[12];
        if (pi->bitrate > 0) snprintf(br, sizeof(br), "%dK", pi->bitrate); else snprintf(br, sizeof(br), "--K");
        snprintf(a, sizeof(a), "%s %s %.1fKHZ", pi->codec, br, pi->samplerate / 1000.0f);
    }
    mono_draw(a, 30, 208, 1, COL_DIM, 0);
    mono_draw("BUF", 186, 208, 1, COL_DIM, 0);
    seg_bar(206, 209, 10, 4, 1, 6, pi->bufferPct / 100.0f, COL_ACC, COL_LINE);
    snprintf(b, sizeof(b), "DSP:%s", dspfw_source_name(g_fw));
    mono_draw(b, 370, 208, 1, g_fw == FW_NONE ? COL_RED : COL_DIM, 2);
    /* hex ticker: the real bytes arriving from the station, refreshed 6 times a second */
    static char hx[48] = "-- -- -- -- -- -- -- -- -- -- -- --";
    static int tick = -1;
    if ((int)(g_t * 6) != tick) {
        tick = (int)(g_t * 6);
        unsigned char b[12] = { 0 };
        int k = pi->state == PS_PLAYING ? player_peek_stream(b, 12) : 0, o = 0;
        for (int i = 0; i < 12; i++) o += snprintf(hx + o, sizeof(hx) - o, i < k ? "%02X " : "-- ", b[i]);
    }
    mono_draw(hx, 30, 220, 1, col_a(COL_DIM, 0.45f), 0);
    /* signature */
    mono_draw("//VENOMSHiNE", 370, 220, 1, col_a(COL_ACC, 0.6f), 2);
}

static void render_top(const PlayerInfo* pi) {
    C2D_DrawRectSolid(0, 0, 0, TOP_W, SCR_H, COL_BG);
    dot_matrix(TOP_W);
    /* kanji signage (dim neon) */
    if (frand() < 0.006f) g_flicker = 0.2f;
    g_flicker += (1 - g_flicker) * 0.12f;
    text_draw("無線", 392, 118, 1.6f, col_a(COL_ACC, 0.05f), 2);
    ch_outline(8, 36, 16, 66, 3, CUT_TL | CUT_BR, col_a(COL_ACC, 0.35f * g_flicker), 1);
    const char* vk[3] = { "ラ", "ジ", "オ" };
    for (int i = 0; i < 3; i++) text_draw(vk[i], 16, 40 + i * 20, 0.6f, col_a(COL_ACC, 0.55f * g_flicker), 1);
    /* frame */
    ch_outline(4, 4, 392, 232, 10, CUT_TL | CUT_BR, COL_LINE, 1);
    C2D_DrawLine(4, 14, COL_ACC, 14, 4, COL_ACC, 2, 0);
    C2D_DrawLine(386, 236, COL_ACC, 396, 226, COL_ACC, 2, 0);
    /* header */
    mono_draw("RADIO//NEON", 18, 10, 1, COL_ACC, 0);
    mono_draw("V" APP_VERSION " JP-NET", 88, 10, 1, COL_DIM, 0);
    time_t now = time(NULL);
    struct tm* tm = localtime(&now);
    char clk[16];
    snprintf(clk, sizeof(clk), "%02d:%02d:%02d", tm->tm_hour, tm->tm_min, tm->tm_sec);
    mono_draw(clk, 222, 10, 1, COL_TEXT, 1);
    status_tag(pi, 386, 8);
    C2D_DrawRectSolid(14, 24, 0, 372, 1, COL_LINE);
    C2D_DrawRectSolid(14, 24, 0, 30, 1, COL_ACC);
    /* channel block */
    int idx = g_playing >= 0 ? g_playing : g_sel;
    char ch[16], id[32], fq[32];
    snprintf(ch, sizeof(ch), "CH.%02d", idx + 1);
    mono_draw(ch, 30, 31, 2, COL_ACC, 0);
    if (g_stationCount) {
        url_host(g_stations[idx].url, id, sizeof(id));
        snprintf(fq, sizeof(fq), "%02d/%02d // %s", idx + 1, g_stationCount, strncmp(g_stations[idx].url, "https", 5) ? "HTTP" : "HTTPS");
        mono_draw(id, 96, 31, 1, COL_DIM, 0);
        mono_draw(fq, 96, 40, 1, COL_DIM, 0);
    }
    /* link quality (wifi) */
    u8 ws = osGetWifiStrength();
    mono_draw("SIG", 290, 31, 1, COL_DIM, 0);
    mono_draw(ws ? "LINK" : "----", 370, 31, 1, ws ? COL_TEXT : COL_RED, 2);
    seg_bar(290, 41, 3, 25, 2, 5, ws / 3.0f, COL_ACC, COL_LINE);

    draw_name(pi, idx);
    draw_icy(pi);
    draw_scope(pi);
    draw_vol();
    draw_footer(pi);
    if (g_fw == FW_NONE) {
        ch_fill(70, 136, 260, 46, 8, CUT_TL | CUT_BR, COL_BLACK);
        ch_outline(70, 136, 260, 46, 8, CUT_TL | CUT_BR, COL_RED, 1.5f);
        lbl(T(S_FW_NOAUDIO), 200, 144, 1, COL_RED, 1);
        mono_draw(g_fwDetail, 200, 156, 1, COL_TEXT, 1);
        lbl(T(S_FW_COPY), 200, 168, 1, COL_DIM, 1);
    }
    scanlines(TOP_W);
}

/* --------------------------------------------------------------- bottom screen */
typedef struct { float x, y, w, h, press; } Btn;
static Btn B_CARD = { 10, 24, 300, 62, 0 }, B_CSTAR = { 266, 24, 44, 62, 0 };   /* star: right column of the card */
static Btn B_SHUF = { 10, 93, 120, 19, 0 }, B_HIST = { 136, 93, 48, 19, 0 }, B_SLEEP = { 190, 93, 120, 19, 0 };   /* chips row */
static Btn B_PREV = { 10, 118, 84, 68, 0 }, B_PLAY = { 104, 118, 112, 68, 0 }, B_NEXT = { 226, 118, 84, 68, 0 };
static Btn B_MENU = { 10, 194, 146, 36, 0 }, B_EXIT = { 164, 194, 146, 36, 0 };
static Btn B_BACK = { 8, 6, 76, 20, 0 };
static Btn B_DOK = { 44, 128, 112, 36, 0 }, B_DNO = { 164, 128, 112, 36, 0 }, B_BOK = { 104, 128, 112, 36, 0 };
#define ROW_H 30
#define LIST_Y 32
#define MENU_Y 36
#define MENU_H 46
enum { MI_ALL, MI_FAV, MI_HIST, MI_SAVED, MI_OPT, MI_HELP, MI_ABOUT, MI_EXIT, NMENU };
#define NOPT 7
enum { OPT_ACCENT, OPT_LANG, OPT_LID, OPT_SAVER, OPT_AUTO, OPT_SCAN, OPT_GLITCH };
static const float OPT_Y[NOPT] = { 31, 74, 100, 126, 152, 178, 204 };
static const float OPT_H[NOPT] = { 41, 26, 26, 26, 26, 26, 26 };
#define NSLEEP 6
static const int SLEEP_MIN[NSLEEP] = { 0, 15, 30, 60, 90, -1 };   /* -1 = custom (keyboard) */
#define SLEEP_Y(i) (LIST_Y + 22 + (i) * 30)

/* shuffle: two crossing arrows */
static void icon_shuffle(float cx, float cy, float s, u32 c) {
    C2D_DrawLine(cx - s, cy - s * 0.55f, c, cx + s * 0.55f, cy + s * 0.55f, c, 1.6f, 0);
    C2D_DrawLine(cx - s, cy + s * 0.55f, c, cx + s * 0.55f, cy - s * 0.55f, c, 1.6f, 0);
    for (int k = -1; k <= 1; k += 2) {
        float ty = cy + k * s * 0.55f;
        C2D_DrawTriangle(cx + s, ty, c, cx + s * 0.45f, ty - 3.5f, c, cx + s * 0.45f, ty + 3.5f, c, 0);
    }
}

/* song history: list lines + a music note */
static void icon_history(float cx, float cy, u32 c) {
    for (int i = -1; i <= 1; i++) C2D_DrawRectSolid(cx - 11, cy + i * 4 - 0.5f, 0, i == 1 ? 7 : 12, 1.5f, c);
    C2D_DrawCircleSolid(cx + 6, cy + 4, 0, 2.6f, c);
    C2D_DrawRectSolid(cx + 7.6f, cy - 6, 0, 1.4f, 10, c);
    C2D_DrawLine(cx + 8.3f, cy - 6, c, cx + 12, cy - 3, c, 1.4f, 0);
}

/* sleep timer: clock face with hands */
static void icon_timer(float cx, float cy, float r, u32 c) {
    for (int i = 0; i < 24; i++) {
        float a0 = i * (float)M_PI / 12, a1 = (i + 1) * (float)M_PI / 12;
        C2D_DrawLine(cx + cosf(a0) * r, cy + sinf(a0) * r, c, cx + cosf(a1) * r, cy + sinf(a1) * r, c, 1.4f, 0);
    }
    C2D_DrawRectSolid(cx - 2, cy - r - 3, 0, 4, 2, c);
    C2D_DrawLine(cx, cy, c, cx, cy - r * 0.65f, c, 1.4f, 0);
    C2D_DrawLine(cx, cy, c, cx + r * 0.5f, cy, c, 1.4f, 0);
}

static bool hit(const Btn* b, int x, int y) { return x >= b->x && x < b->x + b->w && y >= b->y && y < b->y + b->h; }

static void hud_button(const Btn* b, int cuts, bool primary) {
    bool inv = b->press > 0.35f;
    if (primary || inv) ch_fill(b->x, b->y, b->w, b->h, 8, cuts, inv && primary ? COL_TEXT : COL_ACC);
    else ch_fill(b->x, b->y, b->w, b->h, 8, cuts, COL_PANEL);
    ch_outline(b->x, b->y, b->w, b->h, 8, cuts, primary || inv ? COL_ACC : COL_LINE, 1);
}

static void icon_skip(float cx, float cy, int dir, u32 c) {
    float s = 8;
    for (int k = 0; k < 2; k++) {
        float bx = cx + dir * (k ? 0 : -s) + (dir > 0 ? -3 : 3);
        C2D_DrawTriangle(bx, cy - s, c, bx, cy + s, c, bx + s * dir, cy, c, 0);
    }
    float bar = cx + dir * (s + 3);
    C2D_DrawRectSolid(bar - 1, cy - s, 0, 2, s * 2, c);
}

static void icon_playstop(float cx, float cy, float m, u32 c) {
    float s = 11, q = s * 0.8f;
    float tx0 = cx - s * 0.6f, ty0 = cy - s, tx1 = cx - s * 0.6f, ty1 = cy + s, tx2 = cx + s, ty2 = cy;
    float ax = tx0 + ((cx - q) - tx0) * m, ay = ty0 + ((cy - q) - ty0) * m;
    float bx = tx1 + ((cx - q) - tx1) * m, by = ty1 + ((cy + q) - ty1) * m;
    float c1x = tx2 + ((cx + q) - tx2) * m, c1y = ty2 + ((cy - q) - ty2) * m;
    float c2x = tx2 + ((cx + q) - tx2) * m, c2y = ty2 + ((cy + q) - ty2) * m;
    C2D_DrawTriangle(ax, ay, c, bx, by, c, c1x, c1y, c, 0);
    C2D_DrawTriangle(bx, by, c, c2x, c2y, c, c1x, c1y, c, 0);
}

/* 5 squares = 20 % each (1-20 % -> 1 ... 81-100 % -> 5); low under 15 %. Read every 5 s.
   Exact % from mcu::HWC; without it (some .3dsx launchers) the 0-5 level of ptm. */
static int  g_batPct = -1;      /* 0-100, -1 = unknown */
static bool g_batChg = false;

static void battery_read(void) {
    static u64 last = 0;
    u64 now = osGetTime();
    if (last && now - last <= 5000) return;
    last = now;
    u8 pct = 0, lv = 0, c = 0;
    if (g_mcuOk && R_SUCCEEDED(MCUHWC_GetBatteryLevel(&pct))) g_batPct = pct > 100 ? 100 : pct;
    else if (R_SUCCEEDED(PTMU_GetBatteryLevel(&lv))) g_batPct = (lv > 5 ? 5 : lv) * 20;   /* coarse: 0-5 */
    if (R_SUCCEEDED(PTMU_GetBatteryChargeState(&c))) g_batChg = c != 0;
}

static int battery_squares(bool* charging, bool* low) {
    battery_read();
    *charging = g_batChg; *low = g_batPct >= 0 && g_batPct < 15;
    return g_batPct < 0 ? 5 : (g_batPct + 19) / 20;
}

static void bottom_header(const char* title) {
    mono_draw(title, 12, 8, 1, COL_ACC, 0);
    u8 ws = osGetWifiStrength();
    mono_draw("NET", 222, 8, 1, COL_DIM, 0);
    for (int i = 0; i < 3; i++) C2D_DrawRectSolid(242 + i * 5, 15 - (3 + i * 2), 0, 3, 3 + i * 2, i < ws ? COL_ACC : COL_LINE);
    bool chg, low;
    int sq = battery_squares(&chg, &low);
    u32 on = low && !chg ? COL_RED : COL_ACC;
    mono_draw("BAT", 264, 8, 1, chg ? COL_ACC : COL_DIM, 0);
    for (int i = 0; i < 5; i++) {
        bool lit = i < sq || (chg && i == sq && fmodf(g_t, 1.0f) < 0.5f);
        C2D_DrawRectSolid(284 + i * 5, 9, 0, 4, 6, lit ? on : COL_LINE);
    }
}

/* header used by the sub screens: back button, title, kanji, right label */
static void sub_header(const char* title, const char* kanji, const char* right) {
    C2D_DrawRectSolid(0, 0, 0, BOT_W, LIST_Y - 2, COL_BG);
    C2D_DrawRectSolid(8, LIST_Y - 3, 0, 304, 1, COL_LINE);
    C2D_DrawRectSolid(8, LIST_Y - 3, 0, 30, 1, COL_ACC);
    hud_button(&B_BACK, CUT_TL | CUT_BR, false);
    lbl(T(S_BACK), B_BACK.x + B_BACK.w / 2, B_BACK.y + 6, 1, B_BACK.press > 0.35f ? COL_BLACK : COL_ACC, 1);
    lbl(title, 94, 12, 1, COL_TEXT, 0);
    float tw = lbl_width(title, 1);
    if (kanji && g_lang != LANG_JA) text_draw(kanji, 100 + tw, 6, 0.5f, col_a(COL_ACC, 0.55f), 0);
    if (right) mono_draw(right, 308, 12, 1, COL_DIM, 2);
}

static void spring_to(float target) { g_selV += (target - g_selY) * 0.22f; g_selV *= 0.62f; g_selY += g_selV; }

static void render_controls(const PlayerInfo* pi) {
    bottom_header("CTRL//PANEL");
    /* station card */
    Btn* c = &B_CARD;
    ch_fill(c->x, c->y, c->w, c->h, 8, CUT_TL | CUT_BR, COL_PANEL);
    ch_outline(c->x, c->y, c->w, c->h, 8, CUT_TL | CUT_BR, COL_LINE, 1);
    bool live = g_sel == g_playing && pi->state == PS_PLAYING;
    if (live) edge_trace(c->x, c->y, c->w, c->h, 8, CUT_TL | CUT_BR, g_t / 7.0f, 46, COL_ACC);   /* only while on air */
    if (g_stationCount) {
        char ch[24], fq[24], host[32];
        snprintf(ch, sizeof(ch), "CH %02d/%02d", g_sel + 1, g_stationCount);
        mono_draw(ch, c->x + 12, c->y + 7, 1, COL_ACC, 0);
        if (live && pi->codec[0]) {
            if (pi->bitrate > 0) snprintf(fq, sizeof(fq), "%s %dK", pi->codec, pi->bitrate);
            else snprintf(fq, sizeof(fq), "%s", pi->codec);
        } else snprintf(fq, sizeof(fq), "%s", strncmp(g_stations[g_sel].url, "https", 5) ? "HTTP" : "HTTPS");
        mono_draw(fq, c->x + 70, c->y + 7, 1, COL_DIM, 0);
        float rx = B_CSTAR.x;                 /* the star column starts here */
        if (live) {
            ch_fill(rx - 44, c->y + 5, 34, 11, 3, CUT_TL | CUT_BR, COL_RED);
            mono_draw("LIVE", rx - 27, c->y + 7, 1, COL_TEXT, 1);
        }
        /* favourite: own column at the right of the card (touch) */
        bool f = fav_is(g_sel);
        float sp = 1 + B_CSTAR.press * 0.5f;
        C2D_DrawRectSolid(rx, c->y + 8, 0, 1, c->h - 16, COL_LINE);
        star(rx + 22, c->y + 26, 9 * sp, f, f ? COL_ACC : COL_DIM);
        mono_draw("FAV", rx + 22, c->y + 42, 1, f ? COL_ACC : COL_DIM, 1);
        const char* nm = g_stations[g_sel].name;
        text_draw(nm, c->x + 12, c->y + 20, text_fit(nm, 0.7f, rx - c->x - 24), COL_TEXT, 0);
        url_host(g_stations[g_sel].url, host, sizeof(host));
        mono_draw(host, c->x + 12, c->y + 48, 1, COL_DIM, 0);
        lbl(T(S_CARD_LIST), rx - 10, c->y + 48, 1, COL_ACC, 2);
    }
    /* chips: shuffle (L) and sleep timer (R) */
    {
        bool on = g_cfg.shuffle;
        Btn* b = &B_SHUF;
        u32 col = b->press > 0.35f ? COL_BLACK : (on ? COL_ACC : COL_DIM);
        ch_fill(b->x, b->y, b->w, b->h, 5, CUT_TL | CUT_BR, b->press > 0.35f ? COL_ACC : COL_CARBON);
        ch_outline(b->x, b->y, b->w, b->h, 5, CUT_TL | CUT_BR, on ? COL_ACC : COL_LINE, 1);
        icon_shuffle(b->x + 16, b->y + 9.5f, 6, col);
        float tx = lbl(T(S_CHIP_SHUFFLE), b->x + 28, b->y + 6, 1, col, 0);
        mono_draw(on ? "ON" : "OFF", b->x + 28 + tx + 6, b->y + 6, 1, col, 0);

        on = g_sleepEnd != 0;
        b = &B_SLEEP;
        col = b->press > 0.35f ? COL_BLACK : (on ? COL_ACC : COL_DIM);
        ch_fill(b->x, b->y, b->w, b->h, 5, CUT_TL | CUT_BR, b->press > 0.35f ? COL_ACC : COL_CARBON);
        ch_outline(b->x, b->y, b->w, b->h, 5, CUT_TL | CUT_BR, on ? COL_ACC : COL_LINE, 1);
        icon_timer(b->x + 16, b->y + 10, 5, col);
        tx = lbl(T(on ? S_CHIP_SLEEP_ON : S_CHIP_SLEEP_OFF), b->x + 28, b->y + 6, 1, col, 0);
        if (on) { char left[16]; sleep_left_str(left, sizeof(left)); mono_draw(left, b->x + 28 + tx + 5, b->y + 6, 1, col, 0); }

        b = &B_HIST;   /* centre: song history (DOWN) */
        col = b->press > 0.35f ? COL_BLACK : COL_ACC;
        ch_fill(b->x, b->y, b->w, b->h, 5, CUT_TL | CUT_BR, b->press > 0.35f ? COL_ACC : COL_CARBON);
        ch_outline(b->x, b->y, b->w, b->h, 5, CUT_TL | CUT_BR, COL_ACC, 1);
        icon_history(b->x + b->w / 2, b->y + b->h / 2, col);
    }
    /* transport */
    hud_button(&B_PREV, CUT_TL | CUT_BL, false);
    icon_skip(B_PREV.x + B_PREV.w / 2, B_PREV.y + 28, -1, B_PREV.press > 0.35f ? COL_BLACK : COL_ACC);
    mono_draw("PREV", B_PREV.x + B_PREV.w / 2, B_PREV.y + 54, 1, B_PREV.press > 0.35f ? COL_BLACK : COL_DIM, 1);
    hud_button(&B_NEXT, CUT_TR | CUT_BR, false);
    icon_skip(B_NEXT.x + B_NEXT.w / 2, B_NEXT.y + 28, 1, B_NEXT.press > 0.35f ? COL_BLACK : COL_ACC);
    mono_draw(g_cfg.shuffle ? "RANDOM" : "NEXT", B_NEXT.x + B_NEXT.w / 2, B_NEXT.y + 54, 1,
              B_NEXT.press > 0.35f ? COL_BLACK : (g_cfg.shuffle ? COL_ACC : COL_DIM), 1);
    hud_button(&B_PLAY, CUT_TL | CUT_BR, true);
    icon_playstop(B_PLAY.x + B_PLAY.w / 2, B_PLAY.y + 28, g_morph, COL_BLACK);
    const char* pl = pi->state == PS_PLAYING ? "STOP" : (pi->state == PS_STOPPED || pi->state == PS_ERROR) ? "PLAY" : "LINK...";
    mono_draw(pl, B_PLAY.x + B_PLAY.w / 2, B_PLAY.y + 54, 1, COL_BLACK, 1);
    if (pi->state == PS_CONNECTING || pi->state == PS_BUFFERING || pi->state == PS_RECONNECTING) {
        float p = fmodf(g_t * 1.5f, 1.0f);
        C2D_DrawRectSolid(B_PLAY.x + 10, B_PLAY.y + B_PLAY.h - 5, 0, (B_PLAY.w - 20) * p, 2, COL_BLACK);
    }
    /* bottom row */
    hud_button(&B_MENU, CUT_TL | CUT_BR, false);
    lbl(T(S_BTN_MENU), B_MENU.x + B_MENU.w / 2, B_MENU.y + 14, 1, B_MENU.press > 0.35f ? COL_BLACK : COL_TEXT, 1);
    hud_button(&B_EXIT, CUT_TL | CUT_BR, false);
    lbl(T(S_BTN_EXIT), B_EXIT.x + B_EXIT.w / 2, B_EXIT.y + 14, 1, B_EXIT.press > 0.35f ? COL_BLACK : COL_DIM, 1);
}

/* ---------------- station list (all / favourites) */
static void build_view(void) {
    g_viewN = 0;
    for (int i = 0; i < g_stationCount; i++) if (!g_favView || fav_is(i)) g_view[g_viewN++] = i;
    if (g_cur >= g_viewN) g_cur = g_viewN - 1;
    if (g_cur < 0) g_cur = 0;
}

static float list_max_scroll(void) { float m = g_viewN * ROW_H - (SCR_H - LIST_Y); return m > 0 ? m : 0; }

static void list_follow(void) {
    float y = g_cur * ROW_H;
    if (y < g_listScroll) g_listScroll = y;
    if (y + ROW_H > g_listScroll + (SCR_H - LIST_Y)) g_listScroll = y + ROW_H - (SCR_H - LIST_Y);
}

static void set_mode(int m) { g_mode = m; g_modeT = g_t; }

static void open_list(bool fav, int from) {
    g_favView = fav;
    g_listFrom = from;
    build_view();
    g_cur = 0;
    for (int k = 0; k < g_viewN; k++) if (g_view[k] == g_sel) g_cur = k;
    g_selY = g_cur * ROW_H; g_selV = 0;
    float yy = g_cur * ROW_H - 60, m = list_max_scroll();
    g_listScroll = yy < 0 ? 0 : (yy > m ? m : yy);
    set_mode(M_LIST);
}

/* list cursor onto the current station (after L/R or a reload) */
static void list_sync_cursor(void) {
    for (int k = 0; k < g_viewN; k++) if (g_view[k] == g_sel) { g_cur = k; list_follow(); return; }
}

static void render_list(const PlayerInfo* pi) {
    if (fabsf(g_listVel) > 0.1f) { g_listScroll += g_listVel; g_listVel *= 0.92f; }
    float mx = list_max_scroll();
    if (g_listScroll < 0) { g_listScroll *= 0.7f; g_listVel = 0; }
    if (g_listScroll > mx) { g_listScroll = mx + (g_listScroll - mx) * 0.7f; g_listVel = 0; }
    spring_to(g_cur * ROW_H);   /* rareui Bounce Sidebar spring */

    scissor_begin(false, 0, LIST_Y, BOT_W, SCR_H - LIST_Y);
    dashed_vline(5, LIST_Y - fmodf(g_listScroll, 6), SCR_H, 3, 3, COL_LINE);   /* rareui Hook Sidebar rail */
    if (g_viewN) {
        float hy = LIST_Y + g_selY - g_listScroll;
        ch_fill(12, hy + 2, 296, ROW_H - 4, 6, CUT_TL | CUT_BR, COL_CARBON);
        ch_outline(12, hy + 2, 296, ROW_H - 4, 6, CUT_TL | CUT_BR, COL_ACC, 1);
        C2D_DrawRectSolid(4, hy + 6, 0, 3, ROW_H - 12, COL_ACC);
        C2D_DrawRectSolid(7, hy + ROW_H / 2, 0, 5, 1, COL_ACC);
    }
    for (int k = 0; k < g_viewN; k++) {
        int i = g_view[k];
        float y = LIST_Y + k * ROW_H - g_listScroll;
        if (y < LIST_Y - ROW_H || y > SCR_H) continue;
        bool sel = k == g_cur, play = i == g_playing;
        if (!sel) C2D_DrawRectSolid(14, y + ROW_H - 1, 0, 292, 1, COL_LINE);
        char num[12], host[32];
        snprintf(num, sizeof(num), "%02d", i + 1);
        if (sel) { C2D_DrawRectSolid(16, y + 6, 0, 18, 13, COL_ACC); mono_draw(num, 25, y + 9, 1, COL_BLACK, 1); }
        else mono_draw(num, 25, y + 9, 1, COL_DIM, 1);
        /* line 1: name + genre tag (right, next to the star); line 2: server + LIVE/LINK */
        const char* gn = g_stations[i].genre;
        float gw = gn[0] ? lbl_width(gn, 1) : 0, gx = 278 - gw;
        if (gn[0]) {
            ch_outline(gx - 4, y + 3, gw + 8, 12, 3, CUT_TL | CUT_BR, col_a(sel ? COL_ACC : COL_LINE, sel ? 0.7f : 1), 1);
            lbl(gn, gx, y + 6, 1, col_a(sel ? COL_ACC : COL_DIM, sel ? 1 : 0.8f), 0);
        }
        const char* nm = g_stations[i].name;
        float nmax = (gn[0] ? gx - 10 : 274) - 40;
        text_draw(nm, 40, y + 3, text_fit(nm, 0.5f, nmax), sel ? COL_TEXT : col_a(COL_TEXT, 0.75f), 0);
        url_host(g_stations[i].url, host, sizeof(host));
        mono_draw(host, 40, y + 18, 1, col_a(COL_DIM, sel ? 1 : 0.6f), 0);
        if (play) {
            bool live = pi->state == PS_PLAYING;
            ch_fill(244, y + 17, 34, 10, 3, CUT_TL | CUT_BR, live ? COL_RED : COL_CARBON);
            if (!live) ch_outline(244, y + 17, 34, 10, 3, CUT_TL | CUT_BR, COL_ACC, 1);
            mono_draw(live ? "LIVE" : "LINK", 261, y + 18, 1, live ? COL_TEXT : COL_ACC, 1);
        }
        bool f = fav_is(i);
        star(292, y + ROW_H / 2, 6, f, f ? COL_ACC : col_a(COL_DIM, sel ? 1 : 0.55f));
    }
    if (!g_viewN) {
        star(160, 104, 16, false, col_a(COL_ACC, 0.6f));
        lbl(T(S_NOFAV), 160, 132, 2, COL_TEXT, 1);
        lbl(T(S_NOFAV1), 160, 156, 1, COL_DIM, 1);
        lbl(T(S_NOFAV2), 160, 168, 1, COL_DIM, 1);
    }
    scissor_end();
    if (mx > 0) {
        float vis = (SCR_H - LIST_Y) / (float)(g_viewN * ROW_H);
        float bh = (SCR_H - LIST_Y) * vis, by = LIST_Y + (SCR_H - LIST_Y - bh) * (g_listScroll / mx);
        C2D_DrawRectSolid(314, LIST_Y, 0, 1, SCR_H - LIST_Y, COL_LINE);
        C2D_DrawRectSolid(313, by, 0, 3, bh, COL_ACC);
    }
    char cnt[24];
    snprintf(cnt, sizeof(cnt), "%02d/%02d", g_viewN, g_stationCount);
    sub_header(T(g_favView ? S_HDR_FAVS : S_HDR_STATIONS), g_favView ? "お気に入り" : "放送局", cnt);
}

/* ---------------- menu root */
static const char* MENU_KANJI[NMENU] = { "放送局", "お気に入り", "履歴", "保存曲", "設定", "操作", "情報", "終了" };
static const StrId MENU_LABEL[NMENU] = { S_MENU_ALL, S_MENU_FAV, S_MENU_HIST, S_MENU_SAVED, S_MENU_OPT, S_MENU_HELP, S_MENU_ABOUT, S_MENU_EXIT };
static float g_menuScroll = 0;
static bool  g_menuFree = false;   /* dragged by touch: do not follow the cursor until a key moves it */
#define MENU_VIS (SCR_H - MENU_Y)
static float menu_max_scroll(void) { float m = NMENU * MENU_H - MENU_VIS; return m > 0 ? m : 0; }
static int liked_count(void) { int n = 0; for (int k = 0; k < g_songCount; k++) n += g_songs[k].liked; return n; }

static void render_menu(void) {
    spring_to(g_menuCur * MENU_H);
    if (!g_menuFree) {   /* keep the cursor in view */
        float target = g_menuCur * MENU_H - (MENU_VIS - MENU_H) / 2;
        if (target > menu_max_scroll()) target = menu_max_scroll();
        if (target < 0) target = 0;
        g_menuScroll += (target - g_menuScroll) * 0.25f;
    }
    dashed_vline(5, LIST_Y, SCR_H, 3, 3, COL_LINE);
    scissor_begin(false, 0, MENU_Y - 4, BOT_W, SCR_H - MENU_Y + 4);
    float hy = MENU_Y + g_selY - g_menuScroll;
    C2D_DrawRectSolid(4, hy + 8, 0, 3, MENU_H - 20, COL_ACC);
    for (int i = 0; i < NMENU; i++) {
        float y = MENU_Y + i * MENU_H - g_menuScroll;
        if (y < MENU_Y - MENU_H || y > SCR_H) continue;
        bool sel = i == g_menuCur;
        bool exit = i == MI_EXIT;
        ch_fill(12, y + 2, 292, MENU_H - 6, 7, CUT_TL | CUT_BR, sel ? COL_CARBON : COL_PANEL);
        ch_outline(12, y + 2, 292, MENU_H - 6, 7, CUT_TL | CUT_BR, sel ? (exit ? COL_RED : COL_ACC) : COL_LINE, 1);
        if (sel) brackets(10, y, 296, MENU_H - 2, 5, col_a(exit ? COL_RED : COL_ACC, 0.5f + 0.5f * sinf(g_t * 6)), 1);
        char num[8]; snprintf(num, sizeof(num), "%02d", i + 1);
        if (sel) { ch_fill(20, y + 12, 22, 16, 3, CUT_TL | CUT_BR, exit ? COL_RED : COL_ACC); mono_draw(num, 31, y + 16, 1, COL_BLACK, 1); }
        else mono_draw(num, 31, y + 16, 1, COL_DIM, 1);
        lbl(T(MENU_LABEL[i]), 52, y + 10, 2, sel ? COL_TEXT : col_a(COL_TEXT, 0.7f), 0);
        char sub[96];
        switch (i) {
            case MI_ALL:   snprintf(sub, sizeof(sub), T(S_SUB_ALL), g_stationCount); break;
            case MI_FAV:   snprintf(sub, sizeof(sub), T(S_SUB_FAV), fav_count()); break;
            case MI_HIST:  snprintf(sub, sizeof(sub), T(S_SUB_HIST), g_songCount); break;
            case MI_SAVED: snprintf(sub, sizeof(sub), T(S_SUB_SAVED), liked_count()); break;
            case MI_OPT:   snprintf(sub, sizeof(sub), T(S_SUB_OPT), acc_name(g_cfg.accent)); break;
            case MI_HELP:  snprintf(sub, sizeof(sub), "%s", T(S_SUB_HELP)); break;
            case MI_ABOUT: snprintf(sub, sizeof(sub), "%s", T(S_SUB_ABOUT)); break;
            default:       snprintf(sub, sizeof(sub), "%s", T(S_SUB_EXIT)); break;
        }
        lbl(sub, 52, y + 28, 1, col_a(COL_DIM, sel ? 1 : 0.7f), 0);
        u32 ic = sel ? (exit ? COL_RED : COL_ACC) : COL_DIM;
        if (g_lang != LANG_JA) text_draw(MENU_KANJI[i], 296, y + 23, 0.42f, col_a(exit ? COL_RED : COL_ACC, sel ? 0.7f : 0.3f), 2);
        if (i == MI_FAV) star(292, y + 13, 4.5f, fav_count() > 0, ic);
        else if (i == MI_SAVED) heart(292, y + 13, 4.5f, liked_count() > 0, ic);
        else mono_draw(">", 296, y + 10, 1, ic, 2);
    }
    scissor_end();
    /* scrollbar */
    float track = MENU_VIS - 8, total = NMENU * MENU_H;
    float th = track * MENU_VIS / total, ty = MENU_Y + (track - th) * (g_menuScroll / menu_max_scroll());
    C2D_DrawRectSolid(312, MENU_Y, 0, 1, track, COL_LINE);
    C2D_DrawRectSolid(311, ty, 0, 3, th, COL_ACC);
    sub_header("MENU//SYS", "メニュー", "V" APP_VERSION);
}

/* ---------------- options */
static void render_options(void) {
    spring_to(OPT_Y[g_optCur]);
    sub_header(T(S_HDR_OPT), "設定", "CONFIG.INI");
    dashed_vline(5, LIST_Y, SCR_H, 3, 3, COL_LINE);
    C2D_DrawRectSolid(4, g_selY + 7, 0, 3, OPT_H[g_optCur] - 14, COL_ACC);
    static const StrId LABEL[NOPT] = { S_OPT_ACCENT, S_OPT_LANG, S_OPT_LID, S_OPT_SAVER, S_OPT_AUTO, S_OPT_SCAN, S_OPT_GLITCH };
    bool hs = osIsHeadsetConnected();
    for (int i = 0; i < NOPT; i++) {
        float y = OPT_Y[i], h = OPT_H[i];
        bool sel = i == g_optCur;
        ch_fill(12, y + 1, 296, h - 2, 6, CUT_TL | CUT_BR, sel ? COL_CARBON : COL_PANEL);
        ch_outline(12, y + 1, 296, h - 2, 6, CUT_TL | CUT_BR, sel ? COL_ACC : COL_LINE, 1);
        text_draw(T(LABEL[i]), 22, y + (g_lang == LANG_JA ? 3 : 2), 0.46f, sel ? COL_TEXT : col_a(COL_TEXT, 0.75f), 0);
        char sb[96] = "";
        u32 sc = col_a(COL_DIM, sel ? 1 : 0.7f);
        if (i == OPT_ACCENT) {
            for (int k = 0; k < NUM_ACCENTS; k++) {
                const AccentPreset* a = &ACCENTS[k];
                u32 c = C2D_Color32(a->r, a->g, a->b, 255);
                float sx = 22 + k * 24, sy = y + 21;
                ch_fill(sx, sy, 18, 14, 3, CUT_TL | CUT_BR, c);
                if (k == g_cfg.accent) ch_outline(sx - 3, sy - 3, 24, 20, 4, CUT_TL | CUT_BR, COL_TEXT, 1);
            }
            mono_draw("<", 196, y + 25, 1, COL_DIM, 0);
            lbl(acc_name(g_cfg.accent), 250, y + 25, 1, COL_ACC, 1);
            mono_draw(">", 300, y + 25, 1, COL_DIM, 2);
            continue;
        }
        if (i == OPT_LANG) {
            lbl(T(S_OPTS_LANG), 22, y + 15, 1, sc, 0);
            char v[48];
            if (g_cfg.lang == LANGSEL_SYSTEM) {
                static const char* SYS[] = { "ES", "EN", "JA" };
                snprintf(v, sizeof(v), "%s (%s)", T(S_LANG_SYSTEM), SYS[i18n_system_lang()]);
            } else snprintf(v, sizeof(v), "%s", lang_name(g_cfg.lang));
            mono_draw("<", 220, y + 9, 1, COL_DIM, 0);
            lbl(v, 262, y + 9, 1, COL_ACC, 1);
            mono_draw(">", 304, y + 9, 1, COL_DIM, 2);
            continue;
        }
        bool on = false;
        switch (i) {
            case OPT_LID:     snprintf(sb, sizeof(sb), T(S_OPTS_LID), T(hs ? S_HS_YES : S_HS_NO)); on = g_cfg.lidPlay; break;
            case OPT_SAVER:   snprintf(sb, sizeof(sb), "%s", T(S_OPTS_SAVER)); on = g_cfg.saver; break;
            case OPT_AUTO:    snprintf(sb, sizeof(sb), "%s", T(S_OPTS_AUTO)); on = g_cfg.autoplay; break;
            case OPT_SCAN:    snprintf(sb, sizeof(sb), "%s", T(S_OPTS_SCAN)); on = g_cfg.scanlines; break;
            default:          snprintf(sb, sizeof(sb), "%s", T(S_OPTS_GLITCH)); on = g_cfg.glitch; break;
        }
        lbl(sb, 22, y + 15, 1, sc, 0);
        hud_switch(256, y + 5, on);
    }
}

/* ---------------- about */
/* QR codes for the about screen: version 4 (33 modules) for all three, so they have the same size and
   2 px per module exactly (no blurry half pixels). Encoded once. */
#define QR_VER 4
static uint8_t g_qr[3][qrcodegen_BUFFER_LEN_FOR_VERSION(QR_VER)];
static bool g_qrOk[3];

static void qr_init(void) {
    static const char* URLS[3] = { URL_GITHUB, URL_DISCORD, URL_KOFI };
    uint8_t tmp[qrcodegen_BUFFER_LEN_FOR_VERSION(QR_VER)];
    for (int i = 0; i < 3; i++)
        g_qrOk[i] = qrcodegen_encodeText(URLS[i], tmp, g_qr[i], qrcodegen_Ecc_LOW, QR_VER, QR_VER, qrcodegen_Mask_AUTO, true);
}

/* white quiet frame of 4 px outside (x, y), dark modules merged in horizontal runs */
static void draw_qr(const uint8_t* qr, float x, float y, int px) {
    int n = qrcodegen_getSize(qr);
    C2D_DrawRectSolid(x - 4, y - 4, 0, n * px + 8, n * px + 8, C2D_Color32(255, 255, 255, 255));
    for (int r = 0; r < n; r++) {
        for (int c = 0; c < n;) {
            if (!qrcodegen_getModule(qr, c, r)) { c++; continue; }
            int c0 = c;
            while (c < n && qrcodegen_getModule(qr, c, r)) c++;
            C2D_DrawRectSolid(x + c0 * px, y + r * px, 0, (c - c0) * px, px, C2D_Color32(0, 0, 0, 255));
        }
    }
}

/* ---------------- controls: the lower half of the console drawn as a schematic (top screen),
 * a line from each button to what it does on the main panel; the keys of the lists below (bottom screen).
 * Pressing a real button lights it up in the drawing. */
enum { HK_L, HK_R, HK_CPAD, HK_DPAD, HK_X, HK_Y, HK_A, HK_B, HK_START, HK_SELECT, HK_TOUCH, NHK };
static float g_helpHit[NHK];   /* when each one was pressed (0 = never) */

typedef struct { int key; const char* tag; StrId act, act2; int right; float lx, ly; int n; float px[4], py[4]; } Callout;
/* path: from the button to the end of the underline (lx = the label's outer edge) */
static const Callout HELP_CO[] = {
    { HK_L,      "[L]",      S_H_L,      0, 0,   8,  46, 2, { 104,  86 },      {  59,  46 } },
    { HK_CPAD,   "[O]",      S_H_CPAD,   0, 0,   8,  98, 2, {  99,  86 },      {  98,  98 } },
    { HK_DPAD,   "[+]",      S_H_DPAD,   0, 0,   8, 133, 2, {  99,  86 },      { 133, 133 } },
    { HK_R,      "[R]",      S_H_R,      0, 1, 392,  46, 2, { 296, 314 },      {  59,  46 } },
    { HK_X,      "[X]",      S_H_X,      0, 1, 392,  72, 2, { 289, 314 },      {  95,  72 } },
    { HK_A,      "[A]",      S_H_A,      0, 1, 392,  98, 2, { 301, 314 },      { 107,  98 } },
    { HK_B,      "[B]",      S_H_B,      0, 1, 392, 124, 2, { 291, 314 },      { 122, 124 } },
    { HK_Y,      "[Y]",      S_H_Y,      0, 1, 392, 150, 3, { 273, 273, 314 }, { 116, 131, 150 } },
    { HK_START,  "[START]",  S_H_START, S_H_START2, 1, 392, 176, 2, { 277, 314 }, { 147, 176 } },
    { HK_SELECT, "[SELECT]", S_H_SELECT, 0, 1, 392, 210, 2, { 277, 314 },      { 161, 210 } },
};
#define NCO (int)(sizeof(HELP_CO) / sizeof(HELP_CO[0]))

static float help_flash(int k) { float d = g_t - g_helpHit[k]; return g_helpHit[k] > 0 && d < 0.6f ? 1 - d / 0.6f : 0; }

static void ring(float cx, float cy, float r, u32 c) {
    for (int i = 0; i < 20; i++) {
        float a0 = i * 6.2832f / 20, a1 = (i + 1) * 6.2832f / 20;
        C2D_DrawLine(cx + cosf(a0) * r, cy + sinf(a0) * r, c, cx + cosf(a1) * r, cy + sinf(a1) * r, c, 1, 0);
    }
}

/* a round button: ring, letter, filled while lit */
static void help_btn(float cx, float cy, float r, const char* letter, int key) {
    float f = help_flash(key);
    if (f > 0) C2D_DrawCircleSolid(cx, cy, 0, r, col_a(COL_ACC, f));
    ring(cx, cy, r, f > 0 ? COL_ACC : COL_TEXT);
    if (letter) mono_draw(letter, cx + 0.5f, cy - 3, 1, f > 0.5f ? COL_BLACK : COL_TEXT, 1);
}

/* small pill (START, SELECT, HOME) */
static void help_pill(float cx, float cy, int key, u32 c) {
    float f = key >= 0 ? help_flash(key) : 0;
    if (f > 0) ch_fill(cx - 7, cy - 3, 14, 6, 2, CUT_TL | CUT_TR | CUT_BL | CUT_BR, col_a(COL_ACC, f));
    ch_outline(cx - 7, cy - 3, 14, 6, 2, CUT_TL | CUT_TR | CUT_BL | CUT_BR, f > 0 ? COL_ACC : c, 1);
}

/* New 3DS XL lower half, traced from a photo of the shell: 216 px wide (0.277 px per photo px) */
static void help_console(void) {
    u32 body = COL_LINE, det = col_a(COL_TEXT, 0.85f);
    /* body: square top (the hinge), big round corners at the bottom */
    const float bx = 92, by = 62, bw = 216, bh = 122, rr = 22;
    u32 bf = col_a(COL_PANEL, 0.9f);
    C2D_DrawRectSolid(bx, by, 0, bw, bh - rr, bf);
    C2D_DrawRectSolid(bx + rr, by + bh - rr, 0, bw - 2 * rr, rr, bf);
    C2D_DrawCircleSolid(bx + rr, by + bh - rr, 0, rr, bf);
    C2D_DrawCircleSolid(bx + bw - rr, by + bh - rr, 0, rr, bf);
    C2D_DrawLine(bx, by, COL_TEXT, bx + bw, by, COL_TEXT, 1, 0);
    C2D_DrawLine(bx, by, COL_TEXT, bx, by + bh - rr, COL_TEXT, 1, 0);
    C2D_DrawLine(bx + bw, by, COL_TEXT, bx + bw, by + bh - rr, COL_TEXT, 1, 0);
    C2D_DrawLine(bx + rr, by + bh, COL_TEXT, bx + bw - rr, by + bh, COL_TEXT, 1, 0);
    for (int i = 0; i < 8; i++) {   /* the two quarter circles */
        float a0 = i * 1.5708f / 8, a1 = (i + 1) * 1.5708f / 8;
        C2D_DrawLine(bx + rr - sinf(a0) * rr, by + bh - rr + cosf(a0) * rr, COL_TEXT, bx + rr - sinf(a1) * rr, by + bh - rr + cosf(a1) * rr, COL_TEXT, 1, 0);
        C2D_DrawLine(bx + bw - rr + sinf(a0) * rr, by + bh - rr + cosf(a0) * rr, COL_TEXT, bx + bw - rr + sinf(a1) * rr, by + bh - rr + cosf(a1) * rr, COL_TEXT, 1, 0);
    }
    C2D_DrawRectSolid(114, 69, 0, 171, 1, body);   /* hinge bar */
    brackets(88, 56, 224, 132, 6, col_a(COL_ACC, 0.6f), 1);
    /* hinge blocks, with L and R on them */
    float fl = help_flash(HK_L), fr = help_flash(HK_R);
    ch_fill(92, 59, 23, 17, 3, CUT_TL | CUT_TR | CUT_BL | CUT_BR, fl > 0 ? col_a(COL_ACC, fl) : COL_CARBON);
    ch_outline(92, 59, 23, 17, 3, CUT_TL | CUT_TR | CUT_BL | CUT_BR, fl > 0 ? COL_ACC : det, 1);
    ch_fill(285, 59, 23, 17, 3, CUT_TL | CUT_TR | CUT_BL | CUT_BR, fr > 0 ? col_a(COL_ACC, fr) : COL_CARBON);
    ch_outline(285, 59, 23, 17, 3, CUT_TL | CUT_TR | CUT_BL | CUT_BR, fr > 0 ? COL_ACC : det, 1);
    mono_draw("L", 104, 64, 1, fl > 0.5f ? COL_BLACK : COL_DIM, 1);
    mono_draw("R", 297, 64, 1, fr > 0.5f ? COL_BLACK : COL_DIM, 1);
    /* screen bezel down to the bottom edge, the touch screen, HOME under it */
    ch_outline(133, 74, 132, 110, 3, CUT_TL | CUT_TR, body, 1);
    float ft = help_flash(HK_TOUCH);
    ch_fill(139, 80, 120, 90, 2, CUT_TL | CUT_BR, ft > 0 ? col_a(COL_ACC, 0.25f + 0.5f * ft) : COL_CARBON);
    ch_outline(139, 80, 120, 90, 2, CUT_TL | CUT_BR, ft > 0 ? COL_ACC : det, 1);
    lbl(T(S_H_TOUCH), 199, 116, 1, COL_ACC, 1);
    lbl(T(S_H_TOUCH2), 199, 128, 1, COL_DIM, 1);
    help_pill(199, 177, -1, det);
    /* left: circle pad in its recess, D-pad */
    float fc = help_flash(HK_CPAD);
    ring(114, 98, 14, body);
    if (fc > 0) C2D_DrawCircleSolid(114, 98, 0, 9, col_a(COL_ACC, fc));
    ring(114, 98, 9, fc > 0 ? COL_ACC : det);
    ring(114, 98, 5, fc > 0 ? COL_ACC : body);
    float fd = help_flash(HK_DPAD);
    u32 dc = fd > 0 ? COL_ACC : det;
    const float cx = 113, cy = 133, a = 13, w = 4.7f;
    if (fd > 0) { C2D_DrawRectSolid(cx - a, cy - w, 0, a * 2, w * 2, col_a(COL_ACC, fd)); C2D_DrawRectSolid(cx - w, cy - a, 0, w * 2, a * 2, col_a(COL_ACC, fd)); }
    const float DX[] = { cx - w, cx + w, cx + w, cx + a, cx + a, cx + w, cx + w, cx - w, cx - w, cx - a, cx - a, cx - w, cx - w };
    const float DY[] = { cy - a, cy - a, cy - w, cy - w, cy + w, cy + w, cy + a, cy + a, cy + w, cy + w, cy - w, cy - w, cy - a };
    for (int i = 0; i < 12; i++) C2D_DrawLine(DX[i], DY[i], dc, DX[i + 1], DY[i + 1], dc, 1, 0);
    /* right: C-stick, ABXY, START over SELECT */
    ring(273, 87, 4.5f, det);
    help_btn(284.5f, 99.5f, 5.5f, "X", HK_X);
    help_btn(273, 110, 5.5f, "Y", HK_Y);
    help_btn(296, 110, 5.5f, "A", HK_A);
    help_btn(284.5f, 121, 5.5f, "B", HK_B);
    help_btn(273, 147, 3.2f, NULL, HK_START);
    help_btn(273, 161, 3.2f, NULL, HK_SELECT);
    mono_draw("HOME", 199, 188, 1, col_a(COL_DIM, 0.6f), 1);
}

static void render_top_help(void) {
    C2D_DrawRectSolid(0, 0, 0, TOP_W, SCR_H, COL_BG);
    dot_matrix(TOP_W);
    ch_outline(4, 4, 392, 232, 10, CUT_TL | CUT_BR, COL_LINE, 1);
    C2D_DrawLine(4, 14, COL_ACC, 14, 4, COL_ACC, 2, 0);
    C2D_DrawLine(386, 236, COL_ACC, 396, 226, COL_ACC, 2, 0);
    mono_draw("RADIO//NEON", 18, 10, 1, COL_ACC, 0);
    mono_draw("CONTROLS // CTRL PANEL", 88, 10, 1, COL_DIM, 0);
    C2D_DrawRectSolid(14, 24, 0, 372, 1, COL_LINE);
    C2D_DrawRectSolid(14, 24, 0, 30, 1, COL_ACC);
    help_console();
    float t = g_t - g_modeT;
    for (int i = 0; i < NCO; i++) {
        const Callout* c = &HELP_CO[i];
        float p = (t - 0.15f - i * 0.04f) / 0.3f;
        if (p <= 0) continue;
        if (p > 1) p = 1;
        float f = help_flash(c->key);
        u32 lc = f > 0 ? COL_ACC : col_a(COL_ACC, 0.55f);
        /* the line grows from the button: path segments, then the underline under the label */
        float len = 0, seg[5];
        int n = c->n;
        for (int k = 0; k + 1 < n; k++) { seg[k] = hypotf(c->px[k + 1] - c->px[k], c->py[k + 1] - c->py[k]); len += seg[k]; }
        float ul = fabsf(c->lx - c->px[n - 1]);
        seg[n - 1] = ul; len += ul;
        float left = len * ease_out_cubic(p);
        C2D_DrawCircleSolid(c->px[0], c->py[0], 0, 1.5f, lc);
        for (int k = 0; k < n && left > 0; k++) {
            float x0 = c->px[k], y0 = c->py[k];
            float x1 = k + 1 < n ? c->px[k + 1] : c->lx, y1 = k + 1 < n ? c->py[k + 1] : c->ly;
            float sl = seg[k], u = sl > 0 ? (left < sl ? left / sl : 1) : 1;
            C2D_DrawLine(x0, y0, lc, x0 + (x1 - x0) * u, y0 + (y1 - y0) * u, lc, 1, 0);
            left -= sl;
        }
        if (p < 1) continue;
        float tx = c->right ? c->lx - 76 : c->lx + 2;
        if (f > 0) C2D_DrawRectSolid(c->right ? c->lx - 78 : c->lx, c->ly - 20, 0, 78, 20, col_a(COL_ACC, 0.18f * f + 0.05f));
        mono_draw(c->tag, tx, c->ly - 19, 1, COL_ACC, 0);
        float d = t - 0.45f - i * 0.04f;
        char buf[64];
        const char* act = T(c->act);
        if (d < 0.3f && mono_ok(act)) { hyper_text(act, d / 0.3f, buf, sizeof(buf), (unsigned)(g_t * 40) + i); act = buf; }
        lbl(act, tx, c->ly - 10, 1, f > 0 ? COL_ACC : COL_TEXT, 0);
        if (c->act2) lbl(T(c->act2), tx, c->ly + 3, 1, COL_DIM, 0);
    }
    scanlines(TOP_W);
}

/* bottom screen: the keys of the lists and the history, and touch */
static void help_row(const char* key, StrId what, float y) {
    lbl(key, 62, y, 1, COL_ACC, 2);
    lbl(T(what), 72, y, 1, COL_TEXT, 0);
}

static void render_help(void) {
    sub_header(T(S_HDR_HELP), "操作", "");
    dashed_vline(5, LIST_Y, SCR_H, 3, 3, COL_LINE);
    float y = LIST_Y + 8;
    mono_draw("//", 14, y, 1, COL_ACC, 0);
    lbl(T(S_H_SEC_LISTS), 28, y, 1, COL_DIM, 0);
    C2D_DrawRectSolid(14, y + 11, 0, 292, 1, COL_LINE);
    y += 20;
    help_row("↑ ↓", S_H_MOVE, y); y += 15;
    help_row("← →", S_H_PAGE, y); y += 15;
    help_row("A", S_H_PLAY, y); y += 15;
    help_row("Y", S_H_MARK, y); y += 15;
    help_row("B", S_H_BACK, y); y += 15;
    help_row("L  R", S_H_LR, y); y += 22;
    mono_draw("//", 14, y, 1, COL_ACC, 0);
    lbl(T(S_H_SEC_TOUCH), 28, y, 1, COL_DIM, 0);
    C2D_DrawRectSolid(14, y + 11, 0, 292, 1, COL_LINE);
    y += 20;
    help_row(">", S_H_DRAG, y); y += 15;
    help_row(">", S_H_TAP, y);
    float pulse = 0.55f + 0.45f * sinf(g_t * 3);
    ch_outline(14, 212, 292, 20, 5, CUT_TL | CUT_BR, col_a(COL_ACC, 0.4f), 1);
    lbl(T(S_H_PRESS), 160, 219, 1, col_a(COL_ACC, pulse), 1);
}

static void render_about(void) {
    sub_header(T(S_HDR_ABOUT), "情報", "V" APP_VERSION);
    /* title card */
    ch_fill(10, 34, 300, 42, 8, CUT_TL | CUT_BR, COL_PANEL);
    ch_outline(10, 34, 300, 42, 8, CUT_TL | CUT_BR, COL_LINE, 1);
    edge_trace(10, 34, 300, 42, 8, CUT_TL | CUT_BR, g_t / 6.0f, 40, COL_ACC);
    text_draw("Radio Neon ラジオネオン", 20, 36, 0.6f, COL_TEXT, 0);
    text_draw(T(S_CREATED), 20, 56, 0.5f, COL_ACC, 0);
    mono_draw("MPL-2.0 // 2026", 300, 61, 1, COL_DIM, 2);

    /* GitHub / Discord / Ko-fi */
    static const char* QL[3] = { "GITHUB", "DISCORD", "KO-FI" };
    const int PX = 2, QS = 33 * PX;
    for (int i = 0; i < 3; i++) {
        float qx = 34 + i * 93, qy = 86;   /* brackets span 27..293: centred on the screen */
        if (g_qrOk[i]) draw_qr(g_qr[i], qx, qy, PX);
        brackets(qx - 7, qy - 7, QS + 14, QS + 14, 5, col_a(COL_ACC, 0.5f), 1);
        mono_draw(QL[i], qx + QS / 2, qy + QS + 8, 1, COL_ACC, 1);
    }

    /* end credits rolling up in a small window at the bottom, fading in and out at the edges */
    static const char* L[][2] = {
        { "RADIO NEON // VENOMSHINE", "MPL-2.0" },
        { "OPENDSP (C) 2026 CELLENSERES", "BSD-2" },
        { "HELIX AAC/HE-AAC DECODER", "RPSL/RCSL" },
        { "LIBMPG123 (MP3)", "LGPL-2.1" },
        { "LIBCURL", "CURL" },
        { "MBEDTLS", "APACHE-2.0" },
        { "MOZILLA CA CERTIFICATES", "MPL-2.0" },
        { "LIBCTRU / CITRO2D / CITRO3D", "ZLIB" },
        { "LIBICONV / ZLIB", "LGPL / ZLIB" },
        { "QR CODE GENERATOR (NAYUKI)", "MIT" },
        { "SPLEEN 5X8 FONT", "BSD-2" },
        { "DEVKITPRO / DEVKITARM", "TOOLCHAIN" },
    };
    const int NL = sizeof(L) / sizeof(L[0]), LH = 12;
    const float wy = 176, wh = 60;
    C2D_DrawRectSolid(12, wy - 3, 0, 296, 1, COL_LINE);
    C2D_DrawRectSolid(12, wy - 3, 0, 30, 1, COL_ACC);
    int total = (NL + 3) * LH + (int)wh;   /* credits + gap + the Nintendo note, then it starts again */
    float off = fmodf((g_t - g_modeT) * 14, (float)total);   /* from the top every time About opens */
    scissor_begin(false, 10, wy, 300, wh);
    float y0 = wy + wh - off;
    lbl(T(S_CREDITS), 160, y0, 1, COL_ACC, 1);
    for (int i = 0; i < NL; i++) {
        float y = y0 + (i + 2) * LH;
        if (y < wy - LH || y > wy + wh) continue;
        mono_draw(L[i][0], 14, y, 1, i == 0 ? COL_TEXT : col_a(COL_TEXT, 0.8f), 0);
        mono_draw(L[i][1], 306, y, 1, i == 0 ? COL_ACC : COL_DIM, 2);
    }
    lbl(T(S_NO_NINTENDO), 160, y0 + (NL + 3) * LH, 1, col_a(COL_DIM, 0.8f), 1);
    scissor_end();
    u32 bg = COL_BG, clear = col_a(COL_BG, 0);
    C2D_DrawRectangle(10, wy, 0, 300, 14, bg, bg, clear, clear);
    C2D_DrawRectangle(10, wy + wh - 14, 0, 300, 14, clear, clear, bg, bg);
}

/* ---------------- exit confirmation dialog */
static void render_bat_dialog(void) {
    float k = ease_out_cubic((g_t - g_batDialogT) / 0.22f);
    C2D_DrawRectSolid(0, 0, 0, BOT_W, SCR_H, C2D_Color32(0, 0, 0, (u8)(190 * k)));
    float h = 120 * k, y = 120 - h / 2 + 2;
    if (h < 4) return;
    ch_fill(28, y, 264, h, 10, CUT_TL | CUT_BR, COL_BG);
    ch_outline(28, y, 264, h, 10, CUT_TL | CUT_BR, COL_RED, 1);
    brackets(22, y - 6, 276, h + 12, 8, col_a(COL_RED, 0.6f), 1);
    if (k < 0.98f) return;
    ch_fill(40, y + 10, 30, 12, 3, CUT_TL | CUT_BR, COL_RED);
    mono_draw("!!", 55, y + 12, 1, COL_TEXT, 1);
    mono_draw("SYS//POWER", 78, y + 12, 1, COL_DIM, 0);
    char pc[16]; snprintf(pc, sizeof(pc), "%d%%", g_batPct < 0 ? 0 : g_batPct);
    mono_draw(pc, 280, y + 12, 1, COL_RED, 2);
    lbl(T(S_BAT_TITLE), 160, y + 30, 2, COL_TEXT, 1);
    text_draw(T(S_BAT_MSG), 160, y + 50, text_fit(T(S_BAT_MSG), 0.5f, 250), COL_DIM, 1);
    Btn* b = &B_BOK;
    hud_button(b, CUT_TL | CUT_BR, true);
    lbl(T(S_BAT_OK), b->x + b->w / 2, b->y + 14, 1, COL_BLACK, 1);
}

/* battery lock: fixed card over the whole bottom screen, empty battery blinking */
static void render_bat_lock(void) {
    C2D_DrawRectSolid(0, 0, 0, BOT_W, SCR_H, C2D_Color32(0, 0, 0, 215));
    float y = 34, h = 172;
    ch_fill(20, y, 280, h, 10, CUT_TL | CUT_BR, COL_BG);
    ch_outline(20, y, 280, h, 10, CUT_TL | CUT_BR, COL_RED, 1);
    brackets(14, y - 6, 292, h + 12, 8, col_a(COL_RED, 0.6f), 1);
    ch_fill(32, y + 10, 30, 12, 3, CUT_TL | CUT_BR, COL_RED);
    mono_draw("!!", 47, y + 12, 1, COL_TEXT, 1);
    mono_draw("SYS//POWER", 70, y + 12, 1, COL_DIM, 0);
    char pc[16]; snprintf(pc, sizeof(pc), "%d%%", g_batPct < 0 ? 0 : g_batPct);
    mono_draw(pc, 288, y + 12, 1, COL_RED, 2);
    /* empty battery: outline, terminal and one red bar that blinks */
    float bx = 136, by = y + 32;
    C2D_DrawRectSolid(bx, by, 0, 44, 2, COL_RED); C2D_DrawRectSolid(bx, by + 20, 0, 44, 2, COL_RED);
    C2D_DrawRectSolid(bx, by, 0, 2, 22, COL_RED); C2D_DrawRectSolid(bx + 42, by, 0, 2, 22, COL_RED);
    C2D_DrawRectSolid(bx + 44, by + 6, 0, 4, 10, COL_RED);
    if (fmodf(g_t, 1.0f) < 0.5f) C2D_DrawRectSolid(bx + 5, by + 5, 0, 6, 12, COL_RED);
    lbl(T(S_BATL_TITLE), 160, y + 70, 2, COL_TEXT, 1);
    text_draw(T(S_BATL_MSG1), 160, y + 92, text_fit(T(S_BATL_MSG1), 0.46f, 262), COL_DIM, 1);
    text_draw(T(S_BATL_MSG2), 160, y + 110, text_fit(T(S_BATL_MSG2), 0.5f, 262), COL_TEXT, 1);
    lbl(T(S_BTN_EXIT), 160, y + 146, 1, COL_DIM, 1);
}

/* START being held: the EXIT button fills up (or, away from the main panel, a strip at the bottom) */
static void draw_start_hold(void) {
    float held = g_startDown >= 0 ? g_t - g_startDown : 0;
    if (held < 0.3f || g_dialog) return;
    float k = (held - 0.3f) / (START_HOLD - 0.3f);
    if (k > 1) k = 1;
    if (g_mode == M_CTRL && !g_batLock) {
        scissor_begin(false, B_EXIT.x, B_EXIT.y, B_EXIT.w * k, B_EXIT.h);
        ch_fill(B_EXIT.x, B_EXIT.y, B_EXIT.w, B_EXIT.h, 8, CUT_TL | CUT_BR, COL_ACC);
        lbl(T(S_BTN_EXIT), B_EXIT.x + B_EXIT.w / 2, B_EXIT.y + 14, 1, COL_BLACK, 1);
        scissor_end();
        return;
    }
    C2D_DrawRectSolid(0, SCR_H - 18, 0, BOT_W, 18, COL_BLACK);
    C2D_DrawRectSolid(0, SCR_H - 18, 0, BOT_W, 1, COL_LINE);
    C2D_DrawRectSolid(0, SCR_H - 18, 0, BOT_W * k, 18, col_a(COL_ACC, 0.25f));
    C2D_DrawRectSolid(0, SCR_H - 2, 0, BOT_W * k, 2, COL_ACC);
    lbl(T(S_BTN_EXIT), BOT_W / 2, SCR_H - 15, 1, COL_TEXT, 1);
}

static void render_dialog(void) {
    float k = ease_out_cubic((g_t - g_dialogT) / 0.22f);
    C2D_DrawRectSolid(0, 0, 0, BOT_W, SCR_H, C2D_Color32(0, 0, 0, (u8)(190 * k)));
    float h = 120 * k, y = 120 - h / 2 + 2;
    if (h < 4) return;
    ch_fill(28, y, 264, h, 10, CUT_TL | CUT_BR, COL_BG);
    ch_outline(28, y, 264, h, 10, CUT_TL | CUT_BR, COL_ACC, 1);
    brackets(22, y - 6, 276, h + 12, 8, col_a(COL_ACC, 0.6f), 1);
    if (k < 0.98f) return;
    ch_fill(40, y + 10, 30, 12, 3, CUT_TL | CUT_BR, COL_RED);
    mono_draw("!!", 55, y + 12, 1, COL_TEXT, 1);
    mono_draw("SYS//EXIT", 78, y + 12, 1, COL_DIM, 0);
    lbl(T(S_DLG_TITLE), 160, y + 30, 2, COL_TEXT, 1);
    text_draw(T(S_DLG_Q), 160, y + 50, 0.5f, COL_DIM, 1);
    hud_button(&B_DOK, CUT_TL | CUT_BR, true);
    lbl(T(S_DLG_OK), B_DOK.x + B_DOK.w / 2, B_DOK.y + 14, 1, COL_BLACK, 1);
    hud_button(&B_DNO, CUT_TL | CUT_BR, false);
    lbl(T(S_DLG_NO), B_DNO.x + B_DNO.w / 2, B_DNO.y + 14, 1, B_DNO.press > 0.35f ? COL_BLACK : COL_TEXT, 1);
}

/* ---------------- sleep timer */
static int g_sleepCur = 0;

static bool sleep_row_is_current(int i) {
    if (SLEEP_MIN[i] == 0) return !g_sleepEnd;
    if (!g_sleepEnd) return false;
    if (SLEEP_MIN[i] > 0) return g_sleepSet == SLEEP_MIN[i];
    for (int k = 0; k < NSLEEP; k++) if (SLEEP_MIN[k] == g_sleepSet) return false;
    return true;   /* custom */
}

static void render_sleep(void) {
    spring_to(SLEEP_Y(g_sleepCur) - LIST_Y);
    char left[16] = "OFF";
    if (g_sleepEnd) sleep_left_str(left, sizeof(left));
    sub_header(T(S_HDR_SLEEP), "スリープ", left);
    lbl(T(S_SLEEP_SUB), 160, LIST_Y + 8, 1, COL_DIM, 1);
    dashed_vline(5, LIST_Y + 18, SCR_H, 3, 3, COL_LINE);
    C2D_DrawRectSolid(4, LIST_Y + g_selY + 8, 0, 3, 12, COL_ACC);
    for (int i = 0; i < NSLEEP; i++) {
        float y = SLEEP_Y(i);
        bool sel = i == g_sleepCur, cur = sleep_row_is_current(i);
        ch_fill(12, y + 1, 296, 27, 6, CUT_TL | CUT_BR, sel ? COL_CARBON : COL_PANEL);
        ch_outline(12, y + 1, 296, 27, 6, CUT_TL | CUT_BR, sel ? COL_ACC : COL_LINE, 1);
        char t[48];
        if (SLEEP_MIN[i] == 0) snprintf(t, sizeof(t), "%s", T(S_SLEEP_OFF));
        else if (SLEEP_MIN[i] > 0) snprintf(t, sizeof(t), T(S_SLEEP_MIN), SLEEP_MIN[i]);
        else if (cur) snprintf(t, sizeof(t), "%s (%d min)", T(S_SLEEP_CUSTOM), g_sleepSet);
        else snprintf(t, sizeof(t), "%s...", T(S_SLEEP_CUSTOM));
        text_draw(t, 22, y + 6, 0.5f, sel ? COL_TEXT : col_a(COL_TEXT, 0.75f), 0);
        if (cur && g_sleepEnd) { char l[32]; snprintf(l, sizeof(l), T(S_SLEEP_LEFT), left); lbl(l, 300, y + 11, 1, COL_ACC, 2); }
        else if (cur) mono_draw("OK", 300, y + 11, 1, COL_ACC, 2);
    }
}

/* ---------------- song history */
static int   g_songCur = 0;        /* index in g_hview */
static float g_songScroll = 0;
static bool  g_histSaved = false;  /* only the songs saved with a heart */
static int   g_histFrom = M_CTRL;
static int   g_hview[HIST_MAX], g_hviewN = 0;

/* the rows on screen: every song, or only the saved ones (rebuilt every frame: songs come and go) */
static void hist_view(void) {
    g_hviewN = 0;
    for (int k = 0; k < g_songCount; k++) if (!g_histSaved || g_songs[k].liked) g_hview[g_hviewN++] = k;
    if (g_songCur >= g_hviewN) g_songCur = g_hviewN ? g_hviewN - 1 : 0;
}
static int hist_cur(void) { return g_hviewN ? g_hview[g_songCur] : -1; }
#define HIST_OPEN_DROP 0.10f   /* a line drops from the button to the bottom edge... */
#define HIST_OPEN_RISE 0.16f   /* ...and the screen opens upwards behind it */

static void render_history(void) {
    hist_view();
    float vis = SCR_H - LIST_Y, target = g_songCur * ROW_H - vis / 2 + ROW_H / 2;
    float mx = g_hviewN * ROW_H - vis;
    if (target > mx) target = mx;
    if (target < 0) target = 0;
    g_songScroll += (target - g_songScroll) * 0.3f;
    dashed_vline(5, LIST_Y, SCR_H, 3, 3, COL_LINE);
    scissor_begin(false, 0, LIST_Y, BOT_W, vis);
    for (int k = 0; k < g_hviewN; k++) {
        float y = LIST_Y + k * ROW_H - g_songScroll;
        if (y < LIST_Y - ROW_H || y > SCR_H) continue;
        const HistEntry* e = &g_songs[g_hview[k]];
        bool sel = k == g_songCur;
        if (sel) {
            ch_fill(14, y + 1, 292, ROW_H - 2, 5, CUT_TL | CUT_BR, COL_CARBON);
            ch_outline(14, y + 1, 292, ROW_H - 2, 5, CUT_TL | CUT_BR, COL_ACC, 1);
        } else C2D_DrawRectSolid(14, y + ROW_H - 1, 0, 292, 1, COL_LINE);
        struct tm* tm = localtime(&e->when);
        char hm[8]; snprintf(hm, sizeof(hm), "%02d:%02d", tm ? tm->tm_hour : 0, tm ? tm->tm_min : 0);
        mono_draw(hm, 20, y + 9, 1, sel ? COL_ACC : COL_DIM, 0);
        text_draw(e->title, 56, y + 3, text_fit(e->title, 0.5f, 220), sel ? COL_TEXT : col_a(COL_TEXT, 0.8f), 0);
        lbl(e->station, 56, y + 21, 1, col_a(COL_DIM, sel ? 1 : 0.7f), 0);
        heart(292, y + ROW_H / 2, 6, e->liked, e->liked ? COL_ACC : col_a(COL_DIM, 0.6f));
    }
    scissor_end();
    if (!g_hviewN) {
        if (g_histSaved) heart(160, 96, 9, false, col_a(COL_ACC, 0.6f));
        else icon_history(160, 96, col_a(COL_ACC, 0.6f));
        lbl(T(g_histSaved ? S_SAVED_EMPTY : S_HIST_EMPTY), 160, 120, 2, COL_TEXT, 1);
        lbl(T(g_histSaved ? S_SAVED_EMPTY1 : S_HIST_EMPTY1), 160, 146, 1, COL_DIM, 1);
        lbl(T(g_histSaved ? S_SAVED_EMPTY2 : S_HIST_EMPTY2), 160, 158, 1, COL_DIM, 1);
    }
    char cnt[24];
    if (g_histSaved) snprintf(cnt, sizeof(cnt), "%d  ", g_hviewN);
    else snprintf(cnt, sizeof(cnt), "%d // %d  ", g_songCount, liked_count());
    sub_header(T(g_histSaved ? S_HDR_SAVED : S_HDR_HIST), g_histSaved ? "保存曲" : "履歴", cnt);
    heart(303, 15, 4.5f, true, COL_ACC);
}

/* Spotify search for a song: open.spotify.com/search/<title, percent-encoded> */
static void spotify_url(const char* title, char* out, size_t n) {
    static const char HEX[] = "0123456789ABCDEF";
    size_t k = snprintf(out, n, "https://open.spotify.com/search/");
    for (const unsigned char* p = (const unsigned char*)title; *p && k + 4 < n; p++) {
        unsigned char c = *p;
        if (c == ' ' && p[1] == '-' && p[2] == ' ') { p += 2; c = ' '; }   /* "Artist - Song" -> "Artist Song" */
        if ((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9') || c == '-' || c == '_' || c == '.' || c == '~') out[k++] = c;
        else { out[k++] = '%'; out[k++] = HEX[c >> 4]; out[k++] = HEX[c & 15]; }
    }
    out[k] = 0;
}

static bool g_histSongQR = false;   /* top screen of the history: false = phone page, true = Spotify of the song */

/* QR centred in the area under the header, a bit bigger than 150 px; returns where the text starts */
static float qr_block(const uint8_t* qr) {
    int n = qrcodegen_getSize(qr), px = 156 / n;
    if (px < 1) px = 1;
    float qs = n * px, qx = 46, qy = 129 - qs / 2;
    draw_qr(qr, qx, qy, px);
    brackets(qx - 10, qy - 10, qs + 20, qs + 20, 7, col_a(COL_ACC, 0.6f), 1);
    return qx + qs + 30;
}

static void render_top_hist(void) {
    C2D_DrawRectSolid(0, 0, 0, TOP_W, SCR_H, COL_BG);
    dot_matrix(TOP_W);
    ch_outline(4, 4, 392, 232, 10, CUT_TL | CUT_BR, COL_LINE, 1);
    C2D_DrawLine(4, 14, COL_ACC, 14, 4, COL_ACC, 2, 0);
    C2D_DrawLine(386, 236, COL_ACC, 396, 226, COL_ACC, 2, 0);
    mono_draw("RADIO//NEON", 18, 10, 1, COL_ACC, 0);
    mono_draw("HISTORY // LINK", 88, 10, 1, COL_DIM, 0);
    C2D_DrawRectSolid(14, 24, 0, 372, 1, COL_LINE);
    C2D_DrawRectSolid(14, 24, 0, 30, 1, COL_ACC);
    static uint8_t qr[qrcodegen_BUFFER_LEN_MAX], tmp[qrcodegen_BUFFER_LEN_MAX];
    static char done[600] = "";   /* what the QR holds now: encoded once */
    static bool ok = false;
    float ty = 129 - 46;
    if (g_histSongQR && hist_cur() >= 0) {
        const HistEntry* e = &g_songs[hist_cur()];
        char url[600];
        spotify_url(e->title, url, sizeof(url));
        if (strcmp(done, url)) { ok = qrcodegen_encodeText(url, tmp, qr, qrcodegen_Ecc_LOW, 3, 25, qrcodegen_Mask_AUTO, true); snprintf(done, sizeof(done), "%s", url); }
        float tx = ok ? qr_block(qr) : 46, tw = 376 - tx;
        mono_draw("SPOTIFY", tx, ty, 2, COL_ACC, 0);
        char a[160], b[160];
        const char* dash = strstr(e->title, " - ");
        if (dash) { snprintf(a, sizeof(a), "%.*s", (int)(dash - e->title), e->title); snprintf(b, sizeof(b), "%s", dash + 3); }
        else { snprintf(a, sizeof(a), "%s", e->title); b[0] = 0; }
        text_draw(a, tx, ty + 24, text_fit(a, 0.6f, tw), COL_TEXT, 0);
        if (b[0]) text_draw(b, tx, ty + 44, text_fit(b, 0.6f, tw), COL_TEXT, 0);
        struct tm* tm = localtime(&e->when);
        char st[96]; snprintf(st, sizeof(st), "%s // %02d:%02d", e->station, tm ? tm->tm_hour : 0, tm ? tm->tm_min : 0);
        lbl(st, tx, ty + 70, 1, COL_DIM, 0);
        text_draw(T(S_HIST_SCAN), tx, ty + 82, 0.5f, COL_TEXT, 0);
        text_draw(T(S_HIST_KEYS), tx, ty + 102, 0.42f, COL_ACC, 0);
    } else {
        char url[96];
        if (web_url(url, sizeof(url))) {
            if (strcmp(done, url)) { ok = qrcodegen_encodeText(url, tmp, qr, qrcodegen_Ecc_LOW, 3, 25, qrcodegen_Mask_AUTO, true); snprintf(done, sizeof(done), "%s", url); }
            float tx = ok ? qr_block(qr) : 46;
            mono_draw("PHONE LINK", tx, ty, 2, COL_ACC, 0);
            text_draw(T(S_LINK_SCAN1), tx, ty + 24, 0.55f, COL_TEXT, 0);
            text_draw(T(S_LINK_SCAN2), tx, ty + 42, 0.55f, COL_TEXT, 0);
            mono_draw(url + 7, tx, ty + 68, 1, COL_DIM, 0);   /* without "http://" */
            text_draw(T(S_LINK_NET), tx, ty + 78, 0.42f, COL_DIM, 0);
            if (g_hviewN) text_draw(T(S_LINK_A), tx, ty + 98, 0.42f, COL_ACC, 0);
        } else {   /* no Wi-Fi: no address to give */
            mono_draw("PHONE LINK", 200, ty + 10, 2, COL_DIM, 1);
            lbl(T(S_LINK_NOWIFI), 200, ty + 40, 2, COL_TEXT, 1);
            text_draw(T(S_LINK_NOWIFI2), 200, ty + 62, 0.5f, COL_DIM, 1);
            if (g_hviewN) text_draw(T(S_LINK_A), 200, ty + 98, 0.42f, COL_ACC, 1);
        }
    }
    scanlines(TOP_W);
}

static void render_bottom(const PlayerInfo* pi) {
    C2D_DrawRectSolid(0, 0, 0, BOT_W, SCR_H, COL_BG);
    dot_matrix(BOT_W);
    ch_outline(2, 2, 316, 236, 8, CUT_TR | CUT_BL, col_a(COL_LINE, 0.8f), 1);
    switch (g_mode) {
        case M_CTRL: render_controls(pi); break;
        case M_LIST: render_list(pi); break;
        case M_MENU: render_menu(); break;
        case M_OPT: render_options(); break;
        case M_SLEEP: render_sleep(); break;
        case M_HIST: {
            /* opening: a line drops from the button to the bottom edge, then the screen opens upwards */
            float d = g_t - g_modeT;
            if (d < HIST_OPEN_DROP + HIST_OPEN_RISE) {
                render_controls(pi);
                float top = SCR_H - 2;
                if (d < HIST_OPEN_DROP) {
                    float y = B_HIST.y + B_HIST.h + (SCR_H - 2 - B_HIST.y - B_HIST.h) * ease_out_cubic(d / HIST_OPEN_DROP);
                    C2D_DrawRectSolid(B_HIST.x + B_HIST.w / 2 - 1, B_HIST.y + B_HIST.h, 0, 2, y - B_HIST.y - B_HIST.h, COL_ACC);
                } else {
                    top = (SCR_H - 2) * (1 - ease_out_cubic((d - HIST_OPEN_DROP) / HIST_OPEN_RISE));
                    C2D_DrawRectSolid(0, top, 0, BOT_W, SCR_H - top, COL_BG);   /* the screen rises glued to the line */
                    C2D_ViewTranslate(0, top);
                    dot_matrix(BOT_W);
                    render_history();
                    C2D_ViewReset();
                }
                C2D_DrawRectSolid(0, top, 0, BOT_W, 2, COL_ACC);
            } else render_history();
            break;
        }
        case M_HELP: render_help(); break;
        default: render_about(); break;
    }
    if (g_dialog) render_dialog();
    else if (g_batLock) render_bat_lock();
    else if (g_batDialog) render_bat_dialog();
    draw_start_hold();
    /* lock-on reticle at touch point */
    for (int i = 0; i < 6; i++) {
        Lock* l = &g_lock[i];
        if (!l->on) continue;
        float d = (g_t - l->t0) / 0.3f;
        if (d >= 1) { l->on = false; continue; }
        float r = 22 - d * 12;
        brackets(l->x - r, l->y - r, r * 2, r * 2, 5, col_a(COL_ACC, 1 - d), 1);
    }
    float mf = g_mode == M_HIST ? 0 : 1 - (g_t - g_modeT) / 0.2f;   /* the history has its own opening */
    if (mf > 0) { float y = (1 - mf) * SCR_H; C2D_DrawRectSolid(0, y, 0, BOT_W, 2, col_a(COL_ACC, mf)); }
    scanlines(BOT_W);
}

/* --------------------------------------------------------------- boot HUD (DSP firmware) */
static char g_bootLog[8][64];
static int  g_bootN = 0;
static char g_bootL1[64], g_bootL2[96];
static int  g_bootLevel = 0;

static void render_boot_frame(void) {
    float t = (osGetTime() % 100000) / 1000.0f;
    fx_text_frame();
    C3D_FrameBegin(C3D_FRAME_SYNCDRAW);
    C2D_TargetClear(g_top, COL_BG);
    C2D_SceneBegin(g_top);
    dot_matrix(TOP_W);
    ch_outline(4, 4, 392, 232, 10, CUT_TL | CUT_BR, COL_LINE, 1);
    mono_draw("RADIO//NEON", 18, 10, 1, COL_ACC, 0);
    mono_draw("BOOT SEQUENCE", 386, 10, 1, COL_DIM, 2);
    u32 lc = g_bootLevel == 3 ? COL_RED : g_bootLevel == 2 ? COL_RED : COL_ACC;
    ch_fill(40, 80, 320, 80, 10, CUT_TL | CUT_BR, COL_PANEL);
    ch_outline(40, 80, 320, 80, 10, CUT_TL | CUT_BR, lc, 1);
    brackets(34, 74, 332, 92, 8, col_a(lc, 0.6f), 1);
    mono_draw(g_bootL1, 200, 96, 2, lc, 1);
    lbl(g_bootL2, 200, 122, 1, COL_TEXT, 1);
    if (g_bootLevel == 0) {
        float p = fmodf(t * 0.8f, 1.0f);
        C2D_DrawRectSolid(60, 142, 0, 280, 4, COL_LINE);
        C2D_DrawRectSolid(60 + 280 * p * 0.7f, 142, 0, 280 * 0.3f, 4, COL_ACC);
    } else {
        mono_draw(g_bootLevel == 1 ? "[ OK ]" : "[ !! ]", 200, 140, 1, lc, 1);
    }
    text_draw("音声DSP", 392, 200, 0.9f, col_a(COL_ACC, 0.07f), 2);
    scanlines(TOP_W);
    C2D_TargetClear(g_bot, COL_BG);
    C2D_SceneBegin(g_bot);
    dot_matrix(BOT_W);
    mono_draw("SYS//LOG", 12, 8, 1, COL_ACC, 0);
    for (int i = 0; i < g_bootN; i++) mono_draw(g_bootLog[i], 12, 26 + i * 12, 1, i == g_bootN - 1 ? COL_TEXT : COL_DIM, 0);
    if (fmodf(t * 2, 1) < 0.5f) C2D_DrawRectSolid(12, 26 + g_bootN * 12, 0, 5, 8, COL_ACC);
    scanlines(BOT_W);
    C3D_FrameEnd(0);
}

static void boot_cb(int ev, int level) {
    const char* l1 = ev == FWEV_OPENDSP_OK ? "DSP.FW // OPENDSP ONLINE" : ev == FWEV_LOADING_OPENDSP ? "DSP.FW // OPENDSP"
                   : ev == FWEV_SDFW_FAILED ? "DSP.FW // DSPFIRM.CDC ERR" : "DSP.FW // NO AUDIO";
    const char* l2 = T(ev == FWEV_OPENDSP_OK ? S_FW_OK : ev == FWEV_LOADING_OPENDSP ? S_FW_LOADING
                   : ev == FWEV_SDFW_FAILED ? S_FW_TRY : S_FW_COPY);
    snprintf(g_bootL1, sizeof(g_bootL1), "%s", l1);
    snprintf(g_bootL2, sizeof(g_bootL2), "%s", l2);
    g_bootLevel = level;
    if (g_bootN == 8) { memmove(g_bootLog[0], g_bootLog[1], sizeof(g_bootLog[0]) * 7); g_bootN = 7; }
    snprintf(g_bootLog[g_bootN++], sizeof(g_bootLog[0]), "> %s", l1);
    int frames = level == 0 ? 15 : (level == 1 ? 45 : 110);
    for (int i = 0; i < frames && aptMainLoop(); i++) render_boot_frame();
}

/* --------------------------------------------------------------- input */
static int   g_touchMode = 0;
static int   g_tx0, g_ty0, g_tyLast;
static float g_tmax;

static void press(Btn* b, int x, int y) { b->press = 1; add_lock(x, y); }

static void open_dialog(void) { g_dialog = true; g_dialogT = g_t; }

static void toggle_fav(int i) {
    fav_toggle(i);
    if (g_mode == M_LIST && g_favView) build_view();   /* the cursor stays put; the current station does not change */
}

static void open_history(bool saved, int from);

static void menu_select(int i) {
    switch (i) {
        case MI_ALL:   open_list(false, M_MENU); break;
        case MI_FAV:   open_list(true, M_MENU); break;
        case MI_HIST:  open_history(false, M_MENU); break;
        case MI_SAVED: open_history(true, M_MENU); break;
        case MI_OPT:   g_optCur = 0; g_selY = OPT_Y[0]; g_selV = 0; set_mode(M_OPT); break;
        case MI_HELP:  memset(g_helpHit, 0, sizeof(g_helpHit)); set_mode(M_HELP); break;
        case MI_ABOUT: set_mode(M_ABOUT); break;
        default:       open_dialog(); break;
    }
}

static void opt_change(int i, int dir) {
    switch (i) {
        case OPT_ACCENT:  g_cfg.accent = ((g_cfg.accent + dir) % NUM_ACCENTS + NUM_ACCENTS) % NUM_ACCENTS; config_apply_accent(); break;
        case OPT_LANG:    g_cfg.lang = ((g_cfg.lang + dir) % NUM_LANGSEL + NUM_LANGSEL) % NUM_LANGSEL; i18n_resolve(g_cfg.lang); break;
        case OPT_LID:     g_cfg.lidPlay = !g_cfg.lidPlay; break;
        case OPT_SAVER:   g_cfg.saver = !g_cfg.saver; break;
        case OPT_AUTO:    g_cfg.autoplay = !g_cfg.autoplay; break;
        case OPT_SCAN:    g_cfg.scanlines = !g_cfg.scanlines; break;
        case OPT_GLITCH:  g_cfg.glitch = !g_cfg.glitch; break;
    }
    config_save();
}

static void open_sleep(void) {
    g_sleepCur = 0;
    for (int i = 0; i < NSLEEP; i++) if (sleep_row_is_current(i)) g_sleepCur = i;
    g_selY = SLEEP_Y(g_sleepCur) - LIST_Y; g_selV = 0;
    set_mode(M_SLEEP);
}

/* custom minutes on the system numpad (called from handle_input, outside the frame) */
static void sleep_custom(void) {
    SwkbdState kb;
    char buf[8] = "";
    swkbdInit(&kb, SWKBD_TYPE_NUMPAD, 2, 3);
    swkbdSetHintText(&kb, T(S_SLEEP_KB));
    swkbdSetValidation(&kb, SWKBD_NOTEMPTY_NOTBLANK, 0, 0);
    if (swkbdInputText(&kb, buf, sizeof(buf)) != SWKBD_BUTTON_CONFIRM) return;
    int m = atoi(buf);
    if (m < 1) m = 1;
    if (m > 480) m = 480;
    sleep_set(m);
    set_mode(M_CTRL);
}

static void sleep_select(int i) {
    if (SLEEP_MIN[i] < 0) { sleep_custom(); return; }
    sleep_set(SLEEP_MIN[i]);
    set_mode(M_CTRL);
}

static void toggle_shuffle(void) { g_cfg.shuffle = !g_cfg.shuffle; g_histN = 0; config_save(); }

/* the history entry of the song that is playing (-1: not recorded yet) */
static int now_song(const PlayerInfo* pi) {
    if (!pi->title[0] || !g_songCount) return -1;
    return strcmp(g_songs[0].title, pi->title) ? hist_find(pi->title) : 0;
}

/* B on the main screen: save / unsave the song that is playing */
static void save_now_song(const PlayerInfo* pi) {
    if (!pi->title[0] || pi->state != PS_PLAYING) return;
    int k = now_song(pi);
    bool like = k < 0 || !g_songs[k].liked;
    if (k < 0) k = hist_add(pi->title, g_playing >= 0 ? g_stations[g_playing].name : "", true);
    else hist_set_liked(k, like);
    player_beep(like ? 0 : 1);
    g_savedT = g_t; g_savedLike = like;
    netlog("history: %s \"%s\"", like ? "saved" : "unsaved", pi->title);
}

static void open_history(bool saved, int from) {
    g_songCur = 0; g_songScroll = 0; g_histSongQR = false;
    g_histSaved = saved; g_histFrom = from;
    bool anim = g_mode == M_CTRL;   /* the line that drops from the button: only from the panel */
    hist_view();
    set_mode(M_HIST);
    if (!anim) g_modeT -= HIST_OPEN_DROP + HIST_OPEN_RISE;
}

static void toggle_song_like(int k) {
    if (k < 0 || k >= g_songCount) return;
    bool like = !g_songs[k].liked;
    hist_set_liked(k, like);
    if (!g_batLock) player_beep(like ? 0 : 1);
}

static void open_menu(void) { g_menuCur = 0; g_selY = 0; g_selV = 0; g_menuScroll = 0; g_menuFree = false; set_mode(M_MENU); }
static void to_menu(void) { g_selY = g_menuCur * MENU_H; g_selV = 0; set_mode(M_MENU); }

static void go_back(void) {
    if (g_mode == M_LIST) { if (g_listFrom == M_MENU) to_menu(); else set_mode(M_CTRL); }
    else if (g_mode == M_OPT || g_mode == M_ABOUT || g_mode == M_HELP) to_menu();
    else if (g_mode == M_HIST && g_histFrom == M_MENU) to_menu();
    else if (g_mode == M_MENU || g_mode == M_SLEEP || g_mode == M_HIST) set_mode(M_CTRL);
}

/* keys while the controls are shown: they only light up (B or BACK leave, START held still exits) */
static void help_input(u32 kd, bool startTap, int x, int y) {
    static const u32 MAP[NHK] = { KEY_L, KEY_R, KEY_CPAD_UP | KEY_CPAD_DOWN | KEY_CPAD_LEFT | KEY_CPAD_RIGHT,
                                  KEY_DUP | KEY_DDOWN | KEY_DLEFT | KEY_DRIGHT, KEY_X, KEY_Y, KEY_A, 0, KEY_START, KEY_SELECT, 0 };
    for (int k = 0; k < NHK; k++) if (kd & MAP[k]) g_helpHit[k] = g_t;
    if (startTap) g_helpHit[HK_START] = g_t;
    if (kd & KEY_B) { g_helpHit[HK_B] = g_t; go_back(); return; }
    if (kd & KEY_TOUCH) {
        if (hit(&B_BACK, x, y)) { press(&B_BACK, x, y); go_back(); }
        else { g_helpHit[HK_TOUCH] = g_t; add_lock(x, y); }
    }
}

static void handle_input(const PlayerInfo* pi) {
    u32 kd = hidKeysDown(), kh = hidKeysHeld(), ku = hidKeysUp();
    static int repeat = 0;
    bool idle = pi->state == PS_STOPPED || pi->state == PS_ERROR;
    touchPosition tp;
    hidTouchRead(&tp);
    int x = tp.px, y = tp.py;

    /* START: a short press acts on release (menu), held START_HOLD seconds it asks to exit */
    bool startTap = false;
    if (kd & KEY_START) g_startDown = g_t;
    if (g_startDown >= 0 && (kh & KEY_START) && g_t - g_startDown >= START_HOLD) {
        g_startDown = -1;
        if (!g_dialog) { g_batDialog = false; open_dialog(); }
        return;
    }
    if (ku & KEY_START) { startTap = g_startDown >= 0 && g_t - g_startDown < 0.5f; g_startDown = -1; }   /* let go halfway: nothing */

    /* modal exit dialog swallows everything */
    if (g_dialog) {
        if (kd & KEY_A) { B_DOK.press = 1; g_quit = true; }
        if ((kd & KEY_B) || startTap) { g_dialog = false; }
        if (kd & KEY_TOUCH) {
            if (hit(&B_DOK, x, y)) { press(&B_DOK, x, y); g_quit = true; }
            else if (hit(&B_DNO, x, y)) { press(&B_DNO, x, y); g_dialog = false; }
        }
        return;
    }
    /* battery lock (3 %): fixed notice, no button; only START (exit) still works */
    if (g_batLock) {
        if (startTap) open_dialog();
        return;
    }
    /* low battery notice: one button, any of A / B / START / touch closes it */
    if (g_batDialog) {
        if ((kd & (KEY_A | KEY_B)) || startTap || ((kd & KEY_TOUCH) && hit(&B_BOK, x, y))) { B_BOK.press = 1; g_batDialog = false; }
        return;
    }
    /* Buttons (each one does one thing, only where it makes sense):
     *   everywhere : START menu (opens / closes) · START held exit dialog · X song history (opens / closes)
     *                SELECT station list (opens / closes)
     *                L / R previous / next station (or random with shuffle), except in CTRL
     *   CTRL       : A play/stop · Y favourite · LEFT/RIGHT previous / next
     *                L shuffle on/off · R sleep timer (the chips under the card, same sides)
     *                B save / unsave the song that is playing
     *   SLEEP      : UP/DOWN · A choose · B back
     *   HISTORY    : UP/DOWN move · LEFT/RIGHT page · Y save / unsave · B back
     *   LIST       : UP/DOWN move · LEFT/RIGHT page · A play · Y favourite · B back
     *   MENU       : UP/DOWN · A open · B close
     *   OPTIONS    : UP/DOWN · LEFT/RIGHT or A change · B back
     *   ABOUT      : B back */
    if (g_mode == M_HELP) { help_input(kd, startTap, x, y); return; }
    if (startTap) { if (g_mode == M_MENU) set_mode(M_CTRL); else { open_menu(); } return; }
    if (kd & KEY_X) {
        if (g_mode == M_HIST) set_mode(M_CTRL);
        else { if (g_mode == M_CTRL) B_HIST.press = 1; open_history(false, M_CTRL); }
        return;
    }
    if (kd & KEY_SELECT) { if (g_mode == M_LIST) go_back(); else open_list(false, M_CTRL); return; }
    if (g_mode == M_CTRL) {
        if (kd & KEY_L) { B_SHUF.press = 1; toggle_shuffle(); }
        if (kd & KEY_R) { B_SLEEP.press = 1; open_sleep(); return; }
    } else if (g_mode != M_SLEEP && g_mode != M_HIST) {
        if (kd & KEY_L) step_station(-1);
        if (kd & KEY_R) step_station(1);
        if (kd & (KEY_L | KEY_R)) list_sync_cursor();
    }

    int vert = 0;
    if (kd & (KEY_DUP | KEY_CPAD_UP)) { vert = -1; repeat = 0; }
    if (kd & (KEY_DDOWN | KEY_CPAD_DOWN)) { vert = 1; repeat = 0; }
    if (!vert && (kh & (KEY_DUP | KEY_DDOWN | KEY_CPAD_UP | KEY_CPAD_DOWN))) {
        if (++repeat > 24 && repeat % 6 == 0) vert = kh & (KEY_DUP | KEY_CPAD_UP) ? -1 : 1;
    }
    int horiz = (kd & (KEY_DLEFT | KEY_CPAD_LEFT)) ? -1 : (kd & (KEY_DRIGHT | KEY_CPAD_RIGHT)) ? 1 : 0;

    switch (g_mode) {
        case M_CTRL:
            if (kd & KEY_A) { if (idle) play_station(g_sel); else stop_station(); }
            if (kd & KEY_Y) toggle_fav(g_sel);
            if (kd & KEY_B) save_now_song(pi);
            if (horiz) step_station(horiz);
            break;
        case M_LIST:
            if ((kd & KEY_A) && g_viewN) play_station(g_view[g_cur]);
            if ((kd & KEY_Y) && g_viewN) toggle_fav(g_view[g_cur]);
            if (kd & KEY_B) go_back();
            if (vert && g_viewN) { g_cur = (g_cur + vert + g_viewN) % g_viewN; list_follow(); }
            if (horiz && g_viewN) {   /* page */
                g_cur += horiz * 6;
                if (g_cur < 0) g_cur = 0;
                if (g_cur >= g_viewN) g_cur = g_viewN - 1;
                list_follow();
            }
            break;
        case M_MENU:
            if (kd & KEY_A) menu_select(g_menuCur);
            if (kd & KEY_B) set_mode(M_CTRL);
            if (vert) { g_menuCur = (g_menuCur + vert + NMENU) % NMENU; g_menuFree = false; }
            break;
        case M_OPT:
            if (kd & KEY_A) opt_change(g_optCur, 1);
            if (kd & KEY_B) go_back();
            if (vert) g_optCur = (g_optCur + vert + NOPT) % NOPT;
            if (horiz) opt_change(g_optCur, horiz);
            break;
        case M_HIST:
            hist_view();
            if ((kd & KEY_A) && g_hviewN) g_histSongQR = true;   /* top: Spotify of the song */
            if (kd & KEY_B) { if (g_histSongQR) g_histSongQR = false; else go_back(); }
            if (kd & KEY_Y) toggle_song_like(hist_cur());
            if (vert && g_hviewN) g_songCur = (g_songCur + vert + g_hviewN) % g_hviewN;
            if (horiz && g_hviewN) {
                g_songCur += horiz * 6;
                if (g_songCur < 0) g_songCur = 0;
                if (g_songCur >= g_hviewN) g_songCur = g_hviewN - 1;
            }
            break;
        case M_SLEEP:
            if (kd & KEY_A) sleep_select(g_sleepCur);
            if (kd & KEY_B) go_back();
            if (vert) g_sleepCur = (g_sleepCur + vert + NSLEEP) % NSLEEP;
            break;
        default:
            if (kd & KEY_B) go_back();
            break;
    }

    if (kd & KEY_TOUCH) {
        g_tx0 = x; g_ty0 = y; g_tyLast = y; g_tmax = 0; g_touchMode = 0;
        if (g_mode != M_CTRL && hit(&B_BACK, x, y)) { press(&B_BACK, x, y); go_back(); }
        else if (g_mode == M_CTRL) {
            if (hit(&B_PLAY, x, y)) { press(&B_PLAY, x, y); if (idle || g_sel != g_playing) play_station(g_sel); else stop_station(); }
            else if (hit(&B_PREV, x, y)) { press(&B_PREV, x, y); step_station(-1); }
            else if (hit(&B_NEXT, x, y)) { press(&B_NEXT, x, y); step_station(1); }
            else if (hit(&B_CSTAR, x, y)) { press(&B_CSTAR, x, y); toggle_fav(g_sel); }
            else if (hit(&B_SHUF, x, y)) { press(&B_SHUF, x, y); toggle_shuffle(); }
            else if (hit(&B_SLEEP, x, y)) { press(&B_SLEEP, x, y); open_sleep(); }
            else if (hit(&B_HIST, x, y)) { press(&B_HIST, x, y); open_history(false, M_CTRL); }
            else if (hit(&B_CARD, x, y)) { press(&B_CARD, x, y); open_list(false, M_CTRL); }
            else if (hit(&B_MENU, x, y)) { press(&B_MENU, x, y); open_menu(); }
            else if (hit(&B_EXIT, x, y)) { press(&B_EXIT, x, y); open_dialog(); }
        } else if (g_mode == M_LIST) {
            if (y >= LIST_Y) { g_touchMode = 2; g_listVel = 0; }
        } else if (g_mode == M_MENU) {
            if (y >= MENU_Y) { g_touchMode = 3; g_menuFree = true; }
        } else if (g_mode == M_OPT) {
            for (int i = 0; i < NOPT; i++) {
                if (y < OPT_Y[i] || y >= OPT_Y[i] + OPT_H[i]) continue;
                add_lock(x, y);
                g_optCur = i;
                if (i == OPT_ACCENT && y >= OPT_Y[0] + 18 && x >= 20 && x < 20 + NUM_ACCENTS * 24) {
                    g_cfg.accent = (x - 20) / 24; config_apply_accent(); config_save();
                } else opt_change(i, 1);
                break;
            }
        } else if (g_mode == M_HIST) {
            int k = (int)((y - LIST_Y + g_songScroll) / ROW_H);
            if (y >= LIST_Y && k >= 0 && k < g_hviewN) {
                add_lock(x, y);
                if (x >= 278) toggle_song_like(g_hview[k]); else g_songCur = k;
            }
        } else if (g_mode == M_SLEEP) {
            for (int i = 0; i < NSLEEP; i++) {
                if (y < SLEEP_Y(i) || y >= SLEEP_Y(i) + 29) continue;
                add_lock(x, y); g_sleepCur = i; sleep_select(i);
                break;
            }
        } else if (g_mode == M_ABOUT) { add_lock(x, y); }
    }
    if ((kh & KEY_TOUCH) && !(kd & KEY_TOUCH)) {
        float m = fabsf((float)(y - g_ty0)) + fabsf((float)(x - g_tx0));
        if (m > g_tmax) g_tmax = m;
        if (g_touchMode == 2) { float d = (float)(g_tyLast - y); g_listScroll += d; g_listVel = d; g_tyLast = y; }
        if (g_touchMode == 3) {
            g_menuScroll += (float)(g_tyLast - y); g_tyLast = y;
            if (g_menuScroll < 0) g_menuScroll = 0;
            if (g_menuScroll > menu_max_scroll()) g_menuScroll = menu_max_scroll();
        }
    }
    if (ku & KEY_TOUCH) {
        if (g_touchMode == 2 && g_tmax < 8) {
            int k = (int)((g_ty0 - LIST_Y + g_listScroll) / ROW_H);
            if (k >= 0 && k < g_viewN) {
                add_lock(g_tx0, g_ty0);
                g_cur = k;
                if (g_tx0 >= 278) toggle_fav(g_view[k]);   /* star column */
                else play_station(g_view[k]);
            }
            g_listVel = 0;
        }
        if (g_touchMode == 3 && g_tmax < 8) {
            int i = (int)((g_ty0 - MENU_Y + g_menuScroll) / MENU_H);
            if (i >= 0 && i < NMENU) { add_lock(g_tx0, g_ty0); g_menuCur = i; menu_select(i); }
        }
        g_touchMode = 0;
    }
}

/* --------------------------------------------------------------- power / lid
 * Same idea as ctrmus (aptSetSleepAllowed(false) so closing the lid does not suspend the app),
 * but conditional: sleep is only blocked while the option is on, headphones are plugged in
 * (osIsHeadsetConnected, shared-config flag) and a station is playing. With the lid closed
 * we switch both backlights off (gsp::Lcd) and stop rendering. Otherwise the console sleeps
 * normally and on wake-up the audio is restarted and the stream reconnected (see below). */

/* HOME / sleep and the DSP.
 * libctru normally keeps the DSP firmware "suspended" while we are in HOME (or asleep) and then asks it to
 * resume; if that fails it calls svcBreak(USERBREAK_PANIC) = crash. The free OpenDSP firmware does not
 * survive that resume reliably (crash on HOME, silent audio after sleep). So:
 *  - HOME: before leaving (ONSUSPEND) we stop the stream and shut ndsp down completely, so there is
 *    nothing to resume; on return (ONRESTORE) we start ndsp from scratch, exactly like at boot.
 *  - sleep: the DSP is suspended by libctru before our hook runs, so on wake-up we restart ndsp from
 *    scratch anyway and reconnect the station.
 * All hooks run on the main thread, inside aptMainLoop(), never in the middle of a frame. */
static void audio_down(const char* why) {
    if (!g_audioReady || g_audioDown) return;
    g_audioDown = true;
    g_resumeStation = g_playing;
    player_stop();                      /* g_playing is kept: we come back to the same station */
    player_set_ndsp(false);
    if (g_fw != FW_NONE) ndspExit();
    netlog("%s: audio off (station %d)", why, g_resumeStation);
}

static void audio_up(const char* why) {
    if (!g_audioDown) return;
    g_audioDown = false;
    if (g_fw != FW_NONE) {
        Result r = ndspInit();
        if (R_FAILED(r)) {
            netlog("%s: ndspInit failed %08lX", why, (unsigned long)r);
            snprintf(g_fwDetail, sizeof(g_fwDetail), "ndspInit %08lX", (unsigned long)r);
            g_fw = FW_NONE;
        }
    }
    player_set_ndsp(g_fw != FW_NONE);
    netlog("%s: audio on (%s)%s", why, dspfw_source_name(g_fw), g_resumeStation >= 0 ? ", reconnecting" : "");
    if (g_resumeStation >= 0 && g_fw != FW_NONE) play_station(g_resumeStation);
    else if (g_resumeStation >= 0) g_playing = -1;
    g_resumeStation = -1;
}

static void saver_wake(void);
static float g_lastInput;

static void apt_cb(APT_HookType hook, void* param) {
    (void)param;
    switch (hook) {
        case APTHOOK_ONSUSPEND: saver_wake(); audio_down("home"); break;   /* HOME with the screens off: light them first */
        case APTHOOK_ONRESTORE: g_lastInput = g_t; audio_up("home"); break;
        case APTHOOK_ONWAKEUP:  saver_wake(); audio_down("wake"); audio_up("wake"); break;
        default: break;
    }
}

/* gsp::Lcd only for a moment: the HOME Menu needs that service when it takes over, and if we keep a
 * session open it waits for it forever (frozen screen, only a forced power-off gets out). Same as Cloud3DS. */
static void backlights(bool on) {
    if (R_FAILED(gspLcdInit())) return;
    if (on) GSPLCD_PowerOnAllBacklights();
    else    GSPLCD_PowerOffAllBacklights();
    gspLcdExit();
}

/* Battery saver (option): after SAVER_IDLE seconds without touching anything while a station plays, both
 * screens go dark and nothing is drawn; the radio keeps going. Any button or a touch lights them again, and
 * that press does nothing else. Never with the lid closed (that is power_update's job) or with a notice up. */
#define SAVER_IDLE 20.0f
static bool  g_saverOff = false;
static float g_lastInput = 0;

static void saver_wake(void) {
    g_lastInput = g_t;
    if (!g_saverOff) return;
    backlights(true);
    g_saverOff = false;
    netlog("saver: screens on");
}

/* true while the screens are off: skip input and drawing this frame */
static bool saver_update(const PlayerInfo* pi) {
    bool touched = (hidKeysDown() | hidKeysHeld()) != 0;
    bool active = g_playing >= 0 && pi->state != PS_STOPPED && pi->state != PS_ERROR;
    bool notice = g_dialog || g_batDialog || g_batLock;
    if (g_saverOff) {
        if (touched || !g_cfg.saver || !active || notice) { saver_wake(); return true; }   /* the waking press is swallowed */
        return true;
    }
    if (touched) g_lastInput = g_t;
    else if (g_cfg.saver && active && !notice && g_t - g_lastInput > SAVER_IDLE) {
        backlights(false);
        g_saverOff = true;
        netlog("saver: screens off, still playing");
        return true;
    }
    return false;
}

static bool power_update(const PlayerInfo* pi) {
    u8 shell = 1;
    PTMU_GetShellState(&shell);
    bool closed = shell == 0;
    bool hs = osIsHeadsetConnected();
    if (closed && g_lcdOff && !hs && g_playing >= 0) {
        /* headphones pulled out with the lid closed: never blast the speakers. Stop first, then let it sleep. */
        netlog("lid: headphones unplugged with the lid closed -> stop and sleep");
        stop_station();
    }
    /* RELINK / BUFFER count as playing: the stream comes back by itself. A final error or STOP does not,
       so a dead stream never keeps the console awake in the dark. */
    bool active = g_playing >= 0 && pi->state != PS_STOPPED && pi->state != PS_ERROR;
    bool block = g_cfg.lidPlay && hs && active;
    if (g_lcdOff && (!closed || !block)) {
        backlights(true);
        g_lcdOff = false;
        if (closed) netlog("lid: closed but nothing to play any more (state %d) -> sleep", pi->state);
    }
    if (block != g_sleepBlocked) {
        aptSetSleepAllowed(!block);   /* allowing it again with the lid closed sleeps right away (APT_SleepIfShellClosed) */
        g_sleepBlocked = block;
    }
    if (closed && block && !g_lcdOff) {
        backlights(false);
        g_lcdOff = true;
        netlog("lid: closed with headphones -> screens off, still playing");
    }
    return g_lcdOff;
}

/* --------------------------------------------------------------- scope (smoothed + beat) */
static void update_scope(void) {
    float raw[NWAVE * 2], bass = 0;
    bool got = player_get_wave(raw, NWAVE * 2, &bass);
    if (got) {
        float tmp[NWAVE * 2];
        static const float K[5] = { 1, 4, 6, 4, 1 };
        for (int pass = 0; pass < 2; pass++) {          /* extra 5-tap binomial low-pass */
            for (int i = 0; i < NWAVE * 2; i++) {
                float acc = 0, wt = 0;
                for (int j = -2; j <= 2; j++) {
                    int k = i + j; if (k < 0) k = 0; if (k >= NWAVE * 2) k = NWAVE * 2 - 1;
                    acc += raw[k] * K[j + 2]; wt += K[j + 2];
                }
                tmp[i] = acc / wt;
            }
            memcpy(raw, tmp, sizeof(raw));
        }
        float mean = 0;
        for (int i = 0; i < NWAVE * 2; i++) mean += raw[i];
        mean /= NWAVE * 2;
        /* trigger: steepest rising zero crossing in the first half -> stable phase between frames */
        int trig = 0; float best = 0;
        for (int i = 1; i < NWAVE; i++) {
            float a = raw[i - 1] - mean, b = raw[i] - mean;
            if (a < 0 && b >= 0 && b - a > best) { best = b - a; trig = i; }
        }
        float w[NWAVE], mx = 0.02f;
        for (int i = 0; i < NWAVE; i++) { w[i] = raw[trig + i] - mean; if (fabsf(w[i]) > mx) mx = fabsf(w[i]); }
        /* bass energy -> envelope (fast attack, slow release) + simple beat detector */
        g_bassPeak = bass > g_bassPeak ? bass : g_bassPeak * 0.997f;
        if (g_bassPeak < 0.02f) g_bassPeak = 0.02f;
        float bn = bass / g_bassPeak;
        g_bassEnv += (bn - g_bassEnv) * (bn > g_bassEnv ? 0.45f : 0.07f);
        g_bassAvg += (bn - g_bassAvg) * 0.03f;
        if (bn > g_bassAvg * 1.25f + 0.06f && g_t - g_lastBeat > 0.28f) { g_pulse = 1; g_lastBeat = g_t; }
        float A = 0.12f + 0.55f * g_bassEnv + 0.33f * g_pulse;
        if (A > 1) A = 1;
        for (int i = 0; i < NWAVE; i++) {
            float taper = powf(sinf((float)M_PI * i / (NWAVE - 1)), 0.6f);
            float target = w[i] / mx * A * taper;
            g_wave[i] += (target - g_wave[i]) * 0.25f;     /* ease between frames */
        }
    } else {
        for (int i = 0; i < NWAVE; i++) {
            float target = 0.015f * sinf(g_t * 2 + i * 0.12f);
            g_wave[i] += (target - g_wave[i]) * 0.1f;
        }
        g_bassEnv *= 0.9f;
    }
    g_pulse *= 0.88f;
}

/* a song goes into the history once its title has been on air for 30 s (jingles and ads between songs
   are shorter), or straight away when it is saved with B */
static void hist_track(const PlayerInfo* pi) {
    static char cur[sizeof(pi->title)] = "";
    static float since = 0;
    static bool done = false;
    if (pi->state != PS_PLAYING || !pi->title[0] || g_playing < 0) { cur[0] = 0; return; }
    if (strcmp(cur, pi->title)) { snprintf(cur, sizeof(cur), "%s", pi->title); since = g_t; done = now_song(pi) == 0; return; }
    if (!done && g_t - since >= 30) {
        hist_add(pi->title, g_stations[g_playing].name, false);
        done = true;
    }
}

/* low battery, never intrusive: one soft pip every 5 % from 40 % (40, 35, 30, 25), two pips every 2 %
   from 20 % (20, 18, ... 12), and at 10 % three falling pips, the radio pauses and a notice says why.
   Each level sounds once (no repeats if the reading wobbles); nothing while charging. Playing again after
   the pause is the user's choice: it is not paused a second time. */
static void battery_warn(void) {
    static int warned = 101;   /* lowest level already warned */
    battery_read();
    if (g_batPct < 0) return;
    if (!g_batLock && !g_batChg && g_batPct <= 3) {
        g_batLock = true; g_batDialog = false;
        netlog("battery: %d %% -> locked until the charger is plugged in", g_batPct);
        if (g_playing >= 0) stop_station();
    } else if (g_batLock && (g_batChg || g_batPct >= 6)) {   /* 6 %: a reading that wobbles 3 <-> 4 does not unlock */
        g_batLock = false;
        netlog("battery: %d %%%s -> unlocked", g_batPct, g_batChg ? " charging" : "");
    }
    if (g_batLock) return;
    if (g_batChg || g_batPct > warned + 4) { warned = 101; return; }   /* charged again: start over */
    int p = g_batPct;
    if (p <= 10) {
        if (warned <= 10) return;
        warned = 10;
        player_beep(4);
        netlog("battery: %d %% -> pause%s", p, g_playing >= 0 ? " (was playing)" : "");
        if (g_playing >= 0) stop_station();
        g_batDialog = true; g_batDialogT = g_t;
    } else if (p <= 20) {
        int step = 20 - ((20 - p) / 2) * 2;
        if (step < warned) { warned = step; player_beep(3); netlog("battery: %d %%", p); }
    } else if (p <= 40) {
        int step = 40 - ((40 - p) / 5) * 5;
        if (step < warned) { warned = step; player_beep(2); netlog("battery: %d %%", p); }
    }
}

/* JSON for the phone page (/api), rebuilt only when the list or the song on air changes */
static void json_str(char* out, size_t n, size_t* k, const char* s) {
    if (*k + 2 < n) out[(*k)++] = '"';
    for (const unsigned char* p = (const unsigned char*)s; *p && *k + 8 < n; p++) {
        if (*p == '"' || *p == '\\') { out[(*k)++] = '\\'; out[(*k)++] = *p; }
        else if (*p < 0x20) *k += snprintf(out + *k, n - *k, "\\u%04x", *p);
        else out[(*k)++] = *p;
    }
    if (*k + 1 < n) out[(*k)++] = '"';
    out[*k] = 0;
}

static void web_snapshot(const PlayerInfo* pi) {
    static int ver = -1, liked = -1;
    static char title[sizeof(pi->title)] = "x";
    static char* buf = NULL;
    enum { CAP = 96 * 1024 };
    int k = now_song(pi), lk = k >= 0 && g_songs[k].liked;
    const char* t = pi->state == PS_PLAYING ? pi->title : "";
    if (ver == g_songVer && liked == lk && !strcmp(title, t)) return;
    ver = g_songVer; liked = lk; snprintf(title, sizeof(title), "%s", t);
    if (!buf && !(buf = malloc(CAP))) return;
    size_t n = 0;
    n += snprintf(buf + n, CAP - n, "{\"now\":");
    if (t[0] && g_playing >= 0) {
        n += snprintf(buf + n, CAP - n, "{\"t\":"); json_str(buf, CAP, &n, t);
        n += snprintf(buf + n, CAP - n, ",\"st\":"); json_str(buf, CAP, &n, g_stations[g_playing].name);
        n += snprintf(buf + n, CAP - n, ",\"like\":%s}", lk ? "true" : "false");
    } else n += snprintf(buf + n, CAP - n, "null");
    n += snprintf(buf + n, CAP - n, ",\"items\":[");
    for (int i = 0; i < g_songCount && n + 600 < CAP; i++) {
        struct tm* tm = localtime(&g_songs[i].when);
        n += snprintf(buf + n, CAP - n, "%s{\"t\":", i ? "," : ""); json_str(buf, CAP, &n, g_songs[i].title);
        n += snprintf(buf + n, CAP - n, ",\"st\":"); json_str(buf, CAP, &n, g_songs[i].station);
        n += snprintf(buf + n, CAP - n, ",\"hm\":\"%02d:%02d\",\"like\":%s}", tm ? tm->tm_hour : 0, tm ? tm->tm_min : 0, g_songs[i].liked ? "true" : "false");
    }
    n += snprintf(buf + n, CAP - n, "]}");
    web_publish(buf, n);
}

/* --------------------------------------------------------------- main */
int main(int argc, char** argv) {
    osSetSpeedupEnable(true);
    APT_SetAppCpuTimeLimit(30);
    romfsInit();
    ptmuInit();
    cfguInit();
    g_mcuOk = R_SUCCEEDED(mcuHwcInit());
    gfxInitDefault();
    C3D_Init(C3D_DEFAULT_CMDBUF_SIZE * 2);
    C2D_Init(C2D_DEFAULT_MAX_OBJECTS * 4);
    C2D_Prepare();
    g_top = C2D_CreateScreenTarget(GFX_TOP, GFX_LEFT);
    g_bot = C2D_CreateScreenTarget(GFX_BOTTOM, GFX_LEFT);
    fx_text_init();
    qr_init();
    srand((unsigned)osGetTime());
    aptHook(&g_aptCookie, apt_cb, NULL);

    stations_load();
    config_load();
    {   /* the old built-in station list was replaced: keep pointing at the same station */
        int st = stations_remap(g_cfg.station);
        if (st != g_cfg.station) { g_cfg.station = st; config_save(); }
    }
    i18n_resolve(g_cfg.lang);
    fav_load();
    hist_load();
    if (g_cfg.station >= 0 && g_cfg.station < g_stationCount) g_sel = g_cfg.station;
    build_view();
    player_init();
    if (strlen(g_cfg.link) != 5) {   /* phone page code: made once, then kept so a bookmark keeps working */
        static const char AB[] = "ABCDEFGHJKLMNPQRSTUVWXYZ23456789";   /* no 0/O, 1/I */
        for (int i = 0; i < 5; i++) g_cfg.link[i] = AB[rand() % (sizeof(AB) - 1)];
        g_cfg.link[5] = 0;
        config_save();
    }
    web_start(g_cfg.link);
    /* audio bring-up: sdmc:/3ds/dspfirm.cdc -> embedded OpenDSP -> warning */
    g_fw = dspfw_init_audio(dspfw_exists() ? NULL : boot_cb, g_fwDetail, sizeof(g_fwDetail));
    player_set_ndsp(g_fw != FW_NONE);
    player_set_volume(1.0f);   /* fixed: the console's hardware slider controls volume */
    g_audioReady = true;
    if (g_cfg.autoplay && g_fw != FW_NONE) play_station(g_sel);

    u64 t0 = osGetTime();
    PlayerInfo pi;
    while (aptMainLoop() && !g_quit) {
        g_t = (osGetTime() - t0) / 1000.0f;
        hidScanInput();
        sleep_update();
        vol_update();
        battery_warn();
        player_get_info(&pi);
        if (power_update(&pi)) { g_saverOff = false; g_lastInput = g_t; gspWaitForVBlank(); continue; }   /* lid closed: dark, audio only */
        if (saver_update(&pi)) { gspWaitForVBlank(); continue; }   /* battery saver: dark, audio only */
        handle_input(&pi);
        player_get_info(&pi);
        hist_track(&pi);
        web_snapshot(&pi);

        float target = (pi.state == PS_STOPPED || pi.state == PS_ERROR) ? 0 : 1;
        g_morph += (target - g_morph) * 0.2f;
        Btn* bs[] = { &B_PREV, &B_NEXT, &B_PLAY, &B_MENU, &B_EXIT, &B_CARD, &B_BACK, &B_CSTAR, &B_DOK, &B_DNO, &B_SHUF, &B_SLEEP, &B_HIST, &B_BOK };
        for (int i = 0; i < 14; i++) bs[i]->press *= 0.82f;

        float nb[NBANDS];
        player_get_spectrum(nb, NBANDS);
        for (int i = 0; i < NBANDS; i++) {
            float v = nb[i];
            g_bands[i] = v > g_bands[i] ? v : g_bands[i] * 0.85f + v * 0.15f;
            g_peaks[i] = g_bands[i] > g_peaks[i] ? g_bands[i] : g_peaks[i] - 0.01f;
            if (g_peaks[i] < 0) g_peaks[i] = 0;
        }
        update_scope();

        fx_text_frame();
        C3D_FrameBegin(C3D_FRAME_SYNCDRAW);
        C2D_TargetClear(g_top, COL_BG);
        C2D_SceneBegin(g_top);
        if (g_mode == M_HIST && g_t - g_modeT >= HIST_OPEN_DROP + HIST_OPEN_RISE) render_top_hist();
        else if (g_mode == M_HELP) render_top_help();
        else render_top(&pi);
        C2D_TargetClear(g_bot, COL_BG);
        C2D_SceneBegin(g_bot);
        render_bottom(&pi);
        C3D_FrameEnd(0);
    }

    g_cfg.station = g_playing >= 0 ? g_playing : g_sel;
    config_save();
    aptSetSleepAllowed(true);
    if (g_lcdOff || g_saverOff) backlights(true);
    aptUnhook(&g_aptCookie);
    web_stop();
    player_exit();
    C2D_Fini();
    C3D_Fini();
    gfxExit();
    if (g_mcuOk) mcuHwcExit();
    cfguExit();
    ptmuExit();
    romfsExit();
    return 0;
}
