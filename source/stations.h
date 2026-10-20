#pragma once
#define MAX_STATIONS 64

typedef struct {
    char name[96];
    char url[384];
    char genre[24];   /* optional third field; empty = no tag */
} Station;

extern Station g_stations[MAX_STATIONS];
extern int g_stationCount;

/* Loads sdmc:/3ds/RadioNeon/stations.txt, creating it from the built-in list if missing.
   Returns number of stations. */
int stations_load(void);

/* After stations_load() replaced an untouched old built-in list: index in the old list -> index of the
   same URL in the new one (0 if it is gone). Otherwise returns the index unchanged. */
int stations_remap(int oldIndex);
