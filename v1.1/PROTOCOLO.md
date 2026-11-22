# Contrato Pi ↔ Teensy 3.0 — v1.1

Versión original, **en servicio en un arcade con Teensy 3.0**. No usa
comunicación serial: el intercambio son **dos líneas GPIO** más GND.
Cualquier cambio de `teensy_power.ino` o `RPi-power.py` debe respetar
este documento o actualizarlo primero.

Este contrato se escribió a partir del código de `v1.1/`. La estructura
(cableado, semántica de líneas, máquina de estados, obligaciones de cada
lado) toma como referencia los `PROTOCOLO.md` del zip `PSU PC para mis
arcades.zip`, adaptados a esta versión.

## Cableado

| Señal | Teensy 3.0 | Pi (header 40) | BCM | Dirección |
|---|---|---|---|---|
| `PS_ON` de la ATX | Pin 2 (vía transistor) | — | — | Teensy → PSU |
| Botón momentáneo | Pin 3 (`INPUT_PULLUP`) | — | — | Usuario → Teensy |
| Pedido de apagado | Pin 4 | Pin 11 | GPIO 17 | Teensy → Pi |
| Heartbeat "Pi viva" | Pin 5 (`INPUT_PULLDOWN`) | Pin 12 | GPIO 18 | Pi → Teensy |
| LED de estado | `LED_BUILTIN` (D13) | — | — | — |
| GND común | GND | Pin 6 (o cualquier GND) | — | — |

- Lógica 3,3 V en ambas líneas: conexión directa, sin level-shifter.
- La PSU se alimenta con +5 V standby (violeta) para que el control siga
  vivo con la fuente principal apagada. En v1.1 el ESP todavía no existe.

## Semántica de líneas

### Heartbeat — Pi GPIO18 → Teensy pin 5

- **HIGH** = el daemon de la Pi está vivo.
- **LOW** = la Pi no arrancó, el daemon bajó o se está apagando.
- No hay tren de despedida ("farewell") en v1.1: `RPi-power.py` simplemente
  pone la línea en LOW antes de ejecutar el shutdown y otra vez en el
  `finally` al salir.
- El firmware filtra ruido del arranque: considera el arranque confirmado
  recién después de **5 s de HIGH continuo** (`now - lastHeartbeatLowMs >
  5000`).
- Confirmado el arranque, si la línea queda LOW más de **15 s** se corta
  la PSU.

### Pedido de apagado — Teensy pin 4 → Pi GPIO17

- Un pulso HIGH de **2000 ms** (`SHUTDOWN_PULSE_MS`) indica a la Pi que
  debe apagarse.
- La Pi sondea la línea cada **500 ms** (`LOOP_SLEEP_S`).
- Antes de apagar, el daemon baja el heartbeat para que el Teensy no
  interprete el silencio posterior como fallo.

### Botón

| Acción | Duración | Efecto |
|---|---|---|
| Pulsación corta | ≥ 500 ms (`HOLD_TO_ON_MS`) | PSU OFF → enciende; PSU ON → pulso de shutdown (solo si pasó el gracia de 10 s) |
| Pulsación larga | ≥ 6000 ms (`FORCED_OFF_MS`) | Corte forzado de PSU, sin esperar a la Pi |
| Debounce | 20 ms | Se descartan lecturas inestables |

## Máquina de estados del Teensy

```
PSU OFF ── botón corto ─▶ PSU ON
                        │
                        ├─ HIGH en pin 5 ≥ 5 s ⇒ arranque confirmado
                        │    └─ LOW ≥ 15 s ⇒ corte PSU
                        │
                        └─ sin confirmación en 30 s ⇒ corte PSU
```

Tras un corte, `setPsu(false)` aplica un tiempo muerto de **5000 ms**
(`POST_SHUTDOWN_DELAY_MS`) durante el cual se ignoran botones.

## LED

| Estado del LED | Significado |
|---|---|
| Secuencia inicial (HIGH/LOW de 100 ms) | Arranque del firmware |
| Parpadeo rápido (~10 Hz) | Botón presionado |
| Encendido fijo | PSU ON |
| Apagado | PSU OFF |

En v1.1 **no hay códigos de error codificados** (1/2/4): eso aparece recién
en v1.2.

## Timeouts

| Constante | Valor | Uso |
|---|---|---|
| `HOLD_TO_ON_MS` | 500 ms | Pulsación corta |
| `FORCED_OFF_MS` | 6000 ms | Corte forzado |
| `SHUTDOWN_PULSE_MS` | 2000 ms | Pulso a GPIO17 |
| `RPI_BOOT_GRACE_MS` | 10000 ms | Gracia antes de permitir apagar con botón |
| `CONFIRM_DEATH_MS` | 3000 ms | Declarado pero **no usado** en el `loop` |
| `POST_SHUTDOWN_DELAY_MS` | 5000 ms | Tiempo muerto tras corte |
| Confirmación de arranque | 5000 ms | HIGH continuo en GPIO18 |
| Watchdog de arranque | 30000 ms | Sin confirmación ⇒ corte |
| Pérdida de heartbeat | 15000 ms | Tras arranque confirmado ⇒ corte |

## Obligaciones del daemon en la Pi

1. Abrir `gpiochip0` con `lgpio` y reintentar si hace falta.
2. Declarar GPIO18 como salida y dejarla **HIGH** apenas arranca.
3. Declarar GPIO17 como entrada y sondearla cada 500 ms.
4. Al ver HIGH en GPIO17: bajar heartbeat, ejecutar
   `batocera-es-swissknife --shutdown` (fallback: `shutdown -h now`).
5. En `KeyboardInterrupt` / salida: bajar heartbeat y cerrar el chip.

## Obligaciones del firmware

1. Mantener el pin de alive como entrada con `INPUT_PULLDOWN`.
2. No cortar la PSU antes de los 30 s de gracia para el arranque.
3. Respetar el pulso de 2 s en el pin de shutdown.
4. Usar el mismo `POST_SHUTDOWN_DELAY_MS` tras cada corte.
