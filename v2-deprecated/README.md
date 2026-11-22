# Firmware v2 (deprecated)

Este es el `esp8266.ino` original: comunicación serial con la Pi, tiempos
hardcodeados y watchdog de `ALIVE`. Está **deprecado** y se conserva solo
como referencia histórica y como entrada del generador
`tools/make_cfg_firmware.py`.

No flashear. La versión actual es la v3, en `../esp8266_cfg/esp8266_cfg.ino`.

Historial:

- v1: dos pines transmitiendo heartbeats, sin serial.
- v2: este firmware.
- v3: firmware configurable con EEPROM, bypass y avisos a la Pi.
