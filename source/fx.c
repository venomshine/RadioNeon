#include <3ds.h>
#include <citro2d.h>
#include <math.h>
#include <string.h>
#include <stdlib.h>
#include "fx.h"
#include "mono5x8.h"

static C2D_TextBuf g_dyn;

u32 col_a(u32 c, float a) {
    if (a < 0) a = 0;
    if (a > 1) a = 1;
    u32 base = (c >> 24) & 0xFF;
    return (c & 0x00FFFFFF) | ((u32)(base * a) << 24);
}

u32 col_mix(u32 a, u32 b, float t) {
    if (t < 0) t = 0;
    if (t > 1) t = 1;
    u32 r = 0;
    for (int i = 0; i < 32; i += 8) {
        float ca = (a >> i) & 0xFF, cb = (b >> i) & 0xFF;
        r |= ((u32)(ca + (cb - ca) * t) & 0xFF) << i;
    }
    return r;
}

float frand(void) { return (rand() & 0xFFFF) / 65535.0f; }

static int ch_poly(float x, float y, float w, float h, float c, int k, float* px, float* py) {
    int n = 0;
#define P(a, b) do { px[n] = (a); py[n] = (b); n++; } while (0)
    if (k & CUT_TL) { P(x, y + c); P(x + c, y); } else P(x, y);
    if (k & CUT_TR) { P(x + w - c, y); P(x + w, y + c); } else P(x + w, y);
    if (k & CUT_BR) { P(x + w, y + h - c); P(x + w - c, y + h); } else P(x + w, y + h);
    if (k & CUT_BL) { P(x + c, y + h); P(x, y + h - c); } else P(x, y + h);
#undef P
    return n;
}

void ch_fill(float x, float y, float w, float h, float cut, int k, u32 c) {
    float px[8], py[8];
    int n = ch_poly(x, y, w, h, cut, k, px, py);
    float cx = x + w / 2, cy = y + h / 2;
    for (int i = 0; i < n; i++) {
        int j = (i + 1) % n;
        C2D_DrawTriangle(cx, cy, c, px[i], py[i], c, px[j], py[j], c, 0);
    }
}

void ch_outline(float x, float y, float w, float h, float cut, int k, u32 c, float th) {
    float px[8], py[8];
    int n = ch_poly(x, y, w, h, cut, k, px, py);
    for (int i = 0; i < n; i++) {
        int j = (i + 1) % n;
        C2D_DrawLine(px[i], py[i], c, px[j], py[j], c, th, 0);
    }
}

void edge_trace(float x, float y, float w, float h, float cut, int k, float phase, float len, u32 c) {
    float px[9], py[9], seg[8], P = 0;
    int n = ch_poly(x, y, w, h, cut, k, px, py);
    px[n] = px[0]; py[n] = py[0];
    for (int i = 0; i < n; i++) { seg[i] = hypotf(px[i + 1] - px[i], py[i + 1] - py[i]); P += seg[i]; }
    float s0 = fmodf(phase, 1.0f) * P, s1 = s0 + len;
    for (int pass = 0; pass < 2; pass++) {   /* handle wrap-around */
        float a = s0 - pass * P, b = s1 - pass * P, acc = 0;
        for (int i = 0; i < n; i++) {
            float l0 = acc, l1 = acc + seg[i];
            float u0 = a > l0 ? a : l0, u1 = b < l1 ? b : l1;
            if (u1 > u0 && seg[i] > 0) {
                float t0 = (u0 - l0) / seg[i], t1 = (u1 - l0) / seg[i];
                C2D_DrawLine(px[i] + (px[i + 1] - px[i]) * t0, py[i] + (py[i + 1] - py[i]) * t0, c,
                             px[i] + (px[i + 1] - px[i]) * t1, py[i] + (py[i + 1] - py[i]) * t1, c, 2, 0);
            }
            acc = l1;
        }
    }
}

