# Prueba serial basica C1 + C2

Misma filosofia que A/B (pulsos GPIO) pero sobre el enlace UART de
`comunicacion serial/`:

- **`C1_ESP_Serial.ino`** — en el ESP8266 (LoLin). Mantiene PSU ON
  (D2 HIGH), envia `xxx\n` cada 1 s por SoftwareSerial (D5=RX,
  D7=TX @ 9600 8N1) y, cada vez que llega la linea `yyy`, parpadea
  el LED interno D4 por 200 ms. Todo se refleja tambien por Serial
  USB @ 115200 (monitor del Arduino IDE: `TX xxx`, `RX yyy`).
- **`C2_RPi_Serial.py`** — en la Raspberry Pi. Abre `/dev/serial0`
  a 9600 8N1 raw, envia `yyy\n` cada 1 s y loguea cada TX/RX con
  timestamp en stdout y en `/userdata/system/serial_test.log`.

## Cableado

| Señal | Pi (header) | BCM | ESP |
|-------|-------------|-----|-----|
| Pi TX | pin 8 | GPIO14 (TXD) | D5 |
| Pi RX | pin 10 | GPIO15 (RXD) | D7 |
| GND | pin 6 (o cualquier GND) | — | GND |

## Prep de la Pi (Batocera)

1. `/boot/config.txt`: `enable_uart=1` y `dtoverlay=miniuart-bt`
   (mantiene Bluetooth). Solo si **no** usas BT:
   `dtoverlay=disable-bt`.
2. `/boot/cmdline.txt`: sin `console=serial0|ttyAMA0|ttyS0`.
3. Reiniciar.

## Uso

1. Flashear `C1_ESP_Serial.ino` en el ESP.
2. En la Pi: `python3 C2_RPi_Serial.py`. Se vera en pantalla:

   ```
   [HH:MM:SS] --- C2 RPi SERIAL en /dev/serial0 @ 9600 ---
   [HH:MM:SS] puerto abierto
   [HH:MM:SS] TX yyy
   [HH:MM:SS] RX xxx
   ...
   ```

3. En el ESP el LED debe parpadear (200 ms) en cada `RX yyy`.
4. Si ambos extremos ven al otro, el enlace esta bien y se puede
   pasar a `comunicacion serial/`. Si hay trafico en un solo sentido
   o ninguno, revisar cableado, `config.txt`, `cmdline.txt` y la
   libreria SoftwareSerial del core ESP8266.
