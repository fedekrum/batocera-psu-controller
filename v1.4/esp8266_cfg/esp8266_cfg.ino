// ============================================================================
// CONTROLADOR DE PSU - VERSION 1.4
// v1.4: timeout ALIVE 60 s en EEPROM, avisos MODE BYPASS / MODE NORMAL,
//     CFG_SET de link bloqueado en bypass, LED con indicador de fallo,
//     salida de bypass con llave OFF sin espera de 10 s.
// Etiqueta documental: no cambia el binario ni el protocolo serial.
// ============================================================================

// Control de fuente ATX + Raspberry Pi — ESP8266 NodeMCU LoLin v3
// Variante SERIAL (v1.2). Comunicacion por SoftwareSerial a 9600 8N1.
// Ver PROTOCOLO.md en esta misma carpeta.
//
// Cableado Pi <-> NodeMCU:
//   Pi pin 7  BCM4 UART3_TX -> D5 (ESP RX en SoftwareSerial)
//   Pi pin 29 BCM5 UART3_RX <- D7 (ESP TX en SoftwareSerial)
//   GND común
//
// ESP en +5V standby ATX (violeta): vive con PSU principal OFF.
//
// Las lineas de comentarios no pueden ser de mas de 64
// caracteres por linea.

#include <Arduino.h>
#if defined(ESP8266)
#include <SoftwareSerial.h>
#endif
#include <string.h>

// === CONFIGURACION (valores configurables por serial) ===
//
// Los valores de abajo son los DEFAULTS de fabrica. En el primer
// arranque se guardan en EEPROM y a partir de ahi se leen de alla.
// Se pueden cambiar en caliente con:
//   CFG, CFG_GET <key>, CFG_SET <key>=<value>, CFG_SAVE,
//   CFG_LOAD, CFG_RESET
// Los cambios de backend/pines/baud requieren reinicio del ESP.
//
// LINK_BACKEND: 0 = SoftwareSerial (pines configurables PIN_SERIAL_RX/TX)
//               1 = UART0 por USB (CH340). Pines FIJOS: RX=GPIO3, TX=GPIO1.

#include <EEPROM.h>
#include "cfg_types.h"
#include <stdlib.h>


ArcadeConfig cfg;

// --- Constantes internas (no configurables) -----------------------

static const uint8_t MOMENTARY = 0;
static const uint8_t SWITCH = 1;
static const uint8_t LINK_BACKEND_SOFT = 0;
static const uint8_t LINK_BACKEND_USB  = 1;
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
static const char *FIRMWARE_ID = "esp-serial-1";

// --- Defaults de fabrica ------------------------------------------

static void cfgLoadDefaults()
{
  cfg.magic = CFG_MAGIC;
  cfg.version = CFG_VERSION;
  cfg.crc = 0;

  cfg.aliveTimeoutMs = 30000;
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
  cfg.ledLogicalOnIsGpioHigh = 0;
  cfg.linkBackend = LINK_BACKEND_SOFT;
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
  EEPROM.begin(sizeof(ArcadeConfig));
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
  EEPROM.commit();
}

// --- Tabla de claves para comandos CFG ----------------------------


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

#if !CFG_HAS_SOFTWARE_SERIAL
HardwareSerial serialLink(1);   // UART1 con pines remapeables
#endif
Stream *linkPtr = nullptr;
Stream *softLinkPtr = nullptr;
#define link (*linkPtr)

// Modo bypass: boton FLASH de la NodeMCU (GPIO0 / D3). RAM only.
#define BYPASS_BUTTON_PIN 0
#define BYPASS_DEBOUNCE_MS 50
bool bypassMode = false;
bool bypassBtnRaw = true;
bool bypassBtnStable = true;
uint32_t bypassBtnLastChangeMs = 0;

// === VARIABLES GLOBALES ===

bool piBootConfirmed = false;
uint32_t psuPoweredOnAtMs = 0;

// Ultimo instante en que llego CUALQUIER linea valida desde la
// Pi (ALIVE, PONG, BOOT_OK, BYE, QUIET, LOG_LEVEL...). Sirve de
// watchdog post-boot.
uint32_t lastRxLineAtMs = 0;

// Codigo N que se muestra en LED con PSU OFF.
uint8_t ledPsuOffBlinkCode = 0;

// millis() del ultimo POWEROFF_REQ enviado a la Pi (evento boton
// corto post-suppress). 0 = nunca.
uint32_t lastPowerOffReqSentAtMs = 0;

// millis() del ultimo BYE recibido de la Pi. 0 = nunca.
uint32_t lastByeReceivedAtMs = 0;

// Ventana QUIET anunciada por la Pi (millis() absolutos). 0 = no
// hay ventana activa. Mientras now < quietUntilMs no se cortara la
// PSU por silencio de ALIVE. Se cancela al recibir una linea
// distinta de QUIET / BYE (el enlace volvio).
uint32_t quietUntilMs = 0;

