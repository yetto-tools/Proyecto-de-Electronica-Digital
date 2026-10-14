/*
 * Sistema para Casa (Grupo 1) — version Casa Inteligente v2
 * ============================================================
 *
 * Version de archivo UNICO: todo el contenido que antes vivia en los
 * headers e implementaciones de la carpeta modulos/ esta ahora dentro
 * de este main.ino,
 * en dos bloques grandes (DECLARACIONES e IMPLEMENTACIONES) mas
 * setup()/loop() al final. El orden de los bloques respeta las
 * dependencias entre subsistemas para que todo compile como una
 * sola unidad de traduccion.
 *
 * Subsistemas: tiempo, eeprom_cfg (v2 con alarma y humedad), ldr_luz,
 *   ventanas, bomba_riego, cortina_servo (servo), lcd_20x4,
 *   lcd_16x2 (opcional), teclado_i2c (PCF8574 sobre I2C),
 *   humedad (sensor de humedad), puerta (reed switch),
 *   alarma (sirena + LED con armado horario), ui.
 *
 * Diferencias clave con bocetos/main_modular:
 *   - Keypad pasa del Nano al PCF8574 (I2C, addr 0x20).
 *   - Display 7-seg + 74HC595 se retira (la MM:SS del riego se ve
 *     en el LCD 20x4, fila 3).
 *   - Sensor de humedad DHT22 en D2 (protocolo digital de 1 cable).
 *   - Reed switch de puerta en D3 (INPUT_PULLUP).
 *   - Buzzer en D9, LED alarma en D10.
 *
 * MCU: Arduino Nano v3 (ATmega328P @ 16 MHz).
 * Test: Proteus 8.13 (esquematico actualizado con la guia docs/).
 *
 * Librerias externas obligatorias:
 *   - Keypad_I2C no se usa (habla directo al PCF8574 con Wire).
 *   - LcdI2C del curso.
 */

/* ============================================================
 * ===========================  TIPOS  ==========================
 * ============================================================
 * IMPORTANTE: este bloque debe ir ANTES de cualquier #include.
 *
 * El generador de prototipos de Arduino/Proteus (mksketch) inserta
 * los forward-declarations de TODAS las funciones del sketch justo
 * despues del ULTIMO #include que encuentra en el archivo, sin
 * importar que haya codigo entre medio. Si los struct/enum
 * personalizados (usados en firmas de funcion) se definieran
 * despues de los #include de las librerias, el bloque autogenerado
 * quedaria intercalado antes de que esos tipos existan y el
 * archivo no compilaria. Por eso van aqui, al principio de todo.
 * ------------------------------------------------------------ */
struct Hora {
  uint8_t h, m, s;
};

struct Ventana {
  uint8_t hIni;
  uint8_t mIni;
  uint8_t hFin;
  uint8_t mFin;
};

enum IndiceVentana : uint8_t {
  V_RIEGO_AM = 0,
  V_RIEGO_PM = 1,
  V_CORTINA  = 2,
  V_ALARMA   = 3,
  N_VENTANAS = 4
};

struct Config {
  uint8_t magic;
  Ventana v[N_VENTANAS];
  uint8_t luzOn;
  uint8_t luzOff;
  uint8_t humedadSeco;       // % por debajo del cual se considera SECO
  uint8_t alarmaDuracionSeg; // 10..120
};

enum EstadoAlarma : uint8_t {
  ALARMA_DESARMADA = 0,
  ALARMA_ARMADA    = 1,
  ALARMA_DISPARADA = 2
};

/* Codigos de evento para el log de la alarma. */
enum TipoEvento : uint8_t {
  EVT_NINGUNO       = 0,
  EVT_ALARMA_ARMAR  = 1,
  EVT_ALARMA_DISPAR = 2,
  EVT_ALARMA_TIEMPO = 3,  // termino el tiempo de sirena
  EVT_ALARMA_DESARM = 4
};

#define ALARMA_LOG_CAP 8
struct EventoLog {
  uint8_t    h, m, s;
  TipoEvento tipo;
};

enum Pantalla : uint8_t {
 P_INICIO,
 P_MENU,
 P_VENTANA,
 P_UMBRAL,
 P_HUMEDAD,
 P_DURALARM,
 P_HORA,
 P_VELOC,
 P_ARMADO,
 P_LOG,
 P_MSG
};

#include <avr/sleep.h>
#include <Wire.h>
#include <EEPROM.h>     // usado por la seccion eeprom_cfg; la referencia abajo
                        // evita el warning de "defined but not used"
#include <Keypad.h>     // <-- se conserva la firma, pero el sondeo real es teclado_i2c
#include <LcdI2C.h>

/*
 * Referencia explicita para silenciar el warning de compilacion cuando
 * EEPROM no se usa directamente en este bloque (solo la usan
 * cargarConfig()/guardarConfig()). Eliminar esta linea solo si compila
 * sin warning.
*/
static EEPROMClass& _eeprom_ref = EEPROM;

#define DEPURAR     0
#define DEBUG_SERVO 0

/* ============================================================
 * ===========================  DECLARACIONES  =================
 * ============================================================
 */

/* ------------------------------------------------------------
 * tiempo - Reloj en software + base de tiempo (Timer1, 100 Hz).
 *
 * El reloj vive como contador de 24 h en `ahoraG`. No hay RTC; la
 * hora se inicializa con `tiempo_iniciar(h, m)` al arrancar y se puede
 * corregir despues con `tiempo_ajustar(h, m, s)`.
 *
 * `velocidad` es un multiplicador: segundos simulados por cada segundo
 * real. Para uso real dejar en 1.
 * ------------------------------------------------------------ */
extern Hora             ahoraG;
extern uint16_t         velocidad;
extern volatile uint8_t tick10ms;
extern volatile uint8_t segPend;
extern const uint16_t     VELOCIDADES[4];

/* Hora inicial al arrancar (sin bateria que la conserve). */
const uint8_t             HORA_ARRANQUE = 5;
const uint8_t             MIN_ARRANQUE  = 55;

/* Velocidad inicial: 1 = tiempo real (1 s simulado = 1 s real).
   Cambia esto a 60 (o mayor) si queres acelerar la simulacion. */
const uint16_t             VELOCIDAD_INICIAL = 1;

void   tiempo_iniciar(uint8_t h, uint8_t m);             // fija hora y velocidad inicial
void   tiempo_ajustar(uint8_t h, uint8_t m, uint8_t s);  // corrige la hora
uint8_t tiempo_velocidad();
void   tiempo_setVelocidad(uint8_t indice);              // 0..3 sobre VELOCIDADES
void   relojAvanzar(uint32_t seg);

bool    tick10ms_bandera();                              // true si hay tick pendiente (no lo borra)
void    tick10ms_clear();
uint8_t tiempo_segsPendientes();                         // lee y borra segPend de forma atomica

void timer1Iniciar100Hz();

/* ------------------------------------------------------------
 * eeprom_cfg - Configuracion persistida en EEPROM (version 2).
 *
 * Cambios respecto a v1 (bocetos/main_modular):
 *   - Nueva ventana V_ALARMA (indice 3). N_VENTANAS pasa de 3 a 4.
 *   - Nuevos campos humedadSeco y alarmaDuracionSeg.
 *   - CONFIG_MAGIC sube de 0xA7 a 0xA8 para forzar recarga en EEPROMs
 *     que tengan la version vieja.
 *
 * Layout en EEPROM (21 bytes):
 *   [0]     magic
 *   [1..16] v[0..3]  (4 bytes cada uno: hIni, mIni, hFin, mFin)
 *   [17]    luzOn
 *   [18]    luzOff
 *   [19]    humedadSeco
 *   [20]    alarmaDuracionSeg
 * ------------------------------------------------------------ */
extern const uint8_t CONFIG_MAGIC;
extern const Config  CONFIG_DEFECTO;
extern Config        cfg;

void cargarConfig();
void guardarConfig();

/* ------------------------------------------------------------
 * bomba_riego - Actuador de la bomba de riego + contador MM:SS.
 *
 * El contador `riegoSeg` se incrementa mientras `bombaOn` este activa
 * (lo hace el loop()), y se muestra en el LCD 20x4 (fila 3).
 * Al volver a ON se reinicia a 0.
 * ------------------------------------------------------------ */
extern const uint8_t PIN_BOMBA;
extern bool          bombaOn;
extern uint16_t      riegoSeg;

uint8_t             bombaPin();
bool                bombaEncendida();
uint16_t             riegoSegundos();

/* Activa/desactiva la bomba. Imprime evento por serie. */
void setBomba(bool on);

/* Ajusta manualmente el contador (usado por el loop principal). */
void bomba_setSegundos(uint16_t s);

/* ------------------------------------------------------------
 * puerta - Deteccion de puerta abierta (reed switch) con antirrebote.
 *
 * Conexion:
 *   - Reed switch entre D3 (PD3) y GND.
 *   - pinMode(D3, INPUT_PULLUP) habilita el pull-up interno del AVR.
 *   - Reed cerrado (iman cerca) -> D3 = LOW  -> puerta cerrada
 *   - Reed abierto (iman lejos) -> D3 = HIGH -> puerta abierta
 *
 * Antirrebote: 50 ms.
 * ------------------------------------------------------------ */
void puerta_iniciar();
void puerta_tick();
bool puertaAbierta();          // true = reed abierto = puerta abierta

/* ------------------------------------------------------------
 * humedad - Lectura de humedad relativa del aire con sensor DHT22.
 *
 * DHT22 en D2 (protocolo digital de 1 solo cable, con pull-up externo
 * de 4.7k-10k entre DATA y +5V). Reemplaza la vieja simulacion por
 * potenciometro en A3: esta humedad es la del AIRE (no la del suelo),
 * pero sigue usandose con el mismo umbral `cfg.humedadSeco` para
 * disparar el riego correctivo.
 *
 * El DHT22 solo admite una lectura nueva cada ~2 s como minimo; por
 * eso humedad_tick() (llamado cada 500 ms) ignora la mayoria de las
 * llamadas y solo toca el bus cuando ya paso ese intervalo.
 * ------------------------------------------------------------ */
void humedad_iniciar();
void humedad_tick();

