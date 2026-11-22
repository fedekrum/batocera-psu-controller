# Versión 2 (deprecated)

Archivos originales de la versión 2, conservados sin modificaciones como
referencia histórica. **No usar en producción**: la versión actual es la v3,
en `../v3/`.

- `esp8266.ino`: firmware original con comunicación serial, tiempos
  hardcodeados y watchdog. Además es la entrada del generador
  `../v3/tools/make_cfg_firmware.py`.
- `pi.py`: daemon serial original de la Pi (17126 bytes), copia de rescate
  de `SHARE/system/pi.py` antes de cualquier cambio. No conoce los avisos
  `MODE BYPASS` / `MODE NORMAL` ni el flag `bypass_active`.

Historial:

- v1: dos pines transmitiendo heartbeats, sin serial (sin código en el repo).
- v2: estos archivos.
- v3: firmware configurable con EEPROM, bypass y avisos a la Pi.
