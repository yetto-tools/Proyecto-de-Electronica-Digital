/*
 * tiempo.cpp - implementacion.
 * Ver tiempo.h para semantica de cada funcion.
 */
#include "tiempo.h"

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

  // Deshabilita temporalmente las interrupciones globales.
  // Asi se evita que una interrupcion interfiera mientras
  // se modifican los registros de configuracion de Timer1.
  noInterrupts();

  // TCCR1A controla parte del modo de funcionamiento y las
  // salidas de comparacion del Timer1.
  // Al escribir 0, se deshabilitan las salidas OC1A y OC1B
  // y se limpian los bits WGM11 y WGM10.
  // Junto con WGM12 = 1 en TCCR1B, esto selecciona el modo CTC.
  TCCR1A = 0;

  // TCCR1B controla el modo de operacion y el prescaler.
  //
  // _BV(WGM12):
  // Activa el bit WGM12. Junto con los bits WGM13, WGM12,
  // WGM11 y WGM10, selecciona el modo CTC con OCR1A como
  // valor máximo de comparacion (modo 4).
  //
  // _BV(CS11) | _BV(CS10):
  // Activa CS11 y CS10. La combinacion CS12:CS10 = 011
  // selecciona un prescaler de 64 para el reloj de Timer1.
  //
  // El temporizador recibe:
  // 16 000 000 Hz / 64 = 250 000 incrementos por segundo.
  TCCR1B = _BV(WGM12) | _BV(CS11) | _BV(CS10);

  // OCR1A es el registro de comparacion A de Timer1.
  // Cuando el contador TCNT1 alcanza este valor, se produce
  // una coincidencia de comparacion y, en modo CTC, el
  // contador vuelve a cero en el siguiente ciclo de conteo.
  //
  // El contador recorre los valores desde 0 hasta 2499:
  // son 2500 incrementos, no 2499.
  //
  // Tiempo de cada incremento:
  // 64 / 16 000 000 = 0.000004 s = 4 microsegundos.
  //
  // Tiempo entre coincidencias:
  //  2500 * 4 us = 10 000 us = 10 ms.

  OCR1A = 2499; // Frecuencia de interrupcion:  1 / 0.010 s = 100 Hz.
  TCNT1 = 0;    // TCNT1 es el contador de 16 bits de Timer1. Se pone a cero para iniciar la cuenta desde un estado conocido y evitar comenzar desde un valor anterior.

  // TIMSK1 es el registro de habilitacion de interrupciones
  // de Timer1.
  //
  // _BV(OCIE1A) activa la interrupcion por coincidencia
  // de comparacion A. Cuando TCNT1 coincide con OCR1A,
  // el microcontrolador puede ejecutar la rutina asociada:
  // ISR(TIMER1_COMPA_vect).
  //
  // La asignacion tambien deja deshabilitadas las otras
  // interrupciones de Timer1 controladas por TIMSK1.
  TIMSK1 = _BV(OCIE1A);

  // Vuelve a habilitar las interrupciones globales.
  // Desde este momento, la interrupcion de comparacion A
  // puede ejecutarse cada 10 ms.
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