uint8_t humedad_leer();         // 0..100, lectura filtrada actual
bool    humedad_esSeco();       // humedad_leer() < cfg.humedadSeco
uint8_t humedad_ultimaLectura(); // alias del getter, util en UI

/* ------------------------------------------------------------
 * ldr_luz - Lectura del LDR y control de luces con histeresis.
 *
 * LDR en A0 con divisor resistivo a 5V/10k: mas luz -> valor mayor.
 * `modoLuz`: 0 = AUTO, 1 = ON manual, 2 = OFF manual.
 * La histeresis se aplica entre cfg.luzOn y cfg.luzOff: dentro del
 * rango se mantiene el ultimo estado.
 * ------------------------------------------------------------ */
extern const uint8_t PIN_LUCES;
extern bool    lucesOn;
extern uint8_t modoLuz;
extern uint8_t luzG;          // ultima lectura del LDR (%)

uint8_t lucesPin();
uint8_t ldr_ultimaLectura();

/* Lee el LDR y actualiza `luzG`. Llamar en cada ciclo (PERIODO_MS). */
void ldr_leerYActualizar();

/* Modifica `modoLuz` con el orden AUTO -> ON -> OFF -> AUTO. */
void luz_cicloModo();

/* Ajusta manualmente la flag de luces (usado por setup()). */
bool&  lucesOn_flag();

/* Controlador por histeresis segun `modoLuz`. */
void controlLuz(uint8_t luz);

/* Activa/desactiva la luz. Imprime evento por serie. */
void setLuces(bool on);

/* ------------------------------------------------------------
 * ventanas - Ventanas horarias para riego (manana/tarde) y cortina.
 *
 * Reglas:
 *   - Ventana = [inicio, fin). Si fin < inicio cruza la medianoche.
 *   - Si inicio == fin la ventana queda deshabilitada.
 * ------------------------------------------------------------ */
extern uint16_t refMin;       // proximo fin (regando) o inicio (esperando) de riego

/* Getters/setters del estado de cortina (origen del dato: este bloque). */
bool cortinaAbierta();
void cortinaAbierta_set(bool a);

/* Devuelve true si la hora actual cae dentro de cfg.v[idx]. */
bool enVentana(uint8_t idx);

/* Recalcula refMin. Llamar antes de dibujar la pantalla de inicio. */
void calcularRefRiego();

/* Controladores que se llaman desde el ciclo principal. */
void controlRiego();
void controlCortina();

/* ------------------------------------------------------------
 * cortina_servo - Servo de cortina en D4 con Timer2 (CTC 5 kHz, periodo 20 ms).
 *
 * El servo se controla con un solo pin: D4 (PD4, GPIO puro, sin
 * funcion OCnX) emite pulsos de 1 a 2 ms cada 20 ms. La alimentacion
 * (VCC, GND) viene de fuera del Arduino. Se eligio D4 en vez de D9
 * para evitar cualquier confusion con la salida PWM hardware de
 * Timer 1 (D9 = OC1A, D10 = OC1B) y Timer 2 (D11 = OC2A, D3 = OC2B).
 *
 * NO usar la libreria Servo.h en el mismo sketch (conflicto de timers).
 * ------------------------------------------------------------ */
#define PIN_SERVO_CORTINA 4

const uint8_t SERVO_CERRADO = 0;
const uint8_t SERVO_ABIERTO = 90;

/* Bandera de movimiento (0 = detenida, sin uso real para servo posicional
   pero la usa la UI para decidir el texto a mostrar). */
extern uint8_t cortinaMov;

/* Inicia Timer2 (CTC) a 5000 Hz con 80 ticks = 20 ms. Llamar desde setup(). */
void iniciarServoTimer2();

/* Posiciona el servo en el angulo dado (0..180). Thread-safe. */
void establecerAnguloServo(uint8_t angulo);

/* Coloca el servo en SERVO_CERRADO sin pasar por posiciones intermedias. */
void servoColocarCerrado();

/* Mueve a abierto/cerrado y registra el estado (texto en serie). */
void moverCortina(bool abrir);

/* Apaga el indicador de movimiento; el servo mantiene la posicion. */
void detenerCortina();

/* Hook por si en el futuro hay control por tiempo de giro. */
void actualizarCortina();

/* ------------------------------------------------------------
 * teclado_i2c - Keypad 4x4 multiplexado por PCF8574 sobre I2C.
 *
 * Direccion del PCF8574: 0x20 (la del LCD 20x4 es 0x3F, no chocan).
 *
 * Conexion keypad -> PCF8574 (orden CRITICO, debe coincidir con Proteus):
 *   Fila 0 -> P0, Fila 1 -> P1, Fila 2 -> P2, Fila 3 -> P3
 *   Columna 0 -> P4, Columna 1 -> P5, Columna 2 -> P6, Columna 3 -> P7
 *
 * Sondeo completo cada ~30 ms (32 ms para alinear con tick10ms).
 * Antirrebote por confirmacion doble.
 * ------------------------------------------------------------ */
#define I2C_KEYPAD_ADDR 0x20

void teclado_i2c_iniciar();

/* Llamar cada 32 ms aprox desde el bucle principal (o desde el tick
   de 10 ms contando 3 ticks). */
void teclado_i2c_tick();

/* Devuelve la tecla presionada (como caracter) o 0 si no hay.
   Se consume la tecla: la siguiente llamada devuelve 0 hasta que
   el usuario suelte y vuelva a presionar. */
char teclado_i2c_leer();

/* Test/debug: imprime por Serial el estado de las 16 teclas. */
void teclado_i2c_debugScan();

/* ------------------------------------------------------------
 * lcd_20x4 - Pantalla principal LM044L via I2C (PCF8574A).
 *
 * Detecta automaticamente la direccion del expansor probando la tabla
 * `LCD_CANDIDATAS`. Usa la libreria `LcdI2C` del curso (orden de args:
 * `LcdI2C(dir, cols, filas)`).
 *
 * Para minimizar escrituras al LCD, `linea()` mantiene un espejo de
 * 4x20 (`pantallaLcd`) y solo envia al LCD los tramos que cambiaron.
 * ------------------------------------------------------------ */
extern const uint8_t LCD_CANDIDATAS[3];

uint8_t lcd20x4CandidatasCount();             // 3
const uint8_t* lcd20x4Candidatas();           // puntero a la tabla

void lcd20x4_iniciar(uint8_t dir);            // crea el objeto, begin(), backlight()
uint8_t detectarI2C(const uint8_t* tabla, uint8_t n);

void linea(uint8_t fila, const char* s);      // reescribe solo lo que cambia
void campoHora(char* out);                    // HH:MM con '_' en los huecos

/* ------------------------------------------------------------
 * lcd_16x2 - Pantalla secundaria opcional. Muestra la hora del sistema.
 *
 * Dos modos segun `LCD2_JHD`:
 *   1 -> modulo JHD-2X16-I2C de Proteus (protocolo con byte de control
 *        0x80 = comando, 0x40 = datos). Implementacion propia.
 *   0 -> pantalla 16x2 con expansor PCF8574 (reusa la lib LcdI2C).
 *
 * Si no se detecta ningun expansor en el bus, se considera "no
 * conectada" y la pantalla simplemente no se actualiza.
 * ------------------------------------------------------------ */
#define LCD2_JHD 0

const uint8_t LCD2_DIR = 0x3E;   // 7 bits; si no responde, se busca otro

/* Devuelve true si encontro un LCD 16x2 y lo inicializo. */
bool lcd16x2_iniciar(uint8_t lcdPrincipalDir);

/* Direccion I2C detectada (0 = no presente). */
uint8_t lcd16x2_dir();

/* Escribe una fila (0 o 1) segun el modo activo. No hace nada si no hay LCD. */
void lcd2Texto(uint8_t fila, const char* s);

/* Actualiza la fila 1 con la hora del sistema (formato HH:MM:SS). */
void actualizarLcd2();

/* ------------------------------------------------------------
 * alarma - Sistema de alarma con armado por horario.
 *
 * Estados: DESARMADA, ARMADA, DISPARADA (ver maquina de estados en
 * la implementacion). Solo se puede armar dentro de cfg.v[V_ALARMA].
 *
 * Pinout: Buzzer en D9 (PB1), LED en D10 (PB2) con resistencia 330 ohm a GND.
 * ------------------------------------------------------------ */
void alarma_iniciar();

/* Llamar cada 500 ms desde el bucle principal. */
void alarma_tick();

/* Comandos de UI. Devuelven true si la operacion es valida. */
bool alarma_armar();     // intenta armar; devuelve false si fuera de horario
bool alarma_desarmar();  // siempre devuelve true; resetea el temporizador
bool alarma_disparar();  // fuerza disparo manual (util para debug)

/* Getters.
 * NOTA: alarma_getEstado() devuelve uint8_t (en vez de EstadoAlarma) y
 * alarma_getEvento() usa un parametro de salida por referencia (en vez
 * de devolver EventoLog) porque el generador de prototipos de
 * Arduino/Proteus (mksketch) no reconoce bien un struct/enum propio
 * como TIPO DE RETORNO: genera un prototipo automatico sin el tipo de
 * retorno, lo que rompe la compilacion. Como parametro o variable no
 * hay problema, por eso EventoLog si se usa normalmente ahi. */
uint8_t alarma_getEstado();           // valor real: EstadoAlarma
bool    alarma_puedeArmarse();        // helper para el menu

/* Log de eventos. */
/* Escribe en `out` el evento i-esimo (0 = mas reciente), o un evento
   vacio (EVT_NINGUNO) si el slot esta vacio o i fuera de rango. */
void    alarma_getEvento(uint8_t i, EventoLog& out);
uint8_t alarma_logCount();

/* ------------------------------------------------------------
 * ui - Interfaz de usuario: navegacion de pantallas, edicion de
 * horarios/umbrales, dibujo en el LCD principal.
 *
 * Pantallas:
 *   P_INICIO   estado principal en la 20x4 (hora, luz, cortina, riego, alarma, humedad)
 *   P_MENU     navegacion entre las 10 entradas del menu
 *   P_VENTANA  edicion de cfg.v[0..3] (riego AM/PM, cortina, alarma)
 *   P_UMBRAL   edicion de cfg.luzOn / cfg.luzOff
 *   P_HUMEDAD  edicion de cfg.humedadSeco
 *   P_DURALARM edicion de cfg.alarmaDuracionSeg
 *   P_HORA     ajuste de ahoraG (HH:MM)
 *   P_VELOC    cambio de velocidad del reloj (1, 10, 60, 300)
 *   P_ARMADO   armar / desarmar alarma con feedback
 *   P_LOG      ver los ultimos 8 eventos de la alarma
 *   P_MSG      mensaje transitorio (~1.3 s) que ignora pulsaciones
 * ------------------------------------------------------------ */
