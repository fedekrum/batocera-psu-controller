# v1.5-beta — un solo firmware para cinco placas

Puerto de v1.4 a un **único código fuente** que compila en ESP8266, ESP32,
ESP32-S2, ESP32-S3 y Teensy 3.0. Conserva el protocolo de v1.4/v1.1.1-beta y usa
una sola copia del daemon `pi.py`.

No reemplaza a `v1.4/` ni a `v1.1.1-beta/`: es una línea nueva y aditiva. `v1.1/`
sigue siendo el software en servicio del arcade original.

## Estado

- **Compilado** el 2026-11-22 para las cinco placas con los cores ya
  instalados (ESP8266 3.1.2, ESP32 4.0.0, Teensyduino 1.62.0).
- **No flasheado, no probado en hardware.** No se tocó la SD ni la EEPROM
  de ninguna placa.
- `pi.py` pasa `py_compile`.

## Cómo se logra un solo firmware

`firmware/boards.h` tiene un perfil por familia con los únicos datos que
cambian: pines por defecto, polaridad del LED, magic de EEPROM y tipo de
enlace de pines. `firmware/firmware.ino` tiene la lógica común y solo
cuatro puntos condicionales:

1. Acceso a EEPROM: `begin()`/`commit()` en ESP, `EEPROM.put()` directo en
   Teensy (2048 B emulados).
2. Enlace de pines: SoftwareSerial (ESP8266), UART1 con GPIO matrix
   (ESP32/S2/S3) o `Serial1` fijo en 0/1 (Teensy).
3. Bypass: `BYPASS_PIN` configurable, con default GPIO0 en ESP y pin 6 en
   Teensy. Antes era un pin fijo en v1.4.
4. Validación de pines seriales: en Teensy se rechaza cualquier valor que
   no sea 0/1 con `CFG_ERR fixed pin`.

Todo lo demás —mensajes, watchdog de 60 s, `QUIET`, `BYE`, códigos de
LED, bloqueo de `CFG_SET` en bypass— es una sola implementación.

### Los dos tipos de botón

`BUTTON_TYPE` funciona en **cualquier placa**: `1` = llave/switch (default
de fábrica, igual que v1.4) y `0` = pulsador momentáneo ("temp switch", como
el de v1.1). No hay defaults de botón por placa. Para usar momentáneo:

```sh
CFG_SET BUTTON_TYPE=0
CFG_SAVE
# reboot
```

Los tiempos del modo momentáneo ya vienen con los valores de v1.1
(500 ms / 6000 ms / 10 s / 5 s / 20 ms de debounce), así que no hicieron
falta variables nuevas. La equivalencia completa está en
`PROTOCOLO.md` → "Modo momentáneo: equivalencia con v1.1".

## Cambios frente a v1.4 / v1.1.1-beta

| | v1.4 | v1.1.1-beta | v1.5-beta |
|---|---|---|---|
| Placas | ESP8266/ESP32/S2/S3 | Teensy 3.0 | las cinco |
| `ALIVE_TIMEOUT_MS` default | 30000 (EEPROM grabada a 60000) | 60000 | **60000 en todas** |
| Esquema EEPROM | 4 | 4 | **5** (agrega `bypassPin`) |
| Bypass | GPIO0 fijo | pin 6 fijo | configurable (`BYPASS_PIN`) |
| Botón | llave default, momentáneo opcional | llave default | igual que v1.4, en **todas** las placas |
| Fuentes de firmware | generador + archivo generado | archivo a mano | **un archivo** |
| `pi.py` | copia en v1.4 | copia en v1.1.1-beta | **una copia** |

Una EEPROM grabada por v1.4 no se lee en v1.5-beta: cambian el esquema y el
magic. Se cargan los defaults, que ya traen 60000 ms, así que no se pierde
el margen que evita la corrupción de SD.

## Archivos

- `firmware/firmware.ino`: firmware único.
- `firmware/boards.h`: perfiles de placa.
- `firmware/cfg_types.h`: estructura de configuración (esquema 5).
- `system/pi.py`: daemon, mismo protocolo que v1.4 y v1.1.1-beta.
- `system/custom_service`: launcher Batocera.
- `PROTOCOLO.md`: contrato completo y tabla de perfiles.

## Compilar

```sh
arduino-cli compile --fqbn esp8266:esp8266:nodemcuv2 --build-path /tmp/b-v15-e  v1.5-beta/firmware
arduino-cli compile --fqbn esp32:esp32:esp32          --build-path /tmp/b-v15-32 v1.5-beta/firmware
arduino-cli compile --fqbn esp32:esp32:esp32s2        --build-path /tmp/b-v15-s2 v1.5-beta/firmware
arduino-cli compile --fqbn esp32:esp32:esp32s3        --build-path /tmp/b-v15-s3 v1.5-beta/firmware
arduino-cli compile --fqbn teensy:avr:teensy30        --build-path /tmp/b-v15-t3 v1.5-beta/firmware
```

Usar `--build-path` distinto por placa. No se hace `upload`: el Teensy
necesita HalfKay (`teensy-loader-cli`, no instalado) y las ESP requieren
placa conectada.

## Cableado del enlace

```
Pi pin 7  (BCM4, TX) -> RX del controlador
Pi pin 29 (BCM5, RX) <- TX del controlador
Pi GND                 -> GND del controlador
```

Según la placa:

- ESP8266: RX en D5/GPIO14, TX en D7/GPIO13.
- ESP32, S2, S3: RX en GPIO14, TX en GPIO13 (UART1).
- Teensy 3.0: RX en pin 0, TX en pin 1 (`Serial1`).

**El Teensy 3.0 no tolera 5 V.** El +5 V standby de la ATX va por VIN.

## Instalación en Batocera

Igual que v1.4: copiar `system/pi.py` a `/userdata/system/pi.py` y
`system/custom_service` a `/userdata/system/services/custom_service`,
ambos ejecutables. Habilitar `enable_uart=1`, `dtoverlay=miniuart-bt` y
`dtoverlay=uart3` en `/boot/config.txt`, y quitar cualquier
`console=serial0,115200` de `/boot/cmdline.txt`.

## Prueba pendiente

1. Ver `HELLO fw=v15-<placa>` en el monitor serie.
2. Confirmar `CFG_GET ALIVE_TIMEOUT_MS` devuelve 60000.
3. Ciclo llave ON → `BOOT_OK`/`ALIVE` → llave OFF → `BYE` → corte a los 60 s.
4. Probar `CFG_SET BUTTON_TYPE=0` + reboot: pulsación de 500 ms enciende/
   apaga suave y de 6000 ms corta la PSU, en Teensy y en ESP.
5. En Teensy, probar el pin 6: `MODE BYPASS` / `MODE NORMAL` y LED con fade.
6. Varios ciclos y revisar que la SD no se corrompe.
