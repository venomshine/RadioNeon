/* Phone page: a tiny HTTP server on the console (own thread, soc:U sockets).
 *   GET /h/CODE       the page (romfs:/web/history.html.gz, sent gzipped)
 *   GET /h/CODE/api   the history as JSON (a snapshot published by the main thread)
 * Read only: nothing on the console can be changed from the phone. CODE is a random 5-letter code kept in
 * config.ini, so nobody else on the same Wi-Fi stumbles on the page and a bookmark keeps working.
 * Same ideas as Cloud3DS: non-blocking sockets, generous idle limits (browsers open empty pre-connections,
 * the oldest one is dropped when all slots are taken) and the listener is reopened when the IP changes.
 * On the real console two details matter that the emulator hides:
 *   - closesocket() right after send() can cut the answer (the page shows for an instant and then the
 *     browser gives an error). Like Gemi3DS: shutdown, wait for the phone to close, then close.
 *   - soc:U is one session shared with the radio stream: a poll() that waits blocks the stream's recv()
 *     meanwhile. So poll() never waits here; the thread sleeps instead. */
#include <3ds.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <fcntl.h>
#include <poll.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include "web.h"
#include "player.h"
#include "netlog.h"

#define MAX_CONN   6
#define REQ_MAX    4096
#define EMPTY_MS   5000     /* pre-connection that never sends a request */
#define HEAD_MS    15000    /* request started but not finished */
#define CLOSE_MS   2000     /* answer sent, waiting for the phone to close */
#define SEND_MS    6000     /* give up on a client that does not take the answer */
#define TICK_MS    25       /* sleep between polls (poll itself never waits, see above) */

enum { C_FREE, C_HEAD, C_CLOSING };
typedef struct { int st; int fd; int len; u64 t0; char req[REQ_MAX]; } Conn;

static Thread    g_thr;
static volatile bool g_quit = false;
static LightLock g_lock;                    /* g_json, g_ip, g_port */
static char*     g_json = NULL;             /* last snapshot from the main thread */
static size_t    g_jsonLen = 0;
static u32       g_ip = 0;
static int       g_port = 0;
static char      g_code[8] = "";
static u8*       g_page = NULL;             /* gzipped page from romfs */
static size_t    g_pageLen = 0;
static char*     g_out = NULL;              /* answer being sent (headers + body) */
static size_t    g_outCap = 0;

static u64 now_ms(void) { return osGetTime(); }

/* "a.b.c.d" from gethostid() (network order); inet_ntoa shares one buffer between threads */
static void ip_str(u32 ip, char* out, size_t n) {
    snprintf(out, n, "%lu.%lu.%lu.%lu", ip & 0xFF, (ip >> 8) & 0xFF, (ip >> 16) & 0xFF, ip >> 24);
}

static void load_page(void) {
    FILE* f = fopen("romfs:/web/history.html.gz", "rb");
    if (!f) return;
    fseek(f, 0, SEEK_END);
    long n = ftell(f);
    fseek(f, 0, SEEK_SET);
    g_page = malloc(n > 0 ? n : 1);
    if (g_page && fread(g_page, 1, n, f) == (size_t)n) g_pageLen = n;
    fclose(f);
}

static void set_blocking(int fd, bool on) {
    int fl = fcntl(fd, F_GETFL, 0);
    fcntl(fd, F_SETFL, on ? fl & ~O_NONBLOCK : fl | O_NONBLOCK);
}

/* send everything on the non-blocking socket, in 8 KB pieces like Gemi3DS, until done or SEND_MS */
static void send_all(int fd, const char* p, size_t n) {
    u64 t0 = now_ms();
    while (n && now_ms() - t0 < SEND_MS && !g_quit) {
        int r = send(fd, p, n > 8192 ? 8192 : n, 0);
        if (r > 0) { p += r; n -= r; t0 = now_ms(); continue; }
        if (r < 0 && errno != EAGAIN && errno != EWOULDBLOCK) break;
        svcSleepThread(10 * 1000000ULL);
    }
}

static void answer(int fd, const char* status, const char* type, bool gzip, const void* body, size_t len) {
    size_t need = len + 512;
    if (need > g_outCap) { free(g_out); g_out = malloc(need); g_outCap = g_out ? need : 0; }
    if (!g_out) return;
    int h = snprintf(g_out, g_outCap,
        "HTTP/1.1 %s\r\nContent-Type: %s\r\nContent-Length: %u\r\n%sCache-Control: no-store\r\n"
        "X-Content-Type-Options: nosniff\r\nConnection: close\r\n\r\n",
        status, type, (unsigned)len, gzip ? "Content-Encoding: gzip\r\n" : "");
    memcpy(g_out + h, body, len);
    send_all(fd, g_out, h + len);
}