extern Pantalla      pantalla;
extern Pantalla      pantallaSig;
extern uint8_t       menuIdx;
extern uint8_t       editIdx;
extern uint8_t       etapa;
extern char          entrada[5];
extern uint8_t       entradaLen;
extern Ventana       tmpV;
extern uint8_t       tmpLuzOn;
extern bool          redibujar;
extern unsigned long msgFin;
extern char          msgL0[21];
extern char          msgL1[21];

const uint8_t N_MENU     = 10;
const uint8_t M_UMBRAL   = 3;   // entrada 4: "Umbral de luz"
const uint8_t M_HUMEDAD  = 4;   // entrada 5: "Umbral humedad"
const uint8_t M_DURALARM = 5;   // entrada 6: "Duracion sirena"
const uint8_t M_HORA     = 6;   // entrada 7: "Ajustar hora"
const uint8_t M_VELOC    = 7;   // entrada 8: "Velocidad reloj"
const uint8_t M_ARMADO   = 8;   // entrada 9: "Armar/Desarmar"
const uint8_t M_LOG      = 9;   // entrada 10: "Log eventos"

/* Helpers de acceso (la API legible desde el resto del sketch). */
bool&          ui_redibujar();
void           ui_tickMensajes();              // cierra P_MSG cuando vence
void           ui_tickReloj();                 // marca redibujar si P_INICIO/P_HORA
void           ui_dibujar();
void           ui_imprimirEstado(uint8_t luz);

/* Edicion y teclas. */
void entrarPantalla(uint8_t p);
void mostrarMsg(const char* l0, const char* l1, uint8_t sig);
void procesarTecla(char k);

/* Acceso de solo lectura para evitar exponer globales innecesariamente.
   Devuelve uint8_t (valor real: Pantalla) por la misma razon que
   alarma_getEstado(); ver nota junto a esa funcion. */
uint8_t ui_pantalla();
uint8_t ui_menuIdx();

/* ============================================================
 * ===========================  IMPLEMENTACIONES  ===============
 * ============================================================ */

/* ------------------------- tiempo --------------------------- */

Hora       ahoraG = { HORA_ARRANQUE, MIN_ARRANQUE, 0 };
uint16_t   velocidad = VELOCIDAD_INICIAL;
const uint16_t VELOCIDADES[4] = { 1, 10, 60, 300 };

volatile uint8_t tick10ms = 0;
volatile uint8_t cuenta100 = 0;
volatile uint8_t segPend   = 0;

void tiempo_iniciar(uint8_t h, uint8_t m) {
  ahoraG.h = h;
  ahoraG.m = m;
  ahoraG.s = 0;
  velocidad = VELOCIDAD_INICIAL;
}

void tiempo_ajustar(uint8_t h, uint8_t m, uint8_t s) {
  ahoraG.h = h;
  ahoraG.m = m;
  ahoraG.s = s;
}

uint8_t tiempo_velocidad() {
  return velocidad;
}

void tiempo_setVelocidad(uint8_t indice) {
  if (indice < 4) velocidad = VELOCIDADES[indice];
}

bool tick10ms_bandera() {
  return tick10ms != 0;
}

void tick10ms_clear() {
  tick10ms = 0;
}

uint8_t tiempo_segsPendientes() {
  noInterrupts();
  uint8_t n = segPend;
  segPend  = 0;
  interrupts();
  return n;
}

/* Suma `seg` segundos con aritmetica modulo 86400 (24 h). */
void relojAvanzar(uint32_t seg) {
  uint32_t total = ((uint32_t)ahoraG.h * 3600UL
                  + (uint32_t)ahoraG.m * 60UL
                  + ahoraG.s + seg) % 86400UL;
  ahoraG.h = (uint8_t)(total / 3600UL);
  ahoraG.m = (uint8_t)((total / 60UL) % 60UL);
  ahoraG.s = (uint8_t)(total % 60UL);
}

/*
 * Configura Timer1 para generar una interrupcion cada 10 ms,
 * es decir, con una frecuencia de 100 Hz.
 *
 * Microcontrolador: ATmega328P
 * Frecuencia de reloj: 16 MHz
 * Temporizador utilizado: Timer1 (16 bits)
 * Modo de operacion: CTC (Clear Timer on Compare Match)
 * Prescaler: 64
 * Valor de comparacion: OCR1A = 2499
 */
void timer1Iniciar100Hz() {
  noInterrupts();

  TCCR1A = 0;
  TCCR1B = _BV(WGM12) | _BV(CS11) | _BV(CS10);

  OCR1A = 2499; // Frecuencia de interrupcion: 1 / 0.010 s = 100 Hz.
  TCNT1 = 0;

  TIMSK1 = _BV(OCIE1A);

  interrupts();
}

/*
 * ISR: marca el tick de 10 ms y, cada 100 ticks, suma un segundo
 * pendiente. El loop() consume segPend multiplicado por `velocidad`.
 */
ISR(TIMER1_COMPA_vect) {
  tick10ms = 1;
  if (++cuenta100 >= 100) {
    cuenta100 = 0;
    if (segPend < 255) segPend++;
  }
}

/* ------------------------- eeprom_cfg ------------------------ */
/* Tamano total en EEPROM: 1 (magic) + 4 ventanas * 4 bytes + 4 = 21 bytes.
   Si modificas Config o CONFIG_DEFECTO, SUBE CONFIG_MAGIC. */

const uint8_t CONFIG_MAGIC = 0xA8;

const Config CONFIG_DEFECTO = {
  CONFIG_MAGIC,
  {
    { 6, 30, 7, 30 },    // V_RIEGO_AM
    { 17, 0, 18, 0 },   // V_RIEGO_PM
    { 20, 0, 6, 0 },    // V_CORTINA
    { 22, 0, 6, 0 }     // V_ALARMA (22:00 a 06:00, horario nocturno)
  },
  35,   // luzOn
  55,   // luzOff
  35,   // humedadSeco
  30    // alarmaDuracionSeg
};

Config cfg;

void cargarConfig() {
  EEPROM.get(0, cfg);
  if (cfg.magic != CONFIG_MAGIC) {
    cfg = CONFIG_DEFECTO;
    EEPROM.put(0, cfg);
  }
  // Validacion defensiva de los valores criticos.
  if (cfg.luzOff <= cfg.luzOn) cfg.luzOff = cfg.luzOn + 10;
  if (cfg.humedadSeco > 100)   cfg.humedadSeco = 35;
  if (cfg.alarmaDuracionSeg < 10 || cfg.alarmaDuracionSeg > 120) {
    cfg.alarmaDuracionSeg = 30;
  }
}

void guardarConfig() {
  EEPROM.put(0, cfg);
}

/* ------------------------- bomba_riego ------------------------ */

const uint8_t PIN_BOMBA = 7;

bool     bombaOn  = false;
uint16_t riegoSeg = 0;

uint8_t bombaPin()        { return PIN_BOMBA; }
bool    bombaEncendida()  { return bombaOn; }
uint16_t riegoSegundos() { return riegoSeg; }

void setBomba(bool on) {
  if (on == bombaOn) return;
  bombaOn = on;
  if (on) {
    riegoSeg = 0;
  }
  digitalWrite(PIN_BOMBA, on ? HIGH : LOW);
  Serial.println(on ? F("[EVENTO] Riego ENCENDIDO")
                    : F("[EVENTO] Riego APAGADO"));
}

void bomba_setSegundos(uint16_t s) {
  riegoSeg = s;
}

/* ------------------------- puerta ------------------------------ */
/* Antirrebote: solo cambiamos el estado si la lectura coincide durante
   dos ticks consecutivos (50 ms de margen). */

#define PIN_REED_PUERTA 3   // PD3, antes ocupado por 74HC595 SER

static bool     estadoFiltrado = false;  // estado publicado (sin ruido)
static bool     lecturaActual  = false;  // ultima lectura cruda
static uint32_t msUltimoCambio = 0;
static const uint32_t ANTIRREBOTE_MS = 50UL;

void puerta_iniciar() {
  pinMode(PIN_REED_PUERTA, INPUT_PULLUP);
  lecturaActual   = (digitalRead(PIN_REED_PUERTA) == HIGH);
  estadoFiltrado  = lecturaActual;
  msUltimoCambio  = millis();
}

void puerta_tick() {
  bool nuevaLectura = (digitalRead(PIN_REED_PUERTA) == HIGH);
  if (nuevaLectura != lecturaActual) {
    // Cambio brusco. Esperamos a que se confirme antes de publicar.
    if (millis() - msUltimoCambio >= ANTIRREBOTE_MS) {
      lecturaActual  = nuevaLectura;
      estadoFiltrado = nuevaLectura;
      msUltimoCambio = millis();
    }
  } else {
    // La lectura coincide con la anterior; reseteamos el contador.
    msUltimoCambio = millis();
  }
}

bool puertaAbierta() {
  return estadoFiltrado;
}

/* ------------------------- humedad ------------------------------ */
/* DHT22 por protocolo digital de 1 cable (bit-banging, sin libreria
   externa: mismo criterio que teclado_i2c, para no depender de nada
   que haya que instalar aparte). */

#define PIN_DHT22 2   // D2 (PD2). Requiere pull-up externo 4.7k-10k a +5V.
#define DHT22_INTERVALO_MS 2000UL  // minimo entre lecturas que exige el sensor

static uint8_t      ultimaLectHumedad    = 0;  // % humedad relativa del aire (0..100)
static unsigned long dht22_ultimaLecturaMs = 0;

/* Espera a que el pin llegue al nivel pedido; devuelve false si se
   pasa de `timeoutUs` microsegundos (bus colgado / sensor ausente). */
