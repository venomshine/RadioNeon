/* Radio Neon - streaming engine
 * net thread : libcurl (http/https via mbedtls) -> ICY demux -> compressed ring buffer
 * dec thread : ring -> libmpg123 (MP3) / Helix (AAC-LC, HE-AAC v1) -> ndsp wave buffers
 * Sessions are reference counted so switching station never blocks the UI thread. */
#include <3ds.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <ctype.h>
#include <malloc.h>
#include <unistd.h>
#include <curl/curl.h>
#include <mpg123.h>
#include <iconv.h>
#include "aacdec.h"
#include "player.h"
#include "netlog.h"

#define RING_SIZE     (384 * 1024)
#define PREBUF_BYTES  (40 * 1024)
#define REBUF_BYTES   (24 * 1024)
#define NUM_WB        10
#define WB_FRAMES     4096
#define CHN           0
#define CA_BUNDLE     "romfs:/certs/cacert.pem"

typedef enum { CODEC_UNKNOWN, CODEC_MP3, CODEC_AAC } Codec;

typedef struct Session {
    volatile bool stop;
    volatile int  refs;
    char url[400];

    /* ring buffer */
    u8* ring;
    size_t rhead, rtail, rfill;
    LightLock lock;
    CondVar cv;
    volatile bool netDone;

    /* http / icy */
    int  metaint;
    int  icyState;        /* 0 audio, 1 len, 2 meta */
    int  untilMeta;
    int  metaLen, metaPos;
    char meta[4096 + 1];
    char ctype[64];

    /* wave buffers */
    ndspWaveBuf wb[NUM_WB];
    s16* wbData;
    int  wbCur, wbFill;
    int  rate;
    int  queued;

    /* diagnostics */
    CURL* curl;
    bool  gotData;        /* first audio bytes of the current connection seen */
    u64   bytes;          /* audio bytes of the current connection */
} Session;

static LightLock g_lock;          /* protects g_cur, g_info, ndsp channel */
static Session*  g_cur = NULL;
static PlayerInfo g_info;
static float     g_volume = 1.0f;
static bool      g_ndspOk = false;
static u32*      g_socBuf = NULL;
static s32       g_mainPrio = 0x30;

/* ------------------------------------------------------------------ utils */
static void sess_release(Session* s) {
    bool last;
    LightLock_Lock(&g_lock);
    last = (--s->refs == 0);
    LightLock_Unlock(&g_lock);
    if (!last) return;
    svcSleepThread(60 * 1000000ULL); /* let the DSP drop references to our buffers */
    if (s->wbData) linearFree(s->wbData);
    free(s->ring);
    free(s);
}

static void set_state(Session* s, PlayerState st) {
    LightLock_Lock(&g_lock);
    if (s == g_cur) g_info.state = st;
    LightLock_Unlock(&g_lock);
}

static void set_error(Session* s, const char* msg) {
    netlog("error: %s", msg);
    LightLock_Lock(&g_lock);
    if (s == g_cur) { g_info.state = PS_ERROR; snprintf(g_info.error, sizeof(g_info.error), "%s", msg); }
    LightLock_Unlock(&g_lock);
}

static bool utf8_valid(const char* s) {
    const unsigned char* p = (const unsigned char*)s;
    while (*p) {
        int n = 0;
        if (*p < 0x80) { p++; continue; }
        else if ((*p & 0xE0) == 0xC0) n = 1;
        else if ((*p & 0xF0) == 0xE0) n = 2;
        else if ((*p & 0xF8) == 0xF0) n = 3;
        else return false;
        p++;
        while (n--) { if ((*p & 0xC0) != 0x80) return false; p++; }
    }
    return true;
}

/* convert Shift-JIS (CP932) / Latin-1 metadata to UTF-8 when needed */
static void to_utf8(const char* in, char* out, size_t outsz) {
    if (utf8_valid(in)) { snprintf(out, outsz, "%s", in); return; }
    const char* encs[2] = { "CP932", "ISO-8859-1" };
    for (int e = 0; e < 2; e++) {
        iconv_t cd = iconv_open("UTF-8", encs[e]);
        if (cd == (iconv_t)-1) continue;
        char* ip = (char*)in; size_t il = strlen(in);
        char* op = out; size_t ol = outsz - 1;
        size_t r = iconv(cd, &ip, &il, &op, &ol);
        iconv_close(cd);
        if (r != (size_t)-1) { *op = 0; return; }
    }
    /* last resort: strip high bytes */
    size_t j = 0;
    for (size_t i = 0; in[i] && j + 1 < outsz; i++) out[j++] = ((unsigned char)in[i] < 0x80) ? in[i] : '?';
    out[j] = 0;
}

