#pragma once
#include <Arduino.h>
#include <EEPROM.h>

// v1.1.1-beta: esquema de configuracion de v1.4 adaptado a Teensy 3.0.
// CFG_VERSION es la version del ESQUEMA (4), no la del controlador (1.1).
// El magic cambia respecto de v1.4 porque la EEPROM es otro hardware y las
// configuraciones no son intercambiables (los pines son distintos).
#define CFG_MAGIC   0x54454331UL   // "TEC1"
#define CFG_VERSION 4

// Teensy 3.0: pines digitales 0..33. Serial1 usa 0/1 en silicio,
// por lo que PIN_SERIAL_RX/TX solo admiten esos valores (ver firmware).
#define CFG_PIN_MAX 33

// Defaults de fabrica de v1.1.1-beta (heredan la logica de v1.1):
//   boton en 3, PS_ON en 2, LED builtin en 13, Serial1 en 0/1.
// El pin de bypass es nuevo: no existe boton FLASH en el Teensy 3.0.
#define CFG_DEF_BUTTON_PIN 3
#define CFG_DEF_PSU_PIN    2
#define CFG_DEF_LED_PIN   13
#define CFG_DEF_RX_PIN     0
#define CFG_DEF_TX_PIN     1
#define CFG_DEF_BYPASS_PIN 6

struct ArcadeConfig
{
  uint32_t magic;
  uint16_t version;
  uint16_t crc;

  // Tiempos (ms)
  uint32_t aliveTimeoutMs;
  uint32_t quietMaxMs;
  uint32_t bootConfirmSuppressMs;
  uint32_t bootConfirmCutoffMs;
  uint32_t softShutdownLedWindowMs;
  uint32_t psuOffSettleDelayMs;
  uint32_t momentaryShortMs;
  uint32_t momentaryLongForceOffMs;
  uint32_t momentarySoftSuppressMs;
  uint32_t setupLedBootOnMs;
  uint32_t ledEncodePulseMs;
  uint32_t ledEncodeGapMs;
  uint32_t debounceStableMs;

  // Otros
  uint32_t linkBaud;
  uint8_t buttonType;
  uint8_t buttonPin;
  uint8_t psuOnPin;
  uint8_t pinSerialRx;
  uint8_t pinSerialTx;
  uint8_t pinLed;
  uint8_t psuOnActiveHigh;
  uint8_t ledLogicalOnIsGpioHigh;
  uint8_t linkBackend;
  uint8_t bypassPin;
};

struct CfgEntry
{
  const char *key;
  uint8_t wide;       // 1 = uint32, 0 = uint8
  void *ptr;
  uint32_t min;
  uint32_t max;
  bool reboot;
};
