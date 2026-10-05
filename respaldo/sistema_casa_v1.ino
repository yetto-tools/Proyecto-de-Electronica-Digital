/*
  Sistema para Casa (Grupo 1)
  - Riego por horario y nivel de humedad (con histéresis)
  - Luces y cortina por horario
  - Alarma armada por horario
  - Pantalla LCD 16x2 y teclado 4x4 para ver y programar los horarios
    (se guardan en EEPROM)

  Hardware (pensado para simular en Proteus):
  - Arduino Nano v3 (ATmega328P, 16 MHz)
  - RTC DS1307 por I2C (A4 = SDA, A5 = SCL)
  - LCD 16x2 (LM016L) con expansor PCF8574 en el mismo bus I2C, dirección 0x20
  - Teclado 4x4: filas D2,D4,D5,D6 / columnas D11,D12,D13,A1
  - Potenciómetro en A0 que simula el sensor de humedad (0 = seco, 1023 = húmedo)
  - Relé/LED bomba de riego -> D7
  - Relé/LED luces          -> D8
  - Servo de cortina        -> D9
  - Buzzer de alarma        -> D10
  - Sensor de intrusión (botón/PIR) -> D3 (activo en ALTO)

  Teclas (distribución del KEYPAD-SMALLCALC de Proteus):
      7  8  9  A        A = menú programar
      4  5  6  B        B = siguiente        C = anterior
      1  2  3  C        D = armar/desarmar alarma (pide clave)
      *  0  #  D        * = borrar / atrás   # = aceptar

  Librerías: Servo, Keypad, LiquidCrystal I2C (el DS1307 se maneja con un
  controlador propio sobre Wire, sin RTClib, para compilar directo en Proteus)
*/

#include <Wire.h>
#include <EEPROM.h>
#include <Servo.h>
#include <Keypad.h>
#include <LiquidCrystal_I2C.h>

// ---------- Modo demo para Proteus ----------
// 1 = el reloj DS1307 se adelanta solo para ver todos los eventos en pocos minutos.
// 0 = funcionamiento real (hora normal del RTC).
#define MODO_DEMO 1
const uint8_t  DEMO_HORA_INICIO  = 5;       // el demo arranca a las 05:55
const uint8_t  DEMO_MIN_INICIO   = 55;
const uint16_t DEMO_AVANCE_SEG   = 60;      // cada tick se suman 60 s al RTC
const unsigned long DEMO_TICK_MS = 1000;    // un tick por segundo real -> 1 s real = 1 min simulado

// ---------- Pines ----------
const uint8_t PIN_HUMEDAD   = A0;
const uint8_t PIN_BOMBA     = 7;
const uint8_t PIN_LUCES     = 8;
const uint8_t PIN_CORTINA   = 9;
const uint8_t PIN_BUZZER    = 10;
const uint8_t PIN_INTRUSION = 3;

// ---------- Clave de la alarma ----------
const char CLAVE_ALARMA[] = "1234";

// Hora del día leída del RTC
struct Hora {
  uint8_t h, m, s;
};

// ---------- Configuración programable ----------
// Cada ventana es [inicio, fin). Si fin < inicio, cruza la medianoche.
// Si inicio == fin la ventana queda deshabilitada.
struct Ventana {
  uint8_t hIni, mIni;
  uint8_t hFin, mFin;
};

enum IndiceVentana : uint8_t { V_RIEGO_AM, V_RIEGO_PM, V_LUCES, V_CORTINA, V_ALARMA, N_VENTANAS };

struct Config {
  uint8_t magic;
  Ventana v[N_VENTANAS];
  uint8_t humMin;   // % : debajo de este valor se enciende la bomba
  uint8_t humMax;   // % : por encima de este valor se apaga
};

const uint8_t CONFIG_MAGIC = 0xA6;   // cambiar si cambia la estructura Config o los valores por defecto

