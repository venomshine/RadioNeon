#pragma once
#include <citro2d.h>
#include <stdbool.h>

/* Cyberpunk HUD palette: one acid-yellow accent, one small alert red, carbon greys */
#define COL_BG      C2D_Color32(8, 8, 10, 255)
#define COL_CARBON  C2D_Color32(20, 20, 24, 255)
#define COL_PANEL   C2D_Color32(14, 14, 17, 255)
#define COL_LINE    C2D_Color32(52, 52, 60, 255)
#define COL_DIM     C2D_Color32(110, 110, 118, 255)
#define COL_TEXT    C2D_Color32(226, 226, 220, 255)
extern u32 g_accent;                         /* user preset, default #FCEE0A */
#define COL_ACC     g_accent
#define COL_RED     C2D_Color32(255, 0, 60, 255)     /* alert only */
#define COL_BLACK   C2D_Color32(0, 0, 0, 255)

/* corner flags for chamfered shapes */
#define CUT_TL 1
#define CUT_TR 2
#define CUT_BR 4
#define CUT_BL 8

u32 col_a(u32 c, float a);
u32 col_mix(u32 a, u32 b, float t);
float frand(void);

void ch_fill(float x, float y, float w, float h, float cut, int corners, u32 c);
void ch_outline(float x, float y, float w, float h, float cut, int corners, u32 c, float th);
/* small L-shaped brackets on the four corners */
void brackets(float x, float y, float w, float h, float len, u32 c, float th);
/* thin segment travelling along a chamfered outline (monochrome) */
void edge_trace(float x, float y, float w, float h, float cut, int corners, float phase, float len, u32 c);
void reticle(float cx, float cy, float r, u32 c);
void dashed_hline(float x0, float x1, float y, float dash, float gap, u32 c);
void dashed_vline(float x, float y0, float y1, float dash, float gap, u32 c);

void scissor_begin(bool top, float x, float y, float w, float h);
void scissor_end(void);

/* system font (kana/kanji) */
void fx_text_init(void);
void fx_text_frame(void);
float text_draw(const char* s, float x, float y, float sc, u32 col, int align);
float text_width(const char* s, float sc);
float text_fit(const char* s, float maxSc, float maxW);

/* 5x8 pixel monospace (Spleen) for HUD codes; scale 1 or 2; returns width */
float mono_draw(const char* s, float x, float y, int sc, u32 col, int align);
float mono_width(const char* s, int sc);
/* true if the HUD font can draw it: ASCII plus Ñ/ñ (so "ESPAÑOL" matches "ENGLISH") */
bool mono_ok(const char* s);

/* Magic UI Hyper Text style scramble with hex/katakana glyphs */
void hyper_text(const char* src, float progress, char* out, size_t outsz, unsigned seed);