static bool dht22_esperarPin(uint8_t nivel, uint16_t timeoutUs) {
  uint32_t inicio = micros();
  while (digitalRead(PIN_DHT22) != nivel) {
    if ((uint32_t)(micros() - inicio) > timeoutUs) return false;
  }
  return true;
}

/* Lee los 5 bytes crudos (humedad x10 [16 bits], temperatura x10 [16
   bits], checksum). Devuelve false ante timeout de cualquier fase del
   protocolo.
   NO deshabilita interrupciones: `micros()` depende de la interrupcion
   de desborde de Timer0, asi que si se la deshabilita durante los
   ~4-5 ms que dura la lectura, `micros()` deja de avanzar bien y TODA
   lectura falla (justamente lo que pasaba antes de este fix). El
   costo es un poco de jitter si Timer1/Timer2 interrumpen justo en
   medio de una medicion, pero es un riesgo mucho menor que romper
   micros() por completo. */
static bool dht22_leerCrudo(uint8_t datos[5]) {
  datos[0] = datos[1] = datos[2] = datos[3] = datos[4] = 0;

  /* Senal de inicio: el Nano baja DATA y la suelta (pasa a entrada
     con pull-up). El datasheet del DHT22 real pide >=1 ms, pero el
     modelo generico "DHTXX" de Proteus (usado para simular tanto
     DHT11 como DHT22) trae por defecto la propiedad "Time Select Low"
     en 18 ms (el minimo que exige el DHT11), sin importar que el
     "Part Value" diga DHT22. Se usan 20 ms aca para cubrir ambos
     casos sin depender de tocar esa propiedad del componente. */
  pinMode(PIN_DHT22, OUTPUT);
  digitalWrite(PIN_DHT22, LOW);
  delay(20);
  pinMode(PIN_DHT22, INPUT_PULLUP);
  delayMicroseconds(40);

  /* Respuesta del sensor: LOW ~80 us, HIGH ~80 us. Margenes mas
     generosos (200 us) por si la simulacion de Proteus introduce
     algo de latencia extra. */
  if (!dht22_esperarPin(LOW,  200)) return false;
  if (!dht22_esperarPin(HIGH, 200)) return false;
  if (!dht22_esperarPin(LOW,  200)) return false;

  /* 40 bits de datos: cada uno arranca con un LOW ~50 us; el ancho del
     HIGH que sigue indica si es un '0' (~26-28 us) o un '1' (~70 us). */
  for (uint8_t i = 0; i < 40; i++) {
    if (!dht22_esperarPin(HIGH, 150)) return false;
    uint32_t t0 = micros();
    if (!dht22_esperarPin(LOW, 200)) return false;
    uint16_t ancho = (uint16_t)(micros() - t0);
    datos[i / 8] <<= 1;
    if (ancho > 40) datos[i / 8] |= 1;
  }
  return true;
}

void humedad_iniciar() {
  pinMode(PIN_DHT22, INPUT_PULLUP);
  ultimaLectHumedad     = 0;
  dht22_ultimaLecturaMs = 0;  // fuerza una lectura real en el primer tick()
}

void humedad_tick() {
  unsigned long ahora = millis();
  if (ahora - dht22_ultimaLecturaMs < DHT22_INTERVALO_MS) return;
  dht22_ultimaLecturaMs = ahora;

  uint8_t datos[5];
  if (!dht22_leerCrudo(datos)) {
    Serial.println(F("[DHT22] timeout de lectura, se mantiene el valor anterior"));
    return;
  }
  uint8_t suma = (uint8_t)(datos[0] + datos[1] + datos[2] + datos[3]);
  if (suma != datos[4]) {
    Serial.println(F("[DHT22] checksum invalido, se mantiene el valor anterior"));
    return;
  }

  uint16_t humedadX10 = ((uint16_t)datos[0] << 8) | datos[1];
  uint16_t pct = humedadX10 / 10;
  ultimaLectHumedad = (uint8_t)((pct > 100) ? 100 : pct);
}

uint8_t humedad_leer() {
  return ultimaLectHumedad;
}

uint8_t humedad_ultimaLectura() {
  return humedad_leer();
}

bool humedad_esSeco() {
  return (ultimaLectHumedad < cfg.humedadSeco);
}

/* ------------------------- ldr_luz ------------------------------ */

const uint8_t PIN_LDR   = A0;
const uint8_t PIN_LUCES = 8;

bool    lucesOn = false;
uint8_t modoLuz = 0;
uint8_t luzG    = 0;

uint8_t lucesPin()        { return PIN_LUCES; }
uint8_t ldr_ultimaLectura() { return luzG; }
bool&   lucesOn_flag()    { return lucesOn; }

uint8_t leerLuzPct() {
  return map(analogRead(PIN_LDR), 0, 1023, 0, 100);
}

void ldr_leerYActualizar() {
  luzG = leerLuzPct();
}

void luz_cicloModo() {
  modoLuz = (modoLuz + 1) % 3;
  Serial.println((modoLuz == 0) ? F("[PROG] Luces en AUTO")
                : (modoLuz == 1) ? F("[PROG] Luces en ON manual")
                                 : F("[PROG] Luces en OFF manual"));
}

void controlLuz(uint8_t luz) {
  if      (modoLuz == 1)        setLuces(true);
  else if (modoLuz == 2)        setLuces(false);
  else if (luz < cfg.luzOn)     setLuces(true);
  else if (luz > cfg.luzOff)    setLuces(false);
  /* dentro del rango se mantiene el estado (histeresis) */
}

void setLuces(bool on) {
  if (on == lucesOn) return;
  lucesOn = on;
  digitalWrite(PIN_LUCES, on ? HIGH : LOW);
  Serial.println(on ? F("[EVENTO] Luces ENCENDIDAS")
                    : F("[EVENTO] Luces APAGADAS"));
}

/* ------------------------- ventanas ------------------------------ */

uint16_t refMin = 0xFFFF;
static bool s_cortinaAbierta  = false;
static bool s_cortinaVentPrev = false;

bool cortinaAbierta()          { return s_cortinaAbierta; }
void cortinaAbierta_set(bool a){ s_cortinaAbierta = a; }

static uint16_t aMinutos(uint8_t h, uint8_t m) {
  return (uint16_t)h * 60U + m;
}

bool enVentana(uint8_t idx) {
  const Ventana& v = cfg.v[idx];
  uint16_t t   = aMinutos(ahoraG.h, ahoraG.m);
  uint16_t ini = aMinutos(v.hIni, v.mIni);
  uint16_t fin = aMinutos(v.hFin, v.mFin);
  if (ini == fin) return false;
  if (ini <  fin) return (t >= ini) && (t < fin);
  return (t >= ini) || (t < fin);   // cruza la medianoche
}

void calcularRefRiego() {
  uint16_t t        = aMinutos(ahoraG.h, ahoraG.m);
  uint16_t mejorDif = 0xFFFF;
  refMin = 0xFFFF;
  for (uint8_t i = 0; i < 2; i++) {
    uint16_t ini = aMinutos(cfg.v[i].hIni, cfg.v[i].mIni);
    uint16_t fin = aMinutos(cfg.v[i].hFin, cfg.v[i].mFin);
    if (ini == fin) continue;
    if (enVentana(i)) { refMin = fin; return; }
    uint16_t dif = (ini + 1440U - t) % 1440U;
    if (dif < mejorDif) { mejorDif = dif; refMin = ini; }
  }
}

void controlRiego() {
  /* Riego se activa si:
   *   - estamos dentro de la ventana de la manana, o
   *   - estamos dentro de la ventana de la tarde, o
   *   - la humedad del suelo esta por debajo del umbral configurado
   *     (riego correctivo, independiente del horario).
   */
  bool porHorario = enVentana(V_RIEGO_AM) || enVentana(V_RIEGO_PM);
  bool porHumedad = humedad_esSeco();
  setBomba(porHorario || porHumedad);
}

/*
 * Cortina: solo emite orden al cambiar de estado de la ventana,
 * respetando pulsaciones manuales hasta el proximo evento programado.
 */
void controlCortina() {
  bool abrir = enVentana(V_CORTINA);
  if (abrir != s_cortinaVentPrev) {
    s_cortinaVentPrev = abrir;
    cortinaAbierta_set(abrir);
    moverCortina(abrir);
  }
}

/* ------------------------- cortina_servo ------------------------- */
/*
 * Notas sobre el "no se mueve" en Proteus:
 *   1. Verificar que el servo recibe VCC y GND (los 3 cables del
 *      servo: D4 = senal, VCC = +5 V, GND).
 *   2. La senal sale de D4 solo si la ISR de Timer2 esta corriendo.
 *      Para confirmarlo en Proteus, poner `DEBUG_SERVO = 1` y observar
 *      el pin D4 con el osciloscopio virtual durante el barrido 0->90->0.
 *   3. servoAnchoTicks se inicializa en 4 (~ 0 grados) para evitar un salto
 *      brusco al primer arranque.
 */

/* Estado del servo (compartido con la ISR). */
volatile uint8_t servoAngulo     = 0;
volatile uint8_t servoFase       = 0;
volatile uint8_t servoAnchoTicks = 4;   // 4 ticks = 1.0 ms = 0 grados; ver nota al inicio.

/* Bandera de movimiento (solo la usa esta capa). */
uint8_t cortinaMov = 0;

/* Convierte 0..180 a ticks (4..8, paso 0.25 ms). */
static uint8_t anguloATicksLocal(uint8_t angulo) {
  if (angulo > 180) angulo = 180;
  return (uint8_t)(4U + ((uint16_t)angulo * 4U) / 180U);
}

void establecerAnguloServo(uint8_t angulo) {
  uint8_t ancho = anguloATicksLocal(angulo);
  noInterrupts();
  servoAngulo     = angulo;
  servoAnchoTicks = ancho;
  interrupts();
}

void servoColocarCerrado() {
  /* Asegura OUTPUT ANTES del primer digitalWrite, si no, digitalWrite(LOW)
     en un pin que aun es INPUT habilita el pull-up y el servo recibe un
     pulso falsamente largo. */
  pinMode(PIN_SERVO_CORTINA, OUTPUT);
  servoAnchoTicks = anguloATicksLocal(SERVO_CERRADO);
  servoFase       = 0;
  digitalWrite(PIN_SERVO_CORTINA, LOW);
}

