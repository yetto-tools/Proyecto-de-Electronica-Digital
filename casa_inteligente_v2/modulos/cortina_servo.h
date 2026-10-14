/*
 * cortina_servo.h - Servo de cortina en D4 con Timer2 (CTC 5 kHz, periodo 20 ms).
 *
 * Renombrado desde "servo.h" para evitar conflicto con la libreria
 * oficial `Servo.h` de Arduino (case-insensitive en Windows hace que
 * `<servo.h>` se resuelva a la oficial en lugar de la nuestra).
 *
 * El servo se controla con un solo pin: D4 (PD4, GPIO puro, sin
 * funcion OCnX) emite pulsos de 1 a 2 ms cada 20 ms. La alimentacion
 * (VCC, GND) viene de fuera del Arduino. Se eligio D4 en vez de D9
 * para evitar cualquier confusion con la salida PWM hardware de los
 * Timer 1 (D9 = OC1A, D10 = OC1B) y Timer 2 (D11 = OC2A, D3 = OC2B).
 *
 * NO usar la libreria Servo.h en el mismo sketch (conflicto de timers).
 *
 * Estado (cortinaMov, cortinaAbierta, cortinaVentPrev) lo gestiona el
 * modulo de ventanas; este modulo solo posiciona el servo y mantiene
 * la pulsacion.
 */
#ifndef CORTINA_SERVO_H
#define CORTINA_SERVO_H

#include <Arduino.h>

/* Pin del servo (D4). Se define como macro para que la ISR lo vea
   sin indireccion. El servo usa los 3 cables: D4 + VCC + GND. */
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

#endif