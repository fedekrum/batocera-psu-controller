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
#include <SoftwareSerial.h>
#include <string.h>

// === CONFIGURACION ===

// Tipos posibles del control de encendido conectado en BUTTON_PIN:
//   MOMENTARY: pulsador momentáneo. Pulsación corta = toggle /
//              soft-shutdown, pulsación larga (6 s) = corte forzado.
//   SWITCH:    llave biestable (ON/OFF). Flanco OFF->ON enciende la
//              PSU; flanco ON->OFF pide soft-shutdown. NO existe
//              corte forzado desde el control (no hay "pulsación
//              larga" en una llave biestable).
static const uint8_t MOMENTARY = 0;
static const uint8_t SWITCH = 1;

// Boton: Tipo de boton de ON/OFF. MOMENTARY o SWITCH.
static const uint8_t BUTTON_TYPE = SWITCH;

// Pin: control frontal (pulsador o llave), INPUT_PULLUP,
// LOW = apretado (MOMENTARY) o llave en posición ON (SWITCH).
static const uint8_t BUTTON_PIN = D1;

// Pin: salida a transistor / PS_ON ATX.
static const uint8_t PSU_ON_PIN = D2;

// Polaridad del pin PSU_ON_PIN:
//   true  -> la PSU se enciende con GPIO HIGH (driver no inversor,
//            p. ej. transistor NPN con base al pin).
//   false -> la PSU se enciende con GPIO LOW (driver inversor, por
//            ejemplo PS_ON# directo o MOSFET P-ch).
// Si al arrancar el ESP la PSU arranca sola (aun con switch en OFF)
// probablemente esta variable esta al reves.
static const bool PSU_ON_ACTIVE_HIGH = true;

// Pines SoftwareSerial: RX = D5 (desde TX de la Pi), TX = D7
// (hacia RX de la Pi). Ambos 3,3 V.
static const uint8_t PIN_SERIAL_RX = D5;
static const uint8_t PIN_SERIAL_TX = D7;
static const uint32_t LINK_BAUD = 9600;

// Pin: LED azul integrado del modulo LoLin (D4 / GPIO2),
// activo en bajo. Mantener a true solo si se usa LED externo
// con anodo al pin. Nuestro caso: LED interno, dejar false.
static const uint8_t PIN_LED = D4;
static const bool LED_LOGICAL_ON_IS_GPIO_HIGH = false;

// Firmware id (lo anuncia el ESP al arrancar via HELLO fw=..).
static const char *FIRMWARE_ID = "esp-serial-1";

// Botón: tiempo mínimo apretado (ms) para pulsación "corta" al
// soltar (mensaje TOGGLE_OR_SOFT).
static const uint32_t MOMENTARY_BUTTON_HOLD_MS_SHORT_ACTION = 500;

// Botón: tiempo mínimo apretado (ms) para pulsación "larga" al
// soltar → corte forzado PSU (no espera a la Pi).
static const uint32_t MOMENTARY_BUTTON_HOLD_MS_LONG_FORCE_OFF = 6000;

// Tras PSU ON: el botón no puede iniciar apagado suave (enviar
// POWEROFF_REQ por serie) hasta pasar esto.
static const uint32_t MOMENTARY_BUTTON_SOFT_SHUTDOWN_SUPPRESS_MS = 10000;

// Tras PSU ON: no cortar PSU solo porque aún no hubo mensaje Pi
// valido (BOOT_OK / LOG_LEVEL / ALIVE...). Sumado a CUTOFF da el
// total.
static const uint32_t BOOT_CONFIRM_SUPPRESS_MS = 60000;

// Tras SUPPRESS: si piBootConfirmed sigue false, cortar PSU al
// cumplir este lapso. Margen total ≈ SUPPRESS + CUTOFF.
static const uint32_t BOOT_CONFIRM_CUTOFF_MS = 60000;

// Tras boot confirmado: silencio de ALIVE mayor a esto dispara corte
// (watchdog de heartbeat).
static const uint32_t ALIVE_TIMEOUT_MS = 5000;

