/* DSP firmware bring-up for Radio Neon.
 * 1) sdmc:/3ds/dspfirm.cdc if the user already has it (dumped from their own console, e.g. DSP1)
 * 2) otherwise OpenDSP (BSD-2-Clause, (c) 2026 Cellenseres) embedded at romfs:/dsp/opendsp.cdc
 * 3) otherwise: no audio, clear warning on screen.
 * No Nintendo code is shipped with this app and nothing is extracted from the system. */
#include <3ds.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include "dspfw.h"

#define FW_PATH "sdmc:/3ds/dspfirm.cdc"

static u8* g_openDsp = NULL;   /* must stay alive: ndsp reloads it on wake-up */

bool dspfw_exists(void) {
    struct stat st;
    return stat(FW_PATH, &st) == 0 && st.st_size > 0x300;
}

static u32 le32(const u8* p) { return p[0] | (p[1] << 8) | (p[2] << 16) | ((u32)p[3] << 24); }

static bool load_opendsp(u32* size) {
    FILE* f = fopen("romfs:/dsp/opendsp.cdc", "rb");
    if (!f) return false;
    fseek(f, 0, SEEK_END);
    long sz = ftell(f);
    fseek(f, 0, SEEK_SET);
    if (sz < 0x300 || sz > 1024 * 1024) { fclose(f); return false; }
    g_openDsp = malloc(sz);
    if (!g_openDsp) { fclose(f); return false; }
    size_t rd = fread(g_openDsp, 1, sz, f);
    fclose(f);
    u32 declared = le32(g_openDsp + 0x104);
    if (rd != (size_t)sz || memcmp(g_openDsp + 0x100, "DSP1", 4) || declared > (u32)sz) {
        free(g_openDsp); g_openDsp = NULL; return false;
    }
    *size = (u32)sz;
    return true;
}

FwSource dspfw_init_audio(FwStatusCb cb, char* detail, int detailsz) {
    detail[0] = 0;
    if (dspfw_exists()) {
        if (R_SUCCEEDED(ndspInit())) return FW_OFFICIAL_SD;
        snprintf(detail, detailsz, "ndspInit failed (dspfirm.cdc)");
        if (cb) cb(FWEV_SDFW_FAILED, 2);
    } else if (cb) {
        cb(FWEV_LOADING_OPENDSP, 0);
    }
    u32 sz = 0;
    if (load_opendsp(&sz)) {
        ndspUseComponent(g_openDsp, sz, 0xFF, 0xFF);
        if (R_SUCCEEDED(ndspInit())) {
            if (cb) cb(FWEV_OPENDSP_OK, 1);
            return FW_OPENDSP;
        }
        if (!detail[0]) snprintf(detail, detailsz, "ndspInit failed (OpenDSP)");
    } else if (!detail[0]) snprintf(detail, detailsz, "romfs:/dsp/opendsp.cdc unreadable");
    if (cb) cb(FWEV_NO_AUDIO, 3);
    return FW_NONE;
}

const char* dspfw_source_name(FwSource s) {
    switch (s) {
        case FW_OFFICIAL_SD: return "DSPFIRM.CDC";
        case FW_OPENDSP: return "OPENDSP";
        default: return "NONE";
    }
}