static void parse_meta(Session* s) {
    s->meta[s->metaLen] = 0;
    char* t = strstr(s->meta, "StreamTitle='");
    if (!t) return;
    t += 13;
    char* e = strstr(t, "';");
    if (!e) e = strrchr(t, '\'');
    if (!e) e = t + strlen(t);
    char raw[512];
    size_t n = (size_t)(e - t);
    if (n >= sizeof(raw)) n = sizeof(raw) - 1;
    memcpy(raw, t, n); raw[n] = 0;
    char utf[256];
    to_utf8(raw, utf, sizeof(utf));
    LightLock_Lock(&g_lock);
    if (s == g_cur) snprintf(g_info.title, sizeof(g_info.title), "%s", utf);
    LightLock_Unlock(&g_lock);
}

/* ------------------------------------------------------------------ ring */
static void ring_write(Session* s, const u8* p, size_t n) {
    while (n && !s->stop) {
        LightLock_Lock(&s->lock);
        while (s->rfill == RING_SIZE && !s->stop)
            CondVar_WaitTimeout(&s->cv, &s->lock, 50 * 1000000LL);
        size_t space = RING_SIZE - s->rfill;
        size_t k = n < space ? n : space;
        size_t first = RING_SIZE - s->rhead;
        if (first > k) first = k;
        memcpy(s->ring + s->rhead, p, first);
        memcpy(s->ring, p + first, k - first);
        s->rhead = (s->rhead + k) % RING_SIZE;
        s->rfill += k;
        CondVar_Broadcast(&s->cv);
        LightLock_Unlock(&s->lock);
        p += k; n -= k;
    }
}

/* waits until at least `want` bytes (or net finished / stop), then reads up to max */
static size_t ring_read(Session* s, u8* dst, size_t max) {
    LightLock_Lock(&s->lock);
    size_t k = s->rfill < max ? s->rfill : max;
    size_t first = RING_SIZE - s->rtail;
    if (first > k) first = k;
    memcpy(dst, s->ring + s->rtail, first);
    memcpy(dst + first, s->ring, k - first);
    s->rtail = (s->rtail + k) % RING_SIZE;
    s->rfill -= k;
    CondVar_Broadcast(&s->cv);
    LightLock_Unlock(&s->lock);
    return k;
}

static size_t ring_fill(Session* s) {
    LightLock_Lock(&s->lock);
    size_t f = s->rfill;
    LightLock_Unlock(&s->lock);
    return f;
}

static bool ring_wait(Session* s, size_t want) {
    LightLock_Lock(&s->lock);
    while (s->rfill < want && !s->stop && !s->netDone)
        CondVar_WaitTimeout(&s->cv, &s->lock, 50 * 1000000LL);
    bool ok = s->rfill >= want || (s->netDone && s->rfill > 0);
    LightLock_Unlock(&s->lock);
    return ok && !s->stop;
}

/* ------------------------------------------------------------------ net */
static size_t hdr_cb(char* b, size_t sz, size_t nm, void* ud) {
    Session* s = ud;
    size_t n = sz * nm;
    char line[256];
    size_t l = n < sizeof(line) - 1 ? n : sizeof(line) - 1;
    memcpy(line, b, l); line[l] = 0;
    char* e = line + strlen(line);
    while (e > line && (e[-1] == '\r' || e[-1] == '\n')) *--e = 0;
    if (!strncmp(line, "HTTP/", 5) || !strncmp(line, "ICY ", 4)) {
        s->metaint = 0; s->ctype[0] = 0;   /* new response (redirect) */
        return n;
    }
    char* colon = strchr(line, ':');
    if (!colon) return n;
    *colon = 0;
    char* v = colon + 1;
    while (*v == ' ') v++;
    for (char* k = line; *k; k++) *k = tolower((unsigned char)*k);
    if (!strcmp(line, "icy-metaint")) s->metaint = atoi(v);
    else if (!strcmp(line, "content-type")) { snprintf(s->ctype, sizeof(s->ctype), "%s", v); for (char* k = s->ctype; *k; k++) *k = tolower((unsigned char)*k); }
    else if (!strcmp(line, "icy-br")) { LightLock_Lock(&g_lock); if (s == g_cur) g_info.bitrate = atoi(v); LightLock_Unlock(&g_lock); }
    else if (!strcmp(line, "icy-name")) {
        char utf[128]; to_utf8(v, utf, sizeof(utf));
        LightLock_Lock(&g_lock); if (s == g_cur) snprintf(g_info.icyName, sizeof(g_info.icyName), "%s", utf); LightLock_Unlock(&g_lock);
    }
    return n;
}

