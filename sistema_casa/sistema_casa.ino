/*
  Sistema para Casa (Grupo 1)
  - Iluminación automática: enciende y apaga las luces según la luz ambiente (LDR)
  - Riego del jardín programado por horario Y nivel de humedad (con histéresis)
  - Cortinas con motor DC (puente H L293D): abrir y cerrar por horario o manualmente
  - Alarma de seguridad armada por horario (sensor de intrusión + clave por teclado)
  - LCD 20x4 con pantalla de estado y menú de configuración
  - Teclado 4x4 para programar horarios y umbrales (se guardan en EEPROM)

  Hardware (pensado para simular en Proteus 8.13):
  - Arduino Nano v3 (ATmega328P, 16 MHz)
  - LCD 20x4 (LM044L) con expansor PCF8574A por I2C (A4 = SDA, A5 = SCL), dirección 0x38
  - LDR en A0: +5 V - LDR - A0 - 10 kΩ - GND (más luz = más voltaje)
  - Potenciómetro en A6 que simula el sensor de humedad del suelo (0 = seco, 1023 = húmedo)
  - Teclado 4x4: filas D2,D4,D5,D6 / columnas D11,D12,A2,A1
  - Bomba de riego (relé/LED)   -> D7
  - Luces (LED)                 -> D8
  - Motor de cortina (L293D)    -> IN1 (abrir) D9, IN2 (cerrar) D10
  - Display 7 seg (tiempo de riego, MM:SS): 4 x 74HC595 en cadena -> SER D3, SRCLK A3, RCLK D13
  - Buzzer de alarma y sensor de intrusión (botón/PIR): en un expansor PCF8574/PCF8574A
    propio (0x20 u 0x27) en el mismo bus I2C — no quedaba ningún pin digital libre del Nano.
    P0 = buzzer (salida), P1 = sensor de intrusión (entrada, activo en BAJO por el pull-up
    interno del PCF8574: botón/PIR a GND dispara la alarma).

  Teclas (KEYPAD-SMALLCALC de Proteus):
      1   2   3   F0        F0 = menú
      4   5   6   F1        F1 = siguiente (menú) / ajustar hora (estado)
      7   8   9   F2        F2 = anterior (menú) / velocidad del reloj (estado)
      ON/C 0  OK  F3        F3 = modo de luces (AUTO / ON / OFF)
      ON/C = borrar / atrás      OK = aceptar
      En la pantalla de estado: 1 = abrir cortina, 2 = cerrar, 0 = detener motor,
      3 = armar/desarmar alarma (pide clave, ver CLAVE_ALARMA)

  Reloj: contador de 24 h (HH:MM:SS) que corre enteramente en software con el Timer1 del
  ATmega328P (CTC, 100 Hz); no hay RTC externo. Al arrancar se fija a HORA_ARRANQUE:MIN_ARRANQUE
  (no hay batería que conserve la hora) y se corrige desde el teclado (F1). Los eventos (riego,
  luces, cortina, alarma) ocurren a las horas programadas; no hay fecha.

  Librerías: Keypad y LcdI2C (propia).

  Archivo único a propósito: el proyecto de VSM Studio (Proteus) solo admite un `main.ino`
  (ver CLAUDE.md). Por eso todo el sketch vive aquí, en un solo archivo.
*/

#include <Wire.h>
#include <EEPROM.h>
#include <avr/sleep.h>
#include <Keypad.h>
#include <LcdI2C.h>

// ---------- Reloj ----------
// Velocidad del reloj: segundos simulados por cada segundo real (se cambia desde
// el menú "Velocidad reloj"). Para uso real, elegir x1.
const uint16_t VELOCIDADES[4] = { 1, 10, 60, 300 };
uint16_t velocidad = 1;                    // velocidad al arrancar
// Hora fijada en cada arranque (no hay RTC con batería que la conserve); ajustar con F1.
const uint8_t HORA_ARRANQUE = 14, MIN_ARRANQUE = 38;

// ---------- Depuración ----------
// 1 = mensajes [DBG] por serie (etapas del arranque y una línea por segundo real) y los
// dos puntos del display de 7 segmentos parpadean cada segundo real (si parpadean, el
// bucle principal corre). Con 0 los dos puntos quedan fijos.
#define DEPURAR 0

// ---------- Display de 7 segmentos: tiempo de riego (MM:SS) ----------
// 4 x 74HC595 en cadena (uno por dígito, cátodo común), sin multiplexar.
// Cadena: Nano -> 595 #1 (decenas de minuto) -> #2 -> #3 -> #4 (unidades de segundo).
const uint8_t PIN_7S_DATO  = 3;         // SER   (pin 14 del primer 595)
const uint8_t PIN_7S_RELOJ = A3;        // SRCLK (pin 11 de los cuatro)
const uint8_t PIN_7S_LATCH = 13;        // RCLK  (pin 12 de los cuatro)
// Salidas de cada 595: QA..QG = segmentos a..g, QH = punto decimal
const uint8_t SEG7[10] = { 0x3F, 0x06, 0x5B, 0x4F, 0x66, 0x6D, 0x7D, 0x07, 0x7F, 0x6F };

// ---------- Pines ----------
const uint8_t PIN_LDR            = A0;
const uint8_t PIN_HUMEDAD        = A6;   // potenciómetro: simula el sensor de humedad del suelo
const uint8_t PIN_BOMBA          = 7;
const uint8_t PIN_LUCES          = 8;
const uint8_t PIN_CORTINA_ABRIR  = 9;    // IN1 del L293D
const uint8_t PIN_CORTINA_CERRAR = 10;   // IN2 del L293D

// Tiempo que el motor de la cortina gira en cada movimiento
const unsigned long CORTINA_MS = 4000;

// Clave para armar/desarmar la alarma manualmente desde el teclado
const char CLAVE_ALARMA[] = "1234";

