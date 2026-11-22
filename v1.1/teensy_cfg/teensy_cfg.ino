// ============================================================================
// CONTROLADOR DE PSU - VERSION 1.1 (Teensy 3.0)
// ============================================================================
// Misma logica y protocolo que v4, portada a Teensy 3.0 / Teensyduino.
// Diferencias respecto de v4:
//   - Serial1 por hardware en pines 0 (RX) y 1 (TX) en vez de
//     SoftwareSerial en D5/D7.
//   - Bypass por pin dedicado (CFG_DEF_BYPASS_PIN = 6) en vez del
//     boton FLASH de la NodeMCU.
//   - EEPROM emulada por Teensyduino (2048 bytes en Teensy 3.0),
//     sin begin()/commit().
//   - LED builtin (pin 13) activo HIGH.
//   - ALIVE_TIMEOUT_MS nace con 60000 en EEPROM (no 30000): es el
//     margen que evita la corrupcion de la SD.
//
// Ver PROTOCOLO.md en esta misma carpeta.
//
// Cableado con la Pi 5:
//   Pi pin 7  BCM4 UART3_TX -> Teensy pin 0 (Serial1 RX)
//   Pi pin 29 BCM5 UART3_RX <- Teensy pin 1 (Serial1 TX)
//   GND comun
//   Teensy alimentado desde +5V standby ATX (violeta).
// IMPORTANTE: Teensy 3.0 NO tolera 5V. Usar solo 3,3 V.

#include <Arduino.h>
#include <EEPROM.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include "cfg_types.h"

ArcadeConfig cfg;

// --- Constantes internas (no configurables) -------------------------

static const uint8_t MOMENTARY = 0;
static const uint8_t SWITCH = 1;
static const uint8_t LINK_BACKEND_PINS = 0;
static const uint8_t LINK_BACKEND_USB = 1;
static const uint16_t RX_LINE_MAX = 96;
static const uint8_t LEVEL_DEBUG = 10;
static const uint8_t LEVEL_INFO = 20;
static const uint8_t LEVEL_WARNING = 30;
static const uint8_t LEVEL_ERROR = 40;
static const uint8_t LEVEL_OFF = 100;
static const uint8_t LOG_LEVEL_STARTUP = LEVEL_INFO;
static const uint8_t LED_BLINK_CODE_NEVER_BOOT = 1;
static const uint8_t LED_BLINK_CODE_LOST_HEARTBEAT = 2;
static const uint8_t LED_BLINK_CODE_WAIT_PI_BOOT_OK = 4;
static const uint8_t MOMENTARY_BUTTON_MSG_TOGGLE_OR_SOFT = 1;
static const uint8_t MOMENTARY_BUTTON_MSG_FORCE_PSU_OFF = 2;
static const char *FIRMWARE_ID = "teensy11-1";

// --- Defaults de fabrica --------------------------------------------

static void cfgLoadDefaults()
{
  cfg.magic = CFG_MAGIC;
  cfg.version = CFG_VERSION;
  cfg.crc = 0;

  // 60 s desde el primer arranque: con 20/30 s se corrompio la SD.
  cfg.aliveTimeoutMs = 60000;
  cfg.quietMaxMs = 600000;
  cfg.bootConfirmSuppressMs = 60000;
  cfg.bootConfirmCutoffMs = 60000;
  cfg.softShutdownLedWindowMs = 180000;
  cfg.psuOffSettleDelayMs = 5000;
  cfg.momentaryShortMs = 500;
  cfg.momentaryLongForceOffMs = 6000;
  cfg.momentarySoftSuppressMs = 10000;
  cfg.setupLedBootOnMs = 2000;
  cfg.ledEncodePulseMs = 200;
  cfg.ledEncodeGapMs = 1000;
  cfg.debounceStableMs = 20;

  cfg.linkBaud = 9600;
  cfg.buttonType = SWITCH;
  cfg.buttonPin = CFG_DEF_BUTTON_PIN;
  cfg.psuOnPin = CFG_DEF_PSU_PIN;
  cfg.pinSerialRx = CFG_DEF_RX_PIN;
  cfg.pinSerialTx = CFG_DEF_TX_PIN;
  cfg.pinLed = CFG_DEF_LED_PIN;
  cfg.psuOnActiveHigh = 1;
  cfg.ledLogicalOnIsGpioHigh = 1;   // LED builtin del Teensy
  cfg.linkBackend = LINK_BACKEND_PINS;
  cfg.bypassPin = CFG_DEF_BYPASS_PIN;
}