static void log_connected(Session* s) {
    double dns = 0, tcp = 0, tls = 0, first = 0;
    long code = 0;
    char* ip = NULL;
    curl_easy_getinfo(s->curl, CURLINFO_NAMELOOKUP_TIME, &dns);
    curl_easy_getinfo(s->curl, CURLINFO_CONNECT_TIME, &tcp);
    curl_easy_getinfo(s->curl, CURLINFO_APPCONNECT_TIME, &tls);
    curl_easy_getinfo(s->curl, CURLINFO_STARTTRANSFER_TIME, &first);
    curl_easy_getinfo(s->curl, CURLINFO_RESPONSE_CODE, &code);
    curl_easy_getinfo(s->curl, CURLINFO_PRIMARY_IP, &ip);
    netlog("connected: HTTP %ld ip=%s type=%s metaint=%d dns=%.0fms tcp=%.0fms tls=%.0fms first=%.0fms",
           code, ip ? ip : "?", s->ctype[0] ? s->ctype : "?", s->metaint, dns * 1000, tcp * 1000, tls * 1000, first * 1000);
}

static size_t write_cb(char* p, size_t sz, size_t nm, void* ud) {
    Session* s = ud;
    size_t n = sz * nm, total = n;
    if (s->stop) return 0;
    if (!s->gotData) { s->gotData = true; log_connected(s); }
    s->bytes += n;
    if (s->metaint <= 0) { ring_write(s, (u8*)p, n); return s->stop ? 0 : total; }
    while (n) {
        if (s->icyState == 0) {
            if (s->untilMeta <= 0) s->untilMeta = s->metaint;
            size_t k = n < (size_t)s->untilMeta ? n : (size_t)s->untilMeta;
            ring_write(s, (u8*)p, k);
            p += k; n -= k; s->untilMeta -= k;
            if (s->untilMeta == 0) s->icyState = 1;
        } else if (s->icyState == 1) {
            s->metaLen = (u8)*p * 16; s->metaPos = 0;
            p++; n--;
            if (s->metaLen == 0) { s->icyState = 0; s->untilMeta = s->metaint; }
            else s->icyState = 2;
        } else {
            size_t k = (size_t)(s->metaLen - s->metaPos);
            if (k > n) k = n;
            if (s->metaPos + k <= 4096) memcpy(s->meta + s->metaPos, p, k);
            s->metaPos += k; p += k; n -= k;
            if (s->metaPos >= s->metaLen) {
                if (s->metaLen > 4096) s->metaLen = 4096;
                parse_meta(s);
                s->icyState = 0; s->untilMeta = s->metaint;
            }
        }
        if (s->stop) return 0;
    }
    return total;
}

static int xfer_cb(void* ud, curl_off_t a, curl_off_t b, curl_off_t c, curl_off_t d) {
    return ((Session*)ud)->stop ? 1 : 0;
}