const Config CONFIG_DEFECTO = {
  CONFIG_MAGIC,
  {
    {  6,  0,  6, 30 },   // riego mañana
    { 18,  0, 18, 30 },   // riego tarde
    { 18, 30, 23,  0 },   // luces
    {  7,  0, 19,  0 },   // cortina abierta
    { 23,  0,  6,  0 }    // alarma armada (cruza medianoche)
  },
  35, 60
};

Config cfg;

// ---------- Cortina (ángulos del servo) ----------
const uint8_t CORTINA_ANG_CERRADA = 0;
const uint8_t CORTINA_ANG_ABIERTA = 90;

// ---------- Periféricos ----------
Servo cortina;
// Dirección del LCD: se detecta en setup() (0x27 = JHD-2X16-I2C, 0x20 = PCF8574 + LM016L)
LiquidCrystal_I2C *lcd;

const byte FILAS = 4, COLUMNAS = 4;
char teclas[FILAS][COLUMNAS] = {
  { '7', '8', '9', 'A' },
  { '4', '5', '6', 'B' },
  { '1', '2', '3', 'C' },
  { '*', '0', '#', 'D' }
};
byte pinesFilas[FILAS]       = { 2, 4, 5, 6 };
byte pinesColumnas[COLUMNAS] = { 11, 12, 13, A1 };
Keypad teclado = Keypad(makeKeymap(teclas), pinesFilas, pinesColumnas, FILAS, COLUMNAS);

// ---------- Estado de los actuadores ----------
bool bombaOn        = false;
bool lucesOn        = false;
bool cortinaAbierta = false;
bool alarmaSonando  = false;
bool alarmaDesarmadaManual = false;  // se limpia al cambiar de estado por horario
bool alarmaArmadaPrev = false;       // la ventana de alarma está activa

unsigned long ultimoCiclo = 0;
unsigned long ultimoTickDemo = 0;
const unsigned long PERIODO_MS = 500;
uint8_t ultimoMinImpreso = 255;

Hora ahoraG;
uint8_t  humG = 0;

// ---------- Estado de la interfaz ----------
enum Pantalla : uint8_t { P_INICIO, P_MENU, P_VENTANA, P_HUMEDAD, P_HORA, P_CLAVE, P_MSG };

Pantalla pantalla    = P_INICIO;
Pantalla pantallaSig = P_INICIO;
uint8_t menuIdx = 0;
uint8_t editIdx = 0;
uint8_t etapa   = 0;          // 0 = primer dato (inicio/mínimo), 1 = segundo (fin/máximo)
char    entrada[5];
uint8_t entradaLen = 0;
Ventana tmpV;
uint8_t tmpHumMin = 0;
bool    redibujar = true;
unsigned long msgFin = 0;
char msgL0[17], msgL1[17];

const uint8_t N_MENU = 7;     // 5 ventanas + humedad + ajustar hora

// ---------- Reloj DS1307 (controlador mínimo sobre Wire) ----------
// Nota: sin métodos ni clases a propósito. El generador de prototipos de
// VSM Studio (Proteus) se confunde con los métodos en línea de un struct.
const uint8_t DS1307_DIR = 0x68;

uint8_t bcdABin(uint8_t v) { return v - 6 * (v >> 4); }
uint8_t binABcd(uint8_t v) { return v + 6 * (v / 10); }

bool rtcBegin() {
  Wire.begin();
  Wire.beginTransmission(DS1307_DIR);
  return Wire.endTransmission() == 0;
}

// Bit CH (bit 7 del registro 0) en 1 = reloj detenido
bool rtcCorriendo() {
  Wire.beginTransmission(DS1307_DIR);
  Wire.write((uint8_t)0);
  Wire.endTransmission();
  Wire.requestFrom(DS1307_DIR, (uint8_t)1);
  return Wire.available() && !(Wire.read() & 0x80);
}

