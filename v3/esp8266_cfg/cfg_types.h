#pragma once
#include <Arduino.h>
#include <EEPROM.h>

#define CFG_MAGIC   0x45535043UL   // "ESPC"
#define CFG_VERSION 4

// --- Capa de plataforma -------------------------------------------
// UART0 (backend USB) usa pines FIJOS en silicio en todas las familias:
//   ESP8266 / ESP32: RX=GPIO3, TX=GPIO1 (CH340/CP210x en la placa)
//   ESP32-S2/S3:     Serial suele ser USB-CDC nativo (pines USB dedicados)
// El link alternativo usa pines remapeables por GPIO matrix.

// Defaults heredados del diseno original (D1=5, D2=4, D4=2, D5=14,
// D7=13). Siempre sobreescribibles con CFG_SET + CFG_SAVE.
#define CFG_DEF_BUTTON_PIN   5
#define CFG_DEF_PSU_PIN      4
#define CFG_DEF_LED_PIN      2
#define CFG_DEF_RX_PIN      14
#define CFG_DEF_TX_PIN      13

#if defined(ESP8266)
  #define CFG_PIN_MAX         16
  #define CFG_HAS_SOFTWARE_SERIAL 1
  #include <SoftwareSerial.h>
#elif defined(ESP32)
  #define CFG_PIN_MAX         48
  #define CFG_HAS_SOFTWARE_SERIAL 0
#else
  #error "Firmware solo para ESP8266 / ESP32 / ESP32-S2 / ESP32-S3"
#endif

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