static void net_thread(void* arg) {
    Session* s = arg;
    int attempts = 0;
    while (!s->stop) {
        CURL* c = curl_easy_init();
        if (!c) { set_error(s, "curl init failed"); break; }
        char errbuf[CURL_ERROR_SIZE] = "";
        s->curl = c; s->gotData = false; s->bytes = 0;
        netlog("connect #%d: %s (wifi=%lu ip=%s)", attempts + 1, s->url, (unsigned long)osGetWifiStrength(), gethostid() ? "yes" : "no");
        curl_easy_setopt(c, CURLOPT_ERRORBUFFER, errbuf);
        struct curl_slist* h = curl_slist_append(NULL, "Icy-MetaData: 1");
        s->metaint = 0; s->icyState = 0; s->untilMeta = 0;
        curl_easy_setopt(c, CURLOPT_URL, s->url);
        curl_easy_setopt(c, CURLOPT_HTTPHEADER, h);
        curl_easy_setopt(c, CURLOPT_USERAGENT, "RadioNeon3DS/1.0 (Nintendo 3DS; libcurl)");
        curl_easy_setopt(c, CURLOPT_FOLLOWLOCATION, 1L);
        curl_easy_setopt(c, CURLOPT_MAXREDIRS, 8L);
        curl_easy_setopt(c, CURLOPT_CAINFO, CA_BUNDLE);   /* Mozilla roots in romfs, always verified */
        curl_easy_setopt(c, CURLOPT_SSL_VERIFYPEER, 1L);
        curl_easy_setopt(c, CURLOPT_SSL_VERIFYHOST, 2L);
        curl_easy_setopt(c, CURLOPT_CONNECTTIMEOUT, 15L);
        curl_easy_setopt(c, CURLOPT_LOW_SPEED_LIMIT, 512L);
        curl_easy_setopt(c, CURLOPT_LOW_SPEED_TIME, 15L);
        curl_easy_setopt(c, CURLOPT_NOSIGNAL, 1L);
        curl_easy_setopt(c, CURLOPT_BUFFERSIZE, 32768L);
        curl_easy_setopt(c, CURLOPT_HTTP_VERSION, CURL_HTTP_VERSION_1_1);
        curl_easy_setopt(c, CURLOPT_HEADERFUNCTION, hdr_cb);
        curl_easy_setopt(c, CURLOPT_HEADERDATA, s);
        curl_easy_setopt(c, CURLOPT_WRITEFUNCTION, write_cb);
        curl_easy_setopt(c, CURLOPT_WRITEDATA, s);
        curl_easy_setopt(c, CURLOPT_NOPROGRESS, 0L);
        curl_easy_setopt(c, CURLOPT_XFERINFOFUNCTION, xfer_cb);
        curl_easy_setopt(c, CURLOPT_XFERINFODATA, s);
        CURLcode rc = curl_easy_perform(c);
        long code = 0;
        double tcp = 0, total = 0;
        curl_easy_getinfo(c, CURLINFO_RESPONSE_CODE, &code);
        curl_easy_getinfo(c, CURLINFO_CONNECT_TIME, &tcp);
        curl_easy_getinfo(c, CURLINFO_TOTAL_TIME, &total);
        if (!s->stop)
            netlog("closed: curl=%d (%s) HTTP %ld tcp=%.0fms after %.1fs, %llu KB audio%s%s", (int)rc,
                   errbuf[0] ? errbuf : curl_easy_strerror(rc), code, tcp * 1000, total, (unsigned long long)(s->bytes / 1024),
                   s->ctype[0] ? ", type=" : "", s->ctype);
        s->curl = NULL;
        curl_slist_free_all(h);
        curl_easy_cleanup(c);
        if (s->stop) break;
        if (code >= 400) {
            char m[96]; snprintf(m, sizeof(m), "HTTP %ld", code);
            set_error(s, m); break;
        }
        if (++attempts > 6) {
            char m[96]; snprintf(m, sizeof(m), "Red: %s", curl_easy_strerror(rc));
            set_error(s, m); break;
        }
        set_state(s, PS_RECONNECTING);
        for (int i = 0; i < 20 && !s->stop; i++) svcSleepThread(100 * 1000000ULL);
    }
    LightLock_Lock(&s->lock);
    s->netDone = true;
    CondVar_Broadcast(&s->cv);
    LightLock_Unlock(&s->lock);
    sess_release(s);
}

/* ------------------------------------------------------------------ audio out */
static void out_reset(Session* s) {
    LightLock_Lock(&g_lock);
    if (s == g_cur && g_ndspOk) {
        ndspChnWaveBufClear(CHN);
        ndspChnReset(CHN);
        ndspChnSetInterp(CHN, NDSP_INTERP_POLYPHASE);
        ndspChnSetFormat(CHN, NDSP_FORMAT_STEREO_PCM16);
        ndspChnSetRate(CHN, 44100);
        float mix[12] = {0}; mix[0] = mix[1] = g_volume;
        ndspChnSetMix(CHN, mix);
    }
    LightLock_Unlock(&g_lock);
}