// Lee la hora del DS1307 y la deja en la variable global ahoraG
void rtcLeer() {
  Hora t = {0, 0, 0};
  Wire.beginTransmission(DS1307_DIR);
  Wire.write((uint8_t)0);
  Wire.endTransmission();
  Wire.requestFrom(DS1307_DIR, (uint8_t)3);
  if (Wire.available() < 3) { ahoraG = t; return; }
  uint8_t rs = Wire.read();
  uint8_t rm = Wire.read();
  uint8_t rh = Wire.read();
  t.s = bcdABin(rs & 0x7F);
  t.m = bcdABin(rm & 0x7F);
  if (rh & 0x40) {                         // modo 12 h
    t.h = bcdABin(rh & 0x1F) % 12;
    if (rh & 0x20) t.h += 12;
  } else {
    t.h = bcdABin(rh & 0x3F);
  }
  ahoraG = t;
}

void rtcSetHora(uint8_t h, uint8_t m, uint8_t s) {
  Wire.beginTransmission(DS1307_DIR);
  Wire.write((uint8_t)0);
  Wire.write(binABcd(s));                  // CH = 0: el reloj corre
  Wire.write(binABcd(m));
  Wire.write(binABcd(h));                  // modo 24 h
  Wire.endTransmission();
}

// Adelanta la hora del día (solo se usa en el modo demo)
void rtcAvanzar(uint16_t seg) {
  rtcLeer();
  uint32_t total = (uint32_t)ahoraG.h * 3600 + (uint32_t)ahoraG.m * 60 + ahoraG.s + seg;
  total %= 86400UL;
  rtcSetHora(total / 3600, (total / 60) % 60, total % 60);
}

const char NM0[] PROGMEM = "Riego AM";
const char NM1[] PROGMEM = "Riego PM";
const char NM2[] PROGMEM = "Luces";
const char NM3[] PROGMEM = "Cortina";
const char NM4[] PROGMEM = "Alarma";
const char NM5[] PROGMEM = "Humedad";
const char NM6[] PROGMEM = "Ajustar hora";
const char* const NOMBRES[] PROGMEM = { NM0, NM1, NM2, NM3, NM4, NM5, NM6 };

const char* nombreMenu(uint8_t i) {
  return (const char*)pgm_read_ptr(&NOMBRES[i]);
}

// ---------- EEPROM ----------
void guardarConfig() {
  EEPROM.put(0, cfg);
}

void cargarConfig() {
  EEPROM.get(0, cfg);
  if (cfg.magic != CONFIG_MAGIC) {
    cfg = CONFIG_DEFECTO;
    guardarConfig();
    Serial.println(F("Configuración por defecto cargada"));
  }
}

// ---------- Utilidades de tiempo ----------
uint16_t aMinutos(uint8_t h, uint8_t m) {
  return (uint16_t)h * 60 + m;
}

bool enVentana(uint8_t idx) {
  const Ventana& v = cfg.v[idx];
  uint16_t t   = aMinutos(ahoraG.h, ahoraG.m);
  uint16_t ini = aMinutos(v.hIni, v.mIni);
  uint16_t fin = aMinutos(v.hFin, v.mFin);
  if (ini == fin) return false;
  if (ini < fin) return t >= ini && t < fin;
  return t >= ini || t < fin;   // cruza la medianoche
}

uint8_t leerHumedadPct() {
  return map(analogRead(PIN_HUMEDAD), 0, 1023, 0, 100);
}

// ---------- Acciones ----------
void setBomba(bool on) {
  if (on == bombaOn) return;
  bombaOn = on;
  digitalWrite(PIN_BOMBA, on ? HIGH : LOW);
  Serial.println(on ? F("[EVENTO] Bomba ENCENDIDA") : F("[EVENTO] Bomba APAGADA"));
}

void setLuces(bool on) {
  if (on == lucesOn) return;
  lucesOn = on;
  digitalWrite(PIN_LUCES, on ? HIGH : LOW);
  Serial.println(on ? F("[EVENTO] Luces ENCENDIDAS") : F("[EVENTO] Luces APAGADAS"));
}

