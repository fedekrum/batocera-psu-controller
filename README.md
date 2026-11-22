# Controlador de PSU

Control de fuente ATX y apagado ordenado de una Raspberry Pi con Batocera
mediante un ESP8266 (también compila en ESP32 / S2 / S3) o un Teensy 3.0.

El repo está separado por versiones: `v1.1/` (Teensy, **en servicio en un
arcade**), `v1.1.1-beta/` (v1.4 portada a Teensy 3.0, **compilada pero sin probar en
hardware**), `v1.2/` (prototipo ESP8266 por pines, deprecated), `v1.3/`
(firmware serial, deprecated), `v1.4/` (versión actual sobre ESP) y `v1.5-beta/`
(un solo firmware para las cinco placas, **compilado pero sin probar en
hardware**; documentado abajo).

### Mapa de numeración (cambio del 2026-11-22)

| Numeración anterior | Numeración actual | Contenido |
|---|---|---|
| `v1/` | `v1.1/` | Teensy 3.0 original, dos GPIO, en servicio |
| `v1.1/` | `v1.1.1-beta/` | mismo protocolo portado a Teensy 3.0 |
| `v2/` | `v1.2/` | prototipo ESP8266 por pines |
| `v3/` | `v1.3/` | firmware serial |
| `v4/` | `v1.4/` | configurable con EEPROM |
| `v4.1/` | `v1.5-beta/` | firmware único multiplaca |

No hay una carpeta `v5/`: «la 5» es `v1.5-beta/`, que antes se llamaba
`v4.1/`. Los commits y documentos anteriores pueden mencionar los nombres
viejos; este mapa es la referencia.

## Índice rápido

| Querés saber... | Archivo |
|---|---|
| Contexto general, reglas y estado del proyecto (empieza acá si sos una IA/agente) | `AGENTS.md` |
| Por qué se tomaron las decisiones sensibles (60 s, numeración, bypass) | `DECISIONES.md` |
| Cableado, mensajes y timeouts de una versión | `v1.1/`, `v1.1.1-beta/`, `v1.2/`, `v1.3/`, `v1.4/` o `v1.5-beta/` → `PROTOCOLO.md` |
| Instalación en Batocera y comandos de build/flash | este README, abajo |
| Histórico y estado de cada versión | este README, "Historial" |

## Versión 1.4

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
EEPROM; no confundir con la versión del controlador (v1.4).

## Versión 1.1.1 beta (Teensy 3.0)

La misma lógica y el mismo protocolo de v1.4, pero en Teensy 3.0. No
reemplaza a `v1.1/`, que sigue en servicio en el arcade original.

- Serial1 por hardware en pines **0 (RX) y 1 (TX)**; la Pi usa UART3
  (BCM4/BCM5, pines 7 y 29), igual que v1.4.
- Bypass por pin dedicado **6** (no hay botón FLASH).
- LED en pin 13, activo HIGH; EEPROM emulada por Teensyduino.
- `ALIVE_TIMEOUT_MS = 60000` **de fábrica** (no depende de un grabado
  manual como en v1.4).
- Compila con `teensy:avr:teensy30`. Ver `v1.1.1-beta/README.md` y
  `v1.1.1-beta/PROTOCOLO.md`. **No probada en hardware.**

## Versión 1.5 beta (multiplaca)

Un solo firmware para **ESP8266, ESP32, ESP32-S2, ESP32-S3 y Teensy 3.0**,
con el protocolo de v1.4/v1.1.1-beta y una sola copia de `pi.py`. Es aditiva: no
modifica v1.4, v1.1.1-beta ni v1.1.

- `firmware/boards.h` concentra las diferencias de hardware (pines,
  polaridad del LED, EEPROM y tipo de enlace).
- `ALIVE_TIMEOUT_MS = 60000` de fábrica en todas las placas.
- Bypass configurable con `BYPASS_PIN` (GPIO0 en ESP, pin 6 en Teensy).
- `BUTTON_TYPE` funciona en las cinco placas: llave (default v1.4) o
  pulsador momentáneo ("temp switch"). Al elegir momentáneo, los tiempos
  son los de v1.1 (500/6000/10000 ms); no hay defaults de botón por placa.
- Esquema EEPROM 5: una configuración de v1.4 no se lee, pero los defaults
  ya traen 60 s.
- Compilada para las cinco placas. **No probada en hardware.** Ver
  `v1.5-beta/README.md` y `v1.5-beta/PROTOCOLO.md`.

## Hardware

ESP8266 NodeMCU LoLin v3:

- Pi TX (BCM4, pin 7, UART3 TX) -> ESP D5 (RX de SoftwareSerial)
- Pi RX (BCM5, pin 29, UART3 RX) <- ESP D7 (TX de SoftwareSerial)
- GND común
- ESP alimentado desde +5 V standby ATX (violeta)
- Bypass: botón FLASH (GPIO0 / D3), toggle por pulsación, solo RAM

Para ESP32 / S2 / S3 los pines por defecto son 5 / 4 / 2 / 14 / 13.

**Diagrama compartido:** el cableado de la fuente, el botón, el LED y las
dos líneas D5/D7 entre la ESP y la Pi es el mismo en **v1.2, v1.3 y v1.4**. Cada
una de esas carpetas incluye su copia de `diagrama_esp_rpi_psu.txt` para
que sea autocontenida (v1.1 tiene su propio diagrama Teensy). Lo único que
cambia es el significado de esas dos líneas en el lado de la Pi: en v1.2 son
`GPIO14`/`GPIO4` (heartbeat y shutdown), mientras que en v1.3 y v1.4 son UART3
(`BCM4`/`BCM5`, pines 7 y 29).

## Protocolo

Cada versión tiene su propio `PROTOCOLO.md`, porque la lógica cambia:
`v1.1/PROTOCOLO.md` (Teensy, 2 GPIO), `v1.1.1-beta/PROTOCOLO.md` (Teensy, UART),
`v1.2/PROTOCOLO.md` (ESP8266 por pines), `v1.3/PROTOCOLO.md` (UART),
`v1.4/PROTOCOLO.md` (UART configurable, EEPROM y bypass) y
`v1.5-beta/PROTOCOLO.md` (las cinco placas, perfiles por hardware). Los de v1.3 y
v1.4 se tomaron como referencia de los `PROTOCOLO.md` del zip
`PSU PC para mis arcades.zip`, adaptados al código real de cada versión;
v1.1.1-beta deriva de v1.4 y v1.5-beta unifica v1.4 y v1.1.1-beta.

Resumen del enlace serial (v1.1.1-beta/v1.3/v1.4/v1.5-beta): 9600 8N1, línea por mensaje
terminada en `\n`.

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
cp v1.4/system/pi.py "$SHARE/system/pi.py"
chmod +x "$SHARE/system/pi.py"
grep 'VERSION 1.4' "$SHARE/system/pi.py"   # verificación
```

Después desmonta y expulsa la SD con `udisksctl` antes de llevársela a la Pi.

### Desde SSH en la Pi

```sh
scp v1.4/system/pi.py root@<IP-de-la-Pi>:/userdata/system/pi.py
ssh root@<IP-de-la-Pi> 'chmod +x /userdata/system/pi.py'
ssh root@<IP-de-la-Pi> reboot
```

### Launcher (necesario para que arranque)

`v1.4/system/custom_service` es el script que lanza el daemon:

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
  v1.4/esp8266_cfg

arduino-cli upload -p /dev/ttyUSB0 \
  --fqbn esp8266:esp8266:nodemcuv2 \
  --build-path /tmp/opencode/build-esp8266-esp8266-nodemcuv2 \
  v1.4/esp8266_cfg
```

Para el Teensy 3.0 (v1.1.1-beta):

```sh
arduino-cli compile --fqbn teensy:avr:teensy30 \
  --build-path /tmp/opencode/build-teensy30 v1.1.1-beta/teensy_cfg
```

Para v1.5-beta hay un solo directorio (`v1.5-beta/firmware`) y cinco FQBN; los
comandos están en `v1.5-beta/README.md`.

Usar siempre `--build-path` explícito para evitar colisiones de caché entre
familias. No se hace `upload`: hace falta el cargador HalfKay y una placa,
y no hay ninguna conectada.

## Archivos

### `v1.1/` (en servicio, no deprecated)

Sigue instalada en un arcade con Teensy 3.0. Se conserva sin cambios por
compatibilidad con ese equipo.

- `teensy_power.ino`: controlador Teensy 3.0, dos pines (heartbeat y
  shutdown), sin serial.
- `RPi-power.py`: daemon con `lgpio`, GPIO18 heartbeat / GPIO17 shutdown.
- `diagrama_teensy_rpi_psu.txt`: cableado.
- `PROTOCOLO.md`: contrato de las dos líneas GPIO (heartbeat GPIO18 /
  shutdown GPIO17) y del botón.

### `v1.1.1-beta/` (compilada, sin probar en hardware)

Puerto de v1.4 a Teensy 3.0:

- `teensy_cfg/teensy_cfg.ino`: firmware (escrito a mano, no generado).
- `teensy_cfg/cfg_types.h`: estructura de configuración y defaults.
- `system/pi.py`: mismo daemon que v1.4, con textos adaptados.
- `system/custom_service`: launcher Batocera.
- `diagrama_teensy_v1.1.1_psu.txt`: cableado y cambios frente a v1.1.
- `PROTOCOLO.md`: contrato UART del Teensy.

### `v1.5-beta/` (compilada en cinco placas, sin probar en hardware)

- `firmware/firmware.ino`: firmware único, lógica común.
- `firmware/boards.h`: perfiles de placa (pines, polaridad, EEPROM, magic).
- `firmware/cfg_types.h`: esquema EEPROM 5.
- `system/pi.py` y `system/custom_service`: daemon y launcher.
- `PROTOCOLO.md`: contrato y tabla de perfiles.

### `v1.2/` (deprecated)

Prototipo en ESP8266 con la misma lógica por pines (D5/D7), todavía sin
serial:

- `power_on.ino`: prueba mínima de encendido.
- `esp_power.ino`, `esp_power2.ino`: versiones de la lógica Teensy en ESP.
- `RPi-power.py`: `lgpio`, GPIO14 heartbeat / GPIO4 shutdown.
- `diagrama_esp_rpi_psu.txt`: diagrama de la fuente, ESP, botón, LED y
  líneas D5/D7. El mismo archivo está copiado en v1.3 y v1.4.
- `test_psu/`, `com_test/` y `com_test/herramientas_diagnostico/`.
- `PROTOCOLO.md`: contrato de heartbeat (GPIO14 → D7) y shutdown
  (D5 → GPIO4).

### `v1.3/` (deprecated)

- `esp8266.ino`: firmware con comunicación serial, tiempos hardcodeados y
  watchdog. No flashear.
- `pi.py`: daemon serial original (copia de rescate de la SD).
- `README.md`: detalle de la versión.
- `PROTOCOLO.md`: contrato UART (mensajes, timeouts y códigos de LED).
- `diagrama_esp_rpi_psu.txt`: copia del diagrama compartido con v1.2 y v1.4.

### `v1.4/` (actual)

- `v1.4/esp8266_cfg/esp8266_cfg.ino`: firmware generado.
- `v1.4/esp8266_cfg/cfg_types.h`: tipos, defines y plataformas.
- `v1.4/tools/make_cfg_firmware.py`: generador. Lee el firmware v1.3
  (`/home/fede/pacman/ESP8266/esp8266.ino`, guardado acá en
  `v1.3/esp8266.ino`) y escribe `v1.4/esp8266_cfg/esp8266_cfg.ino`. Las rutas
  `SRC`/`DST` son absolutas; ajustarlas si se usa en otra máquina.
- `v1.4/system/pi.py`: daemon serial de la Pi (en la SD:
  `SHARE/system/pi.py`).
- `v1.4/system/custom_service`: launcher Batocera (en la SD:
  `SHARE/system/services/custom_service`).
- `diagrama_esp_rpi_psu.txt`: copia del diagrama compartido con v1.2 y v1.3.
- `PROTOCOLO.md`: contrato UART v1.4 (CFG, EEPROM, bypass y timeout 60 s).

## Historial

- **v1.1**: Teensy 3.0, dos pines con heartbeats, sin serial. **En servicio**
  en un arcade; no está deprecated. La v1.4 es la línea de desarrollo actual,
  pero esta versión se mantiene para ese hardware.
- **v1.1.1-beta**: v1.4 portada a Teensy 3.0: Serial1 en pines 0/1, bypass por pin
  6, EEPROM emulada y timeout de 60 s de fábrica. **Compilada** con
  Teensyduino 1.62.0, **sin probar en hardware**. No reemplaza a v1.1.
- **v1.2**: prototipo ESP8266 con la misma lógica por pines (D5/D7), sin
  serial. **Deprecated**.
- **v1.3**: firmware ESP8266 con serial (tiempos hardcodeados y watchdog) y
  su `pi.py`. **Deprecated**, reemplazado por v1.4.
- **v1.4**: configuración por comandos `CFG_*`, persistencia en EEPROM, modo
  bypass por FLASH, avisos a la Pi, indicador de fallo en el LED y timeout
  de 60 s para asegurar apagados limpios de la SD. El timeout se probó en
  30 s y 20 s antes de fijarse en 60 s.
- **v1.5-beta**: un solo firmware para las cinco placas con `boards.h`, bypass
  configurable y 60 s de fábrica. Compilada en las cinco variantes,
  **sin probar en hardware**. Es la línea a mantener de acá en adelante.