/* returns false when the session should end */
static bool out_push(Session* s, const s16* pcm, int frames, int rate, int ch) {
    if (rate != s->rate) {
        s->rate = rate;
        LightLock_Lock(&g_lock);
        if (s == g_cur && g_ndspOk) ndspChnSetRate(CHN, (float)rate);
        if (s == g_cur) { g_info.samplerate = rate; g_info.channels = ch; }
        LightLock_Unlock(&g_lock);
    }
    while (frames > 0) {
        ndspWaveBuf* w = &s->wb[s->wbCur];
        /* wait for a free buffer */
        while (w->status == NDSP_WBUF_QUEUED || w->status == NDSP_WBUF_PLAYING) {
            if (s->stop) return false;
            svcSleepThread(4 * 1000000ULL);
        }
        int k = WB_FRAMES - s->wbFill;
        if (k > frames) k = frames;
        memcpy(w->data_pcm16 + s->wbFill * 2, pcm, k * 4);
        s->wbFill += k; pcm += k * 2; frames -= k;
        if (s->wbFill == WB_FRAMES) {
            w->nsamples = WB_FRAMES;
            DSP_FlushDataCache(w->data_pcm16, WB_FRAMES * 4);
            LightLock_Lock(&g_lock);
            bool cur = (s == g_cur);
            if (cur && g_ndspOk) ndspChnWaveBufAdd(CHN, w);
            if (cur && ++s->queued == 3) g_info.state = PS_PLAYING;
            LightLock_Unlock(&g_lock);
            if (!cur) return false;
            s->wbCur = (s->wbCur + 1) % NUM_WB;
            s->wbFill = 0;
        }
    }
    return !s->stop;
}

/* ------------------------------------------------------------------ decoders */
static Codec sniff(Session* s, const u8* b, size_t n) {
    if (strstr(s->ctype, "aac") || strstr(s->ctype, "mp4a")) return CODEC_AAC;
    if (strstr(s->ctype, "mpeg") || strstr(s->ctype, "mp3")) return CODEC_MP3;
    if (strstr(s->ctype, "ogg") || strstr(s->ctype, "opus")) return CODEC_UNKNOWN;
    for (size_t i = 0; i + 1 < n; i++) {
        if (b[i] == 0xFF && (b[i + 1] & 0xF0) == 0xF0) {
            if ((b[i + 1] & 0x06) == 0) return CODEC_AAC;  /* ADTS: layer bits 00 */
            return CODEC_MP3;
        }
        if (b[i] == 0xFF && (b[i + 1] & 0xE0) == 0xE0) return CODEC_MP3;
    }
    return CODEC_MP3;
}

static void set_codec_name(Session* s, const char* n) {
    LightLock_Lock(&g_lock);
    if (s == g_cur) snprintf(g_info.codec, sizeof(g_info.codec), "%s", n);
    LightLock_Unlock(&g_lock);
}

static bool rebuffer_if_needed(Session* s) {
    if (ring_fill(s) > 0) return true;
    if (s->netDone) return false;
    set_state(s, PS_BUFFERING);
    if (!ring_wait(s, REBUF_BYTES)) return false;
    set_state(s, PS_PLAYING);
    return true;
}

static void decode_mp3(Session* s) {
    int err = 0;
    mpg123_handle* mh = mpg123_new(NULL, &err);
    if (!mh) { set_error(s, "mpg123 init"); return; }
    mpg123_param(mh, MPG123_FLAGS, MPG123_FORCE_STEREO | MPG123_QUIET, 0);
    mpg123_param(mh, MPG123_RESYNC_LIMIT, -1, 0);
    mpg123_format_none(mh);
    const long* rates; size_t nr;
    mpg123_rates(&rates, &nr);
    for (size_t i = 0; i < nr; i++) mpg123_format(mh, rates[i], MPG123_STEREO, MPG123_ENC_SIGNED_16);
    mpg123_open_feed(mh);
    set_codec_name(s, "MP3");
    netlog("decoder: MP3");
    u8* in = malloc(8192);
    s16* out = malloc(32768);
    long rate = 44100; int ch = 2, enc = 0;
    while (!s->stop) {
        size_t done = 0;
        int r = mpg123_read(mh, (unsigned char*)out, 32768, &done);
        if (r == MPG123_NEW_FORMAT) mpg123_getformat(mh, &rate, &ch, &enc);
        if (done > 0 && !out_push(s, out, (int)(done / 4), (int)rate, 2)) break;
        if (r == MPG123_NEED_MORE) {
            if (!rebuffer_if_needed(s)) break;
            size_t k = ring_read(s, in, 8192);
            if (k) mpg123_feed(mh, in, k);
        } else if (r == MPG123_ERR) {
            /* corrupt data: feed more, mpg123 resyncs */
            size_t k = ring_read(s, in, 4096);
            if (!k && !rebuffer_if_needed(s)) break;
            if (k) mpg123_feed(mh, in, k);
        }
    }
    free(in); free(out);
    mpg123_delete(mh);
}

