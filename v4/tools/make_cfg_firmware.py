#!/usr/bin/env python3
"""Genera esp8266_cfg.ino a partir del firmware original, sin tocarlo."""
import re
import pathlib

SRC = pathlib.Path("/home/fede/pacman/ESP8266/esp8266.ino")
DSTDIR = pathlib.Path("/home/fede/pacman/ESP8266/esp8266_cfg")
DST = DSTDIR / "esp8266_cfg.ino"

text = SRC.read_text(encoding="utf-8")

# nombre original -> campo en cfg
MAP = {
    "MOMENTARY_BUTTON_HOLD_MS_SHORT_ACTION": "momentaryShortMs",
    "MOMENTARY_BUTTON_HOLD_MS_LONG_FORCE_OFF": "momentaryLongForceOffMs",
    "MOMENTARY_BUTTON_SOFT_SHUTDOWN_SUPPRESS_MS": "momentarySoftSuppressMs",
    "BOOT_CONFIRM_SUPPRESS_MS": "bootConfirmSuppressMs",
    "BOOT_CONFIRM_CUTOFF_MS": "bootConfirmCutoffMs",
    "ALIVE_TIMEOUT_MS": "aliveTimeoutMs",
    "SOFT_SHUTDOWN_NORMAL_LED_WINDOW_MS": "softShutdownLedWindowMs",
    "QUIET_MAX_MS": "quietMaxMs",
    "PSU_OFF_SETTLE_DELAY_MS": "psuOffSettleDelayMs",
    "SETUP_LED_BOOT_ON_MS": "setupLedBootOnMs",
    "LED_ENCODE_PULSE_MS": "ledEncodePulseMs",
    "LED_ENCODE_GAP_AFTER_GROUP_MS": "ledEncodeGapMs",
    "DEBOUNCE_STABLE_MS": "debounceStableMs",
    "BUTTON_TYPE": "buttonType",
    "BUTTON_PIN": "buttonPin",
    "PSU_ON_PIN": "psuOnPin",
    "PSU_ON_ACTIVE_HIGH": "psuOnActiveHigh",
    "PIN_SERIAL_RX": "pinSerialRx",
    "PIN_SERIAL_TX": "pinSerialTx",
    "LINK_BAUD": "linkBaud",
    "PIN_LED": "pinLed",
    "LED_LOGICAL_ON_IS_GPIO_HIGH": "ledLogicalOnIsGpioHigh",
}

# Reemplazar usos en todo el archivo (incluye las declaraciones, que luego
# se eliminan al reemplazar el bloque de configuracion).
for old, field in MAP.items():
    text = re.sub(r"\b" + old + r"\b", "cfg." + field, text)

# El debounce era un static const dentro de Button: sacarlo.
text = text.replace(
    "  static const uint32_t cfg.debounceStableMs = 20;\n", ""
)
text = text.replace(
    "    delay(DEBOUNCE_STABLE_MS + 15);", "    delay(cfg.debounceStableMs + 15);"
)

NEW_CONFIG_BLOCK = r'''// === CONFIGURACION (valores configurables por serial) ===
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
#include <stdlib.h>

#define CFG_MAGIC   0x45535043UL   // "ESPC"
#define CFG_VERSION 4

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

struct CfgEntry
{
  const char *key;
  uint8_t wide;       // 1 = uint32, 0 = uint8
  void *ptr;
  uint32_t min;
  uint32_t max;
  bool reboot;
};

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
'''

start_marker = "// === CONFIGURACION ==="
end_marker = "// === FIN CONFIGURACION ===\n"
i = text.index(start_marker)
j = text.index(end_marker, i) + len(end_marker)
text = text[:i] + NEW_CONFIG_BLOCK + text[j:]

VERSION_BANNER = """// ============================================================================
// CONTROLADOR DE PSU - VERSION 4
// v4: timeout ALIVE 60 s en EEPROM, avisos MODE BYPASS / MODE NORMAL,
//     CFG_SET de link bloqueado en bypass, LED con indicador de fallo,
//     salida de bypass con llave OFF sin espera de 10 s.
// Etiqueta documental: no cambia el binario ni el protocolo serial.
// ============================================================================

"""
text = VERSION_BANNER + text

# SoftwareSerial no existe en el core ESP32: solo se incluye en ESP8266
# (cfg_types.h ya lo hace, pero se guarda tambien aca por claridad).
text = text.replace(
    "#include <SoftwareSerial.h>\n",
    "#if defined(ESP8266)\n#include <SoftwareSerial.h>\n#endif\n",
    1,
)

# Los objetos con pines configurables se crean en setup(), porque la EEPROM
# se lee ahi y el constructor global correria con valores viejos.
text = text.replace(
    "static SoftwareSerial link(cfg.pinSerialRx, cfg.pinSerialTx);",
    "#if !CFG_HAS_SOFTWARE_SERIAL\n"
    "HardwareSerial serialLink(1);   // UART1 con pines remapeables\n"
    "#endif\n"
    "Stream *linkPtr = nullptr;\n"
    "Stream *softLinkPtr = nullptr;\n"
    "#define link (*linkPtr)\n"
    "\n"
    "// Modo bypass: boton FLASH de la NodeMCU (GPIO0 / D3). RAM only.\n"
    "#define BYPASS_BUTTON_PIN 0\n"
    "#define BYPASS_DEBOUNCE_MS 50\n"
    "bool bypassMode = false;\n"
    "bool bypassBtnRaw = true;\n"
    "bool bypassBtnStable = true;\n"
    "uint32_t bypassBtnLastChangeMs = 0;",
)
text = text.replace(
    "Button powerBtn(cfg.buttonPin);",
    "Button *powerBtnPtr = nullptr;\n#define powerBtn (*powerBtnPtr)",
)

