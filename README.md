# Controlador de PSU v3

Control de fuente ATX y apagado ordenado de una Raspberry Pi con Batocera
mediante un ESP8266 (también compila en ESP32 / S2 / S3).

El repo está separado por versiones: `v1/` (sin código conservado),
`v2/` (deprecated) y `v3/` (versión actual, documentada abajo).

## Versión 3

- `ALIVE_TIMEOUT_MS = 60000`, grabado en EEPROM (persistente).
- Avisos `MODE BYPASS` / `MODE NORMAL` por la UART de pines para que la Pi
  sepa cuándo el ESP está en modo configuración e ignore un `POWEROFF_REQ`.
- `CFG_SET` de `LINK_BACKEND` / pines / baud bloqueado durante el bypass
  (`CFG_ERR bypass active`).
- Salida de bypass con la llave en OFF sin la supresión de 10 s.
- LED: fade suave con PSU encendida, parpadeo rápido si la PSU no prendió.
- Compila en `esp8266:esp8266:nodemcuv2`, `esp32:esp32:esp32`,
  `esp32:esp32:esp32s2` y `esp32:esp32:esp32s3`.

## Hardware

ESP8266 NodeMCU LoLin v3:

- Pi TX (BCM4, pin 7, UART3 TX) -> ESP D5 (RX de SoftwareSerial)
- Pi RX (BCM5, pin 29, UART3 RX) <- ESP D7 (TX de SoftwareSerial)
- GND común
- ESP alimentado desde +5 V standby ATX (violeta)
- Bypass: botón FLASH (GPIO0 / D3), toggle por pulsación, solo RAM

Para ESP32 / S2 / S3 los pines por defecto son 5 / 4 / 2 / 14 / 13.

## Protocolo serial

9600 8N1, línea por mensaje terminada en `\n`.

Pi -> ESP: `BOOT_OK`, `ALIVE seq=N uptime=N`, `BYE reason=...`, `PONG`,
`LOG_LEVEL <nivel>`, `QUIET seconds=N`, `CFG_*`.

ESP -> Pi: `POWEROFF_REQ`, `PING`, `HELLO fw=...`, `CFG_ACK`, `CFG_SAVED`,
`MODE BYPASS`, `MODE NORMAL`, `LOG <nivel> ...`.

Apagado normal: la llave se apaga -> el ESP envía `POWEROFF_REQ` -> la Pi
responde `BYE` y ejecuta su apagado ordenado -> el ESP corta la PSU por
watchdog a los 60 s. El margen existe porque con 20 s y 30 s se vio
corrupción en la ext4 de la SD cuando el ESP cortaba antes de que la Pi
terminara de desmontar.

## Configuración

Comandos por serial: `CFG`, `CFG_GET <key>`, `CFG_SET <key>=<value>`,
`CFG_SAVE`, `CFG_LOAD`, `CFG_RESET`. Los cambios de backend, pines o baud
requieren reinicio. Los valores se guardan en la EEPROM interna de la
ESP (`CFG_VERSION = 4`).

## Build y flash

```sh
arduino-cli compile \
  --fqbn esp8266:esp8266:nodemcuv2 \
  --build-path /tmp/opencode/build-esp8266-esp8266-nodemcuv2 \
  v3/esp8266_cfg

arduino-cli upload -p /dev/ttyUSB0 \
  --fqbn esp8266:esp8266:nodemcuv2 \
  --build-path /tmp/opencode/build-esp8266-esp8266-nodemcuv2 \
  v3/esp8266_cfg
```

Usar siempre `--build-path` explícito para evitar colisiones de caché entre
familias.

## Archivos

### `v1/`

- `README.md`: única referencia disponible; no se conservó código.

### `v2/` (deprecated)

- `esp8266.ino`: firmware v2 original, sin modificaciones. No flashear.
- `pi.py`: daemon v2 original (copia de rescate de la SD), sin los avisos
  de bypass de la v3. Solo referencia histórica.
- `README.md`: detalle de la versión.

### `v3/` (actual)

- `v3/esp8266_cfg/esp8266_cfg.ino`: firmware generado.
- `v3/esp8266_cfg/cfg_types.h`: tipos, defines y plataformas.
- `v3/tools/make_cfg_firmware.py`: generador del firmware. Lee el firmware
  v2 (`/home/fede/pacman/ESP8266/esp8266.ino`, guardado acá en
  `v2/esp8266.ino`) y escribe `v3/esp8266_cfg/esp8266_cfg.ino`. Las rutas
  `SRC`/`DST` son absolutas; ajustarlas si se usa en otra máquina.
- `v3/system/pi.py`: daemon serial de la Pi (en la SD:
  `SHARE/system/pi.py`).

## Historial

- **v1**: versión inicial, muy básica: dos pines que transmitían
  heartbeats, sin comunicación serial.
- **v2** (`v2/`): firmware y `pi.py` originales con serial, tiempos
  hardcodeados y watchdog. **Deprecated**, reemplazado por v3.
- **v3** (esta versión): configuración por comandos `CFG_*`, persistencia
  en EEPROM, modo bypass por FLASH, avisos a la Pi, indicador de fallo en
  el LED y timeout de 60 s para asegurar apagados limpios de la SD.
  El timeout se probó en 30 s y 20 s antes de fijarse en 60 s.
