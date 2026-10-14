/*
 * alarma.h - Sistema de alarma con armado por horario.
 *
 * Estados posibles:
 *   DESARMADA  - sirena y LED apagados. Trigger no tiene efecto.
 *   ARMADA     - monitoreando la puerta. Si se abre, pasa a DISPARADA.
 *   DISPARADA  - sirena y LED encendidos. Tras alarmaDuracionSeg segundos
 *                vuelve sola a DESARMADA.
 *
 * Zonificacion horaria:
 *   - La alarma SOLO puede armarse dentro de la ventana cfg.v[V_ALARMA].
 *   - Fuera de esa ventana, intentar armar muestra mensaje "Fuera de horario"
 *     y no cambia el estado.
 *
 * Disparador:
 *   - Cuando esta ARMADA y puertaAbierta() devuelve true, se dispara.
 *   - Para evitar falsos positivos al armar (si la puerta ya estaba
 *     abierta al armar), se exige que la puerta este cerrada al menos
 *     una vez despues del armado antes de aceptar la apertura como
 *     intrusion.
 *
 * Pinout:
 *   - Buzzer en D9 (PB1)
 *   - LED   en D10 (PB2) con resistencia 330 ohm a GND
 *
 * Memoria:
 *   - 8 eventos en ring buffer: {tipo, h, m, s} cada uno (4 bytes).
 *   - RAM total ~ 40 bytes para el modulo.
 */
#ifndef ALARMA_H
#define ALARMA_H

#include <Arduino.h>
#include <stdint.h>

enum EstadoAlarma : uint8_t {
  ALARMA_DESARMADA = 0,
  ALARMA_ARMADA    = 1,
  ALARMA_DISPARADA = 2
};

/* Codigos de evento para el log. */
enum TipoEvento : uint8_t {
  EVT_NINGUNO       = 0,
  EVT_ALARMA_ARMAR  = 1,
  EVT_ALARMA_DISPAR = 2,
  EVT_ALARMA_TIEMPO = 3,  // termino el tiempo de sirena
  EVT_ALARMA_DESARM = 4
};

void alarma_iniciar();

/* Llamar cada 500 ms desde el bucle principal. */
void alarma_tick();

/* Comandos de UI. Devuelven true si la operacion es valida. */
bool alarma_armar();     // intenta armar; devuelve false si fuera de horario
bool alarma_desarmar();  // siempre devuelve true; resetea el temporizador
bool alarma_disparar();  // fuerza disparo manual (util para debug)

/* Getters. */
EstadoAlarma alarma_getEstado();
bool         alarma_puedeArmarse();   // helper para el menu

/* Log de eventos. */
#define ALARMA_LOG_CAP 8
struct EventoLog {
  uint8_t    h, m, s;
  TipoEvento tipo;
};

/* Devuelve el evento i-esimo (0 = mas reciente). Devuelve EVT_NINGUNO
   si el slot esta vacio o i fuera de rango. */
EventoLog alarma_getEvento(uint8_t i);
uint8_t   alarma_logCount();

#endif