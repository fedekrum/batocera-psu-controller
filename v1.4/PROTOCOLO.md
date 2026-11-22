# Contrato Pi ↔ ESP — v1.4 (configurable)

Versión actual del controlador. Mantiene el protocolo de texto de v1.3 y
agrega **configuración persistente en EEPROM**, comandos `CFG_*`, modo
bypass por el botón FLASH y avisos explícitos a la Pi. Cualquier cambio
en `esp8266_cfg/` o `system/pi.py` debe respetar este documento o
actualizarlo primero.

Este contrato se construyó a partir del `PROTOCOLO.md` de v1.3 (origen:
`comunicacion serial/PROTOCOLO.md` del zip `PSU PC para mis arcades.zip`)
más lo que realmente implementa el firmware generado por
`tools/make_cfg_firmware.py`.

## Cableado

Igual que v1.2/v1.3 en la parte de la fuente, botón y LED:

| Señal | ESP8266 (default) | ESP32 / S2 / S3 | Pi (header 40) | Dirección |
|---|---|---|---|---|
| Botón | D1 = GPIO 5 | GPIO 5 | — | Usuario → ESP |
| `PS_ON` de la ATX | D2 = GPIO 4 | GPIO 4 | — | ESP → PSU |
| LED de estado | D4 = GPIO 2 | GPIO 2 | — | — |
| RX del enlace a pines | D5 = GPIO 14 | GPIO 14 | TX: pin 7, BCM4 (UART3_TX) | Pi → ESP |
| TX del enlace a pines | D7 = GPIO 13 | GPIO 13 | RX: pin 29, BCM5 (UART3_RX) | ESP → Pi |
| GND común | GND | GND | Pin 6 (o cualquier GND) | — |
| Bypass | GPIO 0 (D3, botón FLASH) | GPIO 0 | — | Usuario → ESP |

- Todos los pines son configurables por `CFG_SET`; la tabla muestra los
  valores de fábrica. Los cambios de pines/baud/backend requieren reboot.
- **Backend 0** (`LINK_BACKEND=0`, default): SoftwareSerial en ESP8266,
  UART1 con pines remapeados en ESP32. Es el enlace con la Pi.
- **Backend 1** (`LINK_BACKEND=1`): UART0/USB. En ESP8266 y ESP32 los
  pines son fijos (RX GPIO3, TX GPIO1); en S2/S3 suele ser USB-CDC.
- UART0 se inicializa siempre porque es el enlace del modo bypass.
- El ESP se alimenta desde +5 V standby ATX (violeta).

## Capa física y robustez

- 9600 8N1 por defecto (`LINK_BAUD`), sin control de flujo.
- Una línea por mensaje, terminada en `\n`; se ignora `\r`; ASCII.
- Buffer de línea de 96 caracteres (`RX_LINE_MAX`).
- El ESP sólo reacciona a líneas con prefijo conocido (`BOOT_OK`, `ALIVE`,
  `QUIET`, `BYE`, `PONG`, `LOG_LEVEL`, `CFG*`). El ruido no puede mantener
  vivo el watchdog ni cancelar un `QUIET`.
- Cualquier línea `CFG*` cuenta como tráfico válido y refresca el
  watchdog de `ALIVE`.

## Configuración en la Pi (Batocera)

1. En `/boot/config.txt`: `enable_uart=1`, `dtoverlay=miniuart-bt` y
   `dtoverlay=uart3` (UART3 en GPIO4/5, pines 7/29). `miniuart-bt`
   conserva Bluetooth; no usar `disable-bt` si se usan mandos BT.
2. En `/boot/cmdline.txt`: quitar cualquier `console=serial0,115200` o
   equivalente.
3. Reiniciar y verificar `/dev/ttyAMA4` o `/dev/ttyAMA3` según la build.
   `pi.py` usa `/dev/ttyAMA4` por defecto; se puede forzar con
   `PI_SERIAL_DEVICE`.
4. Instalar `system/pi.py` en `/userdata/system/pi.py` y el launcher
   `system/custom_service` en `/userdata/system/services/custom_service`,
   ambos ejecutables. El log queda en `/userdata/system/pi_power.log`.

## Mensajes

### Pi → ESP