// CRC simple (suma de bytes) sobre los campos configurables.
static uint16_t cfgCalcCrc(const ArcadeConfig &c)
{
  const uint8_t *p = (const uint8_t *)&c.aliveTimeoutMs;
  const uint8_t *end = (const uint8_t *)&c + sizeof(ArcadeConfig);
  uint16_t sum = 0;
  while (p < end)
    sum += *p++;
  return sum;
}

static bool cfgRead()
{
  ArcadeConfig stored;
  EEPROM.get(0, stored);
  if (stored.magic != CFG_MAGIC || stored.version != CFG_VERSION)
    return false;
  uint16_t savedCrc = stored.crc;
  if (cfgCalcCrc(stored) != savedCrc)
    return false;
  cfg = stored;
  return true;
}

static void cfgWrite()
{
  cfg.magic = CFG_MAGIC;
  cfg.version = CFG_VERSION;
  cfg.crc = cfgCalcCrc(cfg);
  EEPROM.put(0, cfg);
}

// --- Tabla de claves para comandos CFG -------------------------------

static const CfgEntry CFG_ENTRIES[] = {
  {"ALIVE_TIMEOUT_MS",           1, &cfg.aliveTimeoutMs,           1000, 120000, false},
  {"QUIET_MAX_MS",               1, &cfg.quietMaxMs,               1000, 600000, false},
  {"BOOT_CONFIRM_SUPPRESS_MS",   1, &cfg.bootConfirmSuppressMs,       0, 600000, false},
  {"BOOT_CONFIRM_CUTOFF_MS",     1, &cfg.bootConfirmCutoffMs,         0, 600000, false},
  {"SOFT_SHUTDOWN_LED_WINDOW_MS",1, &cfg.softShutdownLedWindowMs,     0, 600000, false},
  {"PSU_OFF_SETTLE_DELAY_MS",    1, &cfg.psuOffSettleDelayMs,         0,  60000, false},
  {"MOMENTARY_SHORT_MS",         1, &cfg.momentaryShortMs,           50,  10000, false},
  {"MOMENTARY_LONG_FORCE_MS",    1, &cfg.momentaryLongForceOffMs,    100,  60000, false},
  {"MOMENTARY_SOFT_SUPPRESS_MS", 1, &cfg.momentarySoftSuppressMs,      0, 300000, false},
  {"SETUP_LED_BOOT_ON_MS",       1, &cfg.setupLedBootOnMs,             0,  60000, false},
  {"LED_ENCODE_PULSE_MS",        1, &cfg.ledEncodePulseMs,            10,   5000, false},
  {"LED_ENCODE_GAP_MS",          1, &cfg.ledEncodeGapMs,               0,  60000, false},
  {"DEBOUNCE_STABLE_MS",         1, &cfg.debounceStableMs,             1,   1000, false},
  {"LINK_BAUD",                  1, &cfg.linkBaud,                  1200, 115200, true},
  {"LINK_BACKEND",               0, &cfg.linkBackend,                  0,      1, true},
  {"BUTTON_TYPE",                0, &cfg.buttonType,                   0,      1, true},
  {"BUTTON_PIN",                 0, &cfg.buttonPin,                    0, CFG_PIN_MAX, true},
  {"PSU_ON_PIN",                 0, &cfg.psuOnPin,                     0, CFG_PIN_MAX, true},
  {"PIN_SERIAL_RX",              0, &cfg.pinSerialRx,                  0, CFG_PIN_MAX, true},
  {"PIN_SERIAL_TX",              0, &cfg.pinSerialTx,                  0, CFG_PIN_MAX, true},
  {"PIN_LED",                    0, &cfg.pinLed,                       0, CFG_PIN_MAX, true},
  {"PSU_ON_ACTIVE_HIGH",         0, &cfg.psuOnActiveHigh,              0,      1, true},
  {"LED_LOGICAL_ON_GPIO_HIGH",   0, &cfg.ledLogicalOnIsGpioHigh,       0,      1, true},
  {"BYPASS_PIN",                 0, &cfg.bypassPin,                    0, CFG_PIN_MAX, true},
};
static const uint8_t CFG_ENTRIES_N =
    sizeof(CFG_ENTRIES) / sizeof(CFG_ENTRIES[0]);

