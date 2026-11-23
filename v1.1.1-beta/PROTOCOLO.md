# Contrato Pi ↔ Teensy 3.0 — v1.1.1-beta

Versión 1.1.1 beta: **v1.4 portada a Teensy 3.0**. Mantiene el protocolo de texto
de v1.3/v1.4, la configuración persistente en EEPROM (emulada por
Teensyduino), los comandos `CFG_*`, el timeout de 60 s y el modo bypass.
El bypass usa un pin dedicado porque el Teensy 3.0 no tiene botón FLASH.
Cualquier cambio en `teensy_cfg/` o `system/pi.py` debe respetar este
documento o actualizarlo primero.

Este contrato deriva de `v1.4/PROTOCOLO.md`, con las adaptaciones reales
del firmware `teensy_cfg.ino`. Conserva la misma semántica de mensajes:
un `pi.py` de v1.4 funciona sin cambios en esta versión.

## Cableado

| Señal | Teensy 3.0 (default) | Pi (header 40) | Dirección |
|---|---|---|---|
| Botón / llave | Pin 3 (`INPUT_PULLUP`) | — | Usuario → Teensy |
| `PS_ON` de la ATX | Pin 2 (vía transistor) | — | Teensy → PSU |
| LED de estado | Pin 13 (LED builtin, activo HIGH) | — | — |
| RX del enlace | **Pin 0** (`Serial1 RX`) | TX: pin 3, BCM8 (UART4_TX) | Pi → Teensy |
| TX del enlace | **Pin 1** (`Serial1 TX`) | RX: pin 5, BCM9 (UART4_RX) | Teensy → Pi |
| Bypass | Pin 6 (`INPUT_PULLUP`, activo LOW) | — | Usuario → Teensy |
| Alimentación | VIN desde +5 V standby (violeta) | — | — |
| GND común | GND | Pin 6 (o cualquier GND) | — |

- **El Teensy 3.0 no tolera 5 V**: todas las señales son 3,3 V, conexión
  directa sin level-shifter. La Pi 5 también es 3,3 V.
- Pines 0/1 son UART por hardware (`Serial1`), **fijos en silicio**.
  En la Pi 5 los pines del enlace son 32/33 (GPIO12/13) con `uart4-pi5`.
  `PIN_SERIAL_RX` y `PIN_SERIAL_TX` sólo aceptan 0 y 1 respectivamente;
  otros valores responden `CFG_ERR fixed pin`.
- **Backend 0** (`LINK_BACKEND=0`, default): `Serial1` en pines 0/1, el
  enlace con la Pi.
- **Backend 1** (`LINK_BACKEND=1`): `Serial` por USB.
- Ambos links se inicializan siempre: USB es el enlace del modo bypass.
- Los otros pines (botón, PSU, LED, bypass) sí son configurables por
  `CFG_SET` y se aplican al reiniciar.
- Las líneas de v1.1 (GPIO18 heartbeat y GPIO17 shutdown) **ya no se usan**:
  se desconectan y se tienden las dos del UART4 en la Pi.

## Capa física y robustez

- 9600 8N1 por defecto (`LINK_BAUD`), sin control de flujo.
- Una línea por mensaje, terminada en `\n`; se ignora `\r`; ASCII.
- Buffer de línea de 96 caracteres (`RX_LINE_MAX`).
- El Teensy sólo reacciona a líneas con prefijo conocido (`BOOT_OK`, `ALIVE`,
  `QUIET`, `BYE`, `PONG`, `LOG_LEVEL`, `CFG*`). El ruido no puede mantener
  vivo el watchdog ni cancelar un `QUIET`.
- Cualquier línea `CFG*` cuenta como tráfico válido y refresca el
  watchdog de `ALIVE`.

## Configuración en la Pi (Batocera)

1. En `/boot/config.txt`: `enable_uart=1` y, según la Pi:
   - Pi 4: `dtoverlay=uart4` (GPIO8/9, pines 3/5). Se puede mantener
     `dtoverlay=miniuart-bt` para conservar Bluetooth; no usar
     `disable-bt` si se usan mandos BT.
   - Pi 5: `dtoverlay=uart4-pi5` (GPIO12/13, pines 32/33). No existe
     `miniuart-bt` en Pi 5.
