# Radio Neon / ラジオネオン (Nintendo 3DS) · v1.0.0

Radio por internet japonesa para 3DS con homebrew. Estética cyberpunk sobria tipo HUD/terminal:
negro carbón, un único color de acento (amarillo ácido #FCEE0A por defecto, configurable) y rojo solo para ON AIR y errores.
Creada por **VENOMSHiNE**.

## Requisitos
- 3DS / 2DS / New 3DS con Luma3DS (CFW) y Wi-Fi configurado.

## Instalación
**Opción A: .3dsx (Homebrew Launcher)**
1. Copia `radio3ds.3dsx` a `sdmc:/3ds/`.
2. Abre el Homebrew Launcher y lanza «Radio Neon ラジオネオン».

**Opción B: .cia (icono en el menú HOME)**
1. Copia `radio3ds.cia` a la SD (por ejemplo `sdmc:/cias/`).
2. Abre **FBI** → SD → carpeta → `radio3ds.cia` → *Install CIA*.

## Controles
| Botón | Acción |
|---|---|
| Pantalla táctil | Anterior · Play/Stop · Siguiente · ★ favorita · MENU · Salir |
| ↑ / ↓ | Elegir emisora / moverse por menús |
| ← / → | Emisora anterior / siguiente (en OPCIONES: cambiar valor) |
| L / R · ZL / ZR | Emisora anterior / siguiente (siempre) |
| A | Reproducir / aceptar |
| B | Parar / volver |
| Y | Marcar o desmarcar la emisora como **favorita** ★ |
| X | Abrir o cerrar el **MENU** |
| SELECT | Recargar `stations.txt` |
| START | Salir (pide confirmación: A = salir, B = cancelar) |

El volumen se controla con el **slider físico** de la consola.

## MENU
- **TODAS LAS EMISORAS** y **FAVORITAS**: la lista filtrada. Toca la estrella de una fila (o pulsa Y) para marcarla o desmarcarla.
- **OPCIONES**:
  - Color de acento: amarillo ácido, blanco, cian, magenta, verde neón, naranja o rojo.
  - Idioma: SISTEMA (usa el idioma de la consola si es español, inglés o japonés; si no, inglés), ESPAÑOL, ENGLISH, 日本語.
  - Tapa cerrada + auriculares: seguir sonando.
  - Scanlines.
  - Glitch RGB.
  - Arrancar sonando la última emisora.
- **ACERCA DE**: versión, autoría, créditos y licencias.

### Tapa cerrada
Con la opción activa, **auriculares conectados** y una emisora sonando, la consola no entra en reposo al cerrar la tapa:
las pantallas se apagan (retroiluminación off) y sigue el audio. Si quitas los auriculares con la tapa cerrada, la radio
se para (nunca suena por los altavoces con la tapa cerrada). Sin auriculares, o con la opción desactivada, la consola duerme
normal y al despertar la app vuelve a conectar la emisora.

## Firmware DSP (audio)
La app **no incluye ni extrae** firmware de Nintendo. Orden:
1. Si existe `sdmc:/3ds/dspfirm.cdc` (por ejemplo, volcado por ti con DSP1), lo usa.
2. Si no, usa **OpenDSP**, un firmware DSP libre (BSD-2) que va dentro de la app.
3. Si nada funciona, se avisa en pantalla.

`DSP:` en el pie de la pantalla de arriba indica el firmware activo (`DSPFIRM.CDC` u `OPENDSP`).

## Archivos en la SD (`sdmc:/3ds/radio3ds/`)
- `stations.txt`: 20 emisoras por defecto. Formato `Nombre | URL`, una por línea, `#` = comentario. Admite **MP3** y **AAC / AAC+ (HE-AAC v1, ADTS)** por `http://` o `https://`. No admite Ogg/Opus, HLS (.m3u8) ni listas .pls/.m3u.
- `config.ini`: opciones (acento, idioma, tapa, scanlines, glitch, autoarranque, última emisora).
- `favorites.txt`: favoritas (una URL por línea).
- `net.log`: registro de red para diagnosticar problemas (conexiones, tiempos, errores). Si una emisora falla, mándame este archivo. Al pasar de 256 KB se guarda como `net.old.log`.

## Notas
- El HTTPS verifica los certificados (raíces de Mozilla incluidas en la app). Si la fecha de la consola está mal, las emisoras https pueden fallar.
- Si una emisora cae o no hay red, la app reintenta sola sin rendirse (cada 2-15 s, o 30 s si el servidor da un error HTTP 4xx o un certificado no válido). Mientras tanto, la pantalla de arriba dice el motivo real (sin Wi-Fi, DNS, el servidor no responde, HTTP 404...) y cuánto falta para el siguiente intento.
- Si la URL no es audio (una web o una lista .m3u/.pls), se avisa en vez de intentar reproducirla.
- Créditos: OpenDSP © 2026 Cellenseres (BSD-2-Clause), libctru/citro2d/citro3d (zlib), libcurl, mbedTLS (Apache-2.0), libmpg123 (LGPL-2.1), Helix AAC (RealNetworks RPSL/RCSL), libiconv (LGPL), zlib, fuente Spleen © Frederic Cambus (BSD-2). Licencias en `third_party/` del zip y en `romfs/dsp/`.