// Hora del día (contador de 24 h en software, sin RTC externo)
struct Hora {
  uint8_t h, m, s;
};

// ---------- Configuración programable ----------
// Cada ventana es [inicio, fin). Si fin < inicio, cruza la medianoche.
// Si inicio == fin la ventana queda deshabilitada.
// Cortina: inicio = hora de abrir, fin = hora de cerrar.
struct Ventana {
  uint8_t hIni, mIni;
  uint8_t hFin, mFin;
};

enum IndiceVentana : uint8_t { V_RIEGO_AM, V_RIEGO_PM, V_CORTINA, V_ALARMA, N_VENTANAS };

struct Config {
  uint8_t magic;
  Ventana v[N_VENTANAS];
  uint8_t luzOn;    // % : por debajo de este valor (oscuro) se encienden las luces
  uint8_t luzOff;   // % : por encima de este valor (claro) se apagan
  uint8_t humMin;   // % : por debajo de este valor (seco) se enciende la bomba
  uint8_t humMax;   // % : por encima de este valor (húmedo) se apaga
};

const uint8_t CONFIG_MAGIC = 0xA8;   // cambiar si cambia Config o sus valores por defecto

const Config CONFIG_DEFECTO = {
  CONFIG_MAGIC,
  {
    {  6,  0,  6, 30 },   // riego mañana
    { 18,  0, 18, 30 },   // riego tarde
    {  7,  0, 19,  0 },   // cortina: abre 07:00, cierra 19:00
    { 23,  0,  6,  0 }    // alarma armada (cruza medianoche)
  },
  30, 45,
  35, 60
};

Config cfg;

// ---------- Periféricos ----------
LcdI2C *lcd;                            // pantalla principal 20x4
LcdI2C *lcd2 = nullptr;                 // pantalla 16x2 con expansor PCF8574 (solo si LCD2_JHD = 0)
const uint8_t LCD2_DIR = 0x3E;          // 7 bits ($7C en el I2C Debugger); si no responde, se busca otro
uint8_t lcd2Dir = 0;                    // dirección encontrada de la pantalla 16x2 (0 = no hay)
// 1 = módulo JHD-2X16-I2C de Proteus (protocolo de comandos: byte de control 0x80 = comando,
//     0x40 = datos); 0 = pantalla 16x2 con expansor PCF8574 (misma librería LcdI2C que la 20x4)
#define LCD2_JHD 1

const byte FILAS = 4, COLUMNAS = 4;
// Teclado: 1 2 3 F0 / 4 5 6 F1 / 7 8 9 F2 / ON/C 0 OK F3
// A..D = F0..F3, X = ON/C, K = OK
char teclas[FILAS][COLUMNAS] = {
  { '1', '2', '3', 'A' },
  { '4', '5', '6', 'B' },
  { '7', '8', '9', 'C' },
  { 'X', '0', 'K', 'D' }
};
byte pinesFilas[FILAS]       = { 2, 4, 5, 6 };
byte pinesColumnas[COLUMNAS] = { 11, 12, A2, A1 };
Keypad teclado = Keypad(makeKeymap(teclas), pinesFilas, pinesColumnas, FILAS, COLUMNAS);

// ---------- Expansor I2C de la alarma (buzzer + sensor de intrusión) ----------
// PCF8574/PCF8574A dedicado: no quedaba ningún pin digital libre del Nano.
// P0 = buzzer (salida); P1 = sensor de intrusión (entrada cuasi-bidireccional: se
// escribe en 1 para que el pull-up interno la deje en alto, el sensor la lleva a GND).
const uint8_t BIT_BUZZER    = 0x01;
const uint8_t BIT_INTRUSION = 0x02;
uint8_t alarmaDir = 0;                // dirección encontrada (0 = no hay expansor, alarma deshabilitada)
uint8_t alarmaExpEstado = BIT_INTRUSION;

// ---------- Estado de los actuadores ----------
bool bombaOn = false;
bool lucesOn = false;
uint8_t modoLuz = 0;                 // 0 = AUTO, 1 = ON manual, 2 = OFF manual

uint8_t cortinaMov = 0;              // 0 = detenida, 1 = abriendo, 2 = cerrando
bool cortinaAbierta = false;         // última posición alcanzada
bool cortinaVentPrev = false;        // estado anterior de la ventana de cortina
unsigned long cortinaFin = 0;

bool alarmaSonando = false;
bool alarmaDesarmadaManual = false;  // se limpia al cambiar de estado por horario
bool alarmaArmadaPrev = false;       // la ventana de alarma está activa

unsigned long ultimoCiclo = 0;
uint8_t tickTeclado = 0;             // cuenta ticks de 10 ms para sondear el teclado a ~33 Hz

// Base de tiempo: Timer1 en modo CTC a 100 Hz (16 MHz / 64 / 2500). Lo usa el reloj en
// software; el Timer0 queda para millis()/delay()/Keypad.
volatile uint8_t tick10ms = 0;       // 1 = pasaron 10 ms
volatile uint8_t cuenta100 = 0;
volatile uint8_t segPend = 0;        // segundos reales pendientes de aplicar al reloj

ISR(TIMER1_COMPA_vect) {
  tick10ms = 1;
  if (++cuenta100 >= 100) {
    cuenta100 = 0;
    if (segPend < 255) segPend++;
  }
}
const unsigned long PERIODO_MS = 500;
uint8_t ultimoMinImpreso = 255;

Hora ahoraG;                         // hora del sistema (contador de 24 h en software)
uint8_t luzG = 0;
uint8_t humG = 0;                    // última lectura de humedad del suelo (%)
uint16_t refMin = 0xFFFF;            // próximo inicio (o fin si está regando) de riego
uint16_t riegoSeg = 0;               // segundos (simulados) que lleva regando; conserva el último valor
bool     dosPuntos = true;           // los dos puntos del display (latido en modo DEPURAR)

