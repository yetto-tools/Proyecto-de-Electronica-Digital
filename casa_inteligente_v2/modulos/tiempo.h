/*
 * tiempo.h - Reloj en software + base de tiempo (Timer1, 100 Hz).
 *
 * El reloj vive como contador de 24 h en `ahoraG`. No hay RTC; la
 * hora se inicializa con `tiempo_iniciar(h, m)` al arrancar y se puede
 * corregir despues con `tiempo_ajustar(h, m, s)`.
 *
 * `velocidad` es un multiplicador: segundos simulados por cada segundo
 * real. Para uso real dejar en 1.
 */
#ifndef TIEMPO_H
#define TIEMPO_H

#include <Arduino.h>
#include <stdint.h>

struct Hora {
  uint8_t h, m, s;
};

/* Estado global del modulo (definido en tiempo.cpp). */
extern Hora         ahoraG;
extern uint16_t     velocidad;
extern volatile uint8_t tick10ms;
extern volatile uint8_t segPend;
extern const uint16_t VELOCIDADES[4];

/* Hora inicial al arrancar (sin bateria que la conserve). */
const uint8_t HORA_ARRANQUE = 5;
const uint8_t MIN_ARRANQUE  = 55;

/* Velocidad inicial: 1 = tiempo real (1 s simulado = 1 s real).
   Cambia esto a 60 (o mayor) si queres acelerar la simulacion. */
const uint16_t VELOCIDAD_INICIAL = 1;

/* ---- API ---- */
void   tiempo_iniciar(uint8_t h, uint8_t m);             // fija hora y velocidad inicial
void   tiempo_ajustar(uint8_t h, uint8_t m, uint8_t s);  // corrige la hora
uint8_t tiempo_velocidad();
void   tiempo_setVelocidad(uint8_t indice);              // 0..3 sobre VELOCIDADES
void   relojAvanzar(uint32_t seg);

/* Bandera de 10 ms y consumo de segundos pendientes. */
bool    tick10ms_bandera();          // true si hay tick pendiente (no lo borra)
void    tick10ms_clear();
uint8_t tiempo_segsPendientes();     // lee y borra segPend de forma atomica

/* Configura Timer1 en CTC a 100 Hz. Llamar una sola vez desde setup(). */
void timer1Iniciar100Hz();

#endif