static const CfgEntry *cfgFind(const char *key)
{
  for (uint8_t i = 0; i < CFG_ENTRIES_N; i++)
  {
    if (!strcmp(CFG_ENTRIES[i].key, key))
      return &CFG_ENTRIES[i];
  }
  return nullptr;
}

static uint32_t cfgValue(const CfgEntry *e)
{
  return e->wide ? *(uint32_t *)e->ptr : *(uint8_t *)e->ptr;
}

// === FIN CONFIGURACION ===

// Serial1 = enlace fisico con la Pi (pines 0/1 del Teensy 3.0).
// Serial  = USB, backend alternativo y link del modo bypass.
Stream *pinLinkPtr = &Serial1;
Stream *usbLinkPtr = &Serial;
Stream *linkPtr = pinLinkPtr;
#define link (*linkPtr)

// Modo bypass: pin dedicado (no existe boton FLASH). RAM only.
#define BYPASS_DEBOUNCE_MS 50
bool bypassMode = false;
bool bypassBtnRaw = true;
bool bypassBtnStable = true;
uint32_t bypassBtnLastChangeMs = 0;

// === VARIABLES GLOBALES ===

bool piBootConfirmed = false;
uint32_t psuPoweredOnAtMs = 0;

// Ultimo instante en que llego CUALQUIER linea valida desde la Pi.
uint32_t lastRxLineAtMs = 0;

// Codigo N que se muestra en LED con PSU OFF.
uint8_t ledPsuOffBlinkCode = 0;

uint32_t lastPowerOffReqSentAtMs = 0;
uint32_t lastByeReceivedAtMs = 0;
uint32_t quietUntilMs = 0;
uint8_t espLogLevel = LOG_LEVEL_STARTUP;

bool switchLastPressed = false;

char rxBuf[RX_LINE_MAX + 1];
uint16_t rxLen = 0;

// === FIN VARIABLES GLOBALES ===

static void ledWriteLogical(bool on)
{
  digitalWrite(cfg.pinLed,
               on == cfg.ledLogicalOnIsGpioHigh ? HIGH : LOW);
}

bool isPsuOn()
{
  const int level = digitalRead(cfg.psuOnPin);
  return cfg.psuOnActiveHigh ? (level == HIGH) : (level == LOW);
}

bool softShutdownFromButtonAllowed()
{
  return (millis() - psuPoweredOnAtMs) >
         cfg.momentarySoftSuppressMs;
}

void setPsu(bool on)
{
  const int level =
      (on == cfg.psuOnActiveHigh) ? HIGH : LOW;
  digitalWrite(cfg.psuOnPin, level);
  if (!on)
    delay(cfg.psuOffSettleDelayMs);
}

void startLEDSequence()
{
  ledWriteLogical(true);
  delay(cfg.setupLedBootOnMs);
  ledWriteLogical(false);
}

static void ledWriteEncodedBlinkCount(uint32_t now, uint8_t code)
{
  const uint32_t pulse = cfg.ledEncodePulseMs;
  const uint32_t gap = cfg.ledEncodeGapMs;
  const uint32_t oneBlinkMs = 2 * pulse;
  const uint32_t cycle = (uint32_t)code * oneBlinkMs + gap;
  const uint32_t ph = now % cycle;
  if (ph < (uint32_t)code * oneBlinkMs)
  {
    const uint32_t posInBlink = ph % oneBlinkMs;
    ledWriteLogical(posInBlink < pulse);
  }
  else
  {
    ledWriteLogical(false);
  }
}

static void linkSendLine(const char *s)
{
  link.print(s);
  link.print('\n');
}

static void linkSendPoweroffReq(uint32_t now)
{
  linkSendLine("POWEROFF_REQ");
  lastPowerOffReqSentAtMs = now;
}

static bool lineEquals(const char *line, const char *literal)
{
  return strcmp(line, literal) == 0;
}

