/*
 * Script de prueba para validar el transistor y la fuente ATX.
 * Enciende la fuente (D2) y el LED (D4) durante 10 segundos, 
 * luego los apaga por 10 segundos, en un ciclo infinito.
 */

#include <Arduino.h>

const int PIN_PSU_ON = D2; // Conectado a la base del transistor D882-Y
const int PIN_LED = D4;    // LED interno del NodeMCU

// El LED interno en ESP8266 suele ser Active-Low
#define LED_ON  LOW
#define LED_OFF HIGH

void setup() {
  // Configurar los pines como salida
  pinMode(PIN_PSU_ON, OUTPUT);
  pinMode(PIN_LED, OUTPUT);
  
  // Iniciar apagado (estado estable)
  digitalWrite(PIN_PSU_ON, LOW);
  digitalWrite(PIN_LED, LED_OFF);
  
  Serial.begin(115200);
  delay(1000);
  Serial.println("\n--- Iniciando test de Fuente ATX ---");
}

void loop() {
  Serial.println("Estado: ON (Encendiendo fuente y LED...)");
  digitalWrite(PIN_PSU_ON, HIGH);
  digitalWrite(PIN_LED, LED_ON);
  
  delay(10000); // Mantener encendido 10 segundos
  
  Serial.println("Estado: OFF (Apagando fuente y LED...)");
  digitalWrite(PIN_PSU_ON, LOW);
  digitalWrite(PIN_LED, LED_OFF);
  
  delay(10000); // Mantener apagado 10 segundos
}
