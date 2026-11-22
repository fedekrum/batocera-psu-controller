/*
 * B2_ESP_Receptor.ino — lee D5/D7 mientras B1 alterna BCM4/BCM14.
 *
 * Cableado alineado con comunicacion/esp.ino + pi.py:
 *   ESP D5 <-> Pi header fisico 7 (BCM 4)
 *   ESP D7 <-> Pi header fisico 8 (BCM 14)
 *
 * LED azul integrado en D4 (NodeMCU LoLin): activo en bajo;
 * LOW en D4 = LED encendido. El LED sigue el nivel *logico* de D5
 * (HIGH en D5 -> LED encendido visualmente).
 *
 * D2 en HIGH mantiene la PSU encendida durante la prueba.
 */
#include <Arduino.h>

static const uint32_t SERIAL_BAUD = 115200;
static const uint32_t POLL_MS = 50;

// D4 en LoLin: encendido logico = GPIO LOW (ver comunicacion/esp.ino).
static void ledSetLogicalOn(bool on)
{
  digitalWrite(D4, on ? LOW : HIGH);
}

void setup()
{
  pinMode(D2, OUTPUT);
  digitalWrite(D2, HIGH);

  pinMode(D5, INPUT);
  pinMode(D7, INPUT);

  pinMode(D4, OUTPUT);
  ledSetLogicalOn(false);

  Serial.begin(SERIAL_BAUD);
  Serial.println("\n--- B2 ESP RECEPTOR (LED = nivel logico D5) ---");
}

void loop()
{
  const bool d5High = digitalRead(D5) == HIGH;
  const bool d7High = digitalRead(D7) == HIGH;

  Serial.print("D5(header7/BCM4): ");
  Serial.print(d5High ? "H" : "L");
  Serial.print(" | D7(header8/BCM14): ");
  Serial.println(d7High ? "H" : "L");

  ledSetLogicalOn(d5High);

  delay(POLL_MS);
}
