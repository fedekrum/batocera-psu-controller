# Contrato Pi ↔ ESP8266 por GPIO — v1.2

Prototipo en ESP8266 NodeMCU LoLin v3 con la misma idea de la v1.1: **dos
líneas GPIO, sin serial**. Este documento se escribió a partir de
`esp_power.ino`, `esp_power2.ino` y `RPi-power.py` de esta carpeta.

El `PROTOCOLO.md` de `comunicacion/` dentro del zip `PSU PC para mis
arcades.zip` se usó sólo como **plantilla de estructura**. Cuidado: ese
documento describe una variante posterior con los roles de D5/D7
**invertidos** y tren "farewell", que no es este firmware. No copiar sus
valores acá.

## Cableado

| Señal | ESP8266 | Pi (header 40) | BCM | Dirección |
|---|---|---|---|---|
| Botón | D1 | — | — | Usuario → ESP |
| `PS_ON` de la ATX | D2 (vía transistor) | — | — | ESP → PSU |
| LED de estado | D4 (activo LOW en LoLin v1.3) | — | — | — |
| Pedido de apagado | **D5** | Pin 7 | GPIO 4 | ESP → Pi |
| Heartbeat "Pi viva" | **D7** | Pin 8 | GPIO 14 | Pi → ESP |
| GND común | GND | Pin 6 (o cualquier GND) | — | — |

- Lógica 3,3 V directa entre Pi y ESP.
- El ESP se alimenta desde +5 V standby ATX (violeta).
- En la Pi, GPIO4 se declara con `SET_PULL_DOWN` para evitar apagados
  falsos por ruido.

## Semántica de líneas

### Heartbeat — Pi GPIO14 → ESP D7

- **HIGH** = daemon vivo; el firmware actualiza `lastAliveTimeMs` y pasa
  el LED a sólido (`255`).
- **LOW** = Pi caída o en apagado. No hay detector de "farewell": en v1.2
  no existen los pulsos rápidos de despedida.
- En modo momentáneo: tras arranque confirmado, 15 s sin heartbeat ⇒
  corte con código de error `2`.
- En modo switch: tras arranque confirmado, 4 s sin heartbeat ⇒ corte.
- El daemon baja el heartbeat **antes** de ejecutar el shutdown cuando
  el pedido vino por GPIO4.

### Pedido de apagado — ESP D5 → Pi GPIO4

- Pulso HIGH de **2000 ms** (`SHUTDOWN_PULSE_MS`).
- La Pi lo sondea cada 500 ms y, al detectarlo, baja GPIO14 y ejecuta
  `batocera-es-swissknife --shutdown` (fallback `shutdown -h now`).
- El botón/shutdown suave sólo está permitido después de
  `RPI_BOOT_GRACE_MS = 10000` desde el encendido. Antes de eso, si el
  switch se apaga, el firmware corta la PSU directamente.

## Modos del firmware

Hay dos firmwares casi idénticos en esta carpeta:

| Archivo | Modo | Comportamiento |
|---|---|---|
| `esp_power.ino` | "híbrido" (`MOMENTARY_BUTTON = false` por defecto) | Con el define en `false`, switch puro: flanco del botón enciende/apaga. Cambiándolo a `true`, lógica Teensy completa con pulsación larga. |
| `esp_power2.ino` | switch permanente | Igual lógica, con constantes `CONFIRM_DEATH_MS` y `POST_SHUTDOWN_DELAY_MS` declaradas (no usadas en el `loop` de `esp_power.ino`). |

Además, en modo momentáneo: pulsación ≥ 500 ms conmuta, pulsación
≥ 6000 ms corte forzado y watchdog de arranque de 60 s.

## Máquina de estados (resumen)

```
PSU OFF ── flanco/pulso del botón ─▶ PSU ON (LED código 1)
                                  │
                                  ├─ HIGH en D7 ⇒ arranque confirmado
                                  │    LED sólido (255)
                                  │    └─ heartbeat perdido ⇒ corte (LED 2)
                                  │
                                  └─ sin arranque en 60 s (momentáneo)
                                       ⇒ corte (LED 2)
```

En modo switch no hay watchdog de "nunca arrancó": si no hay heartbeat,
simplemente no se marca `rpiHasBooted` y no se corta por ese motivo.

## Códigos de LED

Ráfaga de pulsos de **200 ms** y pausa de **1 s**; N es el número de
pulsos por ráfaga (`setLedCode`).

| Código | Significado |
|---|---|
| `0` | PSU OFF (LED apagado) |
| `1` | Esperando arranque de la Pi (1 pulso) |
| `2` | Error: heartbeat perdido / timeout (2 pulsos) |
| `3` | Apagado en curso, pulso enviado en D5 (3 pulsos) |
| `255` | Funcionamiento normal (LED fijo encendido) |

## Timeouts

| Constante | Valor | Uso |
|---|---|---|
| `HOLD_TO_ON_MS` | 500 ms | Pulsación corta |
| `FORCED_OFF_MS` | 6000 ms | Corte forzado |
| `SHUTDOWN_PULSE_MS` | 2000 ms | Pulso en D5 |
| `RPI_BOOT_GRACE_MS` | 10000 ms | Gracia antes de aceptar shutdown por botón |
| Pérdida de heartbeat (momentáneo) | 15000 ms | Tras arranque confirmado |
| Sin arranque (momentáneo) | 60000 ms | Corte con LED 2 |
| Pérdida de heartbeat (switch) | 4000 ms | Tras arranque confirmado |

## Obligaciones del daemon en la Pi

1. Abrir `gpiochip0` con `lgpio`.
2. GPIO14 como salida, HIGH inmediato; GPIO4 como entrada con pull-down.
3. Sondear GPIO4 cada 500 ms.
4. Al detectar HIGH: bajar GPIO14, ejecutar shutdown de Batocera, salir.
5. Escribir eventos en `/userdata/system/rpi_power.log`.
6. Al salir: bajar heartbeat y cerrar el chip.

## Obligaciones del firmware

1. D5 como salida en LOW en reposo; D7 como entrada.
2. No asumir que un solo LOW significa "Pi muerta": esperar el tiempo de
   la tabla según el modo.
3. Enviar el pulso de 2 s antes de cualquier corte suave.
4. Mantener el idioma de LED de esta tabla (distinto del de v1.1 y v1.3).