void iniciarServoTimer2() {
  pinMode(PIN_SERVO_CORTINA, OUTPUT);
  digitalWrite(PIN_SERVO_CORTINA, LOW);

  noInterrupts();
  TCCR2A = 0;
  TCCR2B = 0;
  TCNT2  = 0;
  OCR2A  = 124;
  TCCR2A = _BV(WGM21);                 // modo CTC
  TIFR2  = _BV(OCF2A);                 // limpia bandera pendiente
  TIMSK2 = _BV(OCIE2A);                // habilita compare A
  TCCR2B = _BV(CS21) | _BV(CS20);      // prescaler 32 -> 5 kHz
  interrupts();
}

void moverCortina(bool abrir) {
  establecerAnguloServo(abrir ? SERVO_ABIERTO : SERVO_CERRADO);
  cortinaMov = 0;
  cortinaAbierta_set(abrir);
  Serial.println(abrir ? F("[EVENTO] Cortina abierta")
                       : F("[EVENTO] Cortina cerrada"));
}

void detenerCortina() {
  cortinaMov = 0;
  Serial.println(F("[EVENTO] Servo mantiene su posicion"));
}

void actualizarCortina() {
  /* Reservado para control por tiempo de giro. */
}

/* ISR de Timer2: genera el pulso PWM del servo con acceso directo al puerto.
 *
 * En vez de digitalWrite() (que consulta tablas de mapeo pin->puerto en
 * cada llamada), escribimos PORTD directamente. D4 = PD4 (bit 4 del
 * puerto D). Es importante que el DDR de D4 este en OUTPUT antes de
 * iniciar el Timer2, lo cual ya se asegura en iniciarServoTimer2() y
 * se refuerza en servoColocarCerrado().
 */
ISR(TIMER2_COMPA_vect) {
  if (servoFase == 0)                  PORTD |=  _BV(PD4);
  if (servoFase == servoAnchoTicks)    PORTD &= ~_BV(PD4);
  servoFase++;
  if (servoFase >= 80) servoFase = 0;
}

/* ------------------------- teclado_i2c ------------------------- */
/*
 * Mapa de teclas (orden fisico del keypad Proteus):
 *
 *     C0    C1    C2    C3
 *   +-----+-----+-----+-----+
 * R0|  1  |  2  |  3  |  A  |
 * R1|  4  |  5  |  6  |  B  |
 * R2|  7  |  8  |  9  |  C  |
 * R3|ON/C |  0  |  K  |  D  |
 *   +-----+-----+-----+-----+
 *
 * Las teclas A,B,C,D se mapean a las funciones especiales del menu
 * (A = OK, B = cancelar, C = arriba, D = abajo) por la capa UI.
 */
static const char TECLAS[4][4] = {
  {'1', '2', '3', 'A'},
  {'4', '5', '6', 'B'},
  {'7', '8', '9', 'C'},
  {'X', '0', 'K', 'D'}
};

static char ultimaTecla    = 0;
static char teclaPendiente = 0;
static bool teclaConfirmada = false;

static void teclado_escribir(byte valor) {
  Wire.beginTransmission(I2C_KEYPAD_ADDR);
  Wire.write(valor);
  Wire.endTransmission();
}

static byte teclado_leerByte() {
  Wire.requestFrom(I2C_KEYPAD_ADDR, 1);
  if (Wire.available()) return Wire.read();
  return 0xFF;
}

/* Sondea una fila poniendo solo esa fila en LOW y leyendo. */
static char teclado_scanFila(uint8_t fila) {
  byte mask = 0xFF;
  // Pone solo la fila `fila` en LOW. Filas = bits 0..3.
  mask &= ~(1 << fila);

  teclado_escribir(mask);
  delayMicroseconds(50);  // pequena pausa para que las lineas se estabilicen
  byte lectura = teclado_leerByte();

  // Columnas = bits 4..7. Devuelve la columna que este en LOW.
  for (uint8_t col = 0; col < 4; col++) {
    if ((lectura & (1 << (col + 4))) == 0) {
      return TECLAS[fila][col];
    }
  }
  return 0;
}

static char teclado_scan() {
  /* Primero: poner todas las filas HIGH para evitar residuos. */
  teclado_escribir(0xFF);
  delayMicroseconds(20);

  for (uint8_t f = 0; f < 4; f++) {
    char t = teclado_scanFila(f);
    if (t != 0) {
      /* Restaurar todas las filas en HIGH antes de salir. */
      teclado_escribir(0xFF);
      return t;
    }
  }
  teclado_escribir(0xFF);  // restaurar
  return 0;
}

void teclado_i2c_iniciar() {
  Wire.begin();
  /* Pull-ups en todas las lineas (escritura HIGH en PCF8574 = input). */
  teclado_escribir(0xFF);
  ultimaTecla     = 0;
  teclaPendiente  = 0;
  teclaConfirmada = false;
}

void teclado_i2c_tick() {
  char t = teclado_scan();

  if (t == ultimaTecla) {
    /* Lectura estable dos veces seguidas. Si no estaba ya confirmada,
       la publicamos. */
    if (t != 0 && !teclaConfirmada) {
      teclaPendiente  = t;
      teclaConfirmada = true;
    } else if (t == 0) {
      teclaConfirmada = false;
    }
  } else {
    /* Cambio entre sondeos: reiniciamos confirmacion. */
    ultimaTecla = t;
    /* Si veniamos de una tecla confirmada y ahora hay otra distinta,
       descartamos la primera y esperamos a que se confirme la nueva. */
    if (t == 0) {
      teclaConfirmada = false;
      teclaPendiente  = 0;
    }
  }
}

char teclado_i2c_leer() {
  if (!teclaConfirmada || teclaPendiente == 0) return 0;
  char t = teclaPendiente;
  teclaPendiente = 0;  // consume
  return t;
}

void teclado_i2c_debugScan() {
  /* Imprime las 16 teclas en formato matriz. */
  Serial.println(F("[KBD]"));
  for (uint8_t f = 0; f < 4; f++) {
    Serial.print(F("  "));
    for (uint8_t c = 0; c < 4; c++) {
      Serial.print(TECLAS[f][c]);
      Serial.print(' ');
    }
    Serial.println();
  }
  Serial.print(F("  ultima=")); Serial.print(ultimaTecla);
  Serial.print(F(" pendiente=")); Serial.print(teclaPendiente);
  Serial.print(F(" confirmada=")); Serial.println(teclaConfirmada ? 'S' : 'N');
}

/* ------------------------- lcd_20x4 ------------------------- */

/* Direcciones I2C que prueba el firmware para encontrar el LCD 20x4.
   El orden importa: las mas comunes primero. La 0x20 NO esta incluida
   porque esa direccion la usa el PCF8574 del keypad. */
const uint8_t LCD_CANDIDATAS[3] = { 0x3F, 0x38, 0x27 };

LcdI2C *lcd = nullptr;
char    pantallaLcd[4][20] = { {0}, {0}, {0}, {0} };

uint8_t lcd20x4CandidatasCount() { return 3; }
const uint8_t* lcd20x4Candidatas() { return LCD_CANDIDATAS; }

void lcd20x4_iniciar(uint8_t dir) {
  lcd = new LcdI2C(dir, 20, 4);
  lcd->begin();
  lcd->backlight();
}

/*
 * Devuelve la primera direccion de `tabla` que responda a ACK.
 * Si ninguna responde, devuelve tabla[0] (candidata por defecto).
 */
uint8_t detectarI2C(const uint8_t* tabla, uint8_t n) {
  for (uint8_t i = 0; i < n; i++) {
    Wire.beginTransmission(tabla[i]);
    if (Wire.endTransmission() == 0) return tabla[i];
  }
  return tabla[0];
}

/*
 * Escribe 20 caracteres en la fila, reusando `pantallaLcd` para solo
 * enviar al LCD los tramos que cambiaron. Rellena con espacios.
 */
void linea(uint8_t fila, const char* s) {
  if (!lcd) return;
  char b[21];
  uint8_t i = 0;
  while (i < 20 && s[i]) { b[i] = s[i]; i++; }
  while (i < 20) { b[i] = ' '; i++; }
  b[20] = '\0';

  i = 0;
  while (i < 20) {
    if (b[i] == pantallaLcd[fila][i]) { i++; continue; }
    uint8_t ini = i;
    char     tramo[21];
    uint8_t  n = 0;
    while (i < 20 &&
           (b[i] != pantallaLcd[fila][i] ||
            (i + 1 < 20 && b[i + 1] != pantallaLcd[fila][i + 1]))) {
      tramo[n++] = b[i];
      pantallaLcd[fila][i] = b[i];
      i++;
    }
    tramo[n] = '\0';
    lcd->setCursor(ini, fila);
    lcd->print(tramo);
  }
}

void campoHora(char* out) {
  out[0] = (entradaLen > 0) ? entrada[0] : '_';
  out[1] = (entradaLen > 1) ? entrada[1] : '_';
  out[2] = ':';
  out[3] = (entradaLen > 2) ? entrada[2] : '_';
  out[4] = (entradaLen > 3) ? entrada[3] : '_';
  out[5] = '\0';
}

/* ------------------------- lcd_16x2 ------------------------- */

static LcdI2C *lcd2    = nullptr;
static uint8_t lcd2Dir = 0;

#if LCD2_JHD
static void jhdComando(uint8_t c);
static void jhdTexto(uint8_t fila, const char* s);
#endif

uint8_t lcd16x2_dir() { return lcd2Dir; }

bool lcd16x2_iniciar(uint8_t lcdPrincipalDir) {
  lcd2Dir = 0;
  Wire.beginTransmission(LCD2_DIR);
  if (LCD2_DIR != lcdPrincipalDir && Wire.endTransmission() == 0) {
    lcd2Dir = LCD2_DIR;
  }
  for (uint8_t d = 0x20; !lcd2Dir && d <= 0x3F; d++) {
    if (d == lcdPrincipalDir || (d > 0x27 && d < 0x38)) continue;
    Wire.beginTransmission(d);
    if (Wire.endTransmission() == 0) lcd2Dir = d;
  }
  if (!lcd2Dir) return false;

#if LCD2_JHD
  delay(50);
  jhdComando(0x38); delay(5);
  jhdComando(0x38);
  jhdComando(0x0C);
  jhdComando(0x01); delay(3);
  jhdComando(0x06);
#else
  lcd2 = new LcdI2C(lcd2Dir, 16, 2);
  lcd2->begin();
  lcd2->backlight();
#endif
  lcd2Texto(0, "Hora del sistema");
  return true;
}

