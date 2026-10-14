/*
 * ldr_luz.h - Lectura del LDR y control de luces con histeresis.
 *
 * LDR en A0 con divisor resistivo a 5V/10k: mas luz -> valor mayor.
 * `modoLuz`:
 *   0 = AUTO (controla por umbrales persistidos en EEPROM)
 *   1 = ON manual
 *   2 = OFF manual
 *
 * La histeresis se aplica entre cfg.luzOn y cfg.luzOff: dentro del
 * rango se mantiene el ultimo estado.
 */
#ifndef LDR_LUZ_H
#define LDR_LUZ_H

#include <Arduino.h>

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

#endif
