#pragma once
#include <stddef.h>
#include <stdbool.h>

typedef enum {
    PS_STOPPED = 0,
    PS_CONNECTING,
    PS_BUFFERING,
    PS_PLAYING,
    PS_RECONNECTING,
    PS_ERROR
} PlayerState;

typedef struct {
    PlayerState state;
    char codec[16];      /* "MP3", "AAC", "AAC+" ... */
    int  bitrate;        /* kbps from icy-br, 0 if unknown */
    int  samplerate;
    int  channels;
    int  bufferPct;      /* compressed ring fill 0..100 */
    char title[256];     /* ICY StreamTitle (UTF-8) */
    char icyName[128];
    char error[96];
} PlayerInfo;

bool player_init(void);        /* network + decoders; audio is brought up by dspfw */
void player_set_ndsp(bool ok); /* tell the engine whether ndsp is running */
void player_exit(void);
void player_play(const char* url);
void player_stop(void);
void player_set_volume(float v);
void player_get_info(PlayerInfo* out);
/* Fills `n` spectrum bands (0..1) from the audio that is playing right now.
   Returns overall level 0..1. */
float player_get_spectrum(float* bands, int n);
/* n mono samples (-1..1) of what is playing now; returns false if silent */
bool player_get_wave(float* out, int n, float* bass);
