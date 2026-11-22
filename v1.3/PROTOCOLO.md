# Contrato Pi ↔ ESP vía UART — v1.3

Reemplaza el esquema de 2 GPIOs (heartbeat + shutdown) por **serie
asíncrono** con protocolo de texto. Cualquier cambio de código
debe respetar este archivo o actualizarlo primero.

Documento adaptado del `PROTOCOLO.md` incluido en `comunicacion serial/`
del zip `PSU PC para mis arcades.zip` (que lo rotula "v1.2"). Corresponde a
este `v1.3/` porque sus constantes coinciden con `esp8266.ino`:
`ALIVE_TIMEOUT_MS = 5000`, `BOOT_CONFIRM_SUPPRESS_MS = 60000`,
`BOOT_CONFIRM_CUTOFF_MS = 60000`, `QUIET_MAX_MS = 600000`. Las diferencias
respecto del original se marcan abajo. La v1.4 tiene su propio contrato.

## Cableado

| Señal | Pi (header 40) | BCM | ESP (NodeMCU) | Dirección |
|-------|----------------|-----|---------------|-----------|
| TX Pi → RX ESP | Pin 7 | GPIO 4 (UART3_TX) | D5 (SoftwareSerial RX) | Pi → ESP |
| RX Pi ← TX ESP | Pin 29 | GPIO 5 (UART3_RX) | D7 (SoftwareSerial TX) | ESP → Pi |
| GND común | Pin 6 (o cualquier GND) | — | GND | — |

- Ambos lados son **3,3 V**: conexión directa, sin level-shifter.
- ESP sobre +5V standby ATX (violeta): sigue vivo con PSU principal OFF.
- **No** se usa BCM4 ni el esquema anterior. El cable de pin 7 del
  layout viejo puede retirarse.

## Parámetros de la capa física

- **Baud**: 9600, 8N1, sin control de flujo.
- Terminador de mensaje: `\n` (LF). `\r` se ignora.
- Codificación: ASCII, una línea por mensaje, sin espacios iniciales.
- Longitud máxima de línea: 96 caracteres (`RX_LINE_MAX`). Líneas más
  largas se descartan silenciosamente.
- **Robustez al ruido**: el ESP solo mueve estado (actualiza el
  watchdog de `ALIVE`, cancela ventanas `QUIET`, aplica `LOG_LEVEL`,
  etc.) en respuesta a líneas cuyo **prefijo coincide con un
  mensaje conocido** (`BOOT_OK`, `ALIVE`, `QUIET`, `BYE`, `PONG`,
  `LOG_LEVEL`). Cualquier otra línea se descarta sin efecto — así
  un glitch en la UART no puede mantener vivo el heartbeat ni
  cancelar una ventana `QUIET` por accidente.

## Configuración imprescindible en la Pi (Batocera)

1. `/boot/config.txt`: agregar `enable_uart=1`. En Pi 4 usar
   **`dtoverlay=miniuart-bt`** para mantener Bluetooth y habilitar
   **`dtoverlay=uart3`** para exponer UART3 en GPIO4/5 (pins 7/29).
   Solo si **no** necesitas Bluetooth, puedes usar
   `dtoverlay=disable-bt` (mas simple, pero apaga el BT).
2. `/boot/cmdline.txt`: **quitar** cualquier
   `console=serial0,115200` o equivalente (si no, el kernel escupe
   mensajes por TX al bootear y confunde al ESP).
3. Reiniciar. Confirmar con `ls -l /dev/ttyAMA4` o `/dev/ttyAMA3`,
   según la build de Batocera (el `pi.py` del zip usaba `ttyAMA3`;
   la copia de este repo usa `/dev/ttyAMA4` por defecto). En cualquier
   caso se puede forzar con `PI_SERIAL_DEVICE`.

## Protocolo (mensajes de texto terminados en `\n`)

### Pi → ESP