// Tras POWEROFF_REQ enviado, o BYE recibido: ventana durante la
// cual el corte posterior se considera "normal" (LED apagado).
static const uint32_t SOFT_SHUTDOWN_NORMAL_LED_WINDOW_MS = 180000;

// Tope duro de la ventana QUIET solicitada por la Pi (ms). Si la Pi
// pide mas que esto, se recorta. Protege frente a cuelgues.
static const uint32_t QUIET_MAX_MS = 600000UL;

// Tras setPsu(false): espera (ms) antes de seguir (rebotes /
// ATX).
static const uint32_t PSU_OFF_SETTLE_DELAY_MS = 5000;

// Al reset del ESP: LED fijo ON este tiempo, luego OFF.
// Indica alimentacion antes de bajar el PSU.
static const uint32_t SETUP_LED_BOOT_ON_MS = 2000;

// LED: un parpadeo = LED_ENCODE_PULSE_MS ON y mismo tiempo OFF.
// N parpadeos, luego LED_ENCODE_GAP_AFTER_GROUP_MS apagado.
// Codigos:
//   0 → LED apagado (corte normal).
//   1 → nunca llego trafico valido de la Pi a tiempo.
//   2 → silencio de ALIVE tras boot confirmado.
//   4 (solo PSU ON) → esperando primer mensaje Pi valido (BOOT_OK,
//     LOG_LEVEL, ALIVE, etc.) por UART.
static const uint32_t LED_ENCODE_PULSE_MS = 200;
static const uint32_t LED_ENCODE_GAP_AFTER_GROUP_MS = 1000;
static const uint8_t LED_BLINK_CODE_NEVER_BOOT = 1;
static const uint8_t LED_BLINK_CODE_LOST_HEARTBEAT = 2;
static const uint8_t LED_BLINK_CODE_WAIT_PI_BOOT_OK = 4;

// Buffer maximo de linea RX.
static const uint16_t RX_LINE_MAX = 96;

// Niveles de log (mismos nombres que logging de Python).
static const uint8_t LEVEL_DEBUG = 10;
static const uint8_t LEVEL_INFO = 20;
static const uint8_t LEVEL_WARNING = 30;
static const uint8_t LEVEL_ERROR = 40;
static const uint8_t LEVEL_OFF = 100;

// Default al arrancar, antes de que la Pi negocie un nivel.
// Conviene que sea "hablador" para no perder boot / botones.
static const uint8_t LOG_LEVEL_STARTUP = LEVEL_INFO;

// Mensajes internos del botón.
static const uint8_t MOMENTARY_BUTTON_MSG_TOGGLE_OR_SOFT = 1;
static const uint8_t MOMENTARY_BUTTON_MSG_FORCE_PSU_OFF = 2;

// === FIN CONFIGURACION ===

static SoftwareSerial link(PIN_SERIAL_RX, PIN_SERIAL_TX);

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
  digitalWrite(PIN_LED,
               on == LED_LOGICAL_ON_IS_GPIO_HIGH ? HIGH : LOW);
}

bool isPsuOn()
{
  const int level = digitalRead(PSU_ON_PIN);
  return PSU_ON_ACTIVE_HIGH ? (level == HIGH) : (level == LOW);
}

bool softShutdownFromButtonAllowed()
{
  return (millis() - psuPoweredOnAtMs) >
         MOMENTARY_BUTTON_SOFT_SHUTDOWN_SUPPRESS_MS;
}

void setPsu(bool on)
{
  const int level =
      (on == PSU_ON_ACTIVE_HIGH) ? HIGH : LOW;
  digitalWrite(PSU_ON_PIN, level);
  if (!on)
  {
    delay(PSU_OFF_SETTLE_DELAY_MS);
  }
}

void startLEDSequence()
{
  ledWriteLogical(true);
  delay(SETUP_LED_BOOT_ON_MS);
  ledWriteLogical(false);
}

