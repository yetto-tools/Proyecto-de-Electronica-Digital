/*
 * puerta.cpp - Implementacion del reed switch con antirrebote.
 *
 * Logica:
 *   - D3 con INPUT_PULLUP. Pull-up interno = ~20..50 kOhm. Cuando el reed
 *     esta cerrado (imán pegado al marco), D3 queda pulled-down a GND.
 *     Cuando el reed abre, D3 flota hacia HIGH por el pull-up.
 *   - Antirrebote: solo cambiamos el estado si la lectura coincide durante
 *     dos ticks consecutivos (50 ms de margen).
 *
 * Memoria: 4 bytes de estado.
 */
#include <puerta.h>

#define PIN_REED_PUERTA 3   // PD3, antes ocupado por 74HC595 SER

static bool     estadoFiltrado = false;  // estado publicado (sin ruido)
static bool     lecturaActual  = false;  // ultima lectura cruda
static uint32_t msUltimoCambio = 0;
static const uint32_t ANTIRREBOTE_MS = 50UL;

void puerta_iniciar() {
  pinMode(PIN_REED_PUERTA, INPUT_PULLUP);
  lecturaActual   = (digitalRead(PIN_REED_PUERTA) == HIGH);
  estadoFiltrado  = lecturaActual;
  msUltimoCambio  = millis();
}

void puerta_tick() {
  bool nuevaLectura = (digitalRead(PIN_REED_PUERTA) == HIGH);
  if (nuevaLectura != lecturaActual) {
    // Cambio brusco. Esperamos a que se confirme antes de publicar.
    if (millis() - msUltimoCambio >= ANTIRREBOTE_MS) {
      lecturaActual  = nuevaLectura;
      estadoFiltrado = nuevaLectura;
      msUltimoCambio = millis();
    }
  } else {
    // La lectura coincide con la anterior; reseteamos el contador.
    msUltimoCambio = millis();
  }
}

bool puertaAbierta() {
  return estadoFiltrado;
}