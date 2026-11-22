# AGENTS.md — contexto para agentes e IA

Leer este archivo **antes** de tocar cualquier cosa. Es el índice corto del
proyecto. Los detalles están en los `PROTOCOLO.md` de cada versión y en
`DECISIONES.md`.

## 1. Qué es este proyecto

Controlador casero para **arcades con Raspberry Pi + Batocera**. Un
microcontrolador queda siempre alimentado desde el **+5 V standby (violeta)**
de una fuente ATX y hace tres cosas:

1. Encender/apagar la PSU principal actuando sobre **PS_ON** mediante un
   transistor.
2. Pedir el apagado ordenado de la Pi cuando se apaga la llave/botón.
3. Vigilar que la Pi siga viva (heartbeat serial) y cortar la PSU por
   watchdog si se pierde, con margen para que la SD termine de desmontar.

Hay tres líneas de desarrollo:

- **v1**: Teensy 3.0 con dos GPIO. **Sigue en producción** en un arcade del
  usuario. No está deprecated.
- **v1.1**: la misma lógica de v4 en Teensy 3.0. Compila, **sin probar en
  hardware**. No reemplaza a v1.
- **v4**: firmware configurable con ESP8266/ESP32. Es la línea activa sobre
  ESP.

v2 y v3 son históricas/deprecated, pero se conservan porque explican la
evolución y porque v3 es la fuente del generador de v4.

## 2. Mapa de versiones

| Dir | Estado | Micro | Comunicación con la Pi | Archivo clave |
|---|---|---|---|---|
| `v1/` | **en servicio** | Teensy 3.0 | 2 GPIO: GPIO18 heartbeat, GPIO17 shutdown | `teensy_power.ino`, `RPi-power.py`, `PROTOCOLO.md` |
| `v1.1/` | compilada, sin probar | Teensy 3.0 | Serial1 pins 0/1 (UART3 en la Pi), bypass pin 6 | `teensy_cfg/`, `system/`, `PROTOCOLO.md` |
| `v2/` | deprecated | ESP8266 | 2 GPIO: GPIO14 → D7 alive, D5 → GPIO4 shutdown | `esp_power.ino`, `esp_power2.ino`, `PROTOCOLO.md` |
| `v3/` | deprecated, **fuente** | ESP8266 | UART 9600, `BOOT_OK`/`ALIVE`/`BYE` | `esp8266.ino`, `pi.py`, `PROTOCOLO.md` |
| `v4/` | **actual** | ESP8266 / ESP32 / S2 / S3 | UART 9600 + `CFG_*` + bypass | `esp8266_cfg/`, `system/`, `PROTOCOLO.md` |

Cada carpeta tiene su propio `PROTOCOLO.md` y su diagrama; no asumir que
el protocolo de una versión aplica a otra (los roles de D5/D7 cambian).

## 3. Fuentes de verdad y reglas duras

- **`v3/esp8266.ino` y `v3/pi.py` son originales históricos.** No editar.
  El firmware v3 se usa como entrada del generador de v4; su hash debe
  permanecer `9f3927e16025241a8c1923464cf5a8136d0e77de2ab7a0d63c92f50776801079`.
- **Los archivos en `v4/esp8266_cfg/` son generados.** No editarlos a mano:
  cambiar `v4/tools/make_cfg_firmware.py`, regenerar y copiar el resultado.
  El generador escribe tanto `esp8266_cfg.ino` como `cfg_types.h`.
- **No bajar el timeout de 60 s.** Con 20 s y 30 s se corrompió la SD porque
  la ESP cortaba antes de que la Pi terminara de desmontar. El valor
  persistido en la EEPROM es `ALIVE_TIMEOUT_MS=60000`.
- **No reflashear hardware ni modificar la SD sin pedido explícito.** La SD
  estuvo corrupta y fue reparada; el usuario decide cuándo se intervene.
- **v1 no se toca**: es el software que corre hoy en el arcade Teensy.
- **`v1.1/teensy_cfg/` es código escrito a mano**, no generado: se edita
  directamente. Comparte protocolo con v4, pero no su generador.
- **El Teensy 3.0 no tolera 5 V**: sus señales son 3,3 V. Serial1 está en
  pines fijos 0 (RX) y 1 (TX); no se pueden remapear por `CFG_SET`.
- El repo es **privado**. No publicar.

## 4. Flujo de apagado (v4 / v1.1, el importante)

```
Llave OFF / pulsación
  -> controlador envía POWEROFF_REQ
       (v4: D5/D7; v1.1: Serial1 pines 0/1)
  -> pi.py responde BYE reason=... , escribe traza con fsync, drena UART
  -> Pi ejecuta batocera-es-swissknife --shutdown (fallback shutdown -h now)
  -> controlador espera ALIVE_TIMEOUT_MS (60 s) y corta la PSU por watchdog
```

Otros caminos:

- Sin `BOOT_OK` en `BOOT_CONFIRM_SUPPRESS_MS + BOOT_CONFIRM_CUTOFF_MS`
  (60+60 s): corte con LED código 1.
- Pérdida de `ALIVE` tras boot: corte con LED código 2.
- `QUIET seconds=N` de la Pi: suspende el watchdog durante un reboot.
- Bypass: entra/sale con **botón FLASH (GPIO0/D3)** en v4 o con **pin 6**
  en v1.1 (PSU ON fija, watchdog off, link USB, LED fade 2 s, RAM only).
  Avisa `MODE BYPASS` / `MODE NORMAL` por el enlace de pines; la Pi ignora
  `POWEROFF_REQ` mientras dure.

## 5. Estructura

