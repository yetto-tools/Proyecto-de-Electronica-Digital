/*
 * ventanas.cpp - implementacion.
 *
 * Nota: `cortinaAbierta` y `cortinaVentPrev` tambien se declaran en
 * servo.cpp. En la version actual son `static` aqui (este modulo es
 * el duenio del estado) y se accede a ellas via funciones. Si prefieres
 * un unico global, declarar `extern` en servo.cpp y definir aqui.
 */
#include "ventanas.h"
#include <tiempo.h>
#include <bomba_riego.h>
#include <cortina_servo.h>
#include <humedad.h>

uint16_t refMin = 0xFFFF;
static bool s_cortinaAbierta  = false;
static bool s_cortinaVentPrev = false;

/* getters/setters usados por ui.cpp y servo.cpp */
bool cortinaAbierta()         { return s_cortinaAbierta; }
void cortinaAbierta_set(bool a){ s_cortinaAbierta = a; }

static uint16_t aMinutos(uint8_t h, uint8_t m) {
  return (uint16_t)h * 60U + m;
}

bool enVentana(uint8_t idx) {
  const Ventana& v = cfg.v[idx];
  uint16_t t   = aMinutos(ahoraG.h, ahoraG.m);
  uint16_t ini = aMinutos(v.hIni, v.mIni);
  uint16_t fin = aMinutos(v.hFin, v.mFin);
  if (ini == fin) return false;
  if (ini <  fin) return (t >= ini) && (t < fin);
  return (t >= ini) || (t < fin);   // cruza la medianoche
}

void calcularRefRiego() {
  uint16_t t        = aMinutos(ahoraG.h, ahoraG.m);
  uint16_t mejorDif = 0xFFFF;
  refMin = 0xFFFF;
  for (uint8_t i = 0; i < 2; i++) {
    uint16_t ini = aMinutos(cfg.v[i].hIni, cfg.v[i].mIni);
    uint16_t fin = aMinutos(cfg.v[i].hFin, cfg.v[i].mFin);
    if (ini == fin) continue;
    if (enVentana(i)) { refMin = fin; return; }
    uint16_t dif = (ini + 1440U - t) % 1440U;
    if (dif < mejorDif) { mejorDif = dif; refMin = ini; }
  }
}

void controlRiego() {
  /* Riego se activa si:
   *   - estamos dentro de la ventana de la manana, o
   *   - estamos dentro de la ventana de la tarde, o
   *   - la humedad del suelo esta por debajo del umbral configurado
   *     (riego correctivo, independiente del horario).
   */
  bool porHorario = enVentana(V_RIEGO_AM) || enVentana(V_RIEGO_PM);
  bool porHumedad = humedad_esSeco();
  setBomba(porHorario || porHumedad);
}

/*
 * Cortina: solo emite orden al cambiar de estado de la ventana,
 * respetando pulsaciones manuales hasta el proximo evento programado.
 */
void controlCortina() {
  bool abrir = enVentana(V_CORTINA);
  if (abrir != s_cortinaVentPrev) {
    s_cortinaVentPrev = abrir;
    cortinaAbierta_set(abrir);
    moverCortina(abrir);
  }
}