static void handle(Conn* c) {
    char method[8] = "", path[256] = "";
    sscanf(c->req, "%7s %255s", method, path);
    char* q = strchr(path, '?');
    if (q) *q = 0;
    char base[32];
    snprintf(base, sizeof(base), "/h/%s", g_code);
    size_t bl = strlen(base);
    if (strcmp(method, "GET") || strncmp(path, base, bl)) {
        static const char NF[] = "Not found";
        answer(c->fd, "404 Not Found", "text/plain; charset=utf-8", false, NF, sizeof(NF) - 1);
        return;
    }
    const char* rest = path + bl;
    if (!*rest || !strcmp(rest, "/")) {
        if (g_pageLen) answer(c->fd, "200 OK", "text/html; charset=utf-8", true, g_page, g_pageLen);
        else answer(c->fd, "500 Internal Server Error", "text/plain", false, "no page", 7);
    } else if (!strcmp(rest, "/api")) {
        LightLock_Lock(&g_lock);
        size_t n = g_jsonLen;
        char* copy = n ? malloc(n) : NULL;
        if (copy) memcpy(copy, g_json, n);
        LightLock_Unlock(&g_lock);
        if (copy) { answer(c->fd, "200 OK", "application/json; charset=utf-8", false, copy, n); free(copy); }
        else answer(c->fd, "200 OK", "application/json", false, "{}", 2);
    } else answer(c->fd, "404 Not Found", "text/plain", false, "Not found", 9);
}

static int open_listener(int* port) {
    static const int PORTS[] = { 80, 8080 };
    for (int i = 0; i < 2; i++) {
        int fd = socket(AF_INET, SOCK_STREAM, IPPROTO_IP);
        if (fd < 0) return -1;
        int one = 1;
        setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, &one, sizeof(one));
        struct sockaddr_in a = { 0 };
        a.sin_family = AF_INET;
        a.sin_port = htons(PORTS[i]);
        a.sin_addr.s_addr = htonl(INADDR_ANY);
        if (!bind(fd, (struct sockaddr*)&a, sizeof(a)) && !listen(fd, 4)) {
            fcntl(fd, F_SETFL, fcntl(fd, F_GETFL, 0) | O_NONBLOCK);
            *port = PORTS[i];
            return fd;
        }
        closesocket(fd);
    }
    return -1;
}

static void conn_close(Conn* c) {
    if (c->fd >= 0) {
        set_blocking(c->fd, true);   /* like Gemi3DS: a blocking socket closes gracefully */
        closesocket(c->fd);
    }
    c->fd = -1;
    c->st = C_FREE;
}

static void accept_new(int lfd, Conn* conns) {
    int fd = accept(lfd, NULL, NULL);
    if (fd < 0) return;
    Conn* slot = NULL;
    for (int i = 0; i < MAX_CONN && !slot; i++) if (conns[i].st == C_FREE) slot = &conns[i];
    if (!slot) {   /* no room: drop the oldest empty pre-connection */
        for (int i = 0; i < MAX_CONN; i++)
            if (conns[i].st == C_HEAD && !conns[i].len && (!slot || conns[i].t0 < slot->t0)) slot = &conns[i];
        if (!slot) { closesocket(fd); return; }
        conn_close(slot);
    }
    set_blocking(fd, false);
    int bs = 64 * 1024;
    setsockopt(fd, SOL_SOCKET, SO_SNDBUF, &bs, sizeof(bs));
    slot->st = C_HEAD; slot->fd = fd; slot->len = 0; slot->t0 = now_ms();
}

static void on_readable(Conn* c) {
    if (c->st == C_CLOSING) {   /* the phone's close (0) or leftovers: read and forget */
        char junk[256];
        if (recv(c->fd, junk, sizeof(junk), 0) <= 0) conn_close(c);
        return;
    }
    int got = recv(c->fd, c->req + c->len, REQ_MAX - 1 - c->len, 0);
    if (got <= 0) {
        if (got < 0 && (errno == EAGAIN || errno == EWOULDBLOCK)) return;
        conn_close(c);
        return;
    }
    c->len += got; c->req[c->len] = 0; c->t0 = now_ms();
    if (!strstr(c->req, "\r\n\r\n") && c->len < REQ_MAX - 1) return;
    handle(c);
    shutdown(c->fd, SHUT_WR);   /* everything sent: let it reach the phone before closing */
    c->st = C_CLOSING; c->t0 = now_ms();
}

