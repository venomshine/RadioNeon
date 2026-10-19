#pragma once
#include <stdbool.h>

typedef enum {
    FW_NONE = 0,         /* no audio */
    FW_OFFICIAL_SD,      /* sdmc:/3ds/dspfirm.cdc present (dumped by the user, e.g. with DSP1) */
    FW_OPENDSP           /* open-source OpenDSP firmware embedded in romfs */
} FwSource;

/* Progress callback so the UI can draw HUD frames while we work. */
enum { FWEV_LOADING_OPENDSP, FWEV_OPENDSP_OK, FWEV_SDFW_FAILED, FWEV_NO_AUDIO };
typedef void (*FwStatusCb)(int event, int level /*0 info,1 ok,2 warn,3 err*/);

bool dspfw_exists(void);
/* Audio bring-up: sdmc:/3ds/dspfirm.cdc -> embedded OpenDSP -> none. Calls ndspInit. */
FwSource dspfw_init_audio(FwStatusCb cb, char* detail, int detailsz);
const char* dspfw_source_name(FwSource s);