#if LCD2_JHD

static void jhdComando(uint8_t c) {
  Wire.beginTransmission(lcd2Dir);
  Wire.write((uint8_t)0x80);   // byte de control: comando
  Wire.write(c);
  Wire.endTransmission();
}

static void jhdTexto(uint8_t fila, const char* s) {
  jhdComando((uint8_t)0x80 | (fila ? 0x40 : 0x00));
  Wire.beginTransmission(lcd2Dir);
  Wire.write((uint8_t)0x40);   // byte de control: datos
  for (uint8_t i = 0; i < 16 && s[i]; i++) Wire.write((uint8_t)s[i]);
  Wire.endTransmission();
}

void lcd2Texto(uint8_t fila, const char* s) { jhdTexto(fila, s); }

#else

void lcd2Texto(uint8_t fila, const char* s) {
  if (!lcd2) return;
  lcd2->setCursor(0, fila);
  lcd2->print(s);
}

#endif

void actualizarLcd2() {
  if (!lcd2Dir) return;
  char buf[17];
  snprintf(buf, sizeof(buf), "    %02u:%02u:%02u    ",
           ahoraG.h, ahoraG.m, ahoraG.s);
  lcd2Texto(1, buf);
}

/* ------------------------- alarma ------------------------- */
/*
 * Llamada desde loop() cada 500 ms. Mide el tiempo de la sirena con
 * millis() para no depender del reloj del sistema (que va acelerado).
 *
 * Maquina:
 *   DESARMADA --(armar() && enHorario)--> ARMADA
 *   ARMADA    --(puertaAbierta() && puertaEstuvoCerrada)--> DISPARADA
 *   DISPARADA --(transcurrido alarmaDuracionSeg)--> DESARMADA
 *   *         --(desarmar())--> DESARMADA
 */

#define PIN_BUZZER 9      // PB1
#define PIN_LED    10     // PB2

static EstadoAlarma alarma_estado       = ALARMA_DESARMADA;
static uint32_t     alarma_msDisparoIni = 0;

/* Para evitar disparo inmediato al armar con la puerta ya abierta. */
static bool         alarma_puertaEstuvoCerrada = true;

/* Ring buffer de eventos. */
static EventoLog    alarma_logBuf[ALARMA_LOG_CAP];
static uint8_t      alarma_logHead = 0;  // indice del proximo a escribir
static uint8_t      alarma_logSize = 0;

/* Helpers internos con forward declaration. */
static void alarma_logPush(TipoEvento t);
static void alarma_actuadoresSet(bool on);
static bool alarma_enHorario();

void alarma_iniciar() {
  pinMode(PIN_BUZZER, OUTPUT);
  pinMode(PIN_LED,    OUTPUT);
  alarma_actuadoresSet(false);
  alarma_msDisparoIni       = 0;
  alarma_estado             = ALARMA_DESARMADA;
  alarma_puertaEstuvoCerrada = !puertaAbierta();  // coherente con el estado actual
}

void alarma_tick() {
  /* Maquina principal. */
  switch (alarma_estado) {
    case ALARMA_ARMADA:
      if (puertaAbierta() && alarma_puertaEstuvoCerrada) {
        /* Intrusion real: la puerta estuvo cerrada al menos una vez
           desde que se armo, y ahora esta abierta. */
        alarma_disparar();
      }
      if (!puertaAbierta()) {
        alarma_puertaEstuvoCerrada = true;
      }
      break;

    case ALARMA_DISPARADA: {
      uint32_t duracionMs = (uint32_t)cfg.alarmaDuracionSeg * 1000UL;
      if (millis() - alarma_msDisparoIni >= duracionMs) {
        // Tiempo cumplido: autoapagado.
        alarma_actuadoresSet(false);
        alarma_logPush(EVT_ALARMA_TIEMPO);
        alarma_estado             = ALARMA_DESARMADA;
        alarma_puertaEstuvoCerrada = true;
      }
      break;
    }

    case ALARMA_DESARMADA:
    default:
      // Nada que hacer.
      break;
  }
}

bool alarma_armar() {
  if (!alarma_enHorario()) return false;
  if (alarma_estado == ALARMA_DESARMADA) {
    alarma_estado              = ALARMA_ARMADA;
    alarma_puertaEstuvoCerrada = !puertaAbierta();  // sincronizar con realidad
    alarma_logPush(EVT_ALARMA_ARMAR);
  }
  return true;
}

bool alarma_desarmar() {
  if (alarma_estado != ALARMA_DESARMADA) {
    alarma_actuadoresSet(false);
    alarma_msDisparoIni = 0;
    alarma_logPush(EVT_ALARMA_DESARM);
    alarma_estado = ALARMA_DESARMADA;
  }
  return true;
}

bool alarma_disparar() {
  if (alarma_estado == ALARMA_DESARMADA) return false;
  alarma_actuadoresSet(true);
  alarma_msDisparoIni = millis();
  alarma_estado       = ALARMA_DISPARADA;
  alarma_logPush(EVT_ALARMA_DISPAR);
  return true;
}

uint8_t alarma_getEstado() {
  return alarma_estado;
}

bool alarma_puedeArmarse() {
  return alarma_enHorario();
}

/* ------- Log ------- */

void alarma_getEvento(uint8_t i, EventoLog& out) {
  if (i >= alarma_logSize) { out = {0, 0, 0, EVT_NINGUNO}; return; }
  /* alarma_logHead apunta al proximo slot libre. El evento mas reciente
     esta en logHead-1, el siguiente en logHead-2, etc. */
  uint8_t idx = (uint8_t)((alarma_logHead + ALARMA_LOG_CAP - 1 - i) % ALARMA_LOG_CAP);
  out = alarma_logBuf[idx];
}

uint8_t alarma_logCount() {
  return alarma_logSize;
}

/* ------- Privados ------- */

static void alarma_logPush(TipoEvento t) {
  alarma_logBuf[alarma_logHead].h    = ahoraG.h;
  alarma_logBuf[alarma_logHead].m    = ahoraG.m;
  alarma_logBuf[alarma_logHead].s    = ahoraG.s;
  alarma_logBuf[alarma_logHead].tipo = t;
  alarma_logHead = (alarma_logHead + 1) % ALARMA_LOG_CAP;
  if (alarma_logSize < ALARMA_LOG_CAP) alarma_logSize++;
}

static void alarma_actuadoresSet(bool on) {
  digitalWrite(PIN_BUZZER, on ? HIGH : LOW);
  digitalWrite(PIN_LED,    on ? HIGH : LOW);
}

static bool alarma_enHorario() {
  /* Si la ventana es 0..0, queda deshabilitada (mismo criterio que
     las ventanas de riego: inicio == fin = deshabilitada). */
  return enVentana(V_ALARMA);
}

/* ------------------------- ui ------------------------- */

/* Nombres de menu en flash (PROGMEM). */
const char NM0[] PROGMEM = "Riego manana";
const char NM1[] PROGMEM = "Riego tarde";
const char NM2[] PROGMEM = "Cortinas";
const char NM3[] PROGMEM = "Umbral de luz";
const char NM4[] PROGMEM = "Umbral humedad";
const char NM5[] PROGMEM = "Duracion sirena";
const char NM6[] PROGMEM = "Ajustar hora";
const char NM7[] PROGMEM = "Velocidad reloj";
const char NM8[] PROGMEM = "Armar/Desarmar";
const char NM9[] PROGMEM = "Log eventos";

const char* const NOMBRES[] PROGMEM = { NM0, NM1, NM2, NM3, NM4, NM5, NM6, NM7, NM8, NM9 };

static const char* nombreMenu(uint8_t i) {
  return (const char*)pgm_read_ptr(&NOMBRES[i]);
}

/* Estado de UI. */
Pantalla      pantalla    = P_INICIO;
Pantalla      pantallaSig = P_INICIO;
uint8_t       menuIdx     = 0;
uint8_t       editIdx     = 0;
uint8_t       etapa       = 0;

char          entrada[5];
uint8_t       entradaLen  = 0;

Ventana       tmpV;
uint8_t       tmpLuzOn    = 0;

bool          redibujar   = true;
unsigned long msgFin      = 0;
char          msgL0[21];
char          msgL1[21];

/* Helpers de RAM. */
static void copiarPgm(char* dst, const char* src, uint8_t maxLen) {
  strncpy_P(dst, src, maxLen);
  dst[maxLen - 1] = '\0';
}

uint8_t  ui_pantalla()   { return pantalla; }
uint8_t  ui_menuIdx()    { return menuIdx; }
bool&    ui_redibujar()  { return redibujar; }

void ui_tickMensajes() {
  if (pantalla == P_MSG && (long)(millis() - msgFin) >= 0) {
    entrarPantalla((uint8_t)pantallaSig);
  }
}

void ui_tickReloj() {
  if (pantalla == P_INICIO || pantalla == P_HORA) redibujar = true;
}

void ui_imprimirEstado(uint8_t luz) {
  char buf[48];
  snprintf(buf, sizeof(buf), "%02u:%02u:%02u | Luz %3u%% |",
           ahoraG.h, ahoraG.m, ahoraG.s, luz);
  Serial.print(buf);
  Serial.print(F(" Riego:"));  Serial.print(bombaOn ? F("ON") : F("OFF"));
  Serial.print(F(" Luces:"));  Serial.print(lucesOn ? F("ON") : F("OFF"));
  Serial.print((modoLuz == 0) ? F("(auto)") : F("(manual)"));
  Serial.print(F(" Cortina:"));
  Serial.println(
    (cortinaMov == 1)        ? F("ABRIENDO") :
    (cortinaMov == 2)        ? F("CERRANDO") :
    cortinaAbierta()         ? F("ABIERTA")  : F("CERRADA"));
}

