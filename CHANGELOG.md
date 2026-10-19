# Changelog

## 1.1.0

### Arreglos
- **HOME ya no cierra la app.** Antes de ir a HOME se apaga el audio y al volver se arranca de cero (OpenDSP no sobrevivía a la «reanudación» de libctru).
- **Tras cerrar y abrir la tapa vuelve a sonar:** la red espera a que vuelva el Wi-Fi en vez de rendirse, y el audio se reinicia de cero.
- **El sonido del menú HOME suena bien:** campana FM estéreo a 32728 Hz en lugar de pitidos de onda cuadrada.
- La lista de emisoras tiene su propio cursor: moverse ya no cambia la emisora que se ve fuera.
- B en la lista vuelve a donde se abrió (pantalla principal o menú).
- La rayita de la tarjeta solo aparece con la emisora en vivo.
- «ESPAÑOL» sale con la misma fuente que el resto en el selector de idioma.
- Con la tapa cerrada, si la emisora falla del todo, la consola duerme en vez de quedarse despierta a oscuras.
- Un 404 ya no mete la página de error en el audio; las URL que son una web o una lista `.m3u`/`.pls` se avisan.

### Novedades
- HTTPS con verificación de certificados (raíces de Mozilla).
- La red se reintenta sola sin límite y muestra el motivo real y la cuenta atrás.
- `net.log` en la SD para diagnosticar.
- Opción **Aleatorio**: NEXT salta a una emisora al azar.
- Controles revisados: cada botón hace una sola cosa y solo donde tiene sentido.
- «Acerca de» con QR de GitHub, Discord y Ko-fi y créditos que suben.
- Datos reales en el HUD (servidor, HTTP/HTTPS, códec y bitrate, bytes reales del stream) en lugar de frecuencias inventadas.
- Firma VENOMSHiNE en la pantalla de arriba.
- Batería con el porcentaje exacto en los 5 cuadritos, y aviso al cargar.
- Acentos nuevos: rojo Coca-Cola y morado ultravioleta (sustituye al naranja).
- Nombre definitivo **Radio Neon** (`RadioNeon.cia` / `.3dsx`, carpeta `sdmc:/3ds/RadioNeon`, migrada sola).
- `config.ini` y `favorites.txt` se escriben de forma segura.
- Licencia MPL-2.0.

## 1.0.0

Primera versión.
