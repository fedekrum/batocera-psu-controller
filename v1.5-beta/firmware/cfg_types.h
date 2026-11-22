#pragma once
#include <Arduino.h>
#include <EEPROM.h>
#include "boards.h"

// v1.5-beta: esquema unico para todas las placas. El numero identifica el
// LAYOUT de la EEPROM, no la version del controlador (4.1).
// Cambio a 5 respecto de v1.4 porque se agrego el campo bypassPin.
#define CFG_VERSION 5

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