void setCortina(bool abrir) {
  if (abrir == cortinaAbierta) return;
  cortinaAbierta = abrir;
  cortina.write(abrir ? CORTINA_ANG_ABIERTA : CORTINA_ANG_CERRADA);
  Serial.println(abrir ? F("[EVENTO] Cortina ABIERTA") : F("[EVENTO] Cortina CERRADA"));
}

void setAlarmaSonando(bool on) {
  if (on == alarmaSonando) return;
  alarmaSonando = on;
  digitalWrite(PIN_BUZZER, on ? HIGH : LOW);
  Serial.println(on ? F("[EVENTO] ALARMA DISPARADA") : F("[EVENTO] Alarma silenciada"));
}

// ---------- Lógica por módulo ----------
void controlRiego(uint8_t humedad) {
  bool horarioRiego = enVentana(V_RIEGO_AM) || enVentana(V_RIEGO_PM);

  if (!horarioRiego) {
    setBomba(false);
  } else if (humedad < cfg.humMin) {
    setBomba(true);
  } else if (humedad > cfg.humMax) {
    setBomba(false);
  }
  // entre humMin y humMax se mantiene el estado actual (histéresis)
}

void controlLuz() {
  setLuces(enVentana(V_LUCES));
  setCortina(enVentana(V_CORTINA));
}

void controlAlarma() {
  bool armada = enVentana(V_ALARMA);

  if (armada != alarmaArmadaPrev) {
    alarmaArmadaPrev = armada;
    alarmaDesarmadaManual = false;
    Serial.println(armada ? F("[EVENTO] Alarma ARMADA") : F("[EVENTO] Alarma DESARMADA por horario"));
    if (!armada) setAlarmaSonando(false);
  }

  if (armada && !alarmaDesarmadaManual && digitalRead(PIN_INTRUSION) == HIGH) {
    setAlarmaSonando(true);
  }
}

// Alterna armado/desarmado manual (solo tiene sentido dentro del horario de alarma)
const char* alternarAlarmaManual() {
  if (!alarmaArmadaPrev) return PSTR("Fuera de horario");
  if (alarmaDesarmadaManual) {
    alarmaDesarmadaManual = false;
    Serial.println(F("[EVENTO] Alarma ARMADA manualmente"));
    return PSTR("Alarma ARMADA");
  }
  alarmaDesarmadaManual = true;
  setAlarmaSonando(false);
  Serial.println(F("[EVENTO] Alarma DESARMADA manualmente"));
  return PSTR("Alarma DESARMADA");
}

void imprimirEstado(uint8_t humedad) {
  char buf[48];
  snprintf(buf, sizeof(buf), "%02u:%02u:%02u | Humedad %3u%% |",
           ahoraG.h, ahoraG.m, ahoraG.s, humedad);
  Serial.print(buf);
  Serial.print(F(" Bomba:")); Serial.print(bombaOn ? F("ON") : F("OFF"));
  Serial.print(F(" Luces:")); Serial.print(lucesOn ? F("ON") : F("OFF"));
  Serial.print(F(" Cortina:")); Serial.print(cortinaAbierta ? F("ABIERTA") : F("CERRADA"));
  Serial.print(F(" Alarma:"));
  Serial.println(alarmaSonando ? F("SONANDO") : (alarmaArmadaPrev ? (alarmaDesarmadaManual ? F("DESARMADA(manual)") : F("ARMADA")) : F("DESARMADA")));
}

// ---------- Pantalla ----------
void linea(uint8_t fila, const char* s) {
  char b[17];
  uint8_t i = 0;
  while (i < 16 && s[i]) { b[i] = s[i]; i++; }
  while (i < 16) b[i++] = ' ';
  b[16] = 0;
  lcd->setCursor(0, fila);
  lcd->print(b);
}

// Arma "HH:MM" con los dígitos escritos y '_' en los que faltan
void campoHora(char* out) {
  const char* e = entrada;
  out[0] = entradaLen > 0 ? e[0] : '_';
  out[1] = entradaLen > 1 ? e[1] : '_';
  out[2] = ':';
  out[3] = entradaLen > 2 ? e[2] : '_';
  out[4] = entradaLen > 3 ? e[3] : '_';
  out[5] = 0;
}

