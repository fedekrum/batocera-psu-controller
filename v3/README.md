# Versión 3 — firmware serial (deprecated)

Primera versión con **comunicación serial** entre la ESP y la Pi, en lugar
de los dos pines de heartbeat.

- `esp8266.ino`: firmware original (21294 bytes), tiempos hardcodeados y
  watchdog de `ALIVE`. No flashear.
- `pi.py`: daemon serial original de la Pi (17126 bytes), copia de rescate
  de `SHARE/system/pi.py` antes de cualquier cambio. No conoce los avisos
  `MODE BYPASS` / `MODE NORMAL` ni el flag `bypass_active`.

Incluye `diagrama_esp_rpi_psu.txt`, el **mismo diagrama de cableado que v2 y
v4** (copiado en cada carpeta). La ESP y la fuente se cablean igual; en el
lado de la Pi las líneas D5/D7 son UART3 (`BCM4`/`BCM5`) en lugar de los
pines de heartbeat/shutdown de v2.

Incluye además `PROTOCOLO.md`, adaptado del documento del zip
`comunicacion serial/PROTOCOLO.md` (que lo rotula "v2") a las constantes
reales de este firmware.

Esta es la versión que se usó como base para generar la v4 con
`../v4/tools/make_cfg_firmware.py`. La versión actual es la v4.