// ---------- Estado de la interfaz ----------
enum Pantalla : uint8_t { P_INICIO, P_MENU, P_VENTANA, P_UMBRAL, P_HORA, P_VELOC, P_CLAVE, P_MSG };

Pantalla pantalla    = P_INICIO;
Pantalla pantallaSig = P_INICIO;
uint8_t menuIdx = 0;
uint8_t editIdx = 0;
uint8_t etapa   = 0;          // 0 = primer dato (inicio/oscuro), 1 = segundo (fin/claro)
char    entrada[5];
uint8_t entradaLen = 0;
Ventana tmpV;
uint8_t tmpUmbralMin = 0;
uint8_t tipoUmbral = 0;       // 0 = luz (P_UMBRAL edita luzOn/luzOff), 1 = humedad (humMin/humMax)
bool    redibujar = true;
unsigned long msgFin = 0;
char msgL0[21], msgL1[21];
char pantallaLcd[4][20];   // lo que muestra el LCD ahora (0 = desconocido, fuerza la escritura)

// Menú: riego AM, riego PM, cortinas, alarma, umbral de luz, umbral de humedad, ajustar hora, velocidad
const uint8_t N_MENU = 8;
const uint8_t M_UMBRAL = 4, M_HUMEDAD = 5, M_HORA = 6, M_VELOC = 7;

// ---------- Reloj en software ----------
// Contador de 24 h (HH:MM:SS) que vuelve a 00:00:00 al llegar a 24:00:00. Suma `seg`
// segundos a ahoraG; es la única base de tiempo del sistema (la avanza el Timer1,
// ver ISR TIMER1_COMPA_vect).
void relojAvanzar(uint32_t seg) {
  uint32_t total = ((uint32_t)ahoraG.h * 3600 + (uint32_t)ahoraG.m * 60 + ahoraG.s + seg) % 86400UL;
  ahoraG.h = total / 3600;
  ahoraG.m = (total / 60) % 60;
  ahoraG.s = total % 60;
}

// ---------- Módulo JHD-2X16-I2C (protocolo de comandos) ----------
void jhdComando(uint8_t c) {
  Wire.beginTransmission(lcd2Dir);
  Wire.write((uint8_t)0x80);               // byte de control: un comando
  Wire.write(c);
  Wire.endTransmission();
}

// Escribe `s` (hasta 16 caracteres) desde el inicio de la fila 0 o 1
void jhdTexto(uint8_t fila, const char* s) {
  jhdComando(0x80 | (fila ? 0x40 : 0x00));  // posición DDRAM
  Wire.beginTransmission(lcd2Dir);
  Wire.write((uint8_t)0x40);               // byte de control: datos
  for (uint8_t i = 0; i < 16 && s[i]; i++) Wire.write((uint8_t)s[i]);
  Wire.endTransmission();
}

void jhdInicio() {
  delay(50);
  jhdComando(0x38);                        // 2 líneas, 5x8
  delay(5);
  jhdComando(0x38);
  jhdComando(0x0C);                        // pantalla encendida, sin cursor
  jhdComando(0x01);                        // borrar
  delay(3);
  jhdComando(0x06);                        // el cursor avanza a la derecha
}

// Escribe `s` en la fila de la pantalla 16x2, sea cual sea su tipo
void lcd2Texto(uint8_t fila, const char* s) {
#if LCD2_JHD
  jhdTexto(fila, s);
#else
  if (!lcd2) return;
  lcd2->setCursor(0, fila);
  lcd2->print(s);
#endif
}

void iniciarLcd2(uint8_t lcdDirPrincipal, uint8_t lcdDirAlarma) {
  // Primero LCD2_DIR; si no responde, cualquier otro expansor PCF8574/PCF8574A del
  // bus (0x20-0x27 o 0x38-0x3F) distinto del de la 20x4 y del de la alarma
  Wire.beginTransmission(LCD2_DIR);
  if (LCD2_DIR != lcdDirPrincipal && LCD2_DIR != lcdDirAlarma && Wire.endTransmission() == 0) lcd2Dir = LCD2_DIR;
  for (uint8_t d = 0x20; !lcd2Dir && d <= 0x3F; d++) {
    if (d == lcdDirPrincipal || d == lcdDirAlarma || (d > 0x27 && d < 0x38)) continue;
    Wire.beginTransmission(d);
    if (Wire.endTransmission() == 0) lcd2Dir = d;
  }
  if (lcd2Dir) {
#if LCD2_JHD
    jhdInicio();
#else
    lcd2 = new LcdI2C(lcd2Dir, 16, 2);
    lcd2->begin();
    lcd2->backlight();
#endif
    lcd2Texto(0, "Hora del sistema");
    Serial.print(F("LCD 16x2 en 0x"));
    Serial.println(lcd2Dir, HEX);
  } else {
    Serial.println(F("LCD 16x2 no encontrado (opcional)"));
  }
}

// Pantalla 16x2: muestra la hora del sistema (ahoraG)
void actualizarLcd2() {
  if (!lcd2Dir) return;
  char buf[17];
  snprintf(buf, sizeof(buf), "    %02u:%02u:%02u    ", ahoraG.h, ahoraG.m, ahoraG.s);
  lcd2Texto(1, buf);
}

// ---------- Alarma (armada por horario, expansor I2C para buzzer y sensor) ----------
void alarmaExpWrite() {
  Wire.beginTransmission(alarmaDir);
  Wire.write(alarmaExpEstado);
  Wire.endTransmission();
}

// true si el sensor de intrusión está disparado (pin en bajo)
bool leerIntrusion() {
  Wire.requestFrom(alarmaDir, (uint8_t)1);
  if (!Wire.available()) return false;
  uint8_t v = Wire.read();
  return !(v & BIT_INTRUSION);
}