static void ledWriteEncodedBlinkCount(uint32_t now, uint8_t code)
{
  const uint32_t pulse = LED_ENCODE_PULSE_MS;
  const uint32_t gap = LED_ENCODE_GAP_AFTER_GROUP_MS;
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
    if (v > (QUIET_MAX_MS / 1000UL) + 10UL)
      return QUIET_MAX_MS / 1000UL;
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

// Procesa una linea recibida. Actualiza estado y watchdog SOLO
// para mensajes conocidos: asi el ruido en la UART (glitches,
// crosstalk, bytes sueltos con \n) no mantiene vivo el heartbeat
// ni cancela la ventana QUIET. Lineas desconocidas se descartan.
static void handleRxLine(const char *line, uint32_t now)
{
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
    if (ms > QUIET_MAX_MS)
      ms = QUIET_MAX_MS;
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
  static const uint32_t DEBOUNCE_STABLE_MS = 20;
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
    delay(DEBOUNCE_STABLE_MS + 15);
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

    if ((millis() - lastChangeTime) > DEBOUNCE_STABLE_MS)
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

Button powerBtn(BUTTON_PIN);

void setup()
{
  // Orden importante: fijar nivel OFF ANTES de poner el pin como
  // OUTPUT, para no generar un pulso de encendido espurio durante
  // el boot del ESP (el ATX podria "latchearlo" como encendido).
  digitalWrite(PSU_ON_PIN, PSU_ON_ACTIVE_HIGH ? LOW : HIGH);
  pinMode(PSU_ON_PIN, OUTPUT);
  pinMode(PIN_LED, OUTPUT);
  startLEDSequence();
  setPsu(false);
  ledPsuOffBlinkCode = 0;
  lastPowerOffReqSentAtMs = 0;
  lastByeReceivedAtMs = 0;
  quietUntilMs = 0;
  rxLen = 0;

  link.begin(LINK_BAUD);
  // Identificacion informativa (la Pi puede loguearla).
  char hello[48];
  snprintf(hello, sizeof(hello), "HELLO fw=%s", FIRMWARE_ID);
  linkSendLine(hello);
  espLog(LEVEL_INFO, "esp ready");

  powerBtn.begin();
  // Los mensajes por duracion solo aplican al pulsador momentaneo.
  // En SWITCH, no registramos tiempos: getMessage() devuelve siempre
  // 0 y la logica de flanco se hace aparte (ver loop()).
  if (BUTTON_TYPE == MOMENTARY)
  {
    powerBtn.setTimeAndMessage(MOMENTARY_BUTTON_HOLD_MS_SHORT_ACTION,
                               MOMENTARY_BUTTON_MSG_TOGGLE_OR_SOFT);
    powerBtn.setTimeAndMessage(MOMENTARY_BUTTON_HOLD_MS_LONG_FORCE_OFF,
                               MOMENTARY_BUTTON_MSG_FORCE_PSU_OFF);
  }
  powerBtn.syncInitialState();
  if (BUTTON_TYPE == SWITCH)
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

void loop()
{
  uint32_t now = millis();

  pumpRx(now);
  powerBtn.update();

  if (BUTTON_TYPE == MOMENTARY)
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
  else // BUTTON_TYPE == SWITCH
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
          psuPoweredOnAtMs + BOOT_CONFIRM_SUPPRESS_MS + BOOT_CONFIRM_CUTOFF_MS;
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
      else if (now - lastRxLineAtMs > ALIVE_TIMEOUT_MS)
      {
        if (quietUntilMs != 0)
        {
          espLog(LEVEL_WARNING, "quiet window expired without return");
          quietUntilMs = 0;
        }
        const bool graceful =
            (lastByeReceivedAtMs != 0 &&
             (now - lastByeReceivedAtMs) <= SOFT_SHUTDOWN_NORMAL_LED_WINDOW_MS) ||
            (lastPowerOffReqSentAtMs != 0 &&
             (now - lastPowerOffReqSentAtMs) <= SOFT_SHUTDOWN_NORMAL_LED_WINDOW_MS);
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
  if (BUTTON_TYPE == MOMENTARY && powerBtn.isPressed())
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
