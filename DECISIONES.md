# Decisiones del proyecto

Registro corto de las decisiones que no son obvias mirando el código.
Sirve para no repetir discusiones y para entender por qué algunas cosas están
como están.

## 2026-11-22 — Timeout de apagado: 60 s

- **Decisión**: `ALIVE_TIMEOUT_MS = 60000`, grabado en la EEPROM de la ESP.
- **Por qué**: con 30 s y con 20 s apareció corrupción en la ext4 de la SD
  (inodes y directorios dañados, índice HTREE de `roms/gb`) porque la ESP
  cortaba la energía antes de que la Pi terminara de desmontar. Con 60 s,
  cinco ciclos seguidos quedaron limpios.
- **Consecuencia**: las luces de la Pi dejan de parpadear antes de que se
  corte la PSU. Es esperable: el shutdown termina en ~3 s y el ESP usa el
  resto del margen para garantizar el desmontaje.
- **No hacer**: bajar el valor "para que apague más rápido".

## 2026-11-22 — Numeración de versiones

- **Decisión**: v1 = Teensy, v2 = prototipo ESP8266 por pines, v3 = firmware
  serial, v4 = configurable.
- **Por qué**: los zips originales tenían etiquetas internas contradictorias
  (`comunicacion/` se autodenomina v3, `comunicacion serial/` dice v2 UART).
  Se unificó la numeración para que el repo tenga una sola línea temporal.
- **Consecuencia**: `CFG_VERSION = 4` es el esquema de EEPROM y coincide
  con el número de versión del controlador, pero son conceptos distintos.

## 2026-11-22 — v1 no está deprecated

- **Decisión**: marcar `v1/` como "en servicio".
- **Por qué**: el usuario tiene un arcade funcionando con el Teensy 3.0 y
  ese software. No es código muerto.
- **Consecuencia**: cualquier cambio en v1 debe hacerse con cuidado y no
  asumir que se puede reemplazar por v4.

## 2026-11-22 — Diagrama compartido, copiado por versión

- **Decisión**: el mismo `diagrama_esp_rpi_psu.txt` está en v2, v3 y v4.
- **Por qué**: cada carpeta debe ser autocontenida para entender una
  versión sin tener que abrir otra. El cableado de fuente/ESP/botón/LED es
  idéntico; lo que cambia es el significado de D5/D7 en la Pi.

## 2026-11-22 — Un PROTOCOLO.md por versión

- **Decisión**: generar `PROTOCOLO.md` en las cuatro versiones.
- **Por qué**: la lógica cambia entre versiones (GPIO vs UART, roles de
  D5/D7 invertidos, `CFG_*`, bypass). Un único documento induciría a error.
- **Fuentes**: los `PROTOCOLO.md` de `PSU PC para mis arcades.zip` se usaron
  como plantilla y se adaptaron al código real. El de `comunicacion/`
  describe una variante de GPIO con "farewell" que **no** es v1 ni v2.

## 2026-11-22 — Bypass por FLASH, RAM only

- **Decisión**: el botón FLASH (GPIO0/D3) conmuta un modo de configuración
  que nunca se guarda en EEPROM.
- **Por qué**: permite cambiar/configurar sin que la PSU se apague por el
  watchdog, sin dejar un estado persistente que sorprenda al próximo boot.
- **Consecuencia**: la Pi recibe `MODE BYPASS` por D5/D7 antes de que el
  enlace conmute a USB e ignora `POWEROFF_REQ` mientras tanto. Los
  `CFG_SET` de backend/pines/baud se rechazan con `CFG_ERR bypass active`.

## 2026-11-22 — v1.1: v4 en Teensy 3.0

- **Decisión**: crear `v1.1/` con la lógica de v4 portada a Teensy 3.0,
  sin tocar `v1/` (sigue en servicio en el arcade original).
- **Por qué**: el usuario quiere el mismo protocolo y el mismo margen de
  apagado en hardware Teensy, manteniendo la línea v1 intacta.
- **Serial**: se usa `Serial1`, UART por hardware en pines **0 (RX) y
  1 (TX)**. Estaban libres en v1 (que usaba 2, 3, 4, 5 y 13), no requieren
  mover el botón ni el transistor y no dependen de SoftwareSerial. En la
  Pi se mantienen los pines de v4: BCM4 TX (pin 7) y BCM5 RX (pin 29).
  Alternativas descartadas: `Serial3` (pines 7/8) y `Serial2` (9/10),
  que ocupan pines útiles y no aportan ventaja.
- **Bypass**: pin **6** con `INPUT_PULLUP`, activo LOW, porque el Teensy
  3.0 no tiene botón FLASH. Es configurable con `BYPASS_PIN`.
- **Timeout**: el default compilado es 60000 ms (en v4 era 30000 y se
  dependía de un valor grabado a mano). Así no hay riesgo de volver a
  corromper la SD si se graba un `CFG_RESET`.
- **EEPROM**: emulada por Teensyduino (`E2END=0x7FF`, 2048 B), sin
  `begin()`/`commit()`. El `CFG_MAGIC` cambia (`0x54454331`) para que una
  configuración de v4 no se interprete como válida en otra placa.
- **Estado**: compila con `teensy:avr:teensy30`. No se flasheó ni se
  probó en hardware; `teensy-loader-cli` no está instalado.

## 2026-11-22 — EEPROM con CRC y guardado explícito

- **Decisión**: los cambios de `CFG_SET` viven en RAM hasta `CFG_SAVE`.
- **Por qué**: evitar grabados accidentales y permitir probar valores antes
  de hacerlos permanentes. El CRC y `CFG_VERSION` detectan datos inválidos
  y cargan defaults.
- **Consecuencia**: el default compilado de `ALIVE_TIMEOUT_MS` sigue siendo
  30000, pero la EEPROM de la ESP de este proyecto quedó en 60000.
