/*
 * esp_debug_pong.ino - El Búnker (V4)
 */
#include <Arduino.h>

void setup() {
  // 1. Prender fuente y NO soltarla jamás
  pinMode(D2, OUTPUT); 
  digitalWrite(D2, HIGH); 
  
  // 2. Forzar señal de APAGADO en CERO absoluto (Hacia Pin 7 de RPi)
  // Esto evita que la Pi se auto-apague por ruidos al arrancar.
  pinMode(D5, OUTPUT);
  digitalWrite(D5, LOW); 
  
  // 3. Escuchar el pulso de vida de la Pi (D7 <- Pin 8 de RPi)
  pinMode(D7, INPUT);
  
  // 4. LED Azul indicador
  pinMode(D4, OUTPUT);
  digitalWrite(D4, HIGH); // Apagado inicial
}

void loop() {
  // El LED azul se prende si la Pi vive (recibe Heartbeat en D7)
  if (digitalRead(D7) == HIGH) {
    digitalWrite(D4, LOW); // LED ON
  } else {
    digitalWrite(D4, HIGH); // LED OFF
  }
  
  delay(100);
}
