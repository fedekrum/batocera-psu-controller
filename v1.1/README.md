# Versión 1.1 — Teensy 3.0 (en servicio)

**Esta versión NO está deprecated**: sigue instalada y funcionando en un
arcade con un Teensy 3.0. Se conserva tal cual para ese equipo.

Primera versión del controlador: **dos pines, sin comunicación serial**.

- `teensy_power.ino`: firmware Teensy 3.0. Botón para encender/apagar,
  pulso de shutdown a la Pi y heartbeat de la Pi como confirmación de
  arranque. Tiempos hardcodeados (`HOLD_TO_ON_MS`, `FORCED_OFF_MS`,
  `SHUTDOWN_PULSE_MS`, `RPI_BOOT_GRACE_MS`, ...).
- `RPi-power.py`: daemon en la Pi con `lgpio`.
  - `GPIO_HEARTBEAT = 18`: salida, avisa "estoy vivo" al Teensy.
  - `GPIO_SHUTDOWN = 17`: entrada, recibe el pedido de apagado.
- `diagrama_teensy_rpi_psu.txt`: cableado Teensy / Pi / PSU.
- `PROTOCOLO.md`: contrato de señales, tiempos y obligaciones de cada lado.

No es la línea de desarrollo actual (esa es la v1.4), pero no se considera
obsoleta: está en producción en el arcade con Teensy 3.0 y se mantiene
sin cambios por compatibilidad con ese hardware.
