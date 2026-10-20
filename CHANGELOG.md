# Changelog

## 1.1.0

### Arreglos
- **HOME ya no congela la app.** La app tenía abierto todo el rato el servicio de las pantallas (`gsp::Lcd`) y el menú HOME se quedaba esperándolo; ahora solo se abre un momento para apagar o encender la luz. Además, antes de ir a HOME se apaga el audio y al volver se arranca de cero.
- **Tras cerrar y abrir la tapa vuelve a sonar:** la red espera a que vuelva el Wi-Fi en vez de rendirse, y el audio se reinicia de cero.
- La lista de emisoras tiene su propio cursor: moverse ya no cambia la emisora que se ve fuera.
- B en la lista vuelve a donde se abrió (pantalla principal o menú).
- La rayita de la tarjeta solo aparece con la emisora en vivo.
- Debajo del nombre de la emisora queda solo la línea fina.
- «ESPAÑOL» sale con la misma fuente que el resto en el selector de idioma.
- Con la tapa cerrada, si la emisora falla del todo, la consola duerme en vez de quedarse despierta a oscuras.
- Un 404 ya no mete la página de error en el audio; las URL que son una web o una lista `.m3u`/`.pls` se avisan.
- Si se va la luz justo al guardar, `config.ini` y `favorites.txt` se recuperan de la copia `.tmp`.
- Un nombre de emisora en Latin-1 en `stations.txt` ya no puede hacer leer memoria de más al animar el título.
- Compila sin avisos.
- Los créditos de «Acerca de» empiezan desde el principio cada vez que se abre.
- El menú HOME muestra el nombre completo, «Radio Neon ラジオネオン».
- La estrella de favorita tiene su propia columna en la tarjeta, con «FAV» debajo.

### Novedades
- **Historial de canciones** (X o el botón del centro), con una página para el móvil (QR, misma Wi-Fi o hotspot) con botones de Spotify, YouTube y Copiar, y QR de Spotify de cada canción. **B** guarda con ♥ la que suena, con un bip-bip al estilo del sonido del HOME («GUARDADA»), y otra vez B la quita («ELIMINADA»). La página tiene al pie GitHub, Discord, Ko-fi y la licencia.
- **Menú con scroll**: Todas las emisoras, Favoritas, Historial, Guardadas (solo las del ♥), Opciones, Controles, Acerca de y Salir.
- **Controles**: la consola (New 3DS XL) dibujada como un esquema, con una línea desde cada botón hasta lo que hace; si pulsas uno, se ilumina.
- **Modo ahorro** (Opciones, apagado de serie): tras 20 s sin tocar nada con una emisora sonando las pantallas se quedan en negro y la app deja de dibujar; la radio sigue. Cualquier botón o tocar la pantalla las devuelve.
- **Aviso de batería baja**: bips cortos desde el 40 % y el 20 %, pausa con aviso al 10 %, y al 3 % la radio queda bloqueada con un cartel fijo hasta conectar el cargador.
- **Temporizador de apagado** (botón o R): 15, 30, 60, 90 minutos o personalizado, con fundido de 10 s al final.
- **Aleatorio** en la pantalla principal (botón o L): NEXT salta a una emisora al azar.
- **Barra de volumen** en la pantalla de arriba: sigue al deslizador de la consola y muestra el % al moverlo.
- Lista de emisoras renovada: más radios de Japón (FMかほく, WREP, Gotanno FM, Retro PC Game Music, Justplay de Shibuya, Moon Mission de house de Tokio…), dos de eurobeat, FLY FM de city pop, J-Rock, Visual Kei, J Pop y VocaloPlus!. Si tu `stations.txt` era la lista de serie sin cambios, se actualiza sola y la anterior queda en `stations.old.txt`.
- Género de cada emisora en la lista y en favoritas (`Nombre | URL | Género` en `stations.txt`).
- **START** abre y cierra el menú (mantenido 2 s, salir), **X** el historial y **SELECT** la lista de emisoras.
- La página del móvil carga entera en la consola real (antes se cortaba la respuesta al cerrar la conexión) y no le quita turno a la emisora.
- Las emisoras de bitrate alto ya no se entrecortan: el colchón de audio va según el bitrate (3 s al empezar y 4 s tras un corte).
- HTTPS con verificación de certificados (raíces de Mozilla).
- La red se reintenta sola sin límite y muestra el motivo real y la cuenta atrás.
- `net.log` en la SD para diagnosticar.
- Controles revisados: cada botón hace una sola cosa y solo donde tiene sentido.
- «Acerca de» con QR de GitHub, Discord y Ko-fi y créditos que suben.
- Datos reales en el HUD (servidor, HTTP/HTTPS, códec y bitrate, bytes reales del stream) en lugar de frecuencias inventadas.
- Firma VENOMSHiNE en la pantalla de arriba.
- Batería con el porcentaje exacto en los 5 cuadritos, y aviso al cargar.
- Acentos nuevos: rojo Coca-Cola y morado ultravioleta (sustituye al naranja).
- Nombre definitivo **Radio Neon** (`RadioNeon.cia` / `.3dsx`, carpeta `sdmc:/3ds/RadioNeon`, migrada sola).
- `config.ini` y `favorites.txt` se escriben de forma segura.
- Licencia MPL-2.0.
- Vocales mayúsculas con tilde (Á É Í Ó Ú) en la fuente del HUD, como ya tenía la Ñ.

## 1.0.0

Primera versión.
