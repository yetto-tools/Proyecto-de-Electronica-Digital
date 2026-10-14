/*
 * bomba_riego.h - Actuador de la bomba de riego + contador MM:SS.
 *
 * El contador `riegoSeg` se incrementa mientras `bombaOn` este activa
 * (lo hace el loop()), y se muestra en el display de 7 segmentos.
 * Al volver a ON se reinicia a 0.
 */
#ifndef BOMBA_RIEGO_H
#define BOMBA_RIEGO_H

#include <Arduino.h>

extern const uint8_t PIN_BOMBA;
extern bool          bombaOn;
extern uint16_t      riegoSeg;

uint8_t bombaPin();
bool    bombaEncendida();
uint16_t riegoSegundos();

/* Activa/desactiva la bomba. Imprime evento por serie. */
void setBomba(bool on);

/* Ajusta manualmente el contador (usado por el loop principal). */
void bomba_setSegundos(uint16_t s);

#endif
