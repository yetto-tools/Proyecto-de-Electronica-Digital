/*
 * cortina_servo.cpp - implementacion + ISR de Timer2.
 *
 * Notas sobre el "no se mueve" en Proteus:
 *   1. Verificar que el servo recibe VCC y GND (los 3 cables del
 *      servo: D4 = senal, VCC = +5 V, GND).
 *   2. La senal sale de D4 solo si la ISR de Timer2 esta corriendo.
 *      Para confirmarlo en Proteus, poner `DEBUG_SERVO = 1` y observar
 *      el pin D4 con el osciloscopio virtual durante el barrido 0->90->0.
 *   3. servoAnchoTicks se inicializa en 4 (~ 0 grados) para evitar un salto
 *      brusco al primer arranque.
 */
#include "cortina_servo.h"
#include "ventanas.h"     // cortinaAbierta_set (gestion de estado desde ventanas.cpp)

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
