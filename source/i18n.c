#include <3ds.h>
#include "i18n.h"

int g_lang = LANG_ES;

static const char* STR[S_COUNT][NUM_LANGS] = {
    [S_MENU_ALL]   = { "TODAS LAS EMISORAS", "ALL STATIONS", "すべての放送局" },
    [S_MENU_FAV]   = { "FAVORITAS", "FAVORITES", "お気に入り" },
    [S_MENU_OPT]   = { "OPCIONES", "OPTIONS", "設定" },
    [S_MENU_ABOUT] = { "ACERCA DE", "ABOUT", "このアプリについて" },
    [S_SUB_ALL]    = { "%02d EMISORAS // MP3 + AAC", "%02d STATIONS // MP3 + AAC", "%02d 局 // MP3 + AAC" },
    [S_SUB_FAV]    = { "%02d MARCADAS // [Y] = MARCAR", "%02d MARKED // [Y] = MARK", "%02d 件登録 // [Y]で登録" },
    [S_SUB_OPT]    = { "ACENTO: %s", "ACCENT: %s", "アクセント: %s" },
    [S_SUB_ABOUT]  = { "CREDITOS // LICENCIAS", "CREDITS // LICENSES", "クレジット // ライセンス" },
    [S_HDR_STATIONS] = { "EMISORAS", "STATIONS", "放送局" },
    [S_HDR_FAVS]   = { "FAVORITAS", "FAVORITES", "お気に入り" },
    [S_HDR_OPT]    = { "OPCIONES", "OPTIONS", "設定" },
    [S_HDR_ABOUT]  = { "ACERCA DE", "ABOUT", "情報" },
    [S_BACK]       = { "< VOLVER", "< BACK", "< もどる" },
    [S_OPT_ACCENT] = { "Color de acento", "Accent colour", "アクセントカラー" },
    [S_OPT_LID]    = { "Tapa cerrada + auriculares", "Lid closed + headphones", "フタを閉じても再生 (イヤホン)" },
    [S_OPT_SCAN]   = { "Scanlines (líneas CRT)", "Scanlines (CRT lines)", "走査線 (CRT)" },
    [S_OPT_GLITCH] = { "Glitch RGB", "RGB glitch", "RGBグリッチ" },
    [S_OPT_AUTO]   = { "Arrancar con la última emisora", "Start with the last station", "前回の局で起動" },
    [S_OPT_LANG]   = { "Idioma", "Language", "言語" },
    [S_OPTS_LID]   = { "SEGUIR SONANDO // AURIC: %s", "KEEP PLAYING // PHONES: %s", "再生を続ける // イヤホン: %s" },
    [S_HS_YES]     = { "CONECTADOS", "PLUGGED", "接続中" },
    [S_HS_NO]      = { "NO", "NO", "なし" },
    [S_OPTS_SCAN]  = { "LINEAS EN AMBAS PANTALLAS", "LINES ON BOTH SCREENS", "両画面にライン効果" },
    [S_OPTS_GLITCH]= { "AL CAMBIAR DE EMISORA + ALEATORIO", "ON STATION CHANGE + RANDOM", "局の切替時＋ランダム" },
    [S_OPTS_AUTO]  = { "REPRODUCE AL INICIAR LA APP", "PLAYS WHEN THE APP STARTS", "起動時に自動再生" },
    [S_OPTS_LANG]  = { "SISTEMA = IDIOMA DE LA CONSOLA", "SYSTEM = CONSOLE LANGUAGE", "システム = 本体の言語" },
    [S_LANG_SYSTEM]= { "SISTEMA", "SYSTEM", "システム" },
    [S_CREATED]    = { "Creada por VENOMSHiNE", "Created by VENOMSHiNE", "制作: VENOMSHiNE" },
    [S_CREDITS]    = { "CREDITOS // LICENCIAS", "CREDITS // LICENSES", "クレジット // ライセンス" },
    [S_NO_NINTENDO]= { "SIN FIRMWARE DE NINTENDO INCLUIDO", "NO NINTENDO FIRMWARE INCLUDED", "任天堂のファームウェアは含まれていません" },
    [S_DLG_TITLE]  = { "SALIR?", "EXIT?", "終了しますか？" },
    [S_DLG_Q]      = { "¿Cerrar Radio Neon?", "Close Radio Neon?", "ラジオネオンを閉じますか？" },
    [S_DLG_OK]     = { "[A] SALIR", "[A] EXIT", "[A] 終了" },
    [S_DLG_NO]     = { "[B] CANCELAR", "[B] CANCEL", "[B] キャンセル" },
    [S_NOFAV]      = { "SIN FAVORITAS", "NO FAVORITES", "お気に入りなし" },
    [S_NOFAV1]     = { "PULSA [Y] O TOCA LA ESTRELLA", "PRESS [Y] OR TAP THE STAR", "[Y]を押すか星をタッチして" },
    [S_NOFAV2]     = { "EN UNA EMISORA PARA MARCARLA", "ON A STATION TO MARK IT", "お気に入りに登録しよう" },
    [S_BTN_MENU]   = { "[X] MENU", "[X] MENU", "[X] メニュー" },
    [S_BTN_EXIT]   = { "[START] SALIR", "[START] EXIT", "[START] 終了" },
    [S_CARD_LIST]  = { "LISTA >", "LIST >", "リスト >" },
    [S_IDLE]       = { "Pulsa A o toca PLAY para sintonizar", "Press A or tap PLAY to tune in", "AボタンかPLAYで受信開始" },
    [S_NO_ICY]     = { "-- sin metadatos ICY --", "-- no ICY metadata --", "-- ICY情報なし --" },
    [S_FW_NOAUDIO] = { "!! DSP FIRMWARE: SIN AUDIO", "!! DSP FIRMWARE: NO AUDIO", "!! DSPファームウェア: 音声なし" },
    [S_FW_COPY]    = { "COPIA DSPFIRM.CDC A SDMC:/3DS/", "COPY DSPFIRM.CDC TO SDMC:/3DS/", "DSPFIRM.CDCをSDMC:/3DS/へコピー" },
    [S_FW_LOADING] = { "SIN DSPFIRM.CDC: CARGANDO FIRMWARE LIBRE", "NO DSPFIRM.CDC: LOADING FREE FIRMWARE", "DSPFIRM.CDCなし: フリーFWを読込中" },
    [S_FW_OK]      = { "AUDIO OK CON FIRMWARE LIBRE (BSD-2)", "AUDIO OK WITH FREE FIRMWARE (BSD-2)", "フリーFWで音声OK (BSD-2)" },
    [S_FW_TRY]     = { "DSPFIRM.CDC FALLA: PROBANDO OPENDSP...", "DSPFIRM.CDC FAILED: TRYING OPENDSP...", "DSPFIRM.CDC失敗: OPENDSPを試行中..." },
    [S_ERR_FORMAT] = { "Formato no soportado (Ogg/Opus)", "Unsupported format (Ogg/Opus)", "非対応の形式 (Ogg/Opus)" },
    [S_ERR_END]    = { "Stream finalizado / sin datos", "Stream ended / no data", "ストリーム終了 / データなし" },
    [S_ERR_NODSP]  = { "Sin firmware DSP: no hay audio", "No DSP firmware: no audio", "DSPファームウェアなし: 音声なし" },
    [S_ERR_NET]    = { "Red: %s", "Network: %s", "ネットワーク: %s" },
    [S_ERR_THREAD] = { "No se pudo crear hilo", "Could not create thread", "スレッドを作成できません" },
    [S_ACC0] = { "AMARILLO ACIDO", "ACID YELLOW", "アシッドイエロー" },
    [S_ACC1] = { "BLANCO", "WHITE", "ホワイト" },
    [S_ACC2] = { "CIAN", "CYAN", "シアン" },
    [S_ACC3] = { "MAGENTA", "MAGENTA", "マゼンタ" },
    [S_ACC4] = { "VERDE NEON", "NEON GREEN", "ネオングリーン" },
    [S_ACC5] = { "NARANJA", "ORANGE", "オレンジ" },
    [S_ACC6] = { "ROJO", "RED", "レッド" },
};

const char* T(StrId id) { const char* s = STR[id][g_lang]; return s ? s : STR[id][LANG_EN]; }

int i18n_system_lang(void) {
    u8 l = CFG_LANGUAGE_EN;
    if (R_FAILED(CFGU_GetSystemLanguage(&l))) return LANG_EN;
    if (l == CFG_LANGUAGE_ES) return LANG_ES;
    if (l == CFG_LANGUAGE_JP) return LANG_JA;
    return LANG_EN;   /* English, or any other language */
}

void i18n_resolve(int sel) {
    switch (sel) {
        case LANGSEL_ES: g_lang = LANG_ES; break;
        case LANGSEL_EN: g_lang = LANG_EN; break;
        case LANGSEL_JA: g_lang = LANG_JA; break;
        default: g_lang = i18n_system_lang(); break;
    }
}

const char* lang_name(int sel) {
    switch (sel) {
        case LANGSEL_ES: return "ESPAÑOL";
        case LANGSEL_EN: return "ENGLISH";
        case LANGSEL_JA: return "日本語";
        default: return T(S_LANG_SYSTEM);
    }
}