void brackets(float x, float y, float w, float h, float l, u32 c, float th) {
    C2D_DrawRectSolid(x, y, 0, l, th, c);           C2D_DrawRectSolid(x, y, 0, th, l, c);
    C2D_DrawRectSolid(x + w - l, y, 0, l, th, c);   C2D_DrawRectSolid(x + w - th, y, 0, th, l, c);
    C2D_DrawRectSolid(x, y + h - th, 0, l, th, c);  C2D_DrawRectSolid(x, y + h - l, 0, th, l, c);
    C2D_DrawRectSolid(x + w - l, y + h - th, 0, l, th, c); C2D_DrawRectSolid(x + w - th, y + h - l, 0, th, l, c);
}

void reticle(float cx, float cy, float r, u32 c) {
    float g = r * 0.35f;
    C2D_DrawRectSolid(cx - r, cy, 0, r - g, 1, c);
    C2D_DrawRectSolid(cx + g, cy, 0, r - g, 1, c);
    C2D_DrawRectSolid(cx, cy - r, 0, 1, r - g, c);
    C2D_DrawRectSolid(cx, cy + g, 0, 1, r - g, c);
    C2D_DrawRectSolid(cx - 1, cy - 1, 0, 2, 2, c);
}

void dashed_hline(float x0, float x1, float y, float d, float g, u32 c) {
    for (float x = x0; x < x1; x += d + g) C2D_DrawRectSolid(x, y, 0, (x + d > x1 ? x1 - x : d), 1, c);
}

void dashed_vline(float x, float y0, float y1, float d, float g, u32 c) {
    for (float y = y0; y < y1; y += d + g) C2D_DrawRectSolid(x, y, 0, 1, (y + d > y1 ? y1 - y : d), c);
}

void scissor_begin(bool top, float x, float y, float w, float h) {
    C2D_Flush();
    int sw = top ? 400 : 320;
    int l = 240 - (int)(y + h), t = sw - (int)(x + w), r = 240 - (int)y, b = sw - (int)x;
    if (l < 0) l = 0;
    if (t < 0) t = 0;
    C3D_SetScissor(GPU_SCISSOR_NORMAL, l, t, r, b);
}

void scissor_end(void) {
    C2D_Flush();
    C3D_SetScissor(GPU_SCISSOR_DISABLE, 0, 0, 0, 0);
}

void fx_text_init(void) { g_dyn = C2D_TextBufNew(8192); }
void fx_text_frame(void) { C2D_TextBufClear(g_dyn); }

float text_width(const char* s, float sc) {
    C2D_Text t;
    if (!C2D_TextParse(&t, g_dyn, s)) return 0;
    float w, h;
    C2D_TextGetDimensions(&t, sc, sc, &w, &h);
    return w;
}

float text_draw(const char* s, float x, float y, float sc, u32 col, int align) {
    C2D_Text t;
    if (!C2D_TextParse(&t, g_dyn, s)) return 0;
    C2D_TextOptimize(&t);
    float w, h;
    C2D_TextGetDimensions(&t, sc, sc, &w, &h);
    if (align == 1) x -= w / 2;
    else if (align == 2) x -= w;
    C2D_DrawText(&t, C2D_WithColor, x, y, 0, sc, sc, col);
    return w;
}

float text_fit(const char* s, float maxSc, float maxW) {
    float w = text_width(s, maxSc);
    if (w <= maxW || w <= 0) return maxSc;
    return maxSc * maxW / w;
}

/* next character: ASCII as is, Ñ -> 0x100, ñ -> 0x101, any other UTF-8 sequence -> '?' */
static unsigned mono_next(const unsigned char** pp) {
    const unsigned char* p = *pp;
    if (*p < 0x80) { *pp = p + 1; return *p; }
    if (p[0] == 0xC3 && (p[1] == 0x91 || p[1] == 0xB1)) { *pp = p + 2; return p[1] == 0x91 ? 0x100 : 0x101; }
    *pp = p + 1;
    while (**pp && (**pp & 0xC0) == 0x80) (*pp)++;
    return '?';
}

