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
    [S_MENU_HIST]  = { "HISTORIAL", "SONG HISTORY", "再生履歴" },
    [S_MENU_SAVED] = { "GUARDADAS", "SAVED SONGS", "保存した曲" },
    [S_MENU_EXIT]  = { "SALIR", "EXIT", "終了" },
    [S_SUB_HIST]   = { "%02d CANCIONES // [X]", "%02d SONGS // [X]", "%02d 曲 // [X]" },
    [S_SUB_SAVED]  = { "%02d GUARDADAS // [B] AL SONAR", "%02d SAVED // [B] WHILE PLAYING", "%02d 曲 // 再生中に[B]" },
    [S_SUB_EXIT]   = { "CERRAR RADIO NEON // [MANTÉN START]", "CLOSE RADIO NEON // [HOLD START]", "Radio Neonを終了 // [START長押し]" },
    [S_HDR_SAVED]  = { "GUARDADAS", "SAVED", "保存した曲" },
    [S_SAVED_EMPTY]  = { "Nada guardado aún", "Nothing saved yet", "保存した曲はありません" },
    [S_SAVED_EMPTY1] = { "Pulsa B mientras suena una canción", "Press B while a song is playing", "曲の再生中にBを押すか" },
    [S_SAVED_EMPTY2] = { "o Y en el historial.", "or Y in the history.", "履歴でYを押してください。" },
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
    [S_BTN_MENU]   = { "[START] MENÚ", "[START] MENU", "[START] メニュー" },
    [S_BTN_EXIT]   = { "SALIR [MANTÉN START]", "EXIT [HOLD START]", "終了 [START長押し]" },
    [S_CARD_LIST]  = { "[SELECT] LISTA >", "[SELECT] LIST >", "[SELECT] リスト >" },
    [S_CHIP_SHUFFLE]   = { "ALEATORIO", "SHUFFLE", "シャッフル" },
    [S_CHIP_SLEEP_OFF] = { "TEMPORIZADOR", "SLEEP TIMER", "スリープ" },
    [S_CHIP_SLEEP_ON]  = { "APAGADO", "SLEEP", "スリープ" },
    [S_BAT_TITLE]      = { "BATERÍA BAJA", "LOW BATTERY", "バッテリー残量低下" },
    [S_BAT_MSG]        = { "La radio se ha pausado. Conecta el cargador.", "The radio was paused. Plug in the charger.", "ラジオを一時停止しました。充電してください。" },
    [S_BAT_OK]         = { "[A] VALE", "[A] OK", "[A] OK" },
    [S_BATL_TITLE]     = { "BATERÍA AGOTADA", "BATTERY EMPTY", "バッテリー切れ" },
    [S_BATL_MSG1]      = { "La radio está apagada para no gastar lo poco que queda.", "The radio is off to save what is left.", "残りを守るためラジオを止めています。" },
    [S_BATL_MSG2]      = { "Conecta el cargador para seguir escuchando.", "Plug in the charger to keep listening.", "充電すると再生できます。" },
    [S_BATL_NOW]       = { "Batería agotada: conecta el cargador", "Battery empty: plug in the charger", "バッテリー切れ：充電してください" },
    [S_LINK_SCAN1]     = { "Escanea con el móvil", "Scan with your phone", "スマホでスキャンして" },
    [S_LINK_SCAN2]     = { "para ver el historial.", "to see the history.", "履歴を見られます。" },
    [S_LINK_NET]       = { "Misma Wi-Fi o hotspot del móvil", "Same Wi-Fi or the phone's hotspot", "同じWi-Fiかテザリングで" },
    [S_LINK_A]         = { "[A] Buscarla en Spotify", "[A] Find it on Spotify", "[A] Spotifyで探す" },
    [S_LINK_NOWIFI]    = { "SIN WI-FI", "NO WI-FI", "Wi-Fiなし" },
    [S_LINK_NOWIFI2]   = { "Conéctate para verlo en el móvil.", "Connect to see it on your phone.", "接続するとスマホで見られます。" },
    [S_SAVED]          = { "GUARDADA", "SAVED", "保存しました" },
    [S_HDR_HIST]       = { "HISTORIAL", "HISTORY", "履歴" },
    [S_HIST_EMPTY]     = { "Aún no hay canciones", "No songs yet", "まだ曲がありません" },
    [S_HIST_EMPTY1]    = { "Las canciones se apuntan al sonar 30 s.", "Songs are added after 30 s on air.", "30秒流れた曲が記録されます。" },
    [S_HIST_EMPTY2]    = { "B guarda la que está sonando.", "B saves the one that is playing.", "Bで再生中の曲を保存します。" },
    [S_HIST_SCAN]      = { "Escanea para buscarla.", "Scan to look it up.", "スキャンして検索できます。" },
    [S_HIST_KEYS]      = { "[Y] Guardar   [B] Volver", "[Y] Save   [B] Back", "[Y] 保存   [B] 戻る" },
    [S_HDR_SLEEP]      = { "TEMPORIZADOR", "SLEEP TIMER", "スリープ" },
    [S_SLEEP_OFF]      = { "Desactivado", "Off", "オフ" },
    [S_SLEEP_MIN]      = { "%d minutos", "%d minutes", "%d分" },
    [S_SLEEP_CUSTOM]   = { "Personalizado", "Custom", "カスタム" },
    [S_SLEEP_SUB]      = { "LA RADIO SE APAGA SOLA (FUNDIDO DE 10 S)", "THE RADIO STOPS BY ITSELF (10 S FADE)", "自動で停止します（10秒フェード）" },
    [S_SLEEP_LEFT]     = { "QUEDAN %s", "%s LEFT", "残り %s" },
    [S_SLEEP_KB]       = { "Minutos hasta apagar (1-480)", "Minutes until it stops (1-480)", "停止までの分数（1〜480）" },
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