# Cargo la configuracion y creo los objetos al arranque de setup().
text = text.replace(
    "void setup()\n{\n  // Orden importante:",
    """void setup()
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

  // Orden importante:""",
)

text = text.replace(
    '  espLog(LEVEL_INFO, "esp ready");\n',
    '  espLog(LEVEL_INFO, "esp ready");\n'
    '  if (cfgFirstBoot)\n'
    '    espLog(LEVEL_INFO, "CFG defaults loaded+saved");\n',
)

# begin() ya se llama sobre el objeto concreto (Serial / SoftwareSerial /
# HardwareSerial UART1), no sobre el puntero Stream generico.
text = text.replace("  link.begin(cfg.linkBaud);\n", "")

# Comandos CFG dentro de handleRxLine, antes del LOG_LEVEL para que
# "CFG" no sea confundido con ningun prefijo existente.
CFG_HANDLERS = r'''  if (lineEquals(line, "CFG"))
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
'''
text = text.replace(
    '  if (lineStartsWith(line, "LOG_LEVEL "))\n',
    CFG_HANDLERS + '  if (lineStartsWith(line, "LOG_LEVEL "))\n',
)

# Implementacion de CFG_GET / CFG_SET / CFG_LIST antes de handleRxLine.
CFG_FUNCS = r'''
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

'''
text = text.replace(
    "// Procesa una linea recibida. Actualiza estado y watchdog SOLO",
    CFG_FUNCS + "// Procesa una linea recibida. Actualiza estado y watchdog SOLO",
)

# Los comandos CFG son trafico valido de la Pi: mantienen vivo el watchdog.
CFG_WATCHDOG = '''  if (lineStartsWith(line, "CFG"))
  {
    lastRxLineAtMs = now;
  }

'''
text = text.replace(
    "static void handleRxLine(const char *line, uint32_t now)\n{\n",
    "static void handleRxLine(const char *line, uint32_t now)\n{\n" + CFG_WATCHDOG,
)

# Modo bypass: toggle por boton FLASH (GPIO0). RAM only, nunca EEPROM.
BYPASS_FUNCS = r'''
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

'''
text = text.replace("void loop()\n{", BYPASS_FUNCS + "void loop()\n{", 1)

text = text.replace(
    "void loop()\n{\n  uint32_t now = millis();\n\n  pumpRx(now);\n  powerBtn.update();",
    "void loop()\n{\n  uint32_t now = millis();\n\n"
    "  pumpRx(now);\n"
    "  updateBypass(now);\n\n"
    "  if (bypassMode)\n"
    "  {\n"
    "    // PSU fija en ON: sin watchdogs ni control por switch.\n"
    "    bypassLedUpdate(now);\n"
    "    return;\n"
    "  }\n\n"
    "  powerBtn.update();",
    1,
)

# Arduino genera prototipos de funciones antes de las definiciones de
# tipos del .ino, asi que las structs deben vivir en un header incluido
# al principio. Si no, "CfgEntry does not name a type".
import re as _re

m_cfgstruct = _re.search(r"struct ArcadeConfig\n\{.*?\n\};\n", text, _re.S)
m_entrystruct = _re.search(r"struct CfgEntry\n\{.*?\n\};\n", text, _re.S)
assert m_cfgstruct and m_entrystruct, "no se encontraron las structs"

PLATFORM_DEFS = """// --- Capa de plataforma -------------------------------------------
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

"""

header = (
    "#pragma once\n"
    "#include <Arduino.h>\n"
    "#include <EEPROM.h>\n\n"
    "#define CFG_MAGIC   0x45535043UL   // \"ESPC\"\n"
    "#define CFG_VERSION 4\n\n"
    + PLATFORM_DEFS
    + m_cfgstruct.group(0)
    + "\n"
    + m_entrystruct.group(0)
)
(DSTDIR / "cfg_types.h").write_text(header, encoding="utf-8")

text = text.replace(m_cfgstruct.group(0), "")
text = text.replace(m_entrystruct.group(0), "")
text = text.replace(
    "#define CFG_MAGIC   0x45535043UL   // \"ESPC\"\n#define CFG_VERSION 4\n\n", ""
)
text = text.replace(
    "#include <EEPROM.h>\n",
    '#include <EEPROM.h>\n#include "cfg_types.h"\n',
    1,
)

DSTDIR.mkdir(parents=True, exist_ok=True)
DST.write_text(text, encoding="utf-8")

print(f"generado: {DST} ({DST.stat().st_size} bytes, {len(text.splitlines())} lineas)")
for old in MAP:
    remaining = len(re.findall(r"\b" + old + r"\b", text))
    if remaining:
        print(f"  OJO: queda '{old}' {remaining} vez/veces")