static bool lineStartsWith(const char *line, const char *prefix)
{
  while (*prefix)
  {
    if (*line++ != *prefix++)
      return false;
  }
  return true;
}

static const char *levelName(uint8_t lvl)
{
  switch (lvl)
  {
  case LEVEL_DEBUG:
    return "DEBUG";
  case LEVEL_INFO:
    return "INFO";
  case LEVEL_WARNING:
    return "WARNING";
  case LEVEL_ERROR:
    return "ERROR";
  default:
    return "OFF";
  }
}

static uint8_t parseLevelName(const char *s)
{
  if (!strcasecmp(s, "DEBUG"))
    return LEVEL_DEBUG;
  if (!strcasecmp(s, "INFO"))
    return LEVEL_INFO;
  if (!strcasecmp(s, "WARN"))
    return LEVEL_WARNING;
  if (!strcasecmp(s, "WARNING"))
    return LEVEL_WARNING;
  if (!strcasecmp(s, "ERROR"))
    return LEVEL_ERROR;
  if (!strcasecmp(s, "OFF"))
    return LEVEL_OFF;
  if (!strcasecmp(s, "NONE"))
    return LEVEL_OFF;
  return LEVEL_INFO;
}

static uint32_t parseQuietSeconds(const char *line)
{
  const char *p = strstr(line, "seconds=");
  if (p == nullptr)
    return 0;
  p += 8;
  uint32_t v = 0;
  while (*p >= '0' && *p <= '9')
  {
    v = v * 10 + (uint32_t)(*p - '0');
    if (v > (cfg.quietMaxMs / 1000UL) + 10UL)
      return cfg.quietMaxMs / 1000UL;
    p++;
  }
  return v;
}

static void espLog(uint8_t level, const char *msg)
{
  if (level < espLogLevel)
    return;
  link.print("LOG ");
  link.print(levelName(level));
  link.print(' ');
  link.print(msg);
  link.print('\n');
}

static void trimLine(char *s)
{
  if (s == nullptr || *s == '\0')
    return;
  char *start = s;
  while (*start == ' ' || *start == '\t')
    start++;
  if (start != s)
    memmove(s, start, strlen(start) + 1);
  size_t len = strlen(s);
  while (len > 0 && (s[len - 1] == ' ' || s[len - 1] == '\t'))
    s[--len] = '\0';
}

static void ensurePiBootConfirmed(const char *reason)
{
  if (piBootConfirmed)
    return;
  piBootConfirmed = true;
  espLog(LEVEL_INFO, reason);
}

// === COMANDOS CFG ===================================================

static void cfgList()
{
  char buf[64];
  for (uint8_t i = 0; i < CFG_ENTRIES_N; i++)
  {
    snprintf(buf, sizeof(buf), "CFG %s=%lu",
             CFG_ENTRIES[i].key, (unsigned long)cfgValue(&CFG_ENTRIES[i]));
    linkSendLine(buf);
  }
}

static void cfgGet(const char *key)
{
  const CfgEntry *e = cfgFind(key);
  if (e == nullptr)
  {
    linkSendLine("CFG_ERR unknown key");
    return;
  }
  char buf[96];
  snprintf(buf, sizeof(buf), "CFG %s=%lu",
           key, (unsigned long)cfgValue(e));
  linkSendLine(buf);
}

static void cfgSet(const char *args)
{
  const char *eq = strchr(args, '=');
  if (eq == nullptr)
  {
    linkSendLine("CFG_ERR syntax");
    return;
  }
  char key[32];
  size_t n = (size_t)(eq - args);
  if (n >= sizeof(key))
    n = sizeof(key) - 1;
  memcpy(key, args, n);
  key[n] = '\0';

  const CfgEntry *e = cfgFind(key);
  if (e == nullptr)
  {
    linkSendLine("CFG_ERR unknown key");
    return;
  }

  // Igual que v4: en bypass no se puede cambiar el link fisico.
  if (bypassMode &&
      (strcmp(key, "LINK_BACKEND") == 0 ||
       strcmp(key, "PIN_SERIAL_RX") == 0 ||
       strcmp(key, "PIN_SERIAL_TX") == 0 ||
       strcmp(key, "LINK_BAUD") == 0 ||
       strcmp(key, "BYPASS_PIN") == 0))
  {
    linkSendLine("CFG_ERR bypass active");
    return;
  }

  char *endp = nullptr;
  unsigned long v = strtoul(eq + 1, &endp, 10);
  if (endp == eq + 1 || v < e->min || v > e->max)
  {
    linkSendLine("CFG_ERR out of range");
    return;
  }

  // Serial1 esta en silicio: RX=0, TX=1. No se pueden remapear.
  if ((strcmp(key, "PIN_SERIAL_RX") == 0 && v != 0) ||
      (strcmp(key, "PIN_SERIAL_TX") == 0 && v != 1))
  {
    linkSendLine("CFG_ERR fixed pin");
    return;
  }

  if (e->wide)
    *(uint32_t *)e->ptr = (uint32_t)v;
  else
    *(uint8_t *)e->ptr = (uint8_t)v;

  char buf[80];
  snprintf(buf, sizeof(buf), "CFG_ACK %s=%lu%s", key, v,
           e->reboot ? " reboot=1" : "");
  linkSendLine(buf);
}

