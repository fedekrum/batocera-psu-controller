# Contrato Pi ↔ controlador — v4.1 (multiplaca)

Versión 4.1: **un solo firmware** para ESP8266, ESP32, ESP32-S2, ESP32-S3 y
Teensy 3.0. Mantiene el protocolo de texto de v4/v1.1 y el daemon `pi.py`
de una sola copia. Las diferencias de hardware están en `firmware/boards.h`;
la lógica (watchdog, `CFG_*`, `BYE`, `QUIET`, bypass) está en
`firmware/firmware.ino` y es idéntica para las cinco placas.

Cualquier cambio en `firmware/` o `system/pi.py` debe respetar este
documento o actualizarlo en el mismo commit.

## Perfiles de placa

| Placa | Link de pines (default) | Bypass (default) | LED | EEPROM | `CFG_MAGIC` |
|---|---|---|---|---|---|
| ESP8266 NodeMCU | D5 = GPIO14 RX, D7 = GPIO13 TX (SoftwareSerial) | GPIO0, botón FLASH | GPIO2, activo LOW | flash emulada, `begin()`/`commit()` | `0x56343145` |
| ESP32 | UART1, RX GPIO14 / TX GPIO13 (GPIO matrix) | GPIO0 | GPIO2, activo LOW | flash emulada | `0x56343132` |
| ESP32-S2 | UART1, RX GPIO14 / TX GPIO13 | GPIO0 | GPIO2, activo LOW | flash emulada | `0x56343132` |
| ESP32-S3 | UART1, RX GPIO14 / TX GPIO13 | GPIO0 | GPIO2, activo LOW | flash emulada | `0x56343132` |
| Teensy 3.0 | `Serial1`, pin 0 RX / pin 1 TX (**fijos**) | pin 6 | pin 13, activo HIGH | emulada por Teensyduino, 2048 B, sin `begin()`/`commit()` | `0x56343154` |

- `CFG_VERSION = 5` es la versión del **esquema EEPROM**, no del
  controlador. Cambió respecto de v4 (4) porque se agregó el campo
  `bypassPin`. El magic distinto por familia evita interpretar una
  configuración grabada en otra placa como válida.
- Un volcado de EEPROM de v4 **no se lee** en v4.1: se cargan los defaults.
  Como el default de v4.1 es 60000 ms, no se pierde el margen de apagado.
- Los pines del enlace de pines son configurables salvo en Teensy 3.0:
  `Serial1` está en silicio y un `CFG_SET` con otro valor responde
  `CFG_ERR fixed pin`.

## Cableado del enlace con la Pi

| Señal | Pi (header 40) | Dirección |
|---|---|---|
| TX | pin 7, BCM4 (UART3_TX) | Pi → RX del controlador |
| RX | pin 29, BCM5 (UART3_RX) | Pi ← TX del controlador |
| GND | pin 6 (o cualquier GND) | común |

En la placa, según el perfil:

```
ESP8266 : pin 7 -> D5 (GPIO14)   ·   pin 29 <- D7 (GPIO13)
ESP32   : pin 7 -> GPIO14        ·   pin 29 <- GPIO13
Teensy  : pin 7 -> pin 0         ·   pin 29 <- pin 1
```

- UART a **9600 8N1**, modo raw, una línea por mensaje terminada en `\n`.
- `/dev/ttyAMA4` por default en el daemon; se puede forzar con
  `PI_SERIAL_DEVICE`.
- **El Teensy 3.0 no tolera 5 V**: señales de 3,3 V, sin level-shifter.
  El +5 V standby de la ATX entra solo por VIN.
- Las líneas GPIO viejas del cableado de v1 (GPIO17/GPIO18) ya no se usan.

## Mensajes

### Pi → controlador