static void decode_aac(Session* s) {
    HAACDecoder h = AACInitDecoder();
    if (!h) { set_error(s, "AAC init"); return; }
    const int INSZ = 16384;
    u8* in = malloc(INSZ);
    s16* pcm = malloc(AAC_MAX_NSAMPS * AAC_MAX_NCHANS * 2 * sizeof(s16) * 2);
    s16* st = malloc(AAC_MAX_NSAMPS * 2 * 2 * sizeof(s16));
    int left = 0;
    u8* ptr = in;
    bool named = false;
    while (!s->stop) {
        if (left < 4096) {
            memmove(in, ptr, left);
            ptr = in;
            if (!rebuffer_if_needed(s)) { if (left == 0) break; }
            left += ring_read(s, in + left, INSZ - left);
        }
        int off = AACFindSyncWord(ptr, left);
        if (off < 0) { ptr += left > 1 ? left - 1 : 0; left = left > 1 ? 1 : left; continue; }
        ptr += off; left -= off;
        u8* before = ptr; int beforeLeft = left;
        int err = AACDecode(h, &ptr, &left, pcm);
        if (err == ERR_AAC_INDATA_UNDERFLOW) {
            ptr = before; left = beforeLeft;
            if (s->netDone && ring_fill(s) == 0) break;
            /* force refill */
            memmove(in, ptr, left); ptr = in;
            if (!ring_wait(s, 1)) break;
            left += ring_read(s, in + left, INSZ - left);
            continue;
        }
        if (err) { ptr = before + 1; left = beforeLeft - 1; continue; }
        AACFrameInfo fi;
        AACGetLastFrameInfo(h, &fi);
        if (!named) {
            set_codec_name(s, fi.sampRateOut > fi.sampRateCore ? "AAC+" : "AAC");
            netlog("decoder: %s %d Hz %d ch", fi.sampRateOut > fi.sampRateCore ? "AAC+" : "AAC", fi.sampRateOut, fi.nChans);
            named = true;
        }
        int frames = fi.outputSamps / (fi.nChans ? fi.nChans : 1);
        const s16* src = pcm;
        if (fi.nChans == 1) {
            for (int i = 0; i < frames; i++) { st[2 * i] = st[2 * i + 1] = pcm[i]; }
            src = st;
        }
        if (frames > 0 && !out_push(s, src, frames, fi.sampRateOut, fi.nChans)) break;
    }
    free(in); free(pcm); free(st);
    AACFreeDecoder(h);
}

static void dec_thread(void* arg) {
    Session* s = arg;
    set_state(s, PS_CONNECTING);
    /* wait for the first data */
    if (ring_wait(s, 1)) {
        set_state(s, PS_BUFFERING);
        if (ring_wait(s, PREBUF_BYTES)) {
            u8 peek[64]; size_t n;
            LightLock_Lock(&s->lock);
            n = s->rfill < sizeof(peek) ? s->rfill : sizeof(peek);
            for (size_t i = 0; i < n; i++) peek[i] = s->ring[(s->rtail + i) % RING_SIZE];
            LightLock_Unlock(&s->lock);
            Codec c = sniff(s, peek, n);
            out_reset(s);
            if (c == CODEC_MP3) decode_mp3(s);
            else if (c == CODEC_AAC) decode_aac(s);
            else set_error(s, "Formato no soportado (Ogg/Opus)");
        }
    }
    /* finished: if still current and not stopped by user, flag end */
    LightLock_Lock(&g_lock);
    if (s == g_cur && !s->stop && g_info.state != PS_ERROR) {
        g_info.state = PS_ERROR;
        snprintf(g_info.error, sizeof(g_info.error), "%s", "Stream finalizado / sin datos");
    }
    LightLock_Unlock(&g_lock);
    s->stop = true;
    LightLock_Lock(&s->lock); CondVar_Broadcast(&s->cv); LightLock_Unlock(&s->lock);
    sess_release(s);
}

/* ------------------------------------------------------------------ public */
bool player_init(void) {
    LightLock_Init(&g_lock);
    memset(&g_info, 0, sizeof(g_info));
    svcGetThreadPriority(&g_mainPrio, CUR_THREAD_HANDLE);
    netlog_init();
    g_socBuf = memalign(0x1000, 0x100000);
    if (g_socBuf) { Result r = socInit(g_socBuf, 0x100000); netlog("socInit: %08lX", (unsigned long)r); }
    curl_global_init(CURL_GLOBAL_DEFAULT);
    mpg123_init();
    return true;
}

