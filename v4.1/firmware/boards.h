#pragma once
#include <Arduino.h>

// ============================================================================
// Perfiles de placa de v4.1
// ============================================================================
// Un solo firmware para ESP8266, ESP32, ESP32-S2, ESP32-S3 y Teensy 3.0.
// Aca van SOLO las diferencias de hardware: defaults de pines, polaridad
// del LED, magia de EEPROM y forma de acceder al enlace de pines.
// La logica del controlador (watchdog, CFG_*, BYE, QUIET, bypass) es comun
// y esta en firmware.ino.
//
// CFG_MAGIC es distinto por familia para que una configuracion grabada en
// una placa nunca se interprete como valida en otra.
// CFG_VERSION (en cfg_types.h) es la version del ESQUEMA, no del controlador.

#if defined(TEENSYDUINO) && defined(__MK20DX128__)
  #define BOARD_TEENSY30 1
  #define BOARD_LABEL "Teensy 3.0"
  #define BOARD_FIRMWARE_ID "v41-teensy30"
  #define CFG_MAGIC 0x56343154UL   // "V41T"
  #define CFG_PIN_MAX 33
  #define CFG_HAS_SOFTWARE_SERIAL 0
  #define CFG_HAS_FIXED_SERIAL_PINS 1
  // v1: boton 3, PSU 2, LED 13. Serial1 usa 0/1 en silicio.
  #define CFG_DEF_BUTTON_PIN 3
  #define CFG_DEF_PSU_PIN    2
  #define CFG_DEF_LED_PIN   13
  #define CFG_DEF_RX_PIN     0
  #define CFG_DEF_TX_PIN     1
  #define CFG_DEF_BYPASS_PIN 6
  #define CFG_DEF_LED_ACTIVE_HIGH 1   // LED integrado, activo HIGH

#elif defined(ESP8266)
  #define BOARD_ESP8266 1
  #define BOARD_LABEL "ESP8266"
  #define BOARD_FIRMWARE_ID "v41-esp8266"
  #define CFG_MAGIC 0x56343145UL   // "V41E"
  #define CFG_PIN_MAX 16
  #define CFG_HAS_SOFTWARE_SERIAL 1
  #define CFG_HAS_FIXED_SERIAL_PINS 0
  // Diseno original: D1=5 boton, D2=4 PSU, D4=2 LED, D5=14 RX, D7=13 TX.
  #define CFG_DEF_BUTTON_PIN 5
  #define CFG_DEF_PSU_PIN    4
  #define CFG_DEF_LED_PIN    2
  #define CFG_DEF_RX_PIN    14
  #define CFG_DEF_TX_PIN    13
  #define CFG_DEF_BYPASS_PIN 0      // boton FLASH (GPIO0)
  #define CFG_DEF_LED_ACTIVE_HIGH 0 // LED activo LOW

#elif defined(ESP32)
  #define BOARD_ESP32 1
  #define BOARD_LABEL "ESP32"
  #define BOARD_FIRMWARE_ID "v41-esp32"
  #define CFG_MAGIC 0x56343132UL   // "V412"
  #define CFG_PIN_MAX 48
  #define CFG_HAS_SOFTWARE_SERIAL 0
  #define CFG_HAS_FIXED_SERIAL_PINS 0
  // En ESP32/S2/S3 no hay SoftwareSerial: se usa UART1 con GPIO matrix.
  #define CFG_DEF_BUTTON_PIN 5
  #define CFG_DEF_PSU_PIN    4
  #define CFG_DEF_LED_PIN    2
  #define CFG_DEF_RX_PIN    14
  #define CFG_DEF_TX_PIN    13
  #define CFG_DEF_BYPASS_PIN 0
  #define CFG_DEF_LED_ACTIVE_HIGH 0

#else
  #error "Firmware v4.1 solo para ESP8266 / ESP32 / ESP32-S2 / ESP32-S3 / Teensy 3.0"
#endif

// Margen de apagado: 60 s en TODAS las placas. No bajar.
#define CFG_DEF_ALIVE_TIMEOUT_MS 60000UL
