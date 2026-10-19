#pragma once
/* UI string table: Spanish / English / Japanese. Technical HUD labels (ON AIR, LIVE, SCOPE...) stay in English. */
enum { LANG_ES = 0, LANG_EN, LANG_JA, NUM_LANGS };
enum { LANGSEL_SYSTEM = 0, LANGSEL_ES, LANGSEL_EN, LANGSEL_JA, NUM_LANGSEL };

typedef enum {
    S_MENU_ALL, S_MENU_FAV, S_MENU_OPT, S_MENU_ABOUT,
    S_SUB_ALL, S_SUB_FAV, S_SUB_OPT, S_SUB_ABOUT,
    S_HDR_STATIONS, S_HDR_FAVS, S_HDR_OPT, S_HDR_ABOUT, S_BACK,
    S_OPT_ACCENT, S_OPT_LID, S_OPT_SCAN, S_OPT_GLITCH, S_OPT_AUTO, S_OPT_LANG,
    S_OPTS_LID, S_HS_YES, S_HS_NO, S_OPTS_SCAN, S_OPTS_GLITCH, S_OPTS_AUTO, S_OPTS_LANG,
    S_LANG_SYSTEM,
    S_CREATED, S_CREDITS, S_NO_NINTENDO,
    S_DLG_TITLE, S_DLG_Q, S_DLG_OK, S_DLG_NO,
    S_NOFAV, S_NOFAV1, S_NOFAV2,
    S_BTN_MENU, S_BTN_EXIT, S_CARD_LIST,
    S_IDLE, S_NO_ICY,
    S_FW_NOAUDIO, S_FW_COPY, S_FW_LOADING, S_FW_OK, S_FW_TRY,
    S_ERR_FORMAT, S_ERR_END, S_ERR_NODSP, S_ERR_NET, S_ERR_THREAD,
    /* NetErr messages, same order as NE_SOC..NE_OTHER in player.h */
    S_NE_SOC, S_NE_NOWIFI, S_NE_NOIP, S_NE_DNS, S_NE_CONNECT, S_NE_TIMEOUT,
    S_NE_TLS, S_NE_CERT, S_NE_HTTP, S_NE_CUT, S_NE_NOTAUDIO, S_NE_OTHER,
    S_NET_RETRY_IN, S_NET_RETRYING, S_NET_WAITING,
    S_ACC0, S_ACC1, S_ACC2, S_ACC3, S_ACC4, S_ACC5, S_ACC6,
    S_COUNT
} StrId;

extern int g_lang;               /* resolved LANG_* */
void i18n_resolve(int langSel);  /* LANGSEL_* -> g_lang (SYSTEM uses CFGU_GetSystemLanguage) */
int  i18n_system_lang(void);
const char* T(StrId id);
const char* lang_name(int langSel);