/* Cambio de pantalla / mensajes. */
void limpiarEntrada() {
  entradaLen = 0;
  entrada[0] = '\0';
}

void entrarPantalla(uint8_t p) {
  pantalla = (Pantalla)p;
  limpiarEntrada();
  redibujar = true;
}

void mostrarMsg(const char* l0, const char* l1, uint8_t sig) {
  copiarPgm(msgL0, l0, sizeof(msgL0));
  copiarPgm(msgL1, l1, sizeof(msgL1));
  pantallaSig = (Pantalla)sig;
  msgFin      = millis() + 1300UL;
  pantalla    = P_MSG;
  redibujar   = true;
}

/* Edicion. */
bool parseHora(uint8_t& h, uint8_t& m) {
  if (entradaLen != 4) return false;
  h = (uint8_t)((entrada[0] - '0') * 10 + (entrada[1] - '0'));
  m = (uint8_t)((entrada[2] - '0') * 10 + (entrada[3] - '0'));
  return (h < 24) && (m < 60);
}

void seleccionarMenu() {
  etapa = 0;
  if (menuIdx < N_VENTANAS) {
    editIdx = menuIdx;
    tmpV    = cfg.v[editIdx];
    entrarPantalla(P_VENTANA);
  } else if (menuIdx == M_UMBRAL) {
    entrarPantalla(P_UMBRAL);
  } else if (menuIdx == M_HUMEDAD) {
    entradaLen = 0;
    entrada[0] = '\0';
    entrarPantalla(P_HUMEDAD);
  } else if (menuIdx == M_DURALARM) {
    entradaLen = 0;
    entrada[0] = '\0';
    entrarPantalla(P_DURALARM);
  } else if (menuIdx == M_HORA) {
    entrarPantalla(P_HORA);
  } else if (menuIdx == M_VELOC) {
    entrarPantalla(P_VELOC);
  } else if (menuIdx == M_ARMADO) {
    entrarPantalla(P_ARMADO);
  } else if (menuIdx == M_LOG) {
    entrarPantalla(P_LOG);
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
        Serial.print(F("[PROG] "));
        Serial.print((const __FlashStringHelper*)nombreMenu(editIdx));
        char buf[24];
        snprintf(buf, sizeof(buf), " %02u:%02u - %02u:%02u",
                 tmpV.hIni, tmpV.mIni, tmpV.hFin, tmpV.mFin);
        Serial.println(buf);
        mostrarMsg(PSTR("Guardado"), PSTR(""), P_MENU);
      }
      break;

    case P_UMBRAL: {
      if (entradaLen == 0) return;
      uint8_t valor = (uint8_t)atoi(entrada);
      if (etapa == 0) {
        if (valor > 99) {
          mostrarMsg(PSTR("Valor invalido"), PSTR("0 a 99"), P_UMBRAL);
          return;
        }
        tmpLuzOn = valor;
        etapa = 1;
        entrarPantalla(P_UMBRAL);
      } else {
        if (valor <= tmpLuzOn || valor > 100) {
          mostrarMsg(PSTR("Apagar debe ser"), PSTR("mayor que encender"), P_UMBRAL);
          return;
        }
        cfg.luzOn  = tmpLuzOn;
        cfg.luzOff = valor;
        guardarConfig();
        Serial.print(F("[PROG] Luz enciende/apaga: "));
        Serial.print(cfg.luzOn); Serial.print('/'); Serial.println(cfg.luzOff);
        mostrarMsg(PSTR("Guardado"), PSTR(""), P_MENU);
      }
      break;
    }

    case P_HUMEDAD: {
      if (entradaLen == 0) return;
      uint8_t valor = (uint8_t)atoi(entrada);
      if (valor > 100) {
        mostrarMsg(PSTR("Valor invalido"), PSTR("0 a 100"), P_HUMEDAD);
        return;
      }
      cfg.humedadSeco = valor;
      guardarConfig();
      Serial.print(F("[PROG] Humedad seco bajo: "));
      Serial.println(cfg.humedadSeco);
      mostrarMsg(PSTR("Guardado"), PSTR(""), P_MENU);
      break;
    }

    case P_DURALARM: {
      if (entradaLen == 0) return;
      uint8_t valor = (uint8_t)atoi(entrada);
      if (valor < 10 || valor > 120) {
        mostrarMsg(PSTR("Valor invalido"), PSTR("10 a 120 s"), P_DURALARM);
        return;
      }
      cfg.alarmaDuracionSeg = valor;
      guardarConfig();
      Serial.print(F("[PROG] Duracion sirena: "));
      Serial.println(cfg.alarmaDuracionSeg);
      mostrarMsg(PSTR("Guardado"), PSTR(""), P_MENU);
      break;
    }

    case P_HORA:
      if (!parseHora(h, m)) {
        mostrarMsg(PSTR("Hora invalida"), PSTR("Use HHMM (24h)"), P_HORA);
        return;
      }
      tiempo_ajustar(h, m, 0);
      Serial.println(F("[PROG] Hora ajustada"));
      mostrarMsg(PSTR("Hora ajustada"), PSTR(""), P_INICIO);
      break;

    default:
      break;
  }
}

void teclaEntrada(char k) {
  uint8_t maxLen = (pantalla == P_UMBRAL || pantalla == P_HUMEDAD || pantalla == P_DURALARM) ? 3 : 4;

  if (k >= '0' && k <= '9') {
    if (entradaLen < maxLen) {
      entrada[entradaLen++] = k;
      entrada[entradaLen]   = '\0';
    }
  } else if (k == 'X') {
    if (entradaLen > 0)      entrada[--entradaLen] = '\0';
    else if (etapa == 1)     { etapa = 0; entrarPantalla((uint8_t)pantalla); }
    else                      entrarPantalla(P_MENU);
  } else if (k == 'K') {
    aceptarEntrada();
  }
}

void procesarTecla(char k) {
  redibujar = true;

  switch (pantalla) {

    case P_INICIO:
      if      (k == 'A') { menuIdx = 0; entrarPantalla(P_MENU); }
      else if (k == 'B') { etapa = 0; entrarPantalla(P_HORA); }    // F1: ajustar hora
      else if (k == 'C') {              entrarPantalla(P_VELOC); } // F2: velocidad
      else if (k == 'D') { luz_cicloModo(); }
      else if (k == '1') moverCortina(true);
      else if (k == '2') moverCortina(false);
      else if (k == '0') detenerCortina();
      break;

    case P_MENU:
      if      (k == 'B') menuIdx = (menuIdx + 1) % N_MENU;
      else if (k == 'C') menuIdx = (menuIdx + N_MENU - 1) % N_MENU;
      else if (k >= '1' && k <= '0' + N_MENU) {
        menuIdx = (uint8_t)(k - '1');
        seleccionarMenu();
      }
      else if (k == 'K') seleccionarMenu();
      else if (k == 'X' || k == 'A') entrarPantalla(P_INICIO);
      break;

    case P_VENTANA:
    case P_UMBRAL:
    case P_HUMEDAD:
    case P_DURALARM:
    case P_HORA:
      teclaEntrada(k);
      break;

    case P_ARMADO:
      if (k == 'A') {
        if (alarma_armar()) {
          mostrarMsg(PSTR("Alarma armada"), PSTR(""), P_INICIO);
        } else {
          mostrarMsg(PSTR("Fuera de horario"), PSTR("ver ventana alarma"), P_ARMADO);
        }
      } else if (k == 'D') {
        alarma_desarmar();
        mostrarMsg(PSTR("Alarma desarmada"), PSTR(""), P_INICIO);
      } else if (k == 'X') {
        entrarPantalla(P_MENU);
      }
      break;

    case P_LOG:
      if (k == 'X' || k == 'A' || k == 'B') {
        entrarPantalla(P_MENU);
      }
      break;

    case P_VELOC:
      if (k >= '1' && k <= '4') {
        tiempo_setVelocidad((uint8_t)(k - '1'));
        Serial.print(F("[PROG] Velocidad del reloj: x"));
        Serial.println(tiempo_velocidad());
        mostrarMsg(PSTR("Velocidad cambiada"), PSTR(""), P_INICIO);
      } else if (k == 'X') {
        entrarPantalla(P_MENU);
      }
      break;

    default:
      break;
  }
}

