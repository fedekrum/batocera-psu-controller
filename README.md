# Controlador de PSU v4

Control de fuente ATX y apagado ordenado de una Raspberry Pi con Batocera
mediante un ESP8266 (también compila en ESP32 / S2 / S3).

El repo está separado por versiones: `v1/` (Teensy, **en servicio en un
arcade**), `v2/` (prototipo ESP8266 por pines, deprecated), `v3/` (firmware
serial, deprecated) y `v4/` (versión actual de desarrollo, documentada
abajo).

## Versión 4

- `ALIVE_TIMEOUT_MS = 60000`, grabado en EEPROM (persistente).
- Avisos `MODE BYPASS` / `MODE NORMAL` por la UART de pines para que la Pi
  sepa cuándo el ESP está en modo configuración e ignore un `POWEROFF_REQ`.
- `CFG_SET` de `LINK_BACKEND` / pines / baud bloqueado durante el bypass
  (`CFG_ERR bypass active`).
- Salida de bypass con la llave en OFF sin la supresión de 10 s.
- LED: fade suave con PSU encendida, parpadeo rápido si la PSU no prendió.
- Compila en `esp8266:esp8266:nodemcuv2`, `esp32:esp32:esp32`,
  `esp32:esp32:esp32s2` y `esp32:esp32:esp32s3`.

`CFG_VERSION = 4` es la versión del esquema de configuración guardado en
EEPROM; no confundir con la versión del controlador (v4).

## Hardware

ESP8266 NodeMCU LoLin v3:

- Pi TX (BCM4, pin 7, UART3 TX) -> ESP D5 (RX de SoftwareSerial)
- Pi RX (BCM5, pin 29, UART3 RX) <- ESP D7 (TX de SoftwareSerial)
- GND común
- ESP alimentado desde +5 V standby ATX (violeta)
- Bypass: botón FLASH (GPIO0 / D3), toggle por pulsación, solo RAM

Para ESP32 / S2 / S3 los pines por defecto son 5 / 4 / 2 / 14 / 13.

**Diagrama compartido:** el cableado de la fuente, el botón, el LED y las
dos líneas D5/D7 entre la ESP y la Pi es el mismo en **v2, v3 y v4**. Cada
una de esas carpetas incluye su copia de `diagrama_esp_rpi_psu.txt` para
que sea autocontenida (v1 tiene su propio diagrama Teensy). Lo único que
cambia es el significado de esas dos líneas en el lado de la Pi: en v2 son
`GPIO14`/`GPIO4` (heartbeat y shutdown), mientras que en v3 y v4 son UART3
(`BCM4`/`BCM5`, pines 7 y 29).

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
requieren reinicio. Los valores se guardan en la EEPROM interna de la ESP.

## Instalar `pi.py` en Batocera

En Batocera la partición **SHARE** se monta en `/userdata`. Por eso el
daemon va en `/userdata/system/pi.py` (en la PC, con la SD montada, es
`SHARE/system/pi.py`).

### Desde la PC con la SD montada

```sh
# ajustar el punto de montaje según tu usuario
SHARE=/run/media/fede/SHARE
cp v4/system/pi.py "$SHARE/system/pi.py"
chmod +x "$SHARE/system/pi.py"
grep 'VERSION 4' "$SHARE/system/pi.py"   # verificación
```

Después desmonta y expulsa la SD con `udisksctl` antes de llevársela a la Pi.

### Desde SSH en la Pi

```sh
scp v4/system/pi.py root@<IP-de-la-Pi>:/userdata/system/pi.py
ssh root@<IP-de-la-Pi> 'chmod +x /userdata/system/pi.py'
ssh root@<IP-de-la-Pi> reboot
```

### Launcher (necesario para que arranque)

`v4/system/custom_service` es el script que lanza el daemon:

```
/userdata/system/services/custom_service
```

Arranca `python3 -u /userdata/system/pi.py` con
`PI_SERIAL_DEVICE` (por defecto `/dev/ttyAMA4`) y escribe el log en
`/userdata/system/pi_power.log`. Copiarlo, darle `chmod +x` y reiniciar.
Para reiniciar el daemon sin reboot:

```sh
/userdata/system/services/custom_service stop
/userdata/system/services/custom_service start
```