void player_stop(void) {
    LightLock_Lock(&g_lock);
    Session* s = g_cur;
    g_cur = NULL;
    if (g_ndspOk) ndspChnWaveBufClear(CHN);
    memset(&g_info, 0, sizeof(g_info));
    g_info.state = PS_STOPPED;
    LightLock_Unlock(&g_lock);
    if (s) {
        s->stop = true;
        LightLock_Lock(&s->lock); CondVar_Broadcast(&s->cv); LightLock_Unlock(&s->lock);
        sess_release(s);
    }
}

static Thread spawn(ThreadFunc f, void* arg, size_t stack, int prio, const int* cores, int ncores) {
    for (int i = 0; i < ncores; i++) {
        Thread t = threadCreate(f, arg, stack, prio, cores[i], true);
        if (t) return t;
    }
    return NULL;
}

void player_play(const char* url) {
    player_stop();
    netlog("play: %s", url);
    Session* s = calloc(1, sizeof(Session));
    if (!s) return;
    s->ring = malloc(RING_SIZE);
    s->wbData = linearAlloc(NUM_WB * WB_FRAMES * 4);
    if (!s->ring || !s->wbData) { free(s->ring); if (s->wbData) linearFree(s->wbData); free(s); return; }
    memset(s->wbData, 0, NUM_WB * WB_FRAMES * 4);
    for (int i = 0; i < NUM_WB; i++) {
        s->wb[i].data_pcm16 = s->wbData + i * WB_FRAMES * 2;
        s->wb[i].status = NDSP_WBUF_FREE;
    }
    snprintf(s->url, sizeof(s->url), "%s", url);
    LightLock_Init(&s->lock);
    CondVar_Init(&s->cv);
    s->refs = 3; /* owner (g_cur) + net + dec */
    LightLock_Lock(&g_lock);
    g_cur = s;
    memset(&g_info, 0, sizeof(g_info));
    g_info.state = PS_CONNECTING;
    LightLock_Unlock(&g_lock);

    int prio = g_mainPrio - 1;
    if (prio < 0x18) prio = 0x18;
    static const int netCores[] = { -2 };
    static const int decCores[] = { 2, 1, -2 };   /* N3DS core2, O3DS syscore, fallback */
    if (!spawn(net_thread, s, 128 * 1024, prio, netCores, 1)) { s->refs--; s->netDone = true; }
    if (!spawn(dec_thread, s, 64 * 1024, prio, decCores, 3)) { s->refs--; set_error(s, "No se pudo crear hilo"); }
}

void player_set_ndsp(bool ok) {
    g_ndspOk = ok;
    if (ok) ndspSetOutputMode(NDSP_OUTPUT_STEREO);
}

void player_set_volume(float v) {
    LightLock_Lock(&g_lock);
    g_volume = v;
    if (g_ndspOk) { float mix[12] = {0}; mix[0] = mix[1] = v; ndspChnSetMix(CHN, mix); }
    LightLock_Unlock(&g_lock);
}

void player_get_info(PlayerInfo* out) {
    LightLock_Lock(&g_lock);
    *out = g_info;
    Session* s = g_cur;
    if (s) {
        out->bufferPct = (int)((u64)s->rfill * 100 / RING_SIZE);
        if (!g_ndspOk && out->state != PS_ERROR) { out->state = PS_ERROR; snprintf(out->error, sizeof(out->error), "Sin firmware DSP: no hay audio"); }
    }
    LightLock_Unlock(&g_lock);
}

/* ---- spectrum (tiny radix-2 FFT on what the DSP is playing right now) ---- */
#define FFT_N 256
static void fft(float* re, float* im, int n) {
    for (int i = 1, j = 0; i < n; i++) {
        int bit = n >> 1;
        for (; j & bit; bit >>= 1) j ^= bit;
        j ^= bit;
        if (i < j) { float t = re[i]; re[i] = re[j]; re[j] = t; t = im[i]; im[i] = im[j]; im[j] = t; }
    }
    for (int len = 2; len <= n; len <<= 1) {
        float ang = -2.0f * (float)M_PI / len;
        float wr = cosf(ang), wi = sinf(ang);
        for (int i = 0; i < n; i += len) {
            float cr = 1, ci = 0;
            for (int k = 0; k < len / 2; k++) {
                int a = i + k, b = i + k + len / 2;
                float xr = re[b] * cr - im[b] * ci, xi = re[b] * ci + im[b] * cr;
                re[b] = re[a] - xr; im[b] = im[a] - xi;
                re[a] += xr; im[a] += xi;
                float t = cr * wr - ci * wi; ci = cr * wi + ci * wr; cr = t;
            }
        }
    }
}

