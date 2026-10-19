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

/* why the network is not delivering (shown while RELINK); NE_NOTAUDIO is final (PS_ERROR) */
typedef enum {
    NE_NONE = 0, NE_SOC, NE_NOWIFI, NE_NOIP, NE_DNS, NE_CONNECT, NE_TIMEOUT,
    NE_TLS, NE_CERT, NE_HTTP, NE_CUT, NE_NOTAUDIO, NE_OTHER
} NetErr;

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
    int  netErr;         /* NetErr */
    int  netArg;         /* HTTP code, curl code or socInit result */
    unsigned long long retryAt;   /* osGetTime() of the next attempt, 0 = trying now */
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
