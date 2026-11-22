# Versión 1 — Teensy 3.0 (deprecated)

Primera versión del controlador: **dos pines, sin comunicación serial**.

- `teensy_power.ino`: firmware Teensy 3.0. Botón para encender/apagar,
  pulso de shutdown a la Pi y heartbeat de la Pi como confirmación de
  arranque. Tiempos hardcodeados (`HOLD_TO_ON_MS`, `FORCED_OFF_MS`,
  `SHUTDOWN_PULSE_MS`, `RPI_BOOT_GRACE_MS`, ...).
- `RPi-power.py`: daemon en la Pi con `lgpio`.
  - `GPIO_HEARTBEAT = 18`: salida, avisa "estoy vivo" al Teensy.
  - `GPIO_SHUTDOWN = 17`: entrada, recibe el pedido de apagado.
- `diagrama_teensy_rpi_psu.txt`: cableado Teensy / Pi / PSU.

No usar en producción: fue reemplazada por la v2 (ESP8266 por pines) y
después por las versiones con serial. La versión actual es la v4.
