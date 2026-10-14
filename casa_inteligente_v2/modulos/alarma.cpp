/*
 * alarma.cpp - Maquina de estados de la alarma.
 *
 * Llamada desde loop() cada 500 ms. Mide el tiempo de la sirena con
 * millis() para no depender del reloj del sistema (que va acelerado).
 *
 * Maquina:
 *
 *   DESARMADA --(armar() && enHorario)--> ARMADA
 *   ARMADA    --(puertaAbierta() && puertaEstuvoCerrada)--> DISPARADA
 *   DISPARADA --(transcurrido alarmaDuracionSeg)--> DESARMADA
 *   *         --(desarmar())--> DESARMADA
 *
 * Variables de tiempo:
 *   - msDisparoInicio: cuando empezo la sirena. 0 = no disparada.
 */
#include <alarma.h>
#include <puerta.h>
#include <eeprom_cfg.h>
#include <tiempo.h>
#include <ventanas.h>

#define PIN_BUZZER 9      // PB1
#define PIN_LED    10     // PB2

static EstadoAlarma estado       = ALARMA_DESARMADA;
static uint32_t     msDisparoIni = 0;

/* Para evitar disparo inmediato al armar con la puerta ya abierta. */
static bool         puertaEstuvoCerrada = true;

/* Ring buffer de eventos. */
static EventoLog    logBuf[ALARMA_LOG_CAP];
static uint8_t      logHead = 0;  // indice del proximo a escribir
static uint8_t      logSize = 0;

/* Helpers internos con forward declaration. */
static void log_push(TipoEvento t);
static void actuadores_set(bool on);
static bool enHorarioAlarma();

void alarma_iniciar() {
  pinMode(PIN_BUZZER, OUTPUT);
  pinMode(PIN_LED,    OUTPUT);
  actuadores_set(false);
  msDisparoIni       = 0;
  estado             = ALARMA_DESARMADA;
  puertaEstuvoCerrada = !puertaAbierta();  // coherente con el estado actual
}

void alarma_tick() {
  /* Maquina principal. */
  switch (estado) {
    case ALARMA_ARMADA:
      if (puertaAbierta() && puertaEstuvoCerrada) {
        /* Intrusion real: la puerta estuvo cerrada al menos una vez
           desde que se armo, y ahora esta abierta. */
        alarma_disparar();
      }
      if (!puertaAbierta()) {
        puertaEstuvoCerrada = true;
      }
      break;

    case ALARMA_DISPARADA: {
      uint32_t duracionMs = (uint32_t)cfg.alarmaDuracionSeg * 1000UL;
      if (millis() - msDisparoIni >= duracionMs) {
        // Tiempo cumplido: autoapagado.
        actuadores_set(false);
        log_push(EVT_ALARMA_TIEMPO);
        estado             = ALARMA_DESARMADA;
        puertaEstuvoCerrada = true;
      }
      break;
    }

    case ALARMA_DESARMADA:
    default:
      // Nada que hacer.
      break;
  }
}

bool alarma_armar() {
  if (!enHorarioAlarma()) return false;
  if (estado == ALARMA_DESARMADA) {
    estado              = ALARMA_ARMADA;
    puertaEstuvoCerrada = !puertaAbierta();  // sincronizar con realidad
    log_push(EVT_ALARMA_ARMAR);
  }
  return true;
}

bool alarma_desarmar() {
  if (estado != ALARMA_DESARMADA) {
    actuadores_set(false);
    msDisparoIni = 0;
    log_push(EVT_ALARMA_DESARM);
    estado = ALARMA_DESARMADA;
  }
  return true;
}

bool alarma_disparar() {
  if (estado == ALARMA_DESARMADA) return false;
  actuadores_set(true);
  msDisparoIni = millis();
  estado       = ALARMA_DISPARADA;
  log_push(EVT_ALARMA_DISPAR);
  return true;
}

EstadoAlarma alarma_getEstado() {
  return estado;
}

bool alarma_puedeArmarse() {
  return enHorarioAlarma();
}

/* ------- Log ------- */

EventoLog alarma_getEvento(uint8_t i) {
  EventoLog vacio = {0, 0, 0, EVT_NINGUNO};
  if (i >= logSize) return vacio;
  /* logHead apunta al proximo slot libre. El evento mas reciente
     esta en logHead-1, el siguiente en logHead-2, etc. */
  uint8_t idx = (uint8_t)((logHead + ALARMA_LOG_CAP - 1 - i) % ALARMA_LOG_CAP);
  return logBuf[idx];
}

uint8_t alarma_logCount() {
  return logSize;
}

/* ------- Privados ------- */

static void log_push(TipoEvento t) {
  logBuf[logHead].h    = ahoraG.h;
  logBuf[logHead].m    = ahoraG.m;
  logBuf[logHead].s    = ahoraG.s;
  logBuf[logHead].tipo = t;
  logHead = (logHead + 1) % ALARMA_LOG_CAP;
  if (logSize < ALARMA_LOG_CAP) logSize++;
}

static void actuadores_set(bool on) {
  digitalWrite(PIN_BUZZER, on ? HIGH : LOW);
  digitalWrite(PIN_LED,    on ? HIGH : LOW);
}

static bool enHorarioAlarma() {
  /* Si la ventana es 0..0, queda deshabilitada (mismo criterio que
     las ventanas de riego: inicio == fin = deshabilitada). */
  return enVentana(V_ALARMA);
}