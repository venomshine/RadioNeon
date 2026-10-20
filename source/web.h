#pragma once
#include <stdbool.h>
#include <stddef.h>

/* Phone page served by the console (see web.c). */
void web_start(const char* code);          /* code: the /h/CODE part of the address */
void web_stop(void);
bool web_url(char* out, size_t n);         /* "http://IP[:8080]/h/CODE", false while there is no Wi-Fi */
void web_publish(const char* json, size_t len);   /* history snapshot for /api (main thread) */
