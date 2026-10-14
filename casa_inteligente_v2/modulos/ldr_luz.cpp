/*
 * ldr_luz.cpp - implementacion.
 */
#include "ldr_luz.h"
#include <eeprom_cfg.h>

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