void dibujar() {
  char a[24], b[24], campo[8];

  switch (pantalla) {
    case P_INICIO:
      snprintf(a, sizeof(a), "%02u:%02u:%02u H:%3u%%", ahoraG.h, ahoraG.m, ahoraG.s, humG);
      snprintf(b, sizeof(b), "B%c L%c C%c AL:%s",
               bombaOn ? '1' : '0', lucesOn ? '1' : '0', cortinaAbierta ? 'A' : 'C',
               alarmaSonando ? "SON" : (alarmaArmadaPrev && !alarmaDesarmadaManual ? "ARM" : "OFF"));
      break;

    case P_MENU:
      snprintf_P(a, sizeof(a), PSTR("MENU  B/C  # ok"));
      snprintf_P(b, sizeof(b), PSTR("%u %S"), menuIdx + 1, nombreMenu(menuIdx));
      break;

    case P_VENTANA: {
      const Ventana& v = cfg.v[editIdx];
      snprintf_P(a, sizeof(a), PSTR("%S %S"), nombreMenu(editIdx), etapa == 0 ? PSTR("INICIO") : PSTR("FIN"));
      campoHora(campo);
      if (etapa == 0) snprintf_P(b, sizeof(b), PSTR("Act %02u:%02u %s"), v.hIni, v.mIni, campo);
      else            snprintf_P(b, sizeof(b), PSTR("Act %02u:%02u %s"), v.hFin, v.mFin, campo);
      break;
    }

    case P_HUMEDAD: {
      snprintf_P(a, sizeof(a), etapa == 0 ? PSTR("Humedad MINIMA %%") : PSTR("Humedad MAXIMA %%"));
      uint8_t act = etapa == 0 ? cfg.humMin : cfg.humMax;
      snprintf_P(b, sizeof(b), PSTR("Act %u  Nuevo:%s%s"), act, entrada, entradaLen < 2 ? "_" : "");
      break;
    }

    case P_HORA:
      snprintf_P(a, sizeof(a), PSTR("Ajustar hora"));
      campoHora(campo);
      snprintf_P(b, sizeof(b), PSTR("%02u:%02u Nuevo:%s"), ahoraG.h, ahoraG.m, campo);
      break;

    case P_CLAVE: {
      snprintf_P(a, sizeof(a), PSTR("Clave alarma:"));
      uint8_t i = 0;
      for (; i < entradaLen; i++) b[i] = '*';
      for (; i < 4; i++) b[i] = '_';
      b[i] = 0;
      break;
    }

    case P_MSG:
      strcpy(a, msgL0);
      strcpy(b, msgL1);
      break;
  }

  linea(0, a);
  linea(1, b);
}

void limpiarEntrada() {
  entradaLen = 0;
  entrada[0] = 0;
}

void entrarPantalla(uint8_t p) {
  pantalla = (Pantalla)p;
  limpiarEntrada();
  redibujar = true;
}

// Muestra un mensaje breve (textos en flash) y luego pasa a la pantalla indicada
void mostrarMsg(const char* l0, const char* l1, uint8_t sig) {
  strncpy_P(msgL0, l0, 16); msgL0[16] = 0;
  strncpy_P(msgL1, l1, 16); msgL1[16] = 0;
  pantallaSig = (Pantalla)sig;
  msgFin = millis() + 1300;
  pantalla = P_MSG;
  redibujar = true;
}

// ---------- Teclado ----------
bool parseHora(uint8_t& h, uint8_t& m) {
  if (entradaLen != 4) return false;
  h = (entrada[0] - '0') * 10 + (entrada[1] - '0');
  m = (entrada[2] - '0') * 10 + (entrada[3] - '0');
  return h < 24 && m < 60;
}