// Procesa una linea recibida. Actualiza el watchdog SOLO para
// mensajes conocidos, igual que v4.
static void handleRxLine(const char *line, uint32_t now)
{
  if (lineStartsWith(line, "CFG"))
  {
    lastRxLineAtMs = now;
  }

  if (lineEquals(line, "BOOT_OK"))
  {
    lastRxLineAtMs = now;
    quietUntilMs = 0;
    ensurePiBootConfirmed("pi boot_ok");
    return;
  }
  if (lineStartsWith(line, "ALIVE"))
  {
    lastRxLineAtMs = now;
    quietUntilMs = 0;
    ensurePiBootConfirmed("pi boot_ok (alive)");
    return;
  }
  if (lineStartsWith(line, "QUIET"))
  {
    lastRxLineAtMs = now;
    uint32_t secs = parseQuietSeconds(line);
    uint32_t ms = secs * 1000UL;
    if (ms > cfg.quietMaxMs)
      ms = cfg.quietMaxMs;
    quietUntilMs = ms == 0 ? 0 : (now + ms);
    espLog(LEVEL_INFO, line);
    return;
  }
  if (lineStartsWith(line, "BYE"))
  {
    lastRxLineAtMs = now;
    lastByeReceivedAtMs = now;
    espLog(LEVEL_INFO, "pi bye");
    return;
  }
  if (lineEquals(line, "PONG"))
  {
    lastRxLineAtMs = now;
    quietUntilMs = 0;
    ensurePiBootConfirmed("pi boot_ok (pong)");
    return;
  }
  if (lineEquals(line, "CFG"))
  {
    cfgList();
    return;
  }
  if (lineStartsWith(line, "CFG_GET "))
  {
    cfgGet(line + 8);
    return;
  }
  if (lineStartsWith(line, "CFG_SET "))
  {
    cfgSet(line + 8);
    return;
  }
  if (lineEquals(line, "CFG_SAVE"))
  {
    cfgWrite();
    linkSendLine("CFG_SAVED");
    return;
  }
  if (lineEquals(line, "CFG_LOAD"))
  {
    if (cfgRead())
      linkSendLine("CFG_LOADED");
    else
      linkSendLine("CFG_ERR load failed");
    return;
  }
  if (lineEquals(line, "CFG_RESET"))
  {
    cfgLoadDefaults();
    cfgWrite();
    linkSendLine("CFG_DEFAULTS");
    return;
  }
  if (lineStartsWith(line, "LOG_LEVEL "))
  {
    lastRxLineAtMs = now;
    quietUntilMs = 0;
    uint8_t newLvl = parseLevelName(line + 10);
    espLogLevel = newLvl;
    ensurePiBootConfirmed("pi boot_ok (log_level)");
    char ack[32];
    snprintf(ack, sizeof(ack), "LOG_LEVEL_ACK %s", levelName(newLvl));
    linkSendLine(ack);
    return;
  }
}

static void pumpRx(uint32_t now)
{
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
      trimLine(rxBuf);
      if (rxBuf[0] != '\0')
        handleRxLine(rxBuf, now);
      rxLen = 0;
      continue;
    }
    if (rxLen < RX_LINE_MAX)
    {
      rxBuf[rxLen++] = (char)c;
    }
    else
    {
      espLog(LEVEL_DEBUG, "rx overflow");
      rxLen = 0;
      rxBuf[0] = '\0';
    }
  }
}