```
README.md                 guía humana: instalación, build, archivos
AGENTS.md                 este archivo
DECISIONES.md             por qué se hizo cada cambio sensible
v1/ ... v4/               una carpeta por versión, con README + PROTOCOLO
  diagrama_*.txt          cableado (v2/v3/v4 comparten el de ESP8266)
  PROTOCOLO.md            contrato eléctrico, mensajes y timeouts
v1.1/teensy_cfg/          firmware v1.1 escrito a mano, NO generado
v1.1/system/pi.py         mismo daemon que v4, textos adaptados
v1.1/diagrama_teensy_*    cableado Teensy 3.0 + Pi (pines 0/1, bypass 6)
v3/esp8266.ino            firmware original, entrada del generador
v3/pi.py                  daemon serial original (rescate)
v4/esp8266_cfg/           firmware v4 GENERADO (.ino + cfg_types.h)
v4/tools/                 generador: transforma v3 -> v4
v4/system/pi.py           daemon que se instala en /userdata/system/pi.py
v4/system/custom_service  launcher Batocera
v2/com_test/              herramientas de diagnóstico de la capa física
```

## 6. Cómo trabajar

1. Leer el `PROTOCOLO.md` de la versión que se va a tocar.
2. Cambios de firmware v4: editar `v4/tools/make_cfg_firmware.py` →
   ejecutarlo → copiar los dos archivos generados a `v4/esp8266_cfg/` →
   actualizar `v4/PROTOCOLO.md` si cambia el protocolo.
   Cambios de v1.1: editar `v1.1/teensy_cfg/` directamente (no hay
   generador) y actualizar `v1.1/PROTOCOLO.md`.
3. Validación rápida:
   ```sh
   python3 -m py_compile v4/system/pi.py v4/tools/make_cfg_firmware.py
   python3 -m py_compile v1.1/system/pi.py
   arduino-cli compile --fqbn esp8266:esp8266:nodemcuv2 \
     --build-path /tmp/opencode/build-esp8266-esp8266-nodemcuv2 v4/esp8266_cfg
   arduino-cli compile --fqbn teensy:avr:teensy30 \
     --build-path /tmp/opencode/build-teensy30 v1.1/teensy_cfg
   ```
   Compilar v4 también con `esp32:esp32:esp32`, `esp32:esp32:esp32s2` y
   `esp32:esp32:esp32s3`. Usar siempre `--build-path` explícito.
4. Monitor serial: 9600 8N1. En Linux `/dev/ttyUSB0` puede requerir
   `newgrp uucp` o `pkexec`. Para grabar el Teensy hace falta
   `teensy-loader-cli` y el botón PROGRAM.

## 7. Estado verificado (2026-11-22)

- Repo privado, rama `main`, todo pusheado.
- v1.1 compila para `teensy:avr:teensy30` (Teensyduino 1.62.0), sin
  flashear ni probar en hardware. `teensy-loader-cli` no está instalado.
- ESP8266 NodeMCU LoLin v3 con v4 flasheada; EEPROM confirmada en 60 s
  durante cinco arranques (`RX CFG_ACK ALIVE_TIMEOUT_MS=60000`).
- SD reparada (`e2fsck` + `fsck.vfat`), limpia y ejectada; contiene 18171
  roms, 78 saves y el `pi.py` v4 instalado. Copia de rescate del daemon
  original en `/home/fede/pacman/SHARE/system/pi.py`.
- No hay tests automatizados: las pruebas son compilación + monitor serial
  + ciclos reales de encendido/apagado.

## 8. Deuda y referencias no incluidas

- `v4/tools/make_cfg_firmware.py` usa rutas **absolutas**
  (`/home/fede/pacman/ESP8266/...`). Hay que ajustarlas para otra máquina;
  idealmente convertirlas en rutas relativas al repo.
- Los zips originales (`PSU PC para mis arcades.zip`, antes `ESP8266.zip` y
  `Teensy 3.0.zip`) viven fuera del repo, en `/home/fede/pacman/`. Su
  contenido útil ya está versionado. Dentro de ese zip hay dos variantes
  **cuyo código no se importó**, sólo se usaron como referencia de
  documentación: `comunicacion/` (GPIO con farewell, D5/D7 invertidos) y
  `ESP32/` (adaptación por pines a ESP32).
- Los `CFG_*`, el bypass y el timeout de 60 s están documentados en
  `v4/PROTOCOLO.md` y `v1.1/PROTOCOLO.md`; si se cambian, actualizar el
  archivo correspondiente en el mismo commit.
- El Teensy 3.0 tiene EEPROM emulada de 2048 B (`E2END=0x7FF`) y no
  necesita `EEPROM.begin()`/`commit()`, a diferencia del core ESP8266.

## 9. Glosario mínimo

- **PSU / ATX**: fuente de alimentación de PC. **PS_ON**: pin verde que la
  enciende. **+5VSB**: standby violeta, siempre vivo.
- **ESP**: microcontrolador (ESP8266 o ESP32). **Pi**: Raspberry Pi con
  Batocera.
- **Link de pines**: UART por SoftwareSerial en D5/D7 (ESP8266), UART1
  con pines remapeados (ESP32) o `Serial1` en pines 0/1 (Teensy 3.0).
  **Backend USB**: UART0/USB (`Serial`).
- **ALIVE / heartbeat**: mensaje periódico de la Pi para decir "sigo viva".
- **QUIET**: ventana anunciada por la Pi durante la cual se suspende el
  watchdog (reinicios).
- **BYE**: despedida ordenada antes del shutdown.
- **Bypass**: modo temporal (botón FLASH en v4, pin 6 en v1.1) para
  configurar sin que la PSU se apague; nunca se persiste en EEPROM.
- **FQBN**: identificador de placa de `arduino-cli`.