| Mensaje | Cuándo | Semántica |
|---|---|---|
| `LOG_LEVEL <LVL>` | al abrir el puerto y al recibir `HELLO` | Fija el filtro de `LOG` del ESP. `LVL` ∈ `OFF\|ERROR\|WARNING\|INFO\|DEBUG`. |
| `BOOT_OK` | una vez, tras inicializar | Confirma arranque; sale del estado "esperando Pi". |
| `ALIVE seq=<n> uptime=<s>` | cada 500 ms | Heartbeat. |
| `QUIET seconds=<n> reason=<...>` | antes de una ventana deliberada sin `ALIVE` | "voy a callarme hasta `n` s"; no cortar PSU por silencio. Cualquier mensaje conocido distinto de `BYE`/`QUIET` cancela la ventana. Tope duro: `QUIET_MAX_MS`. |
| `BYE reason=<poweroff\|reboot\|shutdown_req\|sigint>` | justo antes de apagar/reiniciar | Despedida ordenada: el silencio posterior no es error. |
| `PONG` | respuesta a `PING` | Diagnóstico. |
| `CFG` | cuando se quiere volcar la configuración | El ESP responde una línea `CFG <key>=<valor>` por clave. |
| `CFG_GET <key>` | consulta puntual | Responde `CFG <key>=<valor>` o `CFG_ERR unknown key`. |
| `CFG_SET <key>=<value>` | cambiar un valor en RAM | Responde `CFG_ACK <key>=<value>` (más ` reboot=1` si aplica) o `CFG_ERR ...`. **No persiste** hasta `CFG_SAVE`. |
| `CFG_SAVE` | grabar la RAM en EEPROM | Responde `CFG_SAVED`. |
| `CFG_LOAD` | recargar desde EEPROM | Responde `CFG_LOADED` o `CFG_ERR load failed`. |
| `CFG_RESET` | restaurar defaults de fábrica en RAM y EEPROM | Responde `CFG_DEFAULTS`. |

### ESP → Pi

| Mensaje | Cuándo | Semántica |
|---|---|---|
| `POWEROFF_REQ` | botón/shutdown suave con PSU ON y suppress vencido | Pide apagado ordenado. **Se ignora en la Pi mientras esté activo el bypass.** |
| `PING` | diagnóstico | La Pi debe responder `PONG`. |
| `HELLO fw=<ver>` | arranque del ESP | Identificación; la Pi reenvía `LOG_LEVEL`. |
| `LOG <LVL> <texto>` | cambios de estado | La Pi lo reinyecta en su logger con prefijo `[ESP]`. Filtrado en origen según el nivel activo. |
| `LOG_LEVEL_ACK <LVL>` | tras aceptar `LOG_LEVEL` | Eco del nivel aplicado. |
| `MODE BYPASS` | al entrar al bypass | Se envía por el enlace de pines **antes** de conmutar a USB. La Pi marca `bypass_active=True` e ignora `POWEROFF_REQ`. |
| `MODE NORMAL` | al salir del bypass | Se envía por el enlace de pines aunque el backend configurado sea USB. La Pi reanuda el apagado normal. |
| `CFG <key>=<valor>` | respuesta a `CFG` / `CFG_GET` | Una línea por clave. |
| `CFG_ACK`, `CFG_SAVED`, `CFG_LOADED`, `CFG_DEFAULTS`, `CFG_ERR ...` | respuestas `CFG_*` | Ver tabla anterior. |

## Modo bypass (botón FLASH / GPIO0)

- Toggle por pulsación. Es RAM only: **nunca se guarda en EEPROM**.
- Al entrar: PSU forzada en ON, watchdog desactivado, enlace conmutado a
  USB (UART0) y LED con fade continuo de 2 s (1 s subiendo, 1 s bajando).
  Si la PSU no prendió, el LED parpadea rápido como indicador de fallo.
- Se avisa a la Pi con `MODE BYPASS` por D5/D7 antes de conmutar; mientras
  tanto `pi.py` ignora `POWEROFF_REQ` para no apagar un banco en
  configuración.
- Al salir: `BYPASS OFF` por USB, restauración del backend configurado y
  `MODE NORMAL` por D5/D7. Si la llave está en OFF, se pide apagado
  suave sin la supresión de 10 s del botón.
- Durante el bypass están bloqueados `CFG_SET` de `LINK_BACKEND`,
  `PIN_SERIAL_RX`, `PIN_SERIAL_TX` y `LINK_BAUD`: responden
  `CFG_ERR bypass active`.

## Claves de configuración

`CFG_VERSION = 4` es la versión del **esquema EEPROM**, no la del
controlador. Los valores se validan contra un rango; los marcados con
`reboot=1` sólo se aplican al reiniciar.

### Tiempos