static void serve(int lfd, u32 ip) {
    static Conn conns[MAX_CONN];
    for (int i = 0; i < MAX_CONN; i++) { conns[i].st = C_FREE; conns[i].fd = -1; }
    u64 lastIp = now_ms();
    while (!g_quit) {
        struct pollfd pf[MAX_CONN + 1];
        int idx[MAX_CONN + 1], n = 0;
        for (int i = 0; i < MAX_CONN; i++) {
            if (conns[i].st == C_FREE) continue;
            pf[n].fd = conns[i].fd; pf[n].events = POLLIN; pf[n].revents = 0; idx[n++] = i;
        }
        pf[n].fd = lfd; pf[n].events = POLLIN; pf[n].revents = 0; idx[n++] = -1;
        int r = poll(pf, n, 0);
        for (int k = 0; r > 0 && k < n; k++) {
            if (!pf[k].revents) continue;
            if (idx[k] < 0) { accept_new(lfd, conns); continue; }
            Conn* c = &conns[idx[k]];
            if (c->st == C_FREE || c->fd != pf[k].fd) continue;
            if (pf[k].revents & (POLLERR | POLLNVAL)) conn_close(c);
            else on_readable(c);
        }
        u64 t = now_ms();
        for (int i = 0; i < MAX_CONN; i++) {
            Conn* c = &conns[i];
            if (c->st == C_FREE) continue;
            u64 lim = c->st == C_CLOSING ? CLOSE_MS : c->len ? HEAD_MS : EMPTY_MS;
            if (t - c->t0 > lim) conn_close(c);
        }
        if (t - lastIp > 1000) {   /* Wi-Fi changed (or lost): reopen on the new address */
            lastIp = t;
            if ((u32)gethostid() != ip) break;
        }
        if (r <= 0) svcSleepThread(TICK_MS * 1000000ULL);
    }
    for (int i = 0; i < MAX_CONN; i++) if (conns[i].st != C_FREE) conn_close(&conns[i]);
}

static void web_thread(void* arg) {
    (void)arg;
    while (!g_quit) {
        u32 ip = player_soc_ok() ? (u32)gethostid() : 0;
        if (!ip) { svcSleepThread(500 * 1000000ULL); continue; }
        int port = 0, lfd = open_listener(&port);
        if (lfd < 0) { netlog("web: no port (80 / 8080)"); svcSleepThread(2000 * 1000000ULL); continue; }
        LightLock_Lock(&g_lock); g_ip = ip; g_port = port; LightLock_Unlock(&g_lock);
        char a[16]; ip_str(ip, a, sizeof(a));
        netlog("web: http://%s%s/h/%s", a, port == 80 ? "" : ":8080", g_code);
        serve(lfd, ip);
        closesocket(lfd);
        LightLock_Lock(&g_lock); g_ip = 0; g_port = 0; LightLock_Unlock(&g_lock);
    }
}

void web_start(const char* code) {
    LightLock_Init(&g_lock);
    snprintf(g_code, sizeof(g_code), "%s", code);
    load_page();
    s32 prio = 0x30;
    svcGetThreadPriority(&prio, CUR_THREAD_HANDLE);
    g_quit = false;
    g_thr = threadCreate(web_thread, NULL, 32 * 1024, prio + 1, -2, false);   /* below the UI: never in its way */
}

void web_stop(void) {
    if (!g_thr) return;
    g_quit = true;
    threadJoin(g_thr, U64_MAX);
    threadFree(g_thr);
    g_thr = NULL;
}

bool web_url(char* out, size_t n) {
    LightLock_Lock(&g_lock);
    u32 ip = g_ip; int port = g_port;
    LightLock_Unlock(&g_lock);
    if (!ip) return false;
    char a[16]; ip_str(ip, a, sizeof(a));
    snprintf(out, n, "http://%s%s/h/%s", a, port == 80 ? "" : ":8080", g_code);
    return true;
}

void web_publish(const char* json, size_t len) {
    LightLock_Lock(&g_lock);
    char* p = realloc(g_json, len ? len : 1);
    if (p) { g_json = p; memcpy(g_json, json, len); g_jsonLen = len; }
    LightLock_Unlock(&g_lock);
}
