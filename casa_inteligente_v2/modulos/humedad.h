/*
 * humedad.h - Lectura del sensor de humedad del suelo.
 *
 * En Proteus se simula con un potenciometro (POT-HG) en A3 (PC3/ADC3).
 * El firmware no sabe si lo que hay conectado es un sensor real o un
 * potenciometro de calibracion: solo lee el ADC y mapea a porcentaje.
 *
 * Convencion:
 *   - 0 %   = suelo encharcado / sensor en GND
 *   - 100 % = suelo muy humedo / sensor en +5V
 *
 * Antirrebote / filtrado:
 *   - Promedio movil de 8 muestras. Estabiliza la lectura del ADC y
 *     evita saltos visibles en el LCD cuando hay ruido.
 *
 * Configuracion:
 *   - cfg.humedadSeco: por debajo de este valor se considera SECO y se
 *     fuerza el riego correctivo, independientemente del horario.
 *     Se ajusta desde el menu "Umbral humedad".
 */
#ifndef HUMEDAD_H
#define HUMEDAD_H

#include <Arduino.h>

void humedad_iniciar();
void humedad_tick();

uint8_t humedad_leer();         // 0..100, lectura filtrada actual
bool    humedad_esSeco();       // humedad_leer() < cfg.humedadSeco
uint8_t humedad_ultimaLectura(); // alias del getter, util en UI

#endif