El `stop` envía SIGTERM y espera a que `pi.py` loguee el apagado, escriba
la traza en `pi_power.shutdown.log`, envíe `BYE` al ESP y drene la UART
antes de que se desmonte la SD.

### Verificación

```sh
tail -f /userdata/system/pi_power.log
```

Debe aparecer `Arrancando pi.py serial ...`, `TX BOOT_OK` y `TX ALIVE
seq=...`. Si no aparece nada, revisar que `PI_SERIAL_DEVICE` sea el
correcto y que el launcher esté ejecutable.

## Build y flash

```sh
arduino-cli compile \
  --fqbn esp8266:esp8266:nodemcuv2 \
  --build-path /tmp/opencode/build-esp8266-esp8266-nodemcuv2 \
  v4/esp8266_cfg

arduino-cli upload -p /dev/ttyUSB0 \
  --fqbn esp8266:esp8266:nodemcuv2 \
  --build-path /tmp/opencode/build-esp8266-esp8266-nodemcuv2 \
  v4/esp8266_cfg
```

Usar siempre `--build-path` explícito para evitar colisiones de caché entre
familias.

## Archivos

### `v1/` (en servicio, no deprecated)

Sigue instalada en un arcade con Teensy 3.0. Se conserva sin cambios por
compatibilidad con ese equipo.

- `teensy_power.ino`: controlador Teensy 3.0, dos pines (heartbeat y
  shutdown), sin serial.
- `RPi-power.py`: daemon con `lgpio`, GPIO18 heartbeat / GPIO17 shutdown.
- `diagrama_teensy_rpi_psu.txt`: cableado.

### `v2/` (deprecated)

Prototipo en ESP8266 con la misma lógica por pines (D5/D7), todavía sin
serial:

- `power_on.ino`: prueba mínima de encendido.
- `esp_power.ino`, `esp_power2.ino`: versiones de la lógica Teensy en ESP.
- `RPi-power.py`: `lgpio`, GPIO14 heartbeat / GPIO4 shutdown.
- `diagrama_esp_rpi_psu.txt`: diagrama de la fuente, ESP, botón, LED y
  líneas D5/D7. El mismo archivo está copiado en v3 y v4.
- `test_psu/`, `com_test/` y `com_test/herramientas_diagnostico/`.

### `v3/` (deprecated)

- `esp8266.ino`: firmware con comunicación serial, tiempos hardcodeados y
  watchdog. No flashear.
- `pi.py`: daemon serial original (copia de rescate de la SD).
- `README.md`: detalle de la versión.
- `diagrama_esp_rpi_psu.txt`: copia del diagrama compartido con v2 y v4.

### `v4/` (actual)

- `v4/esp8266_cfg/esp8266_cfg.ino`: firmware generado.
- `v4/esp8266_cfg/cfg_types.h`: tipos, defines y plataformas.
- `v4/tools/make_cfg_firmware.py`: generador. Lee el firmware v3
  (`/home/fede/pacman/ESP8266/esp8266.ino`, guardado acá en
  `v3/esp8266.ino`) y escribe `v4/esp8266_cfg/esp8266_cfg.ino`. Las rutas
  `SRC`/`DST` son absolutas; ajustarlas si se usa en otra máquina.
- `v4/system/pi.py`: daemon serial de la Pi (en la SD:
  `SHARE/system/pi.py`).
- `v4/system/custom_service`: launcher Batocera (en la SD:
  `SHARE/system/services/custom_service`).
- `diagrama_esp_rpi_psu.txt`: copia del diagrama compartido con v2 y v3.

## Historial

- **v1**: Teensy 3.0, dos pines con heartbeats, sin serial. **En servicio**
  en un arcade; no está deprecated. La v4 es la línea de desarrollo actual,
  pero esta versión se mantiene para ese hardware.
- **v2**: prototipo ESP8266 con la misma lógica por pines (D5/D7), sin
  serial. **Deprecated**.
- **v3**: firmware ESP8266 con serial (tiempos hardcodeados y watchdog) y
  su `pi.py`. **Deprecated**, reemplazado por v4.
- **v4** (esta versión): configuración por comandos `CFG_*`, persistencia
  en EEPROM, modo bypass por FLASH, avisos a la Pi, indicador de fallo en
  el LED y timeout de 60 s para asegurar apagados limpios de la SD.
  El timeout se probó en 30 s y 20 s antes de fijarse en 60 s.