bool mono_ok(const char* s) {
    const unsigned char* p = (const unsigned char*)s;
    while (*p) {
        if (*p < 0x80) { p++; continue; }
        if (p[0] == 0xC3 && (p[1] == 0x91 || p[1] == 0xB1)) { p += 2; continue; }
        return false;
    }
    return true;
}

float mono_width(const char* s, int sc) {
    int n = 0;
    for (const unsigned char* p = (const unsigned char*)s; *p; n++) mono_next(&p);
    return (float)n * 6 * sc - sc;
}

static void mono_rows(const unsigned char* g, int r0, int nrows, float x, float y, int sc, u32 col) {
    for (int r = 0; r < nrows; r++) {
        unsigned bits = g[r];
        int b = 0;
        while (b < 5) {
            if (bits & (0x10 >> b)) {
                int s0 = b;
                while (b < 5 && (bits & (0x10 >> b))) b++;
                C2D_DrawRectSolid(x + s0 * sc, y + (r0 + r) * sc, 0, (b - s0) * sc, sc, col);
            } else b++;
        }
    }
}

float mono_draw(const char* s, float x, float y, int sc, u32 col, int align) {
    static const unsigned char TILDE[2] = { 0x0D, 0x16 };   /* pixel ~ */
    float w = mono_width(s, sc);
    if (align == 1) x -= w / 2;
    else if (align == 2) x -= w;
    x = floorf(x); y = floorf(y);
    for (const unsigned char* p = (const unsigned char*)s; *p; x += 6 * sc) {
        unsigned c = mono_next(&p);
        if (c == 0x100 || c == 0x101) {
            /* N / n with a tilde above (capitals start on row 1, small letters lower) */
            mono_rows(MONO5x8[(c == 0x100 ? 'N' : 'n') - 32], 0, 8, x, y, sc, col);
            mono_rows(TILDE, c == 0x100 ? -2 : 0, 2, x, y, sc, col);
            continue;
        }
        if (c < 32 || c > 126) c = '?';
        mono_rows(MONO5x8[c - 32], 0, 8, x, y, sc, col);
    }
    return w;
}

/* ---- Hyper Text ---- */
static const char* SCRAMBLE[] = {
    "ア","カ","サ","タ","ナ","ハ","マ","ヤ","ラ","ワ","ン","ネ","オ","ジ","テ","ム",
    "0","1","2","3","4","5","6","7","8","9","A","B","C","D","E","F"
};
#define NSCR (sizeof(SCRAMBLE) / sizeof(SCRAMBLE[0]))

/* length of the UTF-8 sequence at p, never past the terminator (stations.txt may be Latin-1) */
static int u8len(const char* p) {
    unsigned char c = (unsigned char)*p;
    int l = c < 0x80 ? 1 : (c & 0xE0) == 0xC0 ? 2 : (c & 0xF0) == 0xE0 ? 3 : (c & 0xF8) == 0xF0 ? 4 : 1;
    for (int k = 1; k < l; k++) if (!p[k]) return k;
    return l;
}

void hyper_text(const char* src, float progress, char* out, size_t outsz, unsigned seed) {
    int count = 0;
    for (const char* p = src; *p; p += u8len(p)) count++;
    int reveal = (int)(progress * count + 0.0001f);
    size_t o = 0;
    int i = 0;
    unsigned r = seed * 2654435761u;
    for (const char* p = src; *p && o + 5 < outsz; i++) {
        int l = u8len(p);
        if (i < reveal || *p == ' ') { memcpy(out + o, p, l); o += l; }
        else {
            r = r * 1103515245u + 12345u;
            const char* g = SCRAMBLE[(r >> 16) % NSCR];
            size_t gl = strlen(g);
            memcpy(out + o, g, gl); o += gl;
        }
        p += l;
    }
    out[o] = 0;
}
