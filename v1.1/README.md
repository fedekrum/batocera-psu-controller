# v1.1 — v4 en Teensy 3.0

Puerto de la versión 4 al **Teensy 3.0**. No reemplaza a `v1/`: esa
carpeta sigue siendo el software que corre hoy en el arcade Teensy.
Esta es una línea nueva, para poder usar el mismo protocolo y la misma
lógica de apagado en placas Teensy.

## Estado

- Firmware **compilado** para `teensy:avr:teensy30` con Teensyduino 1.62.0
  (2026-11-22): 54984 B de flash, 5264 B de RAM.
- **Sin probar en hardware**: no se flasheó ninguna placa ni se movió la
  SD. La EEPROM, el watchdog y el bypass requieren una prueba real.
- El daemon `system/pi.py` es el de v4 con los textos adaptados; habla
  el mismo protocolo, así que sirve para v4 y v1.1.

## Qué cambia frente a v4

| | v4 (ESP8266/ESP32) | v1.1 (Teensy 3.0) |
|---|---|---|
| Enlace con la Pi | SoftwareSerial D5/D7 (o UART1 en ESP32) | `Serial1` hardware, pines **0 RX / 1 TX** |
| Bypass | botón FLASH (GPIO0) | pin dedicado **6** (`BYPASS_PIN`) |
| EEPROM | flash emulada por el core ESP | EEPROM emulada por Teensyduino, 2048 B |
| LED | D4, activo LOW | pin 13 (builtin), activo HIGH |
| Default `ALIVE_TIMEOUT_MS` | 30000 (EEPROM grabada a 60 s) | **60000** desde el primer arranque |
| `CFG_MAGIC` | `0x45535043` ("ESPC") | `0x54454331` ("TEC1") |
| `CFG_VERSION` | 4 | 4 (mismo esquema lógico) |
| Pines seriales | configurables | fijos: 0 y 1 (`CFG_ERR fixed pin`) |

Todo lo demás (mensajes, timeouts, watchdog, `QUIET`, `BYE`, códigos de
LED, bloqueo de `CFG_SET` en bypass) es igual que v4.

## Archivos

- `teensy_cfg/teensy_cfg.ino`: firmware. Es código escrito a mano, **no**
  generado por un script (a diferencia de `v4/esp8266_cfg/`).
- `teensy_cfg/cfg_types.h`: estructura de configuración y defaults.
- `system/pi.py`: daemon para la Pi (idéntico en protocolo al de v4).
- `system/custom_service`: launcher Batocera.
- `diagrama_teensy_v1_psu.txt`: cableado y cambios frente a v1.
- `PROTOCOLO.md`: contrato completo de esta versión.

## Cableado mínimo

```
Pi pin 7  (BCM4, UART3 TX) ──► Teensy 0 (Serial1 RX)
Pi pin 29 (BCM5, UART3 RX) ◄── Teensy 1 (Serial1 TX)
Pi GND                     ─── Teensy GND
ATX +5VSB (violeta)        ─── Teensy VIN
ATX GND                    ─── Teensy GND
Teensy 2 ──[1k]── base D882-Y ── PS_ON (verde)
Teensy 3 ── botón/llave a GND (INPUT_PULLUP)
Teensy 6 ── botón de bypass a GND (INPUT_PULLUP)
```

**El Teensy 3.0 no tolera 5 V**: no conectar señales de 5 V a sus pines.

## Compilar

```sh
arduino-cli compile --fqbn teensy:avr:teensy30 \
  --build-path /tmp/opencode/build-teensy30 v1.1/teensy_cfg
```

Si falta el core: `arduino-cli core install teensy:avr`.

Para grabar hace falta el cargador HalfKay (botón PROGRAM de la placa).
`teensy-loader-cli` **no está instalado** en esta máquina; no se intentó
flashear nada.

## Instalación en la Pi

Igual que v4:

1. `/boot/config.txt`: `enable_uart=1`, `dtoverlay=miniuart-bt`,
   `dtoverlay=uart3`.
2. `/boot/cmdline.txt`: sacar cualquier `console=serial0,115200`.
3. Copiar `system/pi.py` a `/userdata/system/pi.py` y
   `system/custom_service` a
   `/userdata/system/services/custom_service`, ambos ejecutables.
4. `PI_SERIAL_DEVICE` permite forzar el dispositivo serie si el nombre
   no es `/dev/ttyAMA4`.

## Prueba pendiente

Antes de darla por buena hace falta, en hardware real:

1. Verificar `HELLO fw=teensy11-1` en el monitor serie de la Pi.
2. Confirmar `CFG_GET ALIVE_TIMEOUT_MS` devuelve 60000.
3. Ciclo llave ON → `BOOT_OK`/`ALIVE` → llave OFF → `BYE` → corte
   de PSU ~60 s después.
4. Probar el pin 6: `MODE BYPASS` / `MODE NORMAL` y LED con fade.
5. Dejar varios ciclos y revisar que la SD no se corrompe.