| Mensaje | Cuándo | Semántica |
|---|---|---|
| `LOG_LEVEL <LVL>` | al abrir el puerto y al recibir `HELLO` | Fija el filtro de `LOG`. `LVL` ∈ `OFF\|ERROR\|WARNING\|INFO\|DEBUG`. |
| `BOOT_OK` | una vez, al arrancar el daemon | Confirma que la Pi terminó de arrancar. Cancela el watchdog de arranque. |
| `ALIVE seq=<N> uptime=<S>` | cada 500 ms | Heartbeat. Cualquier línea conocida reinicia el watchdog. |
| `BYE reason=<txt>` | antes del apagado ordenado | Confirma apagado limpio; el corte de PSU ocurre al vencer los 60 s. |
| `QUIET seconds=<N> reason=<txt>` | antes de un reboot | Ventana de silencio; se respeta hasta `QUIET_MAX_MS`. |
| `PONG` | respuesta a `PING` | Confirma enlace. |
| `CFG`, `CFG_GET <key>`, `CFG_SET <key>=<v>`, `CFG_SAVE`, `CFG_LOAD`, `CFG_RESET` | configuración | Ver abajo. |

### controlador → Pi

| Mensaje | Cuándo | Semántica |
|---|---|---|
| `POWEROFF_REQ` | llave OFF / pulsación corta, con PSU ON | Pide apagado ordenado. Se ignora si está activo el bypass. |
| `PING` | diagnóstico | La Pi debe responder `PONG`. |
| `HELLO fw=<ver>` | arranque | `fw=v41-esp8266`, `v41-esp32`, `v41-teensy30`, etc. |
| `LOG <LVL> <texto>` | cambios de estado | La Pi lo reinyecta en su logger con prefijo `[ESP]` (texto heredado de v4). |
| `LOG_LEVEL_ACK <LVL>` | tras `LOG_LEVEL` | Eco del nivel aplicado. |
| `MODE BYPASS` / `MODE NORMAL` | entrar/salir del bypass | Siempre por el enlace de pines, aunque el backend configurado sea USB. |
| `CFG <key>=<valor>` | respuesta a `CFG` / `CFG_GET` | Una línea por clave. |
| `CFG_ACK`, `CFG_SAVED`, `CFG_LOADED`, `CFG_DEFAULTS`, `CFG_ERR ...` | respuestas `CFG_*` | Ver abajo. |

Las líneas desconocidas se descartan: no renuevan el watchdog ni cancelan
`QUIET`. Así el ruido UART no mantiene viva la máquina por error.

## Modo bypass

- Entrada/salida por pulsación en `BYPASS_PIN` (default GPIO0 en ESP,
  pin 6 en Teensy), con `INPUT_PULLUP` y activo LOW. **RAM only**: nunca se
  guarda en EEPROM.
- Al entrar: PSU forzada ON, watchdog desactivado, enlace conmutado a `Serial`
  (USB) y LED con fade de 2 s (1 s subiendo, 1 s bajando). Si la PSU no
  prendió, el LED parpadea rápido.
- `MODE BYPASS` se envía por el enlace de pines **antes** de conmutar a USB;
  `pi.py` ignora `POWEROFF_REQ` mientras dure.
- Al salir: `BYPASS OFF` por USB, restauración del backend configurado y
  `MODE NORMAL` por el enlace de pines. Con la llave en OFF se pide apagado
  suave sin la supresión de 10 s.
- En bypass se rechazan con `CFG_ERR bypass active` los `CFG_SET` de
  `LINK_BACKEND`, `PIN_SERIAL_RX`, `PIN_SERIAL_TX`, `LINK_BAUD` y
  `BYPASS_PIN`.

## Configuración persistente

Comandos: `CFG`, `CFG_GET <key>`, `CFG_SET <key>=<value>`, `CFG_SAVE`,
`CFG_LOAD`, `CFG_RESET`. Los cambios marcados con `reboot=1` se aplican al
reiniciar. Rangos validados; los pines van de 0 a `CFG_PIN_MAX` (16 en
ESP8266, 48 en ESP32, 33 en Teensy).