void seleccionarMenu() {
  etapa = 0;
  if (menuIdx < N_VENTANAS) {
    editIdx = menuIdx;
    tmpV = cfg.v[editIdx];
    entrarPantalla(P_VENTANA);
  } else if (menuIdx == N_VENTANAS) {
    entrarPantalla(P_HUMEDAD);
  } else {
    entrarPantalla(P_HORA);
  }
}

void aceptarEntrada() {
  uint8_t h, m;

  switch (pantalla) {
    case P_VENTANA:
      if (!parseHora(h, m)) {
        mostrarMsg(PSTR("Hora invalida"), PSTR("Use HHMM (24h)"), P_VENTANA);
        return;
      }
      if (etapa == 0) {
        tmpV.hIni = h; tmpV.mIni = m;
        etapa = 1;
        entrarPantalla(P_VENTANA);
      } else {
        tmpV.hFin = h; tmpV.mFin = m;
        cfg.v[editIdx] = tmpV;
        guardarConfig();
        Serial.print(F("[PROG] ")); Serial.print((const __FlashStringHelper*)nombreMenu(editIdx));
        char buf[24];
        snprintf(buf, sizeof(buf), " %02u:%02u - %02u:%02u", tmpV.hIni, tmpV.mIni, tmpV.hFin, tmpV.mFin);
        Serial.println(buf);
        mostrarMsg(PSTR("Guardado"), PSTR(""), P_MENU);
      }
      break;

    case P_HUMEDAD: {
      if (entradaLen == 0) return;
      uint8_t valor = atoi(entrada);
      if (etapa == 0) {
        if (valor > 99) { mostrarMsg(PSTR("Valor invalido"), PSTR("0 a 99"), P_HUMEDAD); return; }
        tmpHumMin = valor;
        etapa = 1;
        entrarPantalla(P_HUMEDAD);
      } else {
        if (valor <= tmpHumMin || valor > 100) {
          mostrarMsg(PSTR("Max debe ser"), PSTR("mayor que min"), P_HUMEDAD);
          return;
        }
        cfg.humMin = tmpHumMin;
        cfg.humMax = valor;
        guardarConfig();
        Serial.print(F("[PROG] Humedad min/max: "));
        Serial.print(cfg.humMin); Serial.print('/'); Serial.println(cfg.humMax);
        mostrarMsg(PSTR("Guardado"), PSTR(""), P_MENU);
      }
      break;
    }

    case P_HORA:
      if (!parseHora(h, m)) {
        mostrarMsg(PSTR("Hora invalida"), PSTR("Use HHMM (24h)"), P_HORA);
        return;
      }
      rtcSetHora(h, m, 0);
      rtcLeer();
      Serial.println(F("[PROG] Hora del RTC ajustada"));
      mostrarMsg(PSTR("Hora ajustada"), PSTR(""), P_INICIO);
      break;

    case P_CLAVE:
      if (entradaLen == 4 && strcmp(entrada, CLAVE_ALARMA) == 0) {
        mostrarMsg(alternarAlarmaManual(), PSTR(""), P_INICIO);
      } else {
        mostrarMsg(PSTR("Clave incorrecta"), PSTR(""), P_INICIO);
      }
      break;

    default:
      break;
  }
}

void teclaEntrada(char k) {
  uint8_t maxLen = (pantalla == P_HUMEDAD) ? 2 : 4;

  if (k >= '0' && k <= '9') {
    if (entradaLen < maxLen) {
      entrada[entradaLen++] = k;
      entrada[entradaLen] = 0;
    }
  } else if (k == '*') {
    if (entradaLen > 0) entrada[--entradaLen] = 0;
    else if (etapa == 1 && pantalla != P_CLAVE) { etapa = 0; entrarPantalla(pantalla); }
    else entrarPantalla(pantalla == P_CLAVE ? P_INICIO : P_MENU);
  } else if (k == '#') {
    aceptarEntrada();
  }
}