2. No hace falta quitar `console=serial0,115200` de `/boot/cmdline.txt`:
   esa consola usa GPIO14/15 (o el header de debug en Pi 5), no este enlace.
3. Reiniciar y verificar que exista `/dev/ttyAMA4`. `pi.py` lo usa por
   defecto en ambas Pi; se puede forzar con `PI_SERIAL_DEVICE`.
4. Instalar `system/pi.py` en `/userdata/system/pi.py` y el launcher
   `system/custom_service` en `/userdata/system/services/custom_service`,
   ambos ejecutables. El log queda en `/userdata/system/pi_power.log`.

## Mensajes

### Pi → Teensy

| Mensaje | Cuándo | Semántica |
|---|---|---|
| `LOG_LEVEL <LVL>` | al abrir el puerto y al recibir `HELLO` | Fija el filtro de `LOG` del Teensy. `LVL` ∈ `OFF\|ERROR\|WARNING\|INFO\|DEBUG`. |
| `BOOT_OK` | una vez, tras inicializar | Confirma arranque; sale del estado "esperando Pi". |
| `ALIVE seq=<n> uptime=<s>` | cada 500 ms | Heartbeat. |
| `QUIET seconds=<n> reason=<...>` | antes de una ventana deliberada sin `ALIVE` | "voy a callarme hasta `n` s"; no cortar PSU por silencio. Cualquier mensaje conocido distinto de `BYE`/`QUIET` cancela la ventana. Tope duro: `QUIET_MAX_MS`. |
| `BYE reason=<poweroff\|reboot\|shutdown_req\|sigint>` | justo antes de apagar/reiniciar | Despedida ordenada: el silencio posterior no es error. |
| `PONG` | respuesta a `PING` | Diagnóstico. |
| `CFG` | cuando se quiere volcar la configuración | El Teensy responde una línea `CFG <key>=<valor>` por clave. |
| `CFG_GET <key>` | consulta puntual | Responde `CFG <key>=<valor>` o `CFG_ERR unknown key`. |
| `CFG_SET <key>=<value>` | cambiar un valor en RAM | Responde `CFG_ACK <key>=<value>` (más ` reboot=1` si aplica) o `CFG_ERR ...`. **No persiste** hasta `CFG_SAVE`. |
| `CFG_SAVE` | grabar la RAM en EEPROM | Responde `CFG_SAVED`. |
| `CFG_LOAD` | recargar desde EEPROM | Responde `CFG_LOADED` o `CFG_ERR load failed`. |
| `CFG_RESET` | restaurar defaults de fábrica en RAM y EEPROM | Responde `CFG_DEFAULTS`. |

### Teensy → Pi

| Mensaje | Cuándo | Semántica |
|---|---|---|
| `POWEROFF_REQ` | botón/shutdown suave con PSU ON y suppress vencido | Pide apagado ordenado. **Se ignora en la Pi mientras esté activo el bypass.** |
| `PING` | diagnóstico | La Pi debe responder `PONG`. |
| `HELLO fw=<ver>` | arranque del Teensy | Identificación; la Pi reenvía `LOG_LEVEL`. |
| `LOG <LVL> <texto>` | cambios de estado | La Pi lo reinyecta en su logger con prefijo `[controlador]`. Filtrado en origen según el nivel activo. |
| `LOG_LEVEL_ACK <LVL>` | tras aceptar `LOG_LEVEL` | Eco del nivel aplicado. |
| `MODE BYPASS` | al entrar al bypass | Se envía por el enlace de pines **antes** de conmutar a USB. La Pi marca `bypass_active=True` e ignora `POWEROFF_REQ`. |
| `MODE NORMAL` | al salir del bypass | Se envía por el enlace de pines aunque el backend configurado sea USB. La Pi reanuda el apagado normal. |
| `CFG <key>=<valor>` | respuesta a `CFG` / `CFG_GET` | Una línea por clave. |
| `CFG_ACK`, `CFG_SAVED`, `CFG_LOADED`, `CFG_DEFAULTS`, `CFG_ERR ...` | respuestas `CFG_*` | Ver tabla anterior. |

## Modo bypass (pin 6)

- Toggle por pulsación en el pin configurado con `BYPASS_PIN` (default 6).
  Es RAM only: **nunca se guarda en EEPROM**.
