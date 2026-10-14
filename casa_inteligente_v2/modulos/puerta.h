/*
 * puerta.h - Deteccion de puerta abierta (reed switch) con antirrebote.
 *
 * Conexion:
 *   - Reed switch entre D3 (PD3) y GND.
 *   - pinMode(D3, INPUT_PULLUP) habilita el pull-up interno del AVR.
 *   - Reed cerrado (imán cerca) -> D3 = LOW  -> puerta cerrada
 *   - Reed abierto (imán lejos) -> D3 = HIGH -> puerta abierta
 *     (El pin flota a HIGH por el pull-up cuando el reed abre el circuito.)
 *
 * Antirrebote:
 *   - 50 ms. Suficiente para un reed real y para los glitches que
 *     mete Proteus al simular interruptores. No es critico en bucle
 *     principal porque puerta_tick() se llama cada 500 ms.
 *
 * Uso:
 *   - puerta_iniciar() desde setup() una sola vez.
 *   - puerta_tick()   desde loop() cada 500 ms.
 *   - puertaAbierta() para consultar el estado actual.
 */
#ifndef PUERTA_H
#define PUERTA_H

#include <Arduino.h>

void puerta_iniciar();
void puerta_tick();
bool puertaAbierta();          // true = reed abierto = puerta abierta

#endif