| Clave | Default de fábrica | Rango |
|---|---|---|
| `ALIVE_TIMEOUT_MS` | 30000 ms (ver nota) | 1000–120000 |
| `QUIET_MAX_MS` | 600000 ms | 1000–600000 |
| `BOOT_CONFIRM_SUPPRESS_MS` | 60000 ms | 0–600000 |
| `BOOT_CONFIRM_CUTOFF_MS` | 60000 ms | 0–600000 |
| `SOFT_SHUTDOWN_LED_WINDOW_MS` | 180000 ms | 0–600000 |
| `PSU_OFF_SETTLE_DELAY_MS` | 5000 ms | 0–60000 |
| `MOMENTARY_SHORT_MS` | 500 ms | 50–10000 |
| `MOMENTARY_LONG_FORCE_MS` | 6000 ms | 100–60000 |
| `MOMENTARY_SOFT_SUPPRESS_MS` | 10000 ms | 0–300000 |
| `SETUP_LED_BOOT_ON_MS` | 2000 ms | 0–60000 |
| `LED_ENCODE_PULSE_MS` | 200 ms | 10–5000 |
| `LED_ENCODE_GAP_MS` | 1000 ms | 0–60000 |
| `DEBOUNCE_STABLE_MS` | 20 ms | 1–1000 |

**Nota importante:** el default compilado es 30000 ms, pero la EEPROM de
la ESP de este proyecto quedó grabada en **60000 ms** y fue verificada en
cinco arranques (`RX CFG_ACK ALIVE_TIMEOUT_MS=60000`). Ese margen es el
que evita la corrupción de la SD vista con 20 s y 30 s: con 60 s la Pi
termina de desmontar antes del corte.

### Enlace y hardware (`reboot=1`)

| Clave | Default | Valores |
|---|---|---|
| `LINK_BAUD` | 9600 | 1200–115200 |
| `LINK_BACKEND` | 0 | 0 = pines, 1 = USB |
| `BUTTON_TYPE` | 1 | 0 = momentáneo, 1 = llave/switch |
| `BUTTON_PIN` | 5 | 0–16 (ESP8266) / 0–48 (ESP32) |
| `PSU_ON_PIN` | 4 | ídem |
| `PIN_SERIAL_RX` | 14 | ídem |
| `PIN_SERIAL_TX` | 13 | ídem |
| `PIN_LED` | 2 | ídem |
| `PSU_ON_ACTIVE_HIGH` | 1 | 0/1 |
| `LED_LOGICAL_ON_GPIO_HIGH` | 0 | 0/1 |

## Máquina de estados

```
PSU OFF ── llave ON / pulsación ─▶ PSU ON (LED: esperando BOOT_OK)
                                 │
                                 ├─ BOOT_OK ⇒ vigilar ALIVE
                                 │    ├─ QUIET activo ⇒ NO cortar
                                 │    ├─ BYE / POWEROFF_REQ reciente
                                 │    │    ⇒ corte normal, LED 0
                                 │    └─ silencio > ALIVE_TIMEOUT_MS
                                 │         ⇒ corte, LED 2
                                 │
                                 └─ sin BOOT_OK en 60 + 60 s
                                      ⇒ corte, LED 1

BOTÓN FLASH ⇒ BYPASS (PSU ON fija, sin watchdog, link USB, fade 2 s)
              └─ otro pulsación ⇒ MODE NORMAL y vuelta al estado según llave
```

Códigos de LED con PSU OFF: `1` nunca bootó, `2` perdió `ALIVE`, `0`
apagado normal, `4` esperando `BOOT_OK` (sólo con PSU ON).

## Obligaciones del daemon en la Pi

1. Abrir el dispositivo serie con reintentos, modo raw y 9600 8N1.
2. Enviar `LOG_LEVEL`, luego `BOOT_OK` una sola vez, luego `ALIVE` cada
   500 ms.
3. Ante `POWEROFF_REQ` (y **no** estar en bypass): enviar
   `BYE reason=shutdown_req`, drenar la UART y ejecutar el apagado
   ordenado con `batocera-es-swissknife --shutdown`.
4. Ante `MODE BYPASS` / `MODE NORMAL`: actualizar `bypass_active`.
5. Ante `SIGTERM`/`SIGINT`: escribir la traza durable en
   `/userdata/system/pi_power.shutdown.log`, detectar reboot y enviar
   `QUIET seconds=180 reason=reboot` antes de `BYE`, o sólo `BYE` si es
   apagado. Drenar la UART antes de cerrar.
6. `LOG_LEVEL_DEFAULT = DEBUG` en esta copia; `DEBUG=1` o `--debug`
   fuerzan DEBUG. (El zip mencionaba `PI_POWER_LOG_LEVEL`, que **no**
   está implementado en v1.4.)

## Obligaciones del firmware

1. Leer EEPROM con CRC y versión; si falla, cargar defaults y grabarlos.
2. Filtrar líneas desconocidas, mantener el watchdog con tráfico válido.
3. Persistir **sólo** cuando llegue `CFG_SAVE` (o `CFG_RESET`).
4. Respetar el modo bypass sin escribir EEPROM ni dejar la PSU apagada.
5. Mantener el timeout persistido de 60 s para apagados limpios de SD.