- Al entrar: PSU forzada en ON, watchdog desactivado, enlace conmutado a
  USB (`Serial`) y LED con fade continuo de 2 s (1 s subiendo, 1 s bajando).
  Si la PSU no prendió, el LED parpadea rápido como indicador de fallo.
- Se avisa a la Pi con `MODE BYPASS` por `Serial1` (pines 0/1) antes de
  conmutar; mientras tanto `pi.py` ignora `POWEROFF_REQ`.
- Al salir: `BYPASS OFF` por USB, restauración del backend configurado y
  `MODE NORMAL` por `Serial1`. Si la llave está en OFF, se pide apagado
  suave sin la supresión de 10 s del botón.
- Durante el bypass están bloqueados `CFG_SET` de `LINK_BACKEND`,
  `PIN_SERIAL_RX`, `PIN_SERIAL_TX`, `LINK_BAUD` y `BYPASS_PIN`: responden
  `CFG_ERR bypass active`.

## Claves de configuración

`CFG_VERSION = 4` es la versión del **esquema EEPROM**, no la del
controlador (1.1). El `CFG_MAGIC` de v1.1.1-beta es distinto del de v1.4 porque la
EEPROM es otro hardware y los pines no son intercambiables. Los valores se
validan contra un rango; los marcados con `reboot=1` sólo se aplican al
reiniciar.

### Tiempos

| Clave | Default de fábrica | Rango |
|---|---|---|
| `ALIVE_TIMEOUT_MS` | 60000 ms | 1000–120000 |
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

**Nota importante:** a diferencia de v1.4 (cuyo default compilado es 30000 y
dependía de un valor grabado a mano), v1.1.1-beta nace con **60000 ms** en los
defaults y los escribe en EEPROM en el primer arranque. Ese margen es el
que evita la corrupción de la SD vista con 20 s y 30 s: con 60 s la Pi
termina de desmontar antes del corte. No bajarlo.

### Enlace y hardware (`reboot=1`)

| Clave | Default | Valores |
|---|---|---|
| `LINK_BAUD` | 9600 | 1200–115200 |
| `LINK_BACKEND` | 0 | 0 = pines, 1 = USB |
| `BUTTON_TYPE` | 1 | 0 = momentáneo, 1 = llave/switch |
| `BUTTON_PIN` | 3 | 0–33 |
| `PSU_ON_PIN` | 2 | 0–33 |
| `PIN_SERIAL_RX` | 0 | **sólo 0** (`CFG_ERR fixed pin`) |
| `PIN_SERIAL_TX` | 1 | **sólo 1** (`CFG_ERR fixed pin`) |
| `PIN_LED` | 13 | 0–33 |
| `BYPASS_PIN` | 6 | 0–33 |
| `PSU_ON_ACTIVE_HIGH` | 1 | 0/1 |
| `LED_LOGICAL_ON_GPIO_HIGH` | 1 | 0/1 |

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

PIN 6 (bypass) ⇒ BYPASS (PSU ON fija, sin watchdog, link USB, fade 2 s)
                 └─ otra pulsación ⇒ MODE NORMAL y vuelta al estado según llave
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
   está implementado en v1.1.1-beta.)

## Obligaciones del firmware

1. Leer EEPROM con CRC y versión; si falla, cargar defaults y grabarlos.
2. Filtrar líneas desconocidas, mantener el watchdog con tráfico válido.
3. Persistir **sólo** cuando llegue `CFG_SAVE` (o `CFG_RESET`).
4. Respetar el modo bypass sin escribir EEPROM ni dejar la PSU apagada.
5. Mantener el timeout persistido de 60 s para apagados limpios de SD.
6. Usar `Serial1` (pines 0/1) como enlace de pines y `Serial` (USB) como
   enlace de bypass; no intentar remapear los pines de `Serial1`.

## Verificación

El firmware compila con:

```sh
arduino-cli compile --fqbn teensy:avr:teensy30 \
  --build-path /tmp/opencode/build-teensy30 v1.1.1-beta/teensy_cfg
```

Compilado el 2026-11-22 con Teensy 3.0 / Teensyduino 1.62.0:
54984 bytes de flash y 5264 bytes de RAM. **No probado en hardware**:
la EEPROM, el watchdog y el bypass requieren una placa real.
