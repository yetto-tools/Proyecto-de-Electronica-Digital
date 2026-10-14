/*
 * bomba_riego.cpp - implementaciÃ³n.
 */
#include "bomba_riego.h"

const uint8_t PIN_BOMBA = 7;

bool     bombaOn  = false;
uint16_t riegoSeg = 0;

uint8_t bombaPin()        { return PIN_BOMBA; }
bool    bombaEncendida()  { return bombaOn; }
uint16_t riegoSegundos() { return riegoSeg; }

void setBomba(bool on) {
  if (on == bombaOn) return;
  bombaOn = on;
  if (on) {
    riegoSeg = 0;
    /* Antes se llamaba mostrar7seg(0) para resetear el display fisico.
       En v2 ya no hay display 7-seg: el contador MM:SS se ve en el
       LCD 20x4. La linea se omite. */
  }
  digitalWrite(PIN_BOMBA, on ? HIGH : LOW);
  Serial.println(on ? F("[EVENTO] Riego ENCENDIDO")
                    : F("[EVENTO] Riego APAGADO"));
}

void bomba_setSegundos(uint16_t s) {
  riegoSeg = s;
}