struct Button
{
  struct TimeEvent
  {
    uint32_t ms;
    uint8_t message;
  };
  uint8_t pin;
  bool state;
  bool lastReading;
  uint32_t lastChangeTime;
  uint32_t pressStartTime;
  TimeEvent events[4];
  uint8_t eventCount;
  uint8_t currentMessage;

  Button(uint8_t p) : pin(p)
  {
    state = false;
    lastReading = HIGH;
    lastChangeTime = 0;
    pressStartTime = 0;
    eventCount = 0;
    currentMessage = 0;
  }

  void begin() { pinMode(pin, INPUT_PULLUP); }

  void setPin(uint8_t p) { pin = p; }

  void syncInitialState()
  {
    delay(cfg.debounceStableMs + 15);
    bool reading = digitalRead(pin);
    lastReading = reading;
    lastChangeTime = millis();
    state = (reading == LOW);
    currentMessage = 0;
    if (state)
      pressStartTime = millis();
  }

  void setTimeAndMessage(uint32_t ms, uint8_t msg)
  {
    if (eventCount >= 4)
      return;
    int i = eventCount - 1;
    while (i >= 0 && events[i].ms > ms)
    {
      events[i + 1] = events[i];
      i--;
    }
    events[i + 1] = {ms, msg};
    eventCount++;
  }

  void update()
  {
    bool reading = digitalRead(pin);
    if (reading != lastReading)
      lastChangeTime = millis();
    lastReading = reading;

    if ((millis() - lastChangeTime) > cfg.debounceStableMs)
    {
      bool stableState = (reading == LOW);
      if (stableState != state)
      {
        state = stableState;
        if (state == true)
        {
          pressStartTime = millis();
          currentMessage = 0;
        }
        else
        {
          uint32_t duration = millis() - pressStartTime;
          currentMessage = getMessageForDuration(duration);
        }
      }
    }
  }

  bool isPressed() { return state; }

  uint8_t getMessage()
  {
    uint8_t msg = currentMessage;
    currentMessage = 0;
    return msg;
  }

  uint8_t getMessageForDuration(uint32_t duration)
  {
    for (int i = eventCount - 1; i >= 0; i--)
    {
      if (duration >= events[i].ms)
        return events[i].message;
    }
    return 0;
  }
};

Button powerBtn(CFG_DEF_BUTTON_PIN);

void setup()
{
  bool cfgFirstBoot = false;
  if (!cfgRead())
  {
    cfgLoadDefaults();
    cfgWrite();
    cfgFirstBoot = true;
  }

  // Ambos links siempre inicializados. Serial1 es el fisico, Serial el USB.
  Serial.begin(cfg.linkBaud);
  Serial1.begin(cfg.linkBaud);
  linkPtr = (cfg.linkBackend == LINK_BACKEND_USB)
                ? usbLinkPtr
                : pinLinkPtr;

  pinMode(cfg.bypassPin, INPUT_PULLUP);
  bypassBtnRaw = bypassBtnStable = (digitalRead(cfg.bypassPin) == LOW);
  bypassBtnLastChangeMs = millis();

  // Fijar nivel OFF ANTES de poner el pin como OUTPUT.
  digitalWrite(cfg.psuOnPin, cfg.psuOnActiveHigh ? LOW : HIGH);
  pinMode(cfg.psuOnPin, OUTPUT);
  pinMode(cfg.pinLed, OUTPUT);
  startLEDSequence();
  setPsu(false);
  ledPsuOffBlinkCode = 0;
  lastPowerOffReqSentAtMs = 0;
  lastByeReceivedAtMs = 0;
  quietUntilMs = 0;
  rxLen = 0;

  char hello[48];
  snprintf(hello, sizeof(hello), "HELLO fw=%s", FIRMWARE_ID);
  linkSendLine(hello);
  espLog(LEVEL_INFO, "teensy ready");
  if (cfgFirstBoot)
    espLog(LEVEL_INFO, "CFG defaults loaded+saved");

  // Los cambios de pines se aplican al reiniciar (lee EEPROM en setup).
  powerBtn.setPin(cfg.buttonPin);
  powerBtn.begin();
  if (cfg.buttonType == MOMENTARY)
  {
    powerBtn.setTimeAndMessage(cfg.momentaryShortMs,
                               MOMENTARY_BUTTON_MSG_TOGGLE_OR_SOFT);
    powerBtn.setTimeAndMessage(cfg.momentaryLongForceOffMs,
                               MOMENTARY_BUTTON_MSG_FORCE_PSU_OFF);
  }
  powerBtn.syncInitialState();
  if (cfg.buttonType == SWITCH)
    switchLastPressed = powerBtn.isPressed();
  else
    switchLastPressed = false;
}

