/*
 * ui.h - Interfaz de usuario: navegacion de pantallas, edicion de
 * horarios/umbrales, dibujo en el LCD principal.
 *
 * Pantallas:
 *   P_INICIO   estado principal en la 20x4 (hora, luz, cortina, riego, alarma, humedad)
 *   P_MENU     navegacion entre las 10 entradas del menu
 *   P_VENTANA  edicion de cfg.v[0..3] (riego AM/PM, cortina, alarma)
 *   P_UMBRAL   edicion de cfg.luzOn / cfg.luzOff
 *   P_HUMEDAD  edicion de cfg.humedadSeco
 *   P_DURALARM edicion de cfg.alarmaDuracionSeg
 *   P_HORA     ajuste de ahoraG (HH:MM)
 *   P_VELOC    cambio de velocidad del reloj (1, 10, 60, 300)
 *   P_ARMADO   armar / desarmar alarma con feedback
 *   P_LOG      ver los ultimos 8 eventos de la alarma
 *   P_MSG      mensaje transitorio (~1.3 s) que ignora pulsaciones
 */
#ifndef UI_H
#define UI_H

#include <Arduino.h>
#include <stdint.h>
#include <eeprom_cfg.h>
#include <cortina_servo.h>

enum Pantalla : uint8_t {
  P_INICIO, P_MENU, P_VENTANA, P_UMBRAL, P_HUMEDAD, P_DURALARM,
  P_HORA, P_VELOC, P_ARMADO, P_LOG, P_MSG
};

extern Pantalla      pantalla;
extern Pantalla      pantallaSig;
extern uint8_t       menuIdx;
extern uint8_t       editIdx;
extern uint8_t       etapa;
extern char          entrada[5];
extern uint8_t       entradaLen;
extern Ventana       tmpV;
extern uint8_t       tmpLuzOn;
extern bool          redibujar;
extern unsigned long msgFin;
extern char          msgL0[21];
extern char          msgL1[21];

const uint8_t N_MENU     = 10;
const uint8_t M_UMBRAL   = 3;   // entrada 4: "Umbral de luz"
const uint8_t M_HUMEDAD  = 4;   // entrada 5: "Umbral humedad"
const uint8_t M_DURALARM = 5;   // entrada 6: "Duracion sirena"
const uint8_t M_HORA     = 6;   // entrada 7: "Ajustar hora"
const uint8_t M_VELOC    = 7;   // entrada 8: "Velocidad reloj"
const uint8_t M_ARMADO   = 8;   // entrada 9: "Armar/Desarmar"
const uint8_t M_LOG      = 9;   // entrada 10: "Log eventos"

/* Helpers de acceso (la API legible desde el resto del sketch). */
bool&          ui_redibujar();
void           ui_tickMensajes();              // cierra P_MSG cuando vence
void           ui_tickReloj();                 // marca redibujar si P_INICIO/P_HORA
void           ui_dibujar();
void           ui_imprimirEstado(uint8_t luz);

/* Edicion y teclas. */
void entrarPantalla(uint8_t p);
void mostrarMsg(const char* l0, const char* l1, uint8_t sig);
void procesarTecla(char k);

/* Acceso de solo lectura para evitar exponer globales innecesariamente. */
Pantalla ui_pantalla();
uint8_t  ui_menuIdx();

#endif
