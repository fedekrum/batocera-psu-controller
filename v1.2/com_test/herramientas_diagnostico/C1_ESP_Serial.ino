/*
 * C1_ESP_Serial.ino — prueba serial basica ESP <-> Pi.
 *
 * El ESP transmite "xxx\n" cada 1 s por SoftwareSerial (D5=RX, D7=TX
 * a 9600 8N1) y, cada vez que recibe una linea "yyy", hace un blink
 * de 200 ms en el LED interno D4 (activo en bajo).
 *
 * Cableado (mismo que comunicacion serial/):
 *   Pi pin 8  (BCM14 TXD) -> ESP D5 (RX de SoftwareSerial)
 *   Pi pin 10 (BCM15 RXD) <- ESP D7 (TX de SoftwareSerial)
 *   GND comun
 *
 * D2 se mantiene HIGH para que la PSU ATX quede encendida durante la
 * prueba (pareja del par C2_RPi_Serial.py que corre en la Pi).
 *
 * Serial USB @ 115200 va al monitor del Arduino IDE para depurar.
 */
#include <Arduino.h>
#include <SoftwareSerial.h>

static const uint8_t PIN_PSU_ON = D2;
static const uint8_t PIN_LED = D4;
static const uint8_t PIN_RX = D5;
static const uint8_t PIN_TX = D7;

static const uint32_t LINK_BAUD = 9600;
static const uint32_t TX_PERIOD_MS = 1000;
static const uint32_t BLINK_ON_MS = 200;
static const uint16_t RX_LINE_MAX = 64;

static SoftwareSerial link(PIN_RX, PIN_TX);

static char rxBuf[RX_LINE_MAX + 1];
static uint16_t rxLen = 0;
static uint32_t lastTxAtMs = 0;
static uint32_t ledOffAtMs = 0;
static bool ledOn = false;

static void ledSet(bool on)
{
  digitalWrite(PIN_LED, on ? LOW : HIGH); // D4 activo en bajo
  ledOn = on;
}

void setup()
{
  pinMode(PIN_PSU_ON, OUTPUT);
  digitalWrite(PIN_PSU_ON, HIGH); // PSU ON durante la prueba

  pinMode(PIN_LED, OUTPUT);
  ledSet(false);

  Serial.begin(115200);
  while (!Serial && millis() < 2000)
  { /* wait usb */
  }
  Serial.println();
  Serial.println("--- C1 ESP SERIAL (9600 8N1, D5=RX / D7=TX) ---");

  link.begin(LINK_BAUD);
  lastTxAtMs = millis() - TX_PERIOD_MS; // transmitir ya en el primer loop
}

void loop()
{
  const uint32_t now = millis();

  // TX periodico
  if ((uint32_t)(now - lastTxAtMs) >= TX_PERIOD_MS)
  {
    link.print("xxx\n");
    Serial.println("TX xxx");
    lastTxAtMs = now;
  }

  // RX por lineas
  while (link.available() > 0)
  {
    int c = link.read();
    if (c < 0)
      break;
    if (c == '\r')
      continue;
    if (c == '\n')
    {
      rxBuf[rxLen] = '\0';
      if (rxLen > 0)
      {
        Serial.print("RX ");
        Serial.println(rxBuf);
        if (strcmp(rxBuf, "yyy") == 0)
        {
          ledSet(true);
          ledOffAtMs = now + BLINK_ON_MS;
        }
      }
      rxLen = 0;
    }
    else if (rxLen < RX_LINE_MAX)
    {
      rxBuf[rxLen++] = (char)c;
    }
    else
    {
      // overflow: descartar linea
      rxLen = 0;
      Serial.println("RX overflow");
    }
  }

  // apagar LED tras el blink
  if (ledOn && (int32_t)(now - ledOffAtMs) >= 0)
  {
    ledSet(false);
  }
}