void procesarTecla(char k) {
  redibujar = true;

  switch (pantalla) {
    case P_INICIO:
      if (k == 'A') { menuIdx = 0; entrarPantalla(P_MENU); }
      else if (k == 'D') entrarPantalla(P_CLAVE);
      break;

    case P_MENU:
      if (k == 'B') menuIdx = (menuIdx + 1) % N_MENU;
      else if (k == 'C') menuIdx = (menuIdx + N_MENU - 1) % N_MENU;
      else if (k >= '1' && k <= '7') { menuIdx = k - '1'; seleccionarMenu(); }
      else if (k == '#') seleccionarMenu();
      else if (k == '*' || k == 'A') entrarPantalla(P_INICIO);
      break;

    case P_VENTANA:
    case P_HUMEDAD:
    case P_HORA:
    case P_CLAVE:
      teclaEntrada(k);
      break;

    default:
      break;
  }
}

// ---------- Arduino ----------
void setup() {
  Serial.begin(9600);

  pinMode(PIN_BOMBA, OUTPUT);
  pinMode(PIN_LUCES, OUTPUT);
  pinMode(PIN_BUZZER, OUTPUT);
  pinMode(PIN_INTRUSION, INPUT);

  cortina.attach(PIN_CORTINA);
  cortina.write(CORTINA_ANG_CERRADA);

  // Detecta la dirección del LCD I2C: 0x27 (JHD-2X16-I2C) o 0x20 (PCF8574 + LM016L)
  Wire.begin();
  uint8_t lcdDir = 0x27;
  Wire.beginTransmission(0x27);
  if (Wire.endTransmission() != 0) lcdDir = 0x20;
  lcd = new LiquidCrystal_I2C(lcdDir, 16, 2);
  lcd->init();
  lcd->backlight();
  Serial.print(F("LCD en 0x"));
  Serial.println(lcdDir, HEX);
  linea(0, "Sistema Casa");
  linea(1, "Iniciando...");

  cargarConfig();

  if (!rtcBegin()) {
    Serial.println(F("ERROR: no se encontró el RTC DS1307"));
    linea(0, "ERROR: sin RTC");
    linea(1, "Revise I2C");
    while (true) { }
  }
#if MODO_DEMO
  // Siempre arranca a una hora fija para que la demostración sea repetible
  rtcSetHora(DEMO_HORA_INICIO, DEMO_MIN_INICIO, 0);
  Serial.println(F("MODO DEMO: reloj acelerado (1 s real = 1 min simulado)"));
#else
  if (!rtcCorriendo()) {
    // Ajusta la hora con la de compilación la primera vez ("HH:MM:SS")
    const char* t = __TIME__;
    rtcSetHora((t[0] - '0') * 10 + (t[1] - '0'), (t[3] - '0') * 10 + (t[4] - '0'), (t[6] - '0') * 10 + (t[7] - '0'));
    Serial.println(F("RTC ajustado con la hora de compilación"));
  }
#endif

  // Fuerza que el primer ciclo emita los eventos de estado inicial
  lucesOn = true;
  cortinaAbierta = true;

  rtcLeer();
  humG = leerHumedadPct();
  Serial.println(F("Sistema para Casa iniciado"));
  delay(1000);
}

void loop() {
#if MODO_DEMO
  if (millis() - ultimoTickDemo >= DEMO_TICK_MS) {
    ultimoTickDemo = millis();
    rtcAvanzar(DEMO_AVANCE_SEG);
  }
#endif

  char k = teclado.getKey();
  if (k && pantalla != P_MSG) procesarTecla(k);

  if (pantalla == P_MSG && (long)(millis() - msgFin) >= 0) entrarPantalla(pantallaSig);

  if (millis() - ultimoCiclo >= PERIODO_MS) {
    ultimoCiclo = millis();

    rtcLeer();
    humG = leerHumedadPct();

    controlRiego(humG);
    controlLuz();
    controlAlarma();

    if (ahoraG.m != ultimoMinImpreso) {
      ultimoMinImpreso = ahoraG.m;
      imprimirEstado(humG);
    }
    if (pantalla == P_INICIO) redibujar = true;
  }

  if (redibujar) {
    redibujar = false;
    dibujar();
  }
}
