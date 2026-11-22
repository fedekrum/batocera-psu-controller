/*
 * power_on.ino - Versión Ultra-Básica y Segura
 */
#include <Arduino.h>

void setup() {
  // Pin 4 es el D2 físico en la Lolin V3 (Control de Transistor PSU)
  pinMode(4, OUTPUT);
  digitalWrite(4, HIGH); // Forzar encendido manual

  // Pin 2 es el LED azul integrado (D4)
  // Lo prendemos para confirmar que el código está corriendo
  pinMode(2, OUTPUT);
  digitalWrite(2, LOW);  // Encendido (LOW = ON en ESP8266)
}

void loop() {
  // Solo mantener el estado
  delay(1000);
}