/* Dibujo en el LCD 20x4 (cuatro lineas). */
void ui_dibujar() {
  char f[4][24];
  char campo[8];
  for (uint8_t i = 0; i < 4; i++) f[i][0] = '\0';

  switch (pantalla) {

    case P_INICIO: {
      const char* cort =
        (cortinaMov == 1)     ? "ABRIENDO" :
        (cortinaMov == 2)     ? "CERRANDO" :
        cortinaAbierta()      ? "ABIERTA"  : "CERRADA";
      const char* estAlarma =
        (alarma_getEstado() == ALARMA_DISPARADA) ? "DISPARO" :
        (alarma_getEstado() == ALARMA_ARMADA)    ? "ARMADA  " : "DESARMAD";
      calcularRefRiego();
      snprintf(f[0], 24, "CASA x%-3u   %02u:%02u:%02u",
               tiempo_velocidad(), ahoraG.h, ahoraG.m, ahoraG.s);
      snprintf(f[1], 24, "Luz:%3u%% Hum:%3u%% %c",
               ldr_ultimaLectura(), humedad_leer(),
               (modoLuz == 0) ? 'A' : 'M');
      snprintf(f[2], 24, "Cortina:%-8s Pta:%s",
               cort, puertaAbierta() ? "ABIERTA" : "CERRADA");
      if (refMin == 0xFFFF) {
        snprintf(f[3], 24, "Alm:%s Riego:%-3s",
                 estAlarma, bombaOn ? "ON" : "OFF");
      } else {
        snprintf(f[3], 24, "Alm:%s Rgo:%s%02u:%02u",
                 estAlarma,
                 bombaOn ? "ON " : "OFF",
                 (uint8_t)(refMin / 60), (uint8_t)(refMin % 60));
      }
      break;
    }

    case P_MENU: {
      uint8_t primero = (menuIdx > 3) ? (menuIdx - 3) : 0;
      for (uint8_t r = 0; r < 4; r++) {
        uint8_t idx = primero + r;
        if (idx < N_MENU) {
          snprintf_P(f[r], 24, PSTR("%c%u %S"),
                     (idx == menuIdx) ? '>' : ' ', idx + 1, nombreMenu(idx));
        }
      }
      break;
    }

    case P_VENTANA: {
      const Ventana& v = cfg.v[editIdx];
      const char* et;
      if (editIdx == V_CORTINA) et = (etapa == 0) ? PSTR("ABRIR") : PSTR("CERRAR");
      else                     et = (etapa == 0) ? PSTR("INICIO") : PSTR("FIN");
      snprintf_P(f[0], 24, PSTR("%S %S"), nombreMenu(editIdx), et);
      snprintf_P(f[1], 24, PSTR("Actual: %02u:%02u"),
                 (etapa == 0) ? v.hIni : v.hFin,
                 (etapa == 0) ? v.mIni : v.mFin);
      campoHora(campo);
      snprintf_P(f[2], 24, PSTR("Nuevo:  %s"), campo);
      snprintf_P(f[3], 24, PSTR("OK=ok  ON/C=borrar"));
      break;
    }

    case P_UMBRAL:
      snprintf_P(f[0], 24,
                 (etapa == 0) ? PSTR("Luz: ENCENDER bajo")
                              : PSTR("Luz: APAGAR sobre"));
      snprintf_P(f[1], 24, PSTR("Actual: %u%%"),
                 (etapa == 0) ? cfg.luzOn : cfg.luzOff);
      snprintf_P(f[2], 24, PSTR("Nuevo:  %s%s"),
                 entrada, (entradaLen < 3) ? "_" : "");
      snprintf_P(f[3], 24, PSTR("OK=ok  ON/C=borrar"));
      break;

    case P_HUMEDAD:
      snprintf_P(f[0], 24, PSTR("Humedad: SECO bajo"));
      snprintf_P(f[1], 24, PSTR("Actual: %u%%"), cfg.humedadSeco);
      snprintf_P(f[2], 24, PSTR("Nuevo:  %s%s"),
                 entrada, (entradaLen < 3) ? "_" : "");
      snprintf_P(f[3], 24, PSTR("OK=ok  ON/C=borrar"));
      break;

    case P_DURALARM:
      snprintf_P(f[0], 24, PSTR("Sirena: duracion"));
      snprintf_P(f[1], 24, PSTR("Actual: %u s"), cfg.alarmaDuracionSeg);
      snprintf_P(f[2], 24, PSTR("Nuevo:  %s%s"),
                 entrada, (entradaLen < 3) ? "_" : "");
      snprintf_P(f[3], 24, PSTR("OK=ok  ON/C=borrar"));
      break;

    case P_ARMADO: {
      const char* est =
        (alarma_getEstado() == ALARMA_DISPARADA) ? "DISPARADA" :
        (alarma_getEstado() == ALARMA_ARMADA)    ? "ARMADA"    : "DESARMADA";
      snprintf_P(f[0], 24, PSTR("Alarma: %s"), est);
      snprintf_P(f[1], 24, PSTR("Horario: %02u:%02u - %02u:%02u"),
                 cfg.v[V_ALARMA].hIni, cfg.v[V_ALARMA].mIni,
                 cfg.v[V_ALARMA].hFin, cfg.v[V_ALARMA].mFin);
      snprintf_P(f[2], 24, PSTR("A=Armar D=Desarmar"));
      snprintf_P(f[3], 24, PSTR("ON/C=atras"));
      break;
    }

    case P_LOG: {
      uint8_t n = alarma_logCount();
      snprintf_P(f[0], 24, PSTR("Log (%u eventos):"), n);
      /* Mostramos los 3 eventos mas recientes (slots 0, 1, 2). */
      EventoLog e;
      uint8_t r = 1;
      for (uint8_t i = 0; i < 3 && i < n; i++, r++) {
        alarma_getEvento(i, e);
        const char* t =
          (e.tipo == EVT_ALARMA_DISPAR) ? "DISPARO " :
          (e.tipo == EVT_ALARMA_ARMAR)  ? "ARMADA  " :
          (e.tipo == EVT_ALARMA_TIEMPO) ? "TIEMPO  " :
          (e.tipo == EVT_ALARMA_DESARM) ? "DESARMAD" : "???     ";
        snprintf_P(f[r], 24, PSTR("%02u:%02u:%02u %S"), e.h, e.m, e.s, t);
      }
      for (; r < 4; r++) f[r][0] = '\0';
      break;
    }

    case P_HORA:
      snprintf_P(f[0], 24, PSTR("Ajustar hora"));
      snprintf_P(f[1], 24, PSTR("Actual: %02u:%02u:%02u"),
                 ahoraG.h, ahoraG.m, ahoraG.s);
      campoHora(campo);
      snprintf_P(f[2], 24, PSTR("Nuevo:  %s"), campo);
      snprintf_P(f[3], 24, PSTR("OK=ok  ON/C=borrar"));
      break;

    case P_VELOC:
      snprintf_P(f[0], 24, PSTR("Velocidad del reloj"));
      snprintf_P(f[1], 24, PSTR("Actual: x%u"), tiempo_velocidad());
      snprintf_P(f[2], 24, PSTR("1=x1  2=x10  3=x60"));
      snprintf_P(f[3], 24, PSTR("4=x300  ON/C=atras"));
      break;

    case P_MSG:
      strcpy(f[1], msgL0);
      strcpy(f[2], msgL1);
      break;
  }

  for (uint8_t i = 0; i < 4; i++) linea(i, f[i]);
}

/* ============================================================
 * ===========================  SETUP / LOOP  ====================
 * ============================================================ */

const unsigned long PERIODO_MS  = 500UL;
const unsigned long PERIODO_500 = 500UL;

unsigned long ultimoCiclo = 0;
uint8_t ultimoMinImpreso  = 255;

void setup() {
  Serial.begin(115200);

  /* Salidas digitales: actuadores. */
  pinMode(bombaPin(), OUTPUT);
  pinMode(lucesPin(), OUTPUT);

  /* Servo (Timer2). */
  servoColocarCerrado();
  iniciarServoTimer2();
  establecerAnguloServo(SERVO_CERRADO);
  detenerCortina();

#if DEBUG_SERVO
  Serial.println(F("[DBG] Barrido del servo"));
  establecerAnguloServo(SERVO_ABIERTO); delay(500);
  establecerAnguloServo(SERVO_CERRADO); delay(500);
#endif

  /* Bus I2C + LCD 20x4 principal. */
  Wire.begin();
  uint8_t lcdDir = detectarI2C(lcd20x4Candidatas(), lcd20x4CandidatasCount());
  lcd20x4_iniciar(lcdDir);
  Serial.print(F("LCD en 0x")); Serial.println(lcdDir, HEX);
  linea(1, "   Sistema Casa v2");
  linea(2, "   Iniciando...");

  /* LCD 16x2 opcional (desactivado por defecto para ahorrar ROM). */
#if LCD2_JHD
  if (lcd16x2_iniciar(lcdDir)) {
    Serial.print(F("LCD 16x2 en 0x")); Serial.println(lcd16x2_dir(), HEX);
  }
#endif

  /* Configuracion + hora inicial. */
  cargarConfig();
  tiempo_iniciar(HORA_ARRANQUE, MIN_ARRANQUE);
  Serial.println(F("Hora inicial fijada (F1 = ajustar hora)"));
  actualizarLcd2();

  /* Subsistemas nuevos. */
  puerta_iniciar();
  humedad_iniciar();
  alarma_iniciar();
  teclado_i2c_iniciar();

  /* Estado inicial coherente con la luz ambiente. */
  ldr_leerYActualizar();
  {
    uint8_t luz = ldr_ultimaLectura();
    bool oscuro = (luz < cfg.luzOn);
    lucesOn_flag() = !oscuro;
    setLuces(oscuro);
  }

  Serial.println(F("Sistema para Casa v2 iniciado"));
  delay(300);

  /* Base de tiempo del reloj (Timer1 a 100 Hz). */
  timer1Iniciar100Hz();
}

/* Loop con dos periodicidades: 10 ms (reloj y teclado) y 500 ms (control). */
void loop() {
  while (!tick10ms_bandera()) {
    set_sleep_mode(SLEEP_MODE_IDLE);
    sleep_mode();
  }
  tick10ms_clear();

  /* Avance del reloj cuando hay segundos pendientes. */
  uint8_t n = tiempo_segsPendientes();
  if (n) {
    relojAvanzar((uint32_t)n * tiempo_velocidad());
    if (bombaEncendida()) {
      uint32_t t = (uint32_t)riegoSegundos() + (uint32_t)n * tiempo_velocidad();
      bomba_setSegundos((t > 5999U) ? 5999U : (uint16_t)t);
    }
#if DEPURAR
    Serial.print(F("[DBG] "));
    Serial.print(ahoraG.h); Serial.print(':');
    Serial.print(ahoraG.m); Serial.print(':');
    Serial.print(ahoraG.s);
    Serial.print(F(" x")); Serial.print(tiempo_velocidad());
    Serial.println();
#endif
    actualizarLcd2();
  }

  /* Sondeo de teclado cada 3 ticks (~33 Hz, igual que la version previa). */
  static uint8_t cntKey = 0;
  if (++cntKey >= 3) {
    cntKey = 0;
    teclado_i2c_tick();
    char k = teclado_i2c_leer();
    if (k) {
      procesarTecla(k);
    }
  }

  /* Mensajes transitorios y cortina. */
  ui_tickMensajes();
  actualizarCortina();

  /* Ciclo de control cada 500 ms. */
  if (millis() - ultimoCiclo >= PERIODO_MS) {
    ultimoCiclo = millis();

    puerta_tick();
    humedad_tick();
    alarma_tick();

    ldr_leerYActualizar();
    controlRiego();
    controlLuz(ldr_ultimaLectura());
    controlCortina();

    if (ahoraG.m != ultimoMinImpreso) {
      ultimoMinImpreso = ahoraG.m;
      ui_imprimirEstado(ldr_ultimaLectura());
    }
    ui_tickReloj();

    if (ui_redibujar()) ui_dibujar();
  }
}

/* Stub para evitar referencias colgadas si la pantalla 16x2 esta deshabilitada. */
void lcd16x2_noUsado() { (void)0; }
