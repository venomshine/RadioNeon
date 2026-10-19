#pragma once
#include <3ds.h>
#include <stdbool.h>

#define NUM_ACCENTS 7
typedef struct { const char* name; u8 r, g, b; } AccentPreset;
extern const AccentPreset ACCENTS[NUM_ACCENTS];

typedef struct {
    int  accent;       /* index into ACCENTS */
    bool lidPlay;      /* keep playing with the lid closed if headphones are plugged in */
    bool scanlines;
    bool glitch;
    bool autoplay;     /* start playing the last station on boot */
    int  lang;         /* LANGSEL_*: 0 system, 1 es, 2 en, 3 ja */
    int  station;      /* last station */
    bool shuffle;      /* NEXT jumps to a random station */
} Config;

extern Config g_cfg;
extern u32 g_accent;   /* current accent colour (C2D) */

void config_load(void);
void config_save(void);
void config_apply_accent(void);

/* favourites are stored by URL in sdmc:/3ds/radio3ds/favorites.txt */
void fav_load(void);            /* call after stations_load() */
bool fav_is(int station);
void fav_toggle(int station);   /* saves immediately */
int  fav_count(void);
