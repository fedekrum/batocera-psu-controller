# Versión 2 — ESP8266 por pines (deprecated)

Prototipo en ESP8266 NodeMCU LoLin v3 con la **misma lógica por pines** que
la v1 (todavía sin comunicación serial):

- `power_on.ino`: prueba mínima, fuerza PSU y LED encendidos.
- `esp_power.ino`: "versión final híbrida" (lógica Teensy o switch puro,
  según `MOMENTARY_BUTTON`).
- `esp_power2.ino`: clon de la lógica Teensy con switch permanente.
- `RPi-power.py`: daemon con `lgpio`.
  - `GPIO_HEARTBEAT = 14`: salida de heartbeat.
  - `GPIO_SHUTDOWN = 4`: entrada de pedido de apagado.
- Pines ESP: D1 botón, D2 PSU, D4 LED, D5 shutdown a la Pi, D7 alive de la Pi.
- `diagrama_esp_rpi_psu.txt`: cableado. **Es el diagrama compartido por v2,
  v3 y v4** (no hay copias en las otras carpetas). En v3/v4 las dos líneas
  hacia la Pi pasan a ser UART3 en `BCM4`/`BCM5` en vez de heartbeat/shutdown
  por `GPIO14`/`GPIO4`.
- `test_psu/`: prueba aislada de la PSU.
- `com_test/` y `com_test/herramientas_diagnostico/`: emisores/receptores
  A1/A2 y B1/B2, pruebas seriales C1/C2/C3 y su README.

Se conserva tal cual venía en `ESP8266.zip` (sin `__MACOSX` ni `.DS_Store`).
No usar en producción: la versión actual es la v4.
