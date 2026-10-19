#pragma once
#define MAX_STATIONS 64

typedef struct {
    char name[96];
    char url[384];
} Station;

extern Station g_stations[MAX_STATIONS];
extern int g_stationCount;

/* Loads sdmc:/3ds/radio3ds/stations.txt, creating it from the built-in list if missing.
   Returns number of stations. */
int stations_load(void);
