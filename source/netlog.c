#include <3ds.h>
#include <stdio.h>
#include <stdarg.h>
#include <time.h>
#include <sys/stat.h>
#include "netlog.h"
#include "app.h"

#define LOG_PATH APP_DIR "/net.log"
#define OLD_PATH APP_DIR "/net.old.log"
#define LOG_MAX  (256 * 1024)

static LightLock g_logLock;
static bool g_logInit = false;

void netlog_init(void) {
    if (g_logInit) return;
    LightLock_Init(&g_logLock);
    g_logInit = true;
    netlog("==== " APP_NAME " " APP_VERSION " start ====");
}

void netlog(const char* fmt, ...) {
    if (!g_logInit) return;
    char msg[512];
    va_list ap;
    va_start(ap, fmt);
    vsnprintf(msg, sizeof(msg), fmt, ap);
    va_end(ap);
    time_t now = time(NULL);
    struct tm* tm = localtime(&now);
    LightLock_Lock(&g_logLock);
    struct stat st;
    if (stat(LOG_PATH, &st) == 0 && st.st_size > LOG_MAX) {
        remove(OLD_PATH);
        rename(LOG_PATH, OLD_PATH);
    }
    FILE* f = fopen(LOG_PATH, "a");
    if (f) {
        fprintf(f, "%04d-%02d-%02d %02d:%02d:%02d %s\n", tm->tm_year + 1900, tm->tm_mon + 1, tm->tm_mday,
                tm->tm_hour, tm->tm_min, tm->tm_sec, msg);
        fclose(f);
    }
    LightLock_Unlock(&g_logLock);
}