| Clave | Default | Observación |
|---|---|---|
| `ALIVE_TIMEOUT_MS` | **60000** | 1000–120000. No bajar: 20/30 s corrompió la SD. |
| `QUIET_MAX_MS` | 600000 | Tope de la ventana `QUIET`. |
| `BOOT_CONFIRM_SUPPRESS_MS` | 60000 | Margen antes del watchdog de arranque. |
| `BOOT_CONFIRM_CUTOFF_MS` | 60000 | Corte si no llega tráfico de la Pi. |
| `SOFT_SHUTDOWN_LED_WINDOW_MS` | 180000 | Ventana para marcar apagado como limpio. |
| `PSU_OFF_SETTLE_DELAY_MS` | 5000 | Espera tras cortar PS_ON. |
| `MOMENTARY_SHORT_MS` | 500 | Pulsación corta. |
| `MOMENTARY_LONG_FORCE_MS` | 6000 | Pulsación larga: corte forzado. |
| `MOMENTARY_SOFT_SUPPRESS_MS` | 10000 | Supresión de apagado suave tras encender. |
| `SETUP_LED_BOOT_ON_MS` | 2000 | LED encendido al arrancar. |
| `LED_ENCODE_PULSE_MS` | 200 | Pulso del código de LED. |
| `LED_ENCODE_GAP_MS` | 1000 | Separación entre pulsos. |
| `DEBOUNCE_STABLE_MS` | 20 | Debounce del botón. |
| `LINK_BAUD` | 9600 | 1200–115200. `reboot=1`. |
| `LINK_BACKEND` | 0 | 0 = pines, 1 = USB. `reboot=1`. |
| `BUTTON_TYPE` | 1 | 0 = momentáneo, 1 = llave. |
| `BUTTON_PIN` | 5 ESP / 3 Teensy | `reboot=1`. |
| `PSU_ON_PIN` | 4 ESP / 2 Teensy | `reboot=1`. |
| `PIN_SERIAL_RX` | 14 ESP / 0 Teensy | Fijo en Teensy (`CFG_ERR fixed pin`). |
| `PIN_SERIAL_TX` | 13 ESP / 1 Teensy | Fijo en Teensy (`CFG_ERR fixed pin`). |
| `PIN_LED` | 2 ESP / 13 Teensy | `reboot=1`. |
| `BYPASS_PIN` | 0 ESP / 6 Teensy | `reboot=1`. |
| `PSU_ON_ACTIVE_HIGH` | 1 | PS_ON activo HIGH. |
| `LED_LOGICAL_ON_GPIO_HIGH` | 0 ESP / 1 Teensy | Polaridad del LED. |

## Flujo de apagado

```
Llave OFF / pulsación
  -> controlador envía POWEROFF_REQ por el enlace de pines
  -> pi.py responde BYE reason=..., escribe traza con fsync y drena la UART
  -> Pi ejecuta batocera-es-swissknife --shutdown (fallback shutdown -h now)
  -> controlador espera ALIVE_TIMEOUT_MS (60 s) y corta la PSU por watchdog
```

Códigos de LED con PSU OFF: `1` nunca bootó, `2` perdió `ALIVE`, `0`
apagado normal, `4` esperando `BOOT_OK` (con PSU ON).

## Obligaciones del daemon

1. Abrir el dispositivo serie con reintentos, modo raw y 9600 8N1.
2. Enviar `LOG_LEVEL`, luego `BOOT_OK` una vez, luego `ALIVE` cada 500 ms.
3. Ante `POWEROFF_REQ` (y sin bypass): `BYE reason=shutdown_req`, traza
   durable en `/userdata/system/pi_power.shutdown.log` y apagado ordenado.
4. Ante `MODE BYPASS` / `MODE NORMAL`: actualizar `bypass_active`.
5. Ante `SIGTERM`/`SIGINT`: si detecta reboot enviar
   `QUIET seconds=180 reason=reboot` antes de `BYE`; si no, solo `BYE`.
6. `LOG_LEVEL_DEFAULT = DEBUG`; `DEBUG=1` o `--debug` fuerzan DEBUG.

## Verificación

Compila con arduino-cli / cores ya instalados:

```sh
arduino-cli compile --fqbn esp8266:esp8266:nodemcuv2 --build-path /tmp/b-v41-e  v4.1/firmware
arduino-cli compile --fqbn esp32:esp32:esp32          --build-path /tmp/b-v41-32 v4.1/firmware
arduino-cli compile --fqbn esp32:esp32:esp32s2        --build-path /tmp/b-v41-s2 v4.1/firmware
arduino-cli compile --fqbn esp32:esp32:esp32s3        --build-path /tmp/b-v41-s3 v4.1/firmware
arduino-cli compile --fqbn teensy:avr:teensy30        --build-path /tmp/b-v41-t3 v4.1/firmware
```

Verificado el 2026-11-22: las cinco compilan. **No se flasheó ninguna
placa**; la EEPROM, el watchdog y el bypass requieren prueba en hardware.
