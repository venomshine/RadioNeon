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
    [S_OPT_SHUFFLE]= { "Aleatorio", "Shuffle", "シャッフル" },
    [S_OPTS_SHUFFLE]={ "NEXT = UNA EMISORA AL AZAR", "NEXT = A RANDOM STATION", "NEXT = ランダムな局" },
    [S_OPTS_LID]   = { "SEGUIR SONANDO // AURIC: %s", "KEEP PLAYING // PHONES: %s", "再生を続ける // イヤホン: %s" },
    [S_HS_YES]     = { "CONECTADOS", "PLUGGED", "接続中" },
    [S_HS_NO]      = { "NO", "NO", "なし" },
    [S_OPTS_SCAN]  = { "LINEAS EN AMBAS PANTALLAS", "LINES ON BOTH SCREENS", "両画面にライン効果" },
    [S_OPTS_GLITCH]= { "AL CAMBIAR DE EMISORA + ALEATORIO", "ON STATION CHANGE + RANDOM", "局の切替時＋ランダム" },
    [S_OPTS_AUTO]  = { "REPRODUCE AL INICIAR LA APP", "PLAYS WHEN THE APP STARTS", "起動時に自動再生" },
    [S_OPTS_LANG]  = { "SISTEMA = IDIOMA DE LA CONSOLA", "SYSTEM = CONSOLE LANGUAGE", "システム = 本体の言語" },
    [S_LANG_SYSTEM]= { "SISTEMA", "SYSTEM", "システム" },
    [S_CREATED]    = { "hecho por VENOMSHiNE", "made by VENOMSHiNE", "制作: VENOMSHiNE" },
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
    [S_CARD_LIST]  = { "[SELECT] LISTA >", "[SELECT] LIST >", "[SELECT] リスト >" },
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
    [S_NE_SOC]     = { "Red de la consola no lista (soc %08X)", "Console network not ready (soc %08X)", "本体のネットワーク準備中 (soc %08X)" },
    [S_NE_NOWIFI]  = { "Sin conexión Wi-Fi", "No Wi-Fi connection", "Wi-Fi未接続" },
    [S_NE_NOIP]    = { "Wi-Fi conectado, esperando IP", "Wi-Fi connected, waiting for IP", "Wi-Fi接続済み、IP取得中" },
    [S_NE_DNS]     = { "No se encuentra el servidor (DNS)", "Server not found (DNS)", "サーバーが見つかりません (DNS)" },
    [S_NE_CONNECT] = { "El servidor rechaza la conexión", "Server refused the connection", "サーバーに接続できません" },
    [S_NE_TIMEOUT] = { "El servidor no responde", "Server not responding", "サーバーが応答しません" },
    [S_NE_TLS]     = { "Falló la conexión segura (TLS)", "Secure connection failed (TLS)", "安全な接続に失敗 (TLS)" },
    [S_NE_CERT]    = { "Certificado HTTPS no válido (revisa la fecha de la consola)", "Invalid HTTPS certificate (check the console date)", "HTTPS証明書が無効 (本体の日付を確認)" },
    [S_NE_HTTP]    = { "El servidor respondió HTTP %d", "Server replied HTTP %d", "サーバーの応答 HTTP %d" },
    [S_NE_CUT]     = { "Se cortó la emisión", "Stream dropped", "配信が切れました" },
    [S_NE_NOTAUDIO]= { "No es audio: parece una web o una lista .m3u/.pls", "Not audio: looks like a web page or a .m3u/.pls list", "音声ではありません (Webページか.m3u/.pls)" },
    [S_NE_OTHER]   = { "Error de red (curl %d)", "Network error (curl %d)", "ネットワークエラー (curl %d)" },
    [S_NET_RETRY_IN] = { "%s · reintento en %d s", "%s · retry in %d s", "%s · %d秒後に再試行" },
    [S_NET_RETRYING] = { "%s · reintentando...", "%s · retrying...", "%s · 再試行中..." },
    [S_NET_WAITING]  = { "%s · esperando...", "%s · waiting...", "%s · 待機中..." },
    [S_ACC0] = { "AMARILLO ACIDO", "ACID YELLOW", "アシッドイエロー" },
    [S_ACC1] = { "BLANCO", "WHITE", "ホワイト" },
    [S_ACC2] = { "CIAN", "CYAN", "シアン" },
    [S_ACC3] = { "MAGENTA", "MAGENTA", "マゼンタ" },
    [S_ACC4] = { "VERDE NEON", "NEON GREEN", "ネオングリーン" },
    [S_ACC5] = { "MORADO UV", "UV PURPLE", "UVパープル" },
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