static void powerOnPsu(uint32_t now, const char *why)
{
  if (isPsuOn())
    return;
  espLog(LEVEL_INFO, why);
  ledPsuOffBlinkCode = 0;
  lastPowerOffReqSentAtMs = 0;
  lastByeReceivedAtMs = 0;
  quietUntilMs = 0;
  setPsu(true);
  psuPoweredOnAtMs = now;
  piBootConfirmed = false;
  lastRxLineAtMs = now;
}

static void requestSoftShutdown(uint32_t now, const char *why)
{
  if (!isPsuOn())
    return;
  if (!softShutdownFromButtonAllowed())
  {
    espLog(LEVEL_DEBUG, "soft shutdown ignored: suppress window");
    return;
  }
  espLog(LEVEL_INFO, why);
  linkSendPoweroffReq(now);
}

// === MODO BYPASS ====================================================

#define BYPASS_FADE_CYCLE_MS 2000UL
#define BYPASS_PWM_PERIOD_US 2000UL   // 500 Hz

static void bypassLedUpdate(uint32_t now)
{
  if (!isPsuOn())
  {
    // Bypass activo pero la PSU no prendio: parpadeo rapido.
    ledWriteLogical((now / 100) % 2 == 0);
    return;
  }
  const uint32_t half = BYPASS_FADE_CYCLE_MS / 2;
  const uint32_t ph = now % BYPASS_FADE_CYCLE_MS;
  uint32_t duty = (ph < half)
                      ? (ph * 1023UL) / half
                      : ((BYPASS_FADE_CYCLE_MS - ph) * 1023UL) / half;
  const uint32_t us = micros() % BYPASS_PWM_PERIOD_US;
  ledWriteLogical((us * 1023UL) < (duty * BYPASS_PWM_PERIOD_US));
}

static void bypassEnter(uint32_t now)
{
  // Avisar a la Pi ANTES de conmutar: el token va por Serial1.
  pinLinkPtr->println("MODE BYPASS");
  linkPtr = usbLinkPtr;
  bypassMode = true;
  linkSendLine("BYPASS ON");
  espLog(LEVEL_INFO, "bypass on: psu forced on, watchdog off, link=USB");
  if (!isPsuOn())
  {
    ledPsuOffBlinkCode = 0;
    setPsu(true);
    psuPoweredOnAtMs = now;
  }
  piBootConfirmed = true;
  quietUntilMs = 0;
  lastRxLineAtMs = now;
}

static void bypassExit(uint32_t now)
{
  linkSendLine("BYPASS OFF");
  bypassMode = false;
  linkPtr = (cfg.linkBackend == LINK_BACKEND_USB)
                ? usbLinkPtr
                : pinLinkPtr;
  lastRxLineAtMs = now;
  quietUntilMs = 0;
  // Aviso a la Pi por Serial1 aunque la config diga USB.
  pinLinkPtr->println("MODE NORMAL");
  char bypassMsg[64];
  snprintf(bypassMsg, sizeof(bypassMsg),
           "bypass off: watchdog on, link=%s",
           cfg.linkBackend == LINK_BACKEND_USB ? "USB" : "pins");
  espLog(LEVEL_INFO, bypassMsg);
  if (cfg.buttonType == SWITCH)
  {
    switchLastPressed = powerBtn.isPressed();
    if (!switchLastPressed)
    {
      psuPoweredOnAtMs = 0;
      requestSoftShutdown(now, "bypass off: sw off -> soft shutdown");
    }
  }
}

