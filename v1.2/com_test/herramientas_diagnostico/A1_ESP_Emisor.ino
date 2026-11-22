/*
 * A1_ESP_Emisor.ino — pulso por D5 y D7 para prueba de cable con A2.
 *
 * Cableado alineado con comunicacion/esp.ino + pi.py:
 *   ESP D5 <-> Pi header fisico 7 (BCM 4)
 *   ESP D7 <-> Pi header fisico 8 (BCM 14)
 * En produccion la Pi *sale* por BCM4 (heartbeat); aqui el ESP
 * *impulsa* D5/D7 como salida y la Pi solo debe *leer* (A2).
 *
 * D2 en HIGH mantiene la PSU encendida durante la prueba.
 */
#include <Arduino.h>

static const uint32_t PULSE_MS = 1000;
static const uint32_t GAP_MS = 500;

void setup()
{
  pinMode(D2, OUTPUT);
  digitalWrite(D2, HIGH);

  pinMode(D5, OUTPUT);
  pinMode(D7, OUTPUT);
  digitalWrite(D5, LOW);
  digitalWrite(D7, LOW);

  Serial.begin(115200);
  Serial.println("--- A1 ESP EMISOR (D5 luego D7) ---");
}

void loop()
{
  Serial.println("Pulso D5 (linea hacia Pi pin 7 / BCM4)...");
  digitalWrite(D5, HIGH);
  delay(PULSE_MS);
  digitalWrite(D5, LOW);

  delay(GAP_MS);

  Serial.println("Pulso D7 (linea hacia Pi pin 8 / BCM14)...");
  digitalWrite(D7, HIGH);
  delay(PULSE_MS);
  digitalWrite(D7, LOW);

  delay(GAP_MS);
}