float player_get_spectrum(float* bands, int n) {
    static float re[FFT_N], im[FFT_N], win[FFT_N];
    static bool winInit = false;
    if (!winInit) { for (int i = 0; i < FFT_N; i++) win[i] = 0.5f - 0.5f * cosf(2 * (float)M_PI * i / (FFT_N - 1)); winInit = true; }
    for (int i = 0; i < n; i++) bands[i] = 0;
    bool got = false;
    LightLock_Lock(&g_lock);
    Session* s = g_cur;
    if (s && g_ndspOk && g_info.state == PS_PLAYING && ndspChnIsPlaying(CHN)) {
        for (int b = 0; b < NUM_WB; b++) {
            if (s->wb[b].status == NDSP_WBUF_PLAYING) {
                u32 pos = ndspChnGetSamplePos(CHN);
                if (pos + FFT_N > WB_FRAMES) pos = WB_FRAMES - FFT_N;
                const s16* d = s->wb[b].data_pcm16 + pos * 2;
                for (int i = 0; i < FFT_N; i++) { re[i] = ((d[2 * i] + d[2 * i + 1]) * (1.0f / 65536.0f)) * win[i]; im[i] = 0; }
                got = true;
                break;
            }
        }
    }
    LightLock_Unlock(&g_lock);
    if (!got) return 0;
    fft(re, im, FFT_N);
    float level = 0;
    /* log-spaced bands over bins 1..FFT_N/2 */
    for (int b = 0; b < n; b++) {
        float lo = powf(FFT_N / 2.0f, (float)b / n), hi = powf(FFT_N / 2.0f, (float)(b + 1) / n);
        int i0 = (int)lo, i1 = (int)hi;
        if (i1 <= i0) i1 = i0 + 1;
        if (i1 > FFT_N / 2) i1 = FFT_N / 2;
        float m = 0;
        for (int i = i0; i < i1; i++) { float v = re[i] * re[i] + im[i] * im[i]; if (v > m) m = v; }
        float db = 10.0f * log10f(m + 1e-9f);           /* ~ -90 .. +20 */
        float v = (db + 4.0f + b * 0.45f) / 38.0f;   /* slight tilt so highs show up */
        if (v < 0) v = 0;
        if (v > 1) v = 1;
        bands[b] = v;
        level += v;
    }
    return level / n;
}

void player_exit(void) {
    player_stop();
    svcSleepThread(200 * 1000000ULL);
    if (g_ndspOk) ndspExit();
    curl_global_cleanup();
    socExit();
}

/* Smoothed scope data: n points, each the average of 12 frames (box low-pass ~3.7 kHz), taken from
   the buffer the DSP is playing now, plus the RMS of a ~150 Hz one-pole low-passed signal (bass). */
bool player_get_wave(float* out, int n, float* bass) {
    int WAVE_SPAN = n * 12;
    if (WAVE_SPAN > WB_FRAMES) { n = WB_FRAMES / 12; WAVE_SPAN = n * 12; }
    bool got = false;
    float lp = 0, acc = 0;
    if (bass) *bass = 0;
    LightLock_Lock(&g_lock);
    Session* s = g_cur;
    if (s && g_ndspOk && g_info.state == PS_PLAYING && ndspChnIsPlaying(CHN)) {
        for (int b = 0; b < NUM_WB; b++) {
            if (s->wb[b].status == NDSP_WBUF_PLAYING) {
                u32 pos = ndspChnGetSamplePos(CHN);
                if (pos + WAVE_SPAN > WB_FRAMES) pos = WB_FRAMES - WAVE_SPAN;
                const s16* d = s->wb[b].data_pcm16 + pos * 2;
                int blk = WAVE_SPAN / n;
                float k = 2.0f * (float)M_PI * 150.0f / (s->rate > 0 ? s->rate : 44100);
                for (int i = 0; i < n; i++) {
                    float sum = 0;
                    for (int j = 0; j < blk; j++) {
                        float v = (d[2 * (i * blk + j)] + d[2 * (i * blk + j) + 1]) * (1.0f / 65536.0f);
                        sum += v;
                        lp += (v - lp) * k;
                        acc += lp * lp;
                    }
                    out[i] = sum / blk;
                }
                if (bass) *bass = sqrtf(acc / (blk * n));
                got = true;
                break;
            }
        }
    }
    LightLock_Unlock(&g_lock);
    if (!got) for (int i = 0; i < n; i++) out[i] = 0;
    return got;
}