void setAlarmaSonando(bool on) {
  if (on == alarmaSonando) return;
  alarmaSonando = on;
  if (on) alarmaExpEstado |= BIT_BUZZER;
  else    alarmaExpEstado &= ~BIT_BUZZER;
  if (alarmaDir) alarmaExpWrite();
  Serial.println(on ? F("[EVENTO] ALARMA DISPARADA") : F("[EVENTO] Alarma silenciada"));
}

void controlAlarma() {
  if (!alarmaDir) return;   // sin expansor: la alarma queda deshabilitada
  bool armada = enVentana(V_ALARMA);
  if (armada != alarmaArmadaPrev) {
    alarmaArmadaPrev = armada;
    alarmaDesarmadaManual = false;
    Serial.println(armada ? F("[EVENTO] Alarma ARMADA") : F("[EVENTO] Alarma DESARMADA por horario"));
    if (!armada) setAlarmaSonando(false);
  }
  if (armada && !alarmaDesarmadaManual && leerIntrusion()) {
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

const char NM0[] PROGMEM = "Riego manana";
const char NM1[] PROGMEM = "Riego tarde";
const char NM2[] PROGMEM = "Cortinas";
const char NM3[] PROGMEM = "Alarma";
const char NM4[] PROGMEM = "Umbral de luz";
const char NM5[] PROGMEM = "Umbral de humedad";
const char NM6[] PROGMEM = "Ajustar hora";
const char NM7[] PROGMEM = "Velocidad reloj";
const char* const NOMBRES[] PROGMEM = { NM0, NM1, NM2, NM3, NM4, NM5, NM6, NM7 };

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

uint8_t leerLuzPct() {
  return map(analogRead(PIN_LDR), 0, 1023, 0, 100);
}

uint8_t leerHumedadPct() {
  return map(analogRead(PIN_HUMEDAD), 0, 1023, 0, 100);
}

// Deja en refMin el fin de la ventana de riego activa o, si no hay, el próximo inicio
void calcularRefRiego() {
  uint16_t t = aMinutos(ahoraG.h, ahoraG.m);
  uint16_t mejorD = 0xFFFF;
  refMin = 0xFFFF;
  for (uint8_t i = 0; i < 2; i++) {
    uint16_t ini = aMinutos(cfg.v[i].hIni, cfg.v[i].mIni);
    uint16_t fin = aMinutos(cfg.v[i].hFin, cfg.v[i].mFin);
    if (ini == fin) continue;
    if (enVentana(i)) { refMin = fin; return; }
    uint16_t d = (ini + 1440 - t) % 1440;
    if (d < mejorD) { mejorD = d; refMin = ini; }
  }
}

// ---------- Display de 7 segmentos ----------
// Muestra MM:SS (máx. 99:59). El punto decimal del 2.º dígito hace de dos puntos.
void mostrar7seg(uint16_t seg) {
  uint8_t m = seg / 60 > 99 ? 99 : seg / 60;
  uint8_t s = seg % 60;
  digitalWrite(PIN_7S_LATCH, LOW);
  shiftOut(PIN_7S_DATO, PIN_7S_RELOJ, MSBFIRST, SEG7[s % 10]);        // llega al 595 #4
  shiftOut(PIN_7S_DATO, PIN_7S_RELOJ, MSBFIRST, SEG7[s / 10]);        // #3
  shiftOut(PIN_7S_DATO, PIN_7S_RELOJ, MSBFIRST, SEG7[m % 10] | (dosPuntos ? 0x80 : 0));   // #2
  shiftOut(PIN_7S_DATO, PIN_7S_RELOJ, MSBFIRST, SEG7[m / 10]);        // #1
  digitalWrite(PIN_7S_LATCH, HIGH);
}

// Prueba de segmentos: enciende todo (88:88 con puntos)
void prueba7seg() {
  digitalWrite(PIN_7S_LATCH, LOW);
  for (uint8_t i = 0; i < 4; i++) shiftOut(PIN_7S_DATO, PIN_7S_RELOJ, MSBFIRST, 0xFF);
  digitalWrite(PIN_7S_LATCH, HIGH);
}

// ---------- Acciones ----------
void setBomba(bool on) {
  if (on == bombaOn) return;
  bombaOn = on;
  if (on) { riegoSeg = 0; mostrar7seg(0); }     // nuevo riego: el contador vuelve a 00:00
  digitalWrite(PIN_BOMBA, on ? HIGH : LOW);
  Serial.println(on ? F("[EVENTO] Riego ENCENDIDO") : F("[EVENTO] Riego APAGADO"));
}

void setLuces(bool on) {
  if (on == lucesOn) return;
  lucesOn = on;
  digitalWrite(PIN_LUCES, on ? HIGH : LOW);
  Serial.println(on ? F("[EVENTO] Luces ENCENDIDAS") : F("[EVENTO] Luces APAGADAS"));
}

void detenerCortina() {
  digitalWrite(PIN_CORTINA_ABRIR, LOW);
  digitalWrite(PIN_CORTINA_CERRAR, LOW);
  if (cortinaMov != 0) Serial.println(F("[EVENTO] Motor de cortina detenido"));
  cortinaMov = 0;
}

// Gira el motor en el sentido pedido durante CORTINA_MS
void moverCortina(bool abrir) {
  if (cortinaMov == (abrir ? 1 : 2)) return;
  digitalWrite(PIN_CORTINA_ABRIR, LOW);
  digitalWrite(PIN_CORTINA_CERRAR, LOW);
  digitalWrite(abrir ? PIN_CORTINA_ABRIR : PIN_CORTINA_CERRAR, HIGH);
  cortinaMov = abrir ? 1 : 2;
  cortinaAbierta = abrir;
  cortinaFin = millis() + CORTINA_MS;
  Serial.println(abrir ? F("[EVENTO] Cortina ABRIENDO") : F("[EVENTO] Cortina CERRANDO"));
}

// Detiene el motor al terminar el tiempo de giro
void actualizarCortina() {
  if (cortinaMov != 0 && (long)(millis() - cortinaFin) >= 0) {
    bool abierta = cortinaAbierta;
    detenerCortina();
    Serial.println(abierta ? F("[EVENTO] Cortina ABIERTA") : F("[EVENTO] Cortina CERRADA"));
  }
}

// ---------- Lógica por módulo ----------
// El horario habilita la ventana de riego; dentro de ella, la humedad (con histéresis)
// decide si realmente hace falta regar. Fuera de horario la bomba siempre está apagada.
void controlRiego(uint8_t humedad) {
  bool horarioRiego = enVentana(V_RIEGO_AM) || enVentana(V_RIEGO_PM);
  if (!horarioRiego) setBomba(false);
  else if (humedad < cfg.humMin) setBomba(true);
  else if (humedad > cfg.humMax) setBomba(false);
  // entre humMin y humMax se mantiene el estado actual (histéresis)
}

void controlLuz(uint8_t luz) {
  if (modoLuz == 1) setLuces(true);
  else if (modoLuz == 2) setLuces(false);
  else if (luz < cfg.luzOn) setLuces(true);
  else if (luz > cfg.luzOff) setLuces(false);
  // entre luzOn y luzOff se mantiene el estado (histéresis)
}

// El horario solo da la orden al cambiar de estado; así las órdenes manuales
// con el teclado se respetan hasta el siguiente evento programado.
void controlCortina() {
  bool abrir = enVentana(V_CORTINA);
  if (abrir != cortinaVentPrev) {
    cortinaVentPrev = abrir;
    moverCortina(abrir);
  }
}

void imprimirEstado(uint8_t luz, uint8_t humedad) {
  char buf[64];
  snprintf(buf, sizeof(buf), "%02u:%02u:%02u | Luz %3u%% | Hum %3u%% |", ahoraG.h, ahoraG.m, ahoraG.s, luz, humedad);
  Serial.print(buf);
  Serial.print(F(" Riego:")); Serial.print(bombaOn ? F("ON") : F("OFF"));
  Serial.print(F(" Luces:")); Serial.print(lucesOn ? F("ON") : F("OFF"));
  Serial.print(modoLuz == 0 ? F("(auto)") : F("(manual)"));
  Serial.print(F(" Cortina:"));
  Serial.print(cortinaMov == 1 ? F("ABRIENDO") : cortinaMov == 2 ? F("CERRANDO") : cortinaAbierta ? F("ABIERTA") : F("CERRADA"));
  Serial.print(F(" Alarma:"));
  Serial.println(alarmaSonando ? F("SONANDO") : (alarmaArmadaPrev && !alarmaDesarmadaManual ? F("ARMADA") : F("DESARMADA")));
}

// ---------- Pantalla ----------
void linea(uint8_t fila, const char* s) {
  char b[21];
  uint8_t i = 0;
  while (i < 20 && s[i]) { b[i] = s[i]; i++; }
  while (i < 20) b[i++] = ' ';
  b[20] = 0;

  // Solo se escriben los tramos que cambiaron respecto a lo que ya muestra el LCD
  i = 0;
  while (i < 20) {
    if (b[i] == pantallaLcd[fila][i]) { i++; continue; }
    uint8_t ini = i;
    char tramo[21];
    uint8_t n = 0;
    // el tramo se extiende hasta que haya 2+ caracteres iguales seguidos (no compensa otro setCursor)
    while (i < 20 && (b[i] != pantallaLcd[fila][i] || (i + 1 < 20 && b[i + 1] != pantallaLcd[fila][i + 1]))) {
      tramo[n++] = b[i];
      pantallaLcd[fila][i] = b[i];
      i++;
    }
    tramo[n] = 0;
    lcd->setCursor(ini, fila);
    lcd->print(tramo);
  }
}

// Arma "HH:MM" con los dígitos escritos y '_' en los que faltan
void campoHora(char* out) {
  out[0] = entradaLen > 0 ? entrada[0] : '_';
  out[1] = entradaLen > 1 ? entrada[1] : '_';
  out[2] = ':';
  out[3] = entradaLen > 2 ? entrada[2] : '_';
  out[4] = entradaLen > 3 ? entrada[3] : '_';
  out[5] = 0;
}

void dibujar() {
  char f[4][24];
  char campo[8];
  for (uint8_t i = 0; i < 4; i++) f[i][0] = 0;

  switch (pantalla) {
    case P_INICIO: {
      const char* cort = cortinaMov == 1 ? "ABRI" : cortinaMov == 2 ? "CIER" : cortinaAbierta ? "ABIE" : "CRDA";
      const char* alCod = alarmaSonando ? "SON" : (alarmaArmadaPrev && !alarmaDesarmadaManual ? "ARM" : "OFF");
      calcularRefRiego();
      snprintf(f[0], 24, "CASA x%-3u   %02u:%02u:%02u", velocidad, ahoraG.h, ahoraG.m, ahoraG.s);
      snprintf(f[1], 24, "Luz:%3u%% %c Hum:%3u%%", luzG, modoLuz == 0 ? 'A' : 'M', humG);
      snprintf(f[2], 24, "Luces:%-3s Cort:%-4s", lucesOn ? "ON" : "OFF", cort);
      if (refMin == 0xFFFF) snprintf(f[3], 24, "R:%-3s --:-- Al:%-3s", bombaOn ? "ON" : "OFF", alCod);
      else snprintf(f[3], 24, "R:%-3s %s%02u:%02u Al:%-3s", bombaOn ? "ON" : "OFF", bombaOn ? "F" : "P", (uint8_t)(refMin / 60), (uint8_t)(refMin % 60), alCod);
      break;
    }

    case P_MENU: {
      uint8_t primero = menuIdx > 3 ? menuIdx - 3 : 0;
      for (uint8_t r = 0; r < 4; r++) {
        uint8_t idx = primero + r;
        if (idx < N_MENU) snprintf_P(f[r], 24, PSTR("%c%u %S"), idx == menuIdx ? '>' : ' ', idx + 1, nombreMenu(idx));
      }
      break;
    }

    case P_VENTANA: {
      const Ventana& v = cfg.v[editIdx];
      const char* et;
      if (editIdx == V_CORTINA)      et = etapa == 0 ? PSTR("ABRIR") : PSTR("CERRAR");
      else if (editIdx == V_ALARMA)  et = etapa == 0 ? PSTR("ARMA") : PSTR("DESARMA");
      else                           et = etapa == 0 ? PSTR("INICIO") : PSTR("FIN");
      snprintf_P(f[0], 24, PSTR("%S %S"), nombreMenu(editIdx), et);
      snprintf_P(f[1], 24, PSTR("Actual: %02u:%02u"), etapa == 0 ? v.hIni : v.hFin, etapa == 0 ? v.mIni : v.mFin);
      campoHora(campo);
      snprintf_P(f[2], 24, PSTR("Nuevo:  %s"), campo);
      snprintf_P(f[3], 24, PSTR("OK=ok  ON/C=borrar"));
      break;
    }

    case P_UMBRAL:
      if (tipoUmbral == 0) {
        snprintf_P(f[0], 24, etapa == 0 ? PSTR("Luz: ENCENDER bajo") : PSTR("Luz: APAGAR sobre"));
        snprintf_P(f[1], 24, PSTR("Actual: %u%%"), etapa == 0 ? cfg.luzOn : cfg.luzOff);
      } else {
        snprintf_P(f[0], 24, etapa == 0 ? PSTR("Hum: BOMBA ON bajo") : PSTR("Hum: BOMBA OFF sobre"));
        snprintf_P(f[1], 24, PSTR("Actual: %u%%"), etapa == 0 ? cfg.humMin : cfg.humMax);
      }
      snprintf_P(f[2], 24, PSTR("Nuevo:  %s%s"), entrada, entradaLen < 3 ? "_" : "");
      snprintf_P(f[3], 24, PSTR("OK=ok  ON/C=borrar"));
      break;

    case P_HORA:
      snprintf_P(f[0], 24, PSTR("Ajustar hora"));
      snprintf_P(f[1], 24, PSTR("Actual: %02u:%02u:%02u"), ahoraG.h, ahoraG.m, ahoraG.s);
      campoHora(campo);
      snprintf_P(f[2], 24, PSTR("Nuevo:  %s"), campo);
      snprintf_P(f[3], 24, PSTR("OK=ok  ON/C=borrar"));
      break;

    case P_VELOC:
      snprintf_P(f[0], 24, PSTR("Velocidad del reloj"));
      snprintf_P(f[1], 24, PSTR("Actual: x%u"), velocidad);
      snprintf_P(f[2], 24, PSTR("1=x1  2=x10  3=x60"));
      snprintf_P(f[3], 24, PSTR("4=x300  ON/C=atras"));
      break;

    case P_CLAVE: {
      snprintf_P(f[0], 24, PSTR("Clave de la alarma"));
      char mask[5];
      uint8_t i = 0;
      for (; i < entradaLen; i++) mask[i] = '*';
      for (; i < 4; i++) mask[i] = '_';
      mask[4] = 0;
      snprintf_P(f[1], 24, PSTR("%s"), mask);
      snprintf_P(f[2], 24, PSTR("OK=ok  ON/C=borrar"));
      break;
    }

    case P_MSG:
      strcpy(f[1], msgL0);
      strcpy(f[2], msgL1);
      break;
  }

  for (uint8_t i = 0; i < 4; i++) linea(i, f[i]);
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
  strncpy_P(msgL0, l0, 20); msgL0[20] = 0;
  strncpy_P(msgL1, l1, 20); msgL1[20] = 0;
  pantallaSig = (Pantalla)sig;
  msgFin = millis() + 1300;
  pantalla = P_MSG;
  redibujar = true;
}

void actualizarMensaje() {
  if (pantalla == P_MSG && (long)(millis() - msgFin) >= 0) entrarPantalla(pantallaSig);
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
  } else if (menuIdx == M_UMBRAL) {
    tipoUmbral = 0;
    entrarPantalla(P_UMBRAL);
  } else if (menuIdx == M_HUMEDAD) {
    tipoUmbral = 1;
    entrarPantalla(P_UMBRAL);
  } else if (menuIdx == M_HORA) {
    entrarPantalla(P_HORA);
  } else {
    entrarPantalla(P_VELOC);
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

    case P_UMBRAL: {
      if (entradaLen == 0) return;
      uint8_t valor = atoi(entrada);
      uint8_t* pMin = tipoUmbral == 0 ? &cfg.luzOn : &cfg.humMin;
      uint8_t* pMax = tipoUmbral == 0 ? &cfg.luzOff : &cfg.humMax;
      if (etapa == 0) {
        if (valor > 99) { mostrarMsg(PSTR("Valor invalido"), PSTR("0 a 99"), P_UMBRAL); return; }
        tmpUmbralMin = valor;
        etapa = 1;
        entrarPantalla(P_UMBRAL);
      } else {
        if (valor <= tmpUmbralMin || valor > 100) {
          mostrarMsg(PSTR("Apagar debe ser"), PSTR("mayor que encender"), P_UMBRAL);
          return;
        }
        *pMin = tmpUmbralMin;
        *pMax = valor;
        guardarConfig();
        Serial.print(tipoUmbral == 0 ? F("[PROG] Luz enciende/apaga: ") : F("[PROG] Humedad bomba on/off: "));
        Serial.print(*pMin); Serial.print('/'); Serial.println(*pMax);
        mostrarMsg(PSTR("Guardado"), PSTR(""), P_MENU);
      }
      break;
    }

    case P_HORA:
      if (!parseHora(h, m)) {
        mostrarMsg(PSTR("Hora invalida"), PSTR("Use HHMM (24h)"), P_HORA);
        return;
      }
      ahoraG.h = h; ahoraG.m = m; ahoraG.s = 0;
      Serial.println(F("[PROG] Hora ajustada"));
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
  uint8_t maxLen = (pantalla == P_UMBRAL) ? 3 : 4;

  if (k >= '0' && k <= '9') {
    if (entradaLen < maxLen) {
      entrada[entradaLen++] = k;
      entrada[entradaLen] = 0;
    }
  } else if (k == 'X') {                    // ON/C: borra un dígito o vuelve atrás
    if (entradaLen > 0) entrada[--entradaLen] = 0;
    else if (etapa == 1) { etapa = 0; entrarPantalla(pantalla); }
    else entrarPantalla(pantalla == P_CLAVE ? P_INICIO : P_MENU);
  } else if (k == 'K') {                    // OK
    aceptarEntrada();
  }
}

void procesarTecla(char k) {
  redibujar = true;

  switch (pantalla) {
    case P_INICIO:
      if (k == 'A') { menuIdx = 0; entrarPantalla(P_MENU); }
      else if (k == 'B') { etapa = 0; entrarPantalla(P_HORA); }     // F1: ajustar hora
      else if (k == 'C') { entrarPantalla(P_VELOC); }               // F2: velocidad del reloj
      else if (k == 'D') {
        modoLuz = (modoLuz + 1) % 3;
        Serial.println(modoLuz == 0 ? F("[PROG] Luces en AUTO") : modoLuz == 1 ? F("[PROG] Luces en ON manual") : F("[PROG] Luces en OFF manual"));
      }
      else if (k == '1') moverCortina(true);
      else if (k == '2') moverCortina(false);
      else if (k == '0') detenerCortina();
      else if (k == '3') entrarPantalla(P_CLAVE);    // armar/desarmar alarma (pide clave)
      break;

    case P_MENU:
      if (k == 'B') menuIdx = (menuIdx + 1) % N_MENU;
      else if (k == 'C') menuIdx = (menuIdx + N_MENU - 1) % N_MENU;
      else if (k >= '1' && k <= '0' + N_MENU) { menuIdx = k - '1'; seleccionarMenu(); }
      else if (k == 'K') seleccionarMenu();
      else if (k == 'X' || k == 'A') entrarPantalla(P_INICIO);
      break;

    case P_VENTANA:
    case P_UMBRAL:
    case P_HORA:
    case P_CLAVE:
      teclaEntrada(k);
      break;

    case P_VELOC:
      if (k >= '1' && k <= '4') {
        velocidad = VELOCIDADES[k - '1'];
        Serial.print(F("[PROG] Velocidad del reloj: x")); Serial.println(velocidad);
        mostrarMsg(PSTR("Velocidad cambiada"), PSTR(""), P_INICIO);
      } else if (k == 'X') entrarPantalla(P_MENU);
      break;

    default:
      break;
  }
}

void sondearTeclado() {
  // El escaneo del teclado (Keypad::scanKeys) hace pinMode()/digitalWrite() por cada
  // combinación de fila/columna; a 100 Hz es el mayor costo de CPU en Proteus. 33 Hz
  // sigue siendo instantáneo para una pulsación humana.
  if (++tickTeclado >= 3) {
    tickTeclado = 0;
    char k = teclado.getKey();
    if (k && pantalla != P_MSG) procesarTecla(k);
  }
}

// ---------- Arduino ----------
void setup() {
  uint8_t causaReset = MCUSR;            // por qué se reinició el chip (se limpia para el próximo reinicio)
  MCUSR = 0;
  Serial.begin(115200);
#if DEPURAR
  Serial.print(F("[DBG] setup: inicio, reinicio por:"));
  if (causaReset & _BV(PORF))  Serial.print(F(" ENCENDIDO"));
  if (causaReset & _BV(EXTRF)) Serial.print(F(" PIN-RESET"));
  if (causaReset & _BV(BORF))  Serial.print(F(" BROWN-OUT"));
  if (causaReset & _BV(WDRF))  Serial.print(F(" WATCHDOG"));
  if (!(causaReset & 0x0F))    Serial.print(F(" SALTO-A-0 (fallo del programa)"));
  Serial.println();
#endif

  pinMode(PIN_7S_DATO, OUTPUT);
  pinMode(PIN_7S_RELOJ, OUTPUT);
  pinMode(PIN_7S_LATCH, OUTPUT);
  prueba7seg();                          // 88:88 durante el arranque

  pinMode(PIN_BOMBA, OUTPUT);
  pinMode(PIN_LUCES, OUTPUT);
  pinMode(PIN_CORTINA_ABRIR, OUTPUT);
  pinMode(PIN_CORTINA_CERRAR, OUTPUT);
  detenerCortina();

  // Detecta la dirección del LCD I2C: 0x38 (PCF8574A), 0x3F, 0x27 o 0x20
  Wire.begin();
  const uint8_t candidatas[4] = { 0x38, 0x3F, 0x27, 0x20 };
  uint8_t lcdDir = candidatas[0];
  for (uint8_t i = 0; i < 4; i++) {
    Wire.beginTransmission(candidatas[i]);
    if (Wire.endTransmission() == 0) { lcdDir = candidatas[i]; break; }
  }
  lcd = new LcdI2C(lcdDir, 20, 4);
  lcd->begin();
  // 400 kHz en vez de los 100 kHz por defecto: cada caracter del LCD cuesta ~4 transacciones
  // I2C (2 nibbles x pulso de E) y ese bus es el mayor costo de CPU que le queda a Proteus.
  // lcd->begin() ya llamó a Wire.begin() (que resetea el reloj), por eso va después.
  // Si el LCD falla o muestra basura en Proteus a esta velocidad, volver a 100000.
  Wire.setClock(400000);
  lcd->backlight();
  Serial.print(F("LCD en 0x"));
  Serial.println(lcdDir, HEX);
  linea(1, "   Sistema Casa");
  linea(2, "   Iniciando...");

  // Expansor I2C de la alarma (buzzer + sensor de intrusión): opcional, 0x20 o 0x27
  const uint8_t alarmaCandidatas[2] = { 0x20, 0x27 };
  for (uint8_t i = 0; i < 2; i++) {
    if (alarmaCandidatas[i] == lcdDir) continue;
    Wire.beginTransmission(alarmaCandidatas[i]);
    if (Wire.endTransmission() == 0) { alarmaDir = alarmaCandidatas[i]; break; }
  }
  if (alarmaDir) {
    alarmaExpWrite();                    // P1 (intrusión) en alto, buzzer apagado
    Serial.print(F("Expansor de alarma en 0x"));
    Serial.println(alarmaDir, HEX);
  } else {
    Serial.println(F("Expansor de alarma no encontrado (alarma deshabilitada)"));
  }

  // Pantalla 16x2 con la hora del sistema: primero LCD2_DIR; si no responde, cualquier otro
  // expansor PCF8574/PCF8574A del bus (0x20-0x27 o 0x38-0x3F) distinto del de la 20x4 y del
  // expansor de la alarma
  iniciarLcd2(lcdDir, alarmaDir);
#if DEPURAR
  Serial.println(F("[DBG] setup: LCD listo"));
#endif

  cargarConfig();

  // Sin RTC: la hora del sistema se fija aquí en cada arranque (no hay batería que la
  // conserve) y se corrige desde el teclado (F1)
  ahoraG.h = HORA_ARRANQUE; ahoraG.m = MIN_ARRANQUE; ahoraG.s = 0;
  Serial.println(F("Hora inicial fijada (F1 = ajustar hora)"));
  actualizarLcd2();
  luzG = leerLuzPct();
  humG = leerHumedadPct();
#if DEPURAR
  Serial.println(F("[DBG] setup: hora, LDR y humedad listos"));
#endif

  // Estado inicial: emite los eventos del primer ciclo
  bool oscuro = luzG < cfg.luzOn;
  lucesOn = !oscuro;
  setLuces(oscuro);
  cortinaVentPrev = !enVentana(V_CORTINA);
  alarmaArmadaPrev = !enVentana(V_ALARMA);

  Serial.println(F("Sistema para Casa iniciado"));
  delay(300);                          // deja ver "Iniciando..." un instante

  // Timer1 en CTC a 100 Hz: base de tiempo del reloj en software
  noInterrupts();
  TCCR1A = 0;
  TCCR1B = _BV(WGM12) | _BV(CS11) | _BV(CS10);   // CTC, prescaler 64
  OCR1A  = 2499;                                  // 16 MHz / 64 / 2500 = 100 Hz
  TCNT1  = 0;
  TIMSK1 = _BV(OCIE1A);
  interrupts();
  mostrar7seg(riegoSeg);                 // 00:00 (o el tiempo si ya hay riego en curso)
#if DEPURAR
  Serial.println(F("[DBG] setup: Timer1 activo, entrando al loop"));
#endif
}

void loop() {
  // Dormir (IDLE) hasta el siguiente tick de 10 ms: el CPU no gira en vacío y
  // Proteus se salta esas instrucciones. Timer0 puede despertarlo antes; se vuelve a dormir.
  while (!tick10ms) {
    set_sleep_mode(SLEEP_MODE_IDLE);
    sleep_mode();
  }
  tick10ms = 0;

  // Reloj en software: cada segundo real suma `velocidad` segundos simulados
  noInterrupts();
  uint8_t n = segPend;
  segPend = 0;
  interrupts();
  if (n) {
    relojAvanzar((uint32_t)n * velocidad);
    if (bombaOn) {                       // el contador sube mientras riega
      uint32_t t = riegoSeg + (uint32_t)n * velocidad;
      riegoSeg = t > 5999 ? 5999 : t;
    }
#if DEPURAR
    dosPuntos = !dosPuntos;              // latido: los dos puntos cambian 1 vez por segundo real
    char dbg[40];
    snprintf(dbg, sizeof(dbg), "[DBG] %02u:%02u:%02u x%u pant=%u", ahoraG.h, ahoraG.m, ahoraG.s, velocidad, (uint8_t)pantalla);
    Serial.println(dbg);
#endif
    if (bombaOn || DEPURAR) mostrar7seg(riegoSeg);
    actualizarLcd2();                    // pantalla 16x2: hora del sistema (no hace nada sin pantalla)
  }

  sondearTeclado();

  actualizarMensaje();

  actualizarCortina();

  if (millis() - ultimoCiclo >= PERIODO_MS) {
    ultimoCiclo = millis();

    luzG = leerLuzPct();
    humG = leerHumedadPct();

    controlRiego(humG);
    controlLuz(luzG);
    controlCortina();
    controlAlarma();

    if (ahoraG.m != ultimoMinImpreso) {
      ultimoMinImpreso = ahoraG.m;
      imprimirEstado(luzG, humG);
    }
    if (pantalla == P_INICIO || pantalla == P_HORA) redibujar = true;
  }

  if (redibujar) {
    redibujar = false;
    dibujar();
  }
}
