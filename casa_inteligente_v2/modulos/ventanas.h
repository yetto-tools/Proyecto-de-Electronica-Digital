/*
 * ventanas.h - Ventanas horarias para riego (maniana/tarde) y cortina.
 *
 * Reglas:
 *   - Ventana = [inicio, fin). Si fin < inicio cruza la medianoche.
 *   - Si inicio == fin la ventana queda deshabilitada.
 *
 * El estado de la cortina se publica desde aqui porque lo consume
 * ui.cpp para dibujar y sxxervo.cpp ya define sus propios flags.
 */
#ifndef VENTANAS_H
#define VENTANAS_H

#include <Arduino.h>
#include <stdint.h>
#include <eeprom_cfg.h>

extern uint16_t refMin;       // proximo fin (regando) o inicio (esperando) de riego

/* Getters/setters del estado de cortina (origen del dato: este modulo). */
bool cortinaAbierta();
void cortinaAbierta_set(bool a);

/* Devuelve true si la hora actual cae dentro de cfg.v[idx]. */
bool enVentana(uint8_t idx);

/* Recalcula refMin. Llamar antes de dibujar la pantalla de inicio. */
void calcularRefRiego();

/* Controladores que se llaman desde el ciclo principal. */
void controlRiego();
void controlCortina();

#endif