// Nivel de log actual (threshold). Se baja con LOG_LEVEL.
uint8_t espLogLevel = LOG_LEVEL_STARTUP;

// Modo SWITCH: ultimo estado estable leido, para deteccion de
// flanco. Inicializar en false: si la llave arranca en ON, la
// primera update() detecta el flanco OFF->ON y enciende PSU.
bool switchLastPressed = false;

// Buffer de linea RX.
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
  {
    delay(cfg.psuOffSettleDelayMs);
  }
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

// Envia una linea terminada en \n.
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

// Emite LOG <LVL> <msg> si level >= threshold actual. Filtra en
// origen para no saturar el enlace.
static uint32_t parseQuietSeconds(const char *line)
{
  // Espera un substring "seconds=<N>". Devuelve 0 si no hay o N=0.
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

// Quita espacios/tabs al inicio y final (BOOT_OK con basura).
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

// Pi.py manda BOOT_OK al arranque; si ese paquete se pierde por
// ruido UART al encender, el primer ALIVE o LOG_LEVEL demuestra
// igual que el daemon esta vivo. Sin esto el LED queda en codigo
// 4 aunque la Pi hable bien.
static void ensurePiBootConfirmed(const char *reason)
{
  if (piBootConfirmed)
    return;
  piBootConfirmed = true;
  espLog(LEVEL_INFO, reason);
}


// === COMANDOS CFG =================================================

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

  // En bypass el link fisico es USB: cambiar backend/pines/baud no se
  // puede aplicar hasta salir. Evitamos dejar la Pi sin link al reboot.
  if (bypassMode &&
      (strcmp(key, "LINK_BACKEND") == 0 ||
       strcmp(key, "PIN_SERIAL_RX") == 0 ||
       strcmp(key, "PIN_SERIAL_TX") == 0 ||
       strcmp(key, "LINK_BAUD") == 0))
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

  if (e->wide)
    *(uint32_t *)e->ptr = (uint32_t)v;
  else
    *(uint8_t *)e->ptr = (uint8_t)v;

  char buf[80];
  snprintf(buf, sizeof(buf), "CFG_ACK %s=%lu%s", key, v,
           e->reboot ? " reboot=1" : "");
  linkSendLine(buf);
}

// =================================================================

// Procesa una linea recibida. Actualiza estado y watchdog SOLO
// para mensajes conocidos: asi el ruido en la UART (glitches,
// crosstalk, bytes sueltos con \n) no mantiene vivo el heartbeat
// ni cancela la ventana QUIET. Lineas desconocidas se descartan.
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
  // Desconocido: se ignora (no toca lastRxLineAtMs ni quietUntilMs).
  // No loguear: el ruido podria generar muchas lineas y saturar.
}

// Lee bytes del link y completa lineas. Llama a handleRxLine.
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
      // Overflow: descartar linea hasta el proximo \n.
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
  const uint8_t pin;
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

  // Tras begin(): alinea estado interno con el pin sin generar
  // flancos. Evita que al boot con llave ya en ON (o pulsador
  // apretado) el primer loop dispare powerOnPsu / mensajes.
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

Button *powerBtnPtr = nullptr;
#define powerBtn (*powerBtnPtr)