static void updateBypass(uint32_t now)
{
  const bool raw = (digitalRead(cfg.bypassPin) == LOW);
  if (raw != bypassBtnRaw)
  {
    bypassBtnRaw = raw;
    bypassBtnLastChangeMs = now;
  }
  if (raw == bypassBtnStable)
    return;
  if ((uint32_t)(now - bypassBtnLastChangeMs) < BYPASS_DEBOUNCE_MS)
    return;
  bypassBtnStable = raw;
  if (raw)
  {
    if (bypassMode)
      bypassExit(now);
    else
      bypassEnter(now);
  }
}

// ====================================================================

void loop()
{
  uint32_t now = millis();

  pumpRx(now);
  updateBypass(now);

  if (bypassMode)
  {
    bypassLedUpdate(now);
    return;
  }

  powerBtn.update();

  if (cfg.buttonType == MOMENTARY)
  {
    uint8_t msg = powerBtn.getMessage();
    if (msg == MOMENTARY_BUTTON_MSG_TOGGLE_OR_SOFT)
    {
      if (!isPsuOn())
        powerOnPsu(now, "btn short: psu on");
      else
        requestSoftShutdown(now, "btn short: soft shutdown requested");
    }
    else if (msg == MOMENTARY_BUTTON_MSG_FORCE_PSU_OFF)
    {
      if (isPsuOn())
      {
        espLog(LEVEL_WARNING, "btn long: force psu off");
        ledPsuOffBlinkCode = 0;
        lastPowerOffReqSentAtMs = 0;
        quietUntilMs = 0;
        setPsu(false);
      }
    }
  }
  else // SWITCH
  {
    bool pressed = powerBtn.isPressed();
    if (pressed != switchLastPressed)
    {
      switchLastPressed = pressed;
      if (pressed)
        powerOnPsu(now, "sw on: psu on");
      else
        requestSoftShutdown(now, "sw off: soft shutdown requested");
    }
  }

  if (isPsuOn())
  {
    if (!piBootConfirmed)
    {
      const uint32_t bootCutoffAtMs =
          psuPoweredOnAtMs + cfg.bootConfirmSuppressMs + cfg.bootConfirmCutoffMs;
      if (now >= bootCutoffAtMs)
      {
        espLog(LEVEL_ERROR,
               "boot watchdog: no Pi serial in time, cutting psu");
        ledPsuOffBlinkCode = LED_BLINK_CODE_NEVER_BOOT;
        lastPowerOffReqSentAtMs = 0;
        setPsu(false);
      }
    }
    else
    {
      if (quietUntilMs != 0 && (int32_t)(quietUntilMs - now) > 0)
      {
        // Ventana QUIET activa: no cortar.
      }
      else if (now - lastRxLineAtMs > cfg.aliveTimeoutMs)
      {
        if (quietUntilMs != 0)
        {
          espLog(LEVEL_WARNING, "quiet window expired without return");
          quietUntilMs = 0;
        }
        const bool graceful =
            (lastByeReceivedAtMs != 0 &&
             (now - lastByeReceivedAtMs) <= cfg.softShutdownLedWindowMs) ||
            (lastPowerOffReqSentAtMs != 0 &&
             (now - lastPowerOffReqSentAtMs) <= cfg.softShutdownLedWindowMs);
        if (graceful)
          espLog(LEVEL_INFO, "alive timeout after graceful bye/req, psu off");
        else
          espLog(LEVEL_WARNING, "alive timeout, psu off code=2");
        ledPsuOffBlinkCode = graceful ? 0 : LED_BLINK_CODE_LOST_HEARTBEAT;
        lastPowerOffReqSentAtMs = 0;
        lastByeReceivedAtMs = 0;
        setPsu(false);
      }
    }
  }

  if (cfg.buttonType == MOMENTARY && powerBtn.isPressed())
  {
    ledWriteLogical((now / 100) % 2 == 0);
  }
  else if (!isPsuOn())
  {
    if (ledPsuOffBlinkCode == 0)
      ledWriteLogical(false);
    else
      ledWriteEncodedBlinkCount(now, ledPsuOffBlinkCode);
  }
  else if (!piBootConfirmed)
  {
    ledWriteEncodedBlinkCount(now, LED_BLINK_CODE_WAIT_PI_BOOT_OK);
  }
  else
  {
    ledWriteLogical(true);
  }
}
