# Licencias / Licenses

**Radio Neon** © 2026 VENOMSHiNE — [Mozilla Public License 2.0](LICENSE).

> **ES:** Puedes usar, estudiar, modificar y redistribuir Radio Neon. Si distribuyes una versión
> modificada, los archivos de Radio Neon que cambies tienen que seguir bajo la MPL-2.0 y publicarse
> con su código. Mantén este aviso y menciona a VENOMSHiNE como autor original.
>
> **EN:** You may use, study, modify and redistribute Radio Neon. If you distribute a modified version,
> the Radio Neon files you change must stay under the MPL-2.0 and be published with their source.
> Keep this notice and credit VENOMSHiNE as the original author.

## Por qué MPL-2.0 y no GPL / Why MPL-2.0 and not GPL

The AAC / HE-AAC decoder (Helix) is under the RealNetworks RPSL/RCSL, which is not compatible with the GPL.
The MPL-2.0 is a file-level copyleft that can be combined with it, with the LGPL (libmpg123) and with the
permissive licences below in the same program.

## Código de terceros / Third-party code

| Componente | Licencia | Dónde |
|---|---|---|
| Helix AAC / HE-AAC decoder © RealNetworks | RPSL 1.0 / RCSL | `source/helix/` (`LICENSE-HELIX.txt`) |
| OpenDSP © 2026 Cellenseres (DSP firmware) | BSD-2-Clause | `romfs/dsp/`, `third_party/opendsp/` |
| Spleen 5×8 font © Frederic Cambus | BSD-2-Clause | `source/mono5x8.h`, `third_party/spleen/` |
| QR Code generator © Project Nayuki | MIT | `lib/qrcodegen.c`, `lib/qrcodegen.h` |
| Mozilla CA certificate bundle | MPL-2.0 | `romfs/certs/cacert.pem` |
| libctru, citro2d, citro3d (devkitPro) | zlib | linked |
| libcurl | curl (MIT-style) | linked |
| Mbed TLS | Apache-2.0 | linked |
| libmpg123 | LGPL-2.1 | linked statically¹ |
| libiconv | LGPL | linked statically¹ |
| zlib | zlib | linked |

¹ The full source of Radio Neon and the build instructions in the README are published, so anyone can
rebuild the app against a modified libmpg123 / libiconv, as the LGPL requires.

No Nintendo code or firmware is included. The app uses `sdmc:/3ds/dspfirm.cdc` only if the user already
has it; otherwise it uses OpenDSP.