void setup()
{
  bool cfgFirstBoot = false;
  if (!cfgRead())
  {
    cfgLoadDefaults();
    cfgWrite();
    cfgFirstBoot = true;
  }
  // UART0 siempre inicializado: es el link del modo bypass.
  Serial.begin(cfg.linkBaud);
#if CFG_HAS_SOFTWARE_SERIAL
  SoftwareSerial *ss = new SoftwareSerial(cfg.pinSerialRx, cfg.pinSerialTx);
  ss->begin(cfg.linkBaud);
  softLinkPtr = ss;
#else
  // En ESP32 no hay SoftwareSerial: se usa UART1 con pines
  // remapeados libremente por la GPIO matrix (Serial0 queda
  // reservado para el backend USB).
  serialLink.begin(cfg.linkBaud, SERIAL_8N1,
                   cfg.pinSerialRx, cfg.pinSerialTx);
  softLinkPtr = &serialLink;
#endif
  linkPtr = (cfg.linkBackend == LINK_BACKEND_USB)
                ? (Stream *)&Serial
                : softLinkPtr;

  // Boton FLASH (GPIO0 / D3): modo bypass, solo RAM.
  pinMode(BYPASS_BUTTON_PIN, INPUT_PULLUP);
  bypassBtnRaw = bypassBtnStable = (digitalRead(BYPASS_BUTTON_PIN) == LOW);
  bypassBtnLastChangeMs = millis();

  powerBtnPtr = new Button(cfg.buttonPin);

  // Orden importante: fijar nivel OFF ANTES de poner el pin como
  // OUTPUT, para no generar un pulso de encendido espurio durante
  // el boot del ESP (el ATX podria "latchearlo" como encendido).
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

  // Identificacion informativa (la Pi puede loguearla).
  char hello[48];
  snprintf(hello, sizeof(hello), "HELLO fw=%s", FIRMWARE_ID);
  linkSendLine(hello);
  espLog(LEVEL_INFO, "esp ready");
  if (cfgFirstBoot)
    espLog(LEVEL_INFO, "CFG defaults loaded+saved");

  powerBtn.begin();
  // Los mensajes por duracion solo aplican al pulsador momentaneo.
  // En SWITCH, no registramos tiempos: getMessage() devuelve siempre
  // 0 y la logica de flanco se hace aparte (ver loop()).
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

// Enciende la PSU y pone en estado inicial las variables que
// dependen del ciclo "PSU off -> PSU on". Idempotente: si ya esta
// encendida, no hace nada.
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

// Pide soft-shutdown a la Pi (POWEROFF_REQ). El corte real de PSU
// lo hace la Pi via BYE, o bien uno de los watchdogs si la Pi no
// responde. No corta energia.
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


// === MODO BYPASS (boton FLASH / GPIO0) ==========================
// RAM only: nunca se guarda en EEPROM. Al entrar, PSU fija en ON,
// watchdog desactivado y link conmutado al USB. Al salir, el link
// vuelve a los pines y la PSU retoma el estado de la llave.

// LED: fade in/fade out continuo, ciclo completo de 2 s (1 s subiendo,
// 1 s bajando). PWM por software para no depender de analogWrite/LEDC,
// que cambian de API entre ESP8266 y ESP32 core 3.x/4.x.
#define BYPASS_FADE_CYCLE_MS 2000UL
#define BYPASS_PWM_PERIOD_US 2000UL   // 500 Hz

static void bypassLedUpdate(uint32_t now)
{
  if (!isPsuOn())
  {
    // Parpadeo rapido = bypass activo pero la PSU NO encendio (fallo).
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
  // Avisar a la Pi ANTES de conmutar: el token va por D5/D7, no por USB.
  if (softLinkPtr != nullptr)
    softLinkPtr->println("MODE BYPASS");
  linkPtr = &Serial;
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
  linkSendLine("BYPASS OFF");            // por USB, para el monitor
  bypassMode = false;
  // Restaurar el link CONFIGURADO, no asumir pines: respeta CFG_SET.
  linkPtr = (cfg.linkBackend == LINK_BACKEND_USB)
                ? (Stream *)&Serial
                : softLinkPtr;
  lastRxLineAtMs = now;
  quietUntilMs = 0;
  // Aviso a la Pi por D5/D7 aunque la config diga USB: es su link fisico.
  if (softLinkPtr != nullptr)
    softLinkPtr->println("MODE NORMAL");
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
      // Evita que la suppression de 10 s del boton ignore el apagado
      // cuando entramos y salimos rapidamente con la llave en OFF.
      psuPoweredOnAtMs = 0;
      requestSoftShutdown(now, "bypass off: sw off -> soft shutdown");
    }
  }
}

static void updateBypass(uint32_t now)
{
  const bool raw = (digitalRead(BYPASS_BUTTON_PIN) == LOW);
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

// ================================================================

void loop()
{
  uint32_t now = millis();

  pumpRx(now);
  updateBypass(now);

  if (bypassMode)
  {
    // PSU fija en ON: sin watchdogs ni control por switch.
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
        // Unico camino de corte forzado por control de usuario, y
        // solo existe en modo MOMENTARY (mantener 6 s). En SWITCH
        // este bloque es inalcanzable.
        espLog(LEVEL_WARNING, "btn long: force psu off");
        ledPsuOffBlinkCode = 0;
        lastPowerOffReqSentAtMs = 0;
        quietUntilMs = 0;
        setPsu(false);
      }
    }
  }
  else // cfg.buttonType == SWITCH
  {
    // Deteccion de flanco sobre el estado estable (debounced) de
    // la llave. La llave NO tiene "pulsacion larga" -> no hay
    // camino de corte forzado desde aca. Todos los cortes salen
    // por BYE (Pi) o por watchdogs.
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
        // Ventana QUIET activa: la Pi anuncio silencio temporal
        // (p. ej. reboot). No cortamos la PSU hasta que venza o
        // vuelva trafico distinto de QUIET / BYE.
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

  // LED titilando rapido = "estoy viendo que mantenes apretado el
  // pulsador". Solo tiene sentido con MOMENTARY: en SWITCH el
  // control queda pisado en ON de forma permanente y el LED
  // quedaria titilando para siempre.
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