| Mensaje | Cuándo | Semántica |
|---------|--------|-----------|
| `LOG_LEVEL <LVL>` | apenas abre el puerto y tras recibir `HELLO` | fija el nivel que debe usar el ESP para filtrar sus `LOG` antes de enviarlos. `LVL` ∈ `OFF\|ERROR\|WARNING\|INFO\|DEBUG`. |
| `BOOT_OK` | una sola vez, tras inicializar | confirma arranque (quita el estado "esperando Pi") |
| `ALIVE seq=<n> uptime=<s>` | cada `ALIVE_PERIOD_MS` (~500 ms) | heartbeat: la Pi está viva |
| `QUIET seconds=<n> reason=<reboot\|maintenance\|...>` | antes de una ventana deliberada sin `ALIVE` (p. ej. reinicio, actualización) | "voy a estar hasta `n` s sin hablar; no cortes la PSU por silencio hasta entonces". Cualquier **mensaje conocido** posterior que no sea `BYE` (p. ej. `ALIVE`, `BOOT_OK`, `LOG_LEVEL`, `PONG`) cancela la ventana. Enviar `QUIET seconds=0 reason=cancel` para cancelarla explícitamente. El ESP aplica un tope duro (`QUIET_MAX_MS`). |
| `BYE reason=<poweroff\|reboot\|shutdown_req\|sigint>` | justo antes de apagar/reiniciar | despedida ordenada: el próximo silencio **no** es error (si antes vino `QUIET reason=reboot`, la PSU se mantiene hasta expirar esa ventana). |
| `PONG` | respuesta a `PING` | opcional, diagnóstico |

### ESP → Pi

| Mensaje | Cuándo | Semántica |
|---------|--------|-----------|
| `POWEROFF_REQ` | botón corto con PSU ON y suppress vencido | pide apagado ordenado a la Pi |
| `PING` | manual / diagnóstico | la Pi debería responder `PONG` |
| `HELLO fw=<ver>` | al arranque del ESP | identifica firmware (informativo) |
| `LOG <LVL> <texto>` | en cambios de estado relevantes | la Pi lo reinyecta en su propio logger con prefijo `[ESP]`; la visibilidad real la decide el logger de la Pi. Emitido sólo si `LVL >= log_level_actual` del ESP (filtro en origen para no saturar la línea). |
| `LOG_LEVEL_ACK <LVL>` | tras aceptar un `LOG_LEVEL` | eco informativo del nivel aplicado. |

### Niveles de log

Mismos nombres que `logging` de Python. Orden de menor a mayor:
`DEBUG < INFO < WARNING < ERROR < OFF`. `OFF` inhibe por
completo los `LOG` en el ESP (no así `POWEROFF_REQ`, `HELLO`,
`LOG_LEVEL_ACK`, etc., que son control, no log).

## Máquina de estados del ESP (resumen)

```
PSU OFF ── botón corto ─▶ PSU ON
                       │
                       ├─ esperar BOOT_OK hasta SUPPRESS + CUTOFF (60 s + 60 s)
                       │    sin BOOT_OK  ⇒ corte PSU, LED codigo 1
                       │
                       ├─ confirmado: vigilar ALIVE
                       │    silencio > ALIVE_TIMEOUT_MS  ⇒ corte PSU
                       │      • si hay ventana QUIET activa  ⇒ NO cortar PSU
                       │        (se acaba al vencer QUIET o al llegar una
                       │        linea distinta de BYE/QUIET)
                       │      • si llegó BYE dentro de
                       │        SOFT_SHUTDOWN_NORMAL_LED_WINDOW_MS (sin QUIET)
                       │        o ESP envió POWEROFF_REQ en esa ventana
                       │        ⇒ LED 0 y corte PSU (apagado normal)
                       │      • sino ⇒ LED codigo 2 (heartbeat perdido)
                       │
                       └─ botón largo ⇒ corte forzado, LED apagado
```

Códigos visibles en el LED con PSU OFF (codificados por el ESP):

| Código N | Significado |
|----------|-------------|
| 0 | LED apagado: corte "normal" (usuario/botón, o BYE recibido, o POWEROFF_REQ reciente). |
| 1 | Nunca se recibió `BOOT_OK` a tiempo. |
| 2 | Se perdió `ALIVE` tras BOOT_OK. |
| 4 (solo con PSU ON) | Esperando `BOOT_OK`. |
| LED fijo encendido (PSU ON) | Operación normal. |

## Timeouts sugeridos

| Nombre | Valor | Notas |
|--------|-------|-------|
| `ALIVE_PERIOD_MS` (Pi) | 500 ms | frecuencia de ALIVE |
| `ALIVE_TIMEOUT_MS` (ESP) | 5000 ms | silencio que dispara corte |
| `BOOT_CONFIRM_SUPPRESS_MS` + `BOOT_CONFIRM_CUTOFF_MS` (ESP) | 60 s + 60 s = 120 s | desde PSU ON hasta cortar si no llegó `BOOT_OK` |
| `SOFT_SHUTDOWN_NORMAL_LED_WINDOW_MS` (ESP) | 180 s | ventana tras BYE / POWEROFF_REQ para considerar "normal" |
| `QUIET_MAX_MS` (ESP) | 600 s | tope duro de la ventana `QUIET` |
| `QUIET_SECONDS_REBOOT` (Pi) | 180 s | ventana que la Pi anuncia antes de reiniciar (cubre boot + fsck) |

## Obligaciones del daemon en la Pi

1. Abrir `/dev/ttyAMA4` (UART3 en esta build) a 9600 8N1 y ponerlo en
   modo raw (o el dispositivo indicado en `PI_SERIAL_DEVICE`).
2. Apenas esté listo: enviar **`LOG_LEVEL <lvl>\n`** con el nivel
   resuelto (`LOG_LEVEL_DEFAULT` en `pi.py`, salvo `--debug` /
   `DEBUG=1`). Luego enviar **`BOOT_OK\n`** (una sola vez).
3. Enviar `ALIVE seq=N uptime=S\n` cada ~500 ms.
4. Escuchar líneas entrantes:
   - `POWEROFF_REQ` → `BYE reason=shutdown_req` + apagado ordenado.
   - `LOG <lvl> <texto>` → reinyectar por el logger con prefijo
     `[ESP]` al nivel indicado.
   - `HELLO` → informativo; reemitir `LOG_LEVEL` por si el ESP
     reinició sin haber recibido todavía el nivel.
5. Ante `SIGTERM`/`SIGINT` (p. ej. cierre del proceso en apagado o
   reinicio del sistema): detectar si es **reinicio** (p. ej.
   `reboot.target` activo en `systemctl list-jobs` o indicios bajo
   `/run/systemd/shutdown/`). Si es reinicio:
   - Enviar `QUIET seconds=<QUIET_SECONDS_REBOOT> reason=reboot\n`.
   - Enviar `BYE reason=reboot\n`, flush, cerrar.
   Si es apagado normal: solo `BYE reason=poweroff\n` (o `sigint`).
6. No asumir que el puerto está listo al instante: reintentar abrir.

## Obligaciones del firmware del ESP

1. `SoftwareSerial(D5, D7)` a 9600 bps; buffer de línea ≤ 96 chars.
2. Parsear a nivel de línea; descartar con silencio lo desconocido.
3. Mantener los mismos códigos de LED y tiempos de botón de v1.1.
4. Sólo mandar `POWEROFF_REQ` si está habilitado (tras
   `MOMENTARY_BUTTON_SOFT_SHUTDOWN_SUPPRESS_MS`) y PSU ON.
5. Ante botón largo: cortar PSU **sin** depender del serie.
6. Mantener un `espLogLevel` local (default `INFO`) y **filtrar en
   origen**: un `LOG <lvl> ...` se emite sólo si `lvl >=
   espLogLevel`. Actualizar ante `LOG_LEVEL <lvl>` de la Pi y
   responder `LOG_LEVEL_ACK <lvl>`.
7. Emitir `LOG` únicamente en **cambios de estado** (no en cada
   vuelta del `loop`) para no saturar el enlace a 9600 bps.
