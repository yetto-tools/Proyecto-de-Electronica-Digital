/*
 * ui.cpp â€” implementacion de la interfaz de usuario.
 * Ver ui.h para semantica.
 */
#include "ui.h"
#include <lcd_20x4.h>
#include <lcd_16x2.h>
#include <tiempo.h>
#include <eeprom_cfg.h>
#include <bomba_riego.h>
#include <ldr_luz.h>
#include <ventanas.h>
#include <cortina_servo.h>
#include <humedad.h>
#include <puerta.h>
#include <alarma.h>

/* ----------------------------------------------------------
 *  Nombres de menu en flash (PROGMEM).
 * ---------------------------------------------------------- */
const char NM0[] PROGMEM = "Riego manana";
const char NM1[] PROGMEM = "Riego tarde";
const char NM2[] PROGMEM = "Cortinas";
const char NM3[] PROGMEM = "Umbral de luz";
const char NM4[] PROGMEM = "Umbral humedad";
const char NM5[] PROGMEM = "Duracion sirena";
const char NM6[] PROGMEM = "Ajustar hora";
const char NM7[] PROGMEM = "Velocidad reloj";
const char NM8[] PROGMEM = "Armar/Desarmar";
const char NM9[] PROGMEM = "Log eventos";

const char* const NOMBRES[] PROGMEM = { NM0, NM1, NM2, NM3, NM4, NM5, NM6, NM7, NM8, NM9 };

static const char* nombreMenu(uint8_t i) {
  return (const char*)pgm_read_ptr(&NOMBRES[i]);
}

/* ----------------------------------------------------------
 *  Estado de UI.
 * ---------------------------------------------------------- */
Pantalla      pantalla    = P_INICIO;
Pantalla      pantallaSig = P_INICIO;
uint8_t       menuIdx     = 0;
uint8_t       editIdx     = 0;
uint8_t       etapa       = 0;

char          entrada[5];
uint8_t       entradaLen  = 0;

Ventana       tmpV;
uint8_t       tmpLuzOn    = 0;

bool          redibujar   = true;
unsigned long msgFin      = 0;
char          msgL0[21];
char          msgL1[21];

/* ----------------------------------------------------------
 *  Helpers de RAM.
 * ---------------------------------------------------------- */
static void copiarPgm(char* dst, const char* src, uint8_t maxLen) {
  strncpy_P(dst, src, maxLen);
  dst[maxLen - 1] = '\0';
}

Pantalla ui_pantalla()   { return pantalla; }
uint8_t  ui_menuIdx()    { return menuIdx; }
bool&    ui_redibujar()  { return redibujar; }

void ui_tickMensajes() {
  if (pantalla == P_MSG && (long)(millis() - msgFin) >= 0) {
    entrarPantalla((uint8_t)pantallaSig);
  }
}

void ui_tickReloj() {
  if (pantalla == P_INICIO || pantalla == P_HORA) redibujar = true;
}

void ui_imprimirEstado(uint8_t luz) {
  char buf[48];
  snprintf(buf, sizeof(buf), "%02u:%02u:%02u | Luz %3u%% |",
           ahoraG.h, ahoraG.m, ahoraG.s, luz);
  Serial.print(buf);
  Serial.print(F(" Riego:"));  Serial.print(bombaOn ? F("ON") : F("OFF"));
  Serial.print(F(" Luces:"));  Serial.print(lucesOn ? F("ON") : F("OFF"));
  Serial.print((modoLuz == 0) ? F("(auto)") : F("(manual)"));
  Serial.print(F(" Cortina:"));
  Serial.println(
    (cortinaMov == 1)        ? F("ABRIENDO") :
    (cortinaMov == 2)        ? F("CERRANDO") :
    cortinaAbierta()         ? F("ABIERTA")  : F("CERRADA"));
}

/* ----------------------------------------------------------
 *  Cambio de pantalla / mensajes.
 * ---------------------------------------------------------- */
void limpiarEntrada() {
  entradaLen = 0;
  entrada[0] = '\0';
}

void entrarPantalla(uint8_t p) {
  pantalla = (Pantalla)p;
  limpiarEntrada();
  redibujar = true;
}

void mostrarMsg(const char* l0, const char* l1, uint8_t sig) {
  copiarPgm(msgL0, l0, sizeof(msgL0));
  copiarPgm(msgL1, l1, sizeof(msgL1));
  pantallaSig = (Pantalla)sig;
  msgFin      = millis() + 1300UL;
  pantalla    = P_MSG;
  redibujar   = true;
}

/* ----------------------------------------------------------
 *  Edicion.
 * ---------------------------------------------------------- */
bool parseHora(uint8_t& h, uint8_t& m) {
  if (entradaLen != 4) return false;
  h = (uint8_t)((entrada[0] - '0') * 10 + (entrada[1] - '0'));
  m = (uint8_t)((entrada[2] - '0') * 10 + (entrada[3] - '0'));
  return (h < 24) && (m < 60);
}

void seleccionarMenu() {
  etapa = 0;
  if (menuIdx < N_VENTANAS) {
    editIdx = menuIdx;
    tmpV    = cfg.v[editIdx];
    entrarPantalla(P_VENTANA);
  } else if (menuIdx == M_UMBRAL) {
    entrarPantalla(P_UMBRAL);
  } else if (menuIdx == M_HUMEDAD) {
    entradaLen = 0;
    entrada[0] = '\0';
    entrarPantalla(P_HUMEDAD);
  } else if (menuIdx == M_DURALARM) {
    entradaLen = 0;
    entrada[0] = '\0';
    entrarPantalla(P_DURALARM);
  } else if (menuIdx == M_HORA) {
    entrarPantalla(P_HORA);
  } else if (menuIdx == M_VELOC) {
    entrarPantalla(P_VELOC);
  } else if (menuIdx == M_ARMADO) {
    entrarPantalla(P_ARMADO);
  } else if (menuIdx == M_LOG) {
    entrarPantalla(P_LOG);
  } else {
    entrarPantalla(P_VELOC);
  }
}

void aceptarEntrada() {
  uint8_t h, m;

  switch (pantalla) {

    case P_VENTANA:
      if (!parseHora(h, m)) {
        mostrarMsg(PSTR("Hora invalida"), PSTR("Use HHMM (24h)"), P_VENTANA);
        return;
      }
      if (etapa == 0) {
        tmpV.hIni = h; tmpV.mIni = m;
        etapa = 1;
        entrarPantalla(P_VENTANA);
      } else {
        tmpV.hFin = h; tmpV.mFin = m;
        cfg.v[editIdx] = tmpV;
        guardarConfig();
        Serial.print(F("[PROG] "));
        Serial.print((const __FlashStringHelper*)nombreMenu(editIdx));
        char buf[24];
        snprintf(buf, sizeof(buf), " %02u:%02u - %02u:%02u",
                 tmpV.hIni, tmpV.mIni, tmpV.hFin, tmpV.mFin);
        Serial.println(buf);
        mostrarMsg(PSTR("Guardado"), PSTR(""), P_MENU);
      }
      break;

    case P_UMBRAL: {
      if (entradaLen == 0) return;
      uint8_t valor = (uint8_t)atoi(entrada);
      if (etapa == 0) {
        if (valor > 99) {
          mostrarMsg(PSTR("Valor invalido"), PSTR("0 a 99"), P_UMBRAL);
          return;
        }
        tmpLuzOn = valor;
        etapa = 1;
        entrarPantalla(P_UMBRAL);
      } else {
        if (valor <= tmpLuzOn || valor > 100) {
          mostrarMsg(PSTR("Apagar debe ser"), PSTR("mayor que encender"), P_UMBRAL);
          return;
        }
        cfg.luzOn  = tmpLuzOn;
        cfg.luzOff = valor;
        guardarConfig();
        Serial.print(F("[PROG] Luz enciende/apaga: "));
        Serial.print(cfg.luzOn); Serial.print('/'); Serial.println(cfg.luzOff);
        mostrarMsg(PSTR("Guardado"), PSTR(""), P_MENU);
      }
      break;
    }

    case P_HUMEDAD: {
      if (entradaLen == 0) return;
      uint8_t valor = (uint8_t)atoi(entrada);
      if (valor > 100) {
        mostrarMsg(PSTR("Valor invalido"), PSTR("0 a 100"), P_HUMEDAD);
        return;
      }
      cfg.humedadSeco = valor;
      guardarConfig();
      Serial.print(F("[PROG] Humedad seco bajo: "));
      Serial.println(cfg.humedadSeco);
      mostrarMsg(PSTR("Guardado"), PSTR(""), P_MENU);
      break;
    }

    case P_DURALARM: {
      if (entradaLen == 0) return;
      uint8_t valor = (uint8_t)atoi(entrada);
      if (valor < 10 || valor > 120) {
        mostrarMsg(PSTR("Valor invalido"), PSTR("10 a 120 s"), P_DURALARM);
        return;
      }
      cfg.alarmaDuracionSeg = valor;
      guardarConfig();
      Serial.print(F("[PROG] Duracion sirena: "));
      Serial.println(cfg.alarmaDuracionSeg);
      mostrarMsg(PSTR("Guardado"), PSTR(""), P_MENU);
      break;
    }

    case P_HORA:
      if (!parseHora(h, m)) {
        mostrarMsg(PSTR("Hora invalida"), PSTR("Use HHMM (24h)"), P_HORA);
        return;
      }
      tiempo_ajustar(h, m, 0);
      Serial.println(F("[PROG] Hora ajustada"));
      mostrarMsg(PSTR("Hora ajustada"), PSTR(""), P_INICIO);
      break;

    default:
      break;
  }
}

void teclaEntrada(char k) {
  uint8_t maxLen = (pantalla == P_UMBRAL || pantalla == P_HUMEDAD || pantalla == P_DURALARM) ? 3 : 4;

  if (k >= '0' && k <= '9') {
    if (entradaLen < maxLen) {
      entrada[entradaLen++] = k;
      entrada[entradaLen]   = '\0';
    }
  } else if (k == 'X') {
    if (entradaLen > 0)      entrada[--entradaLen] = '\0';
    else if (etapa == 1)     { etapa = 0; entrarPantalla((uint8_t)pantalla); }
    else                      entrarPantalla(P_MENU);
  } else if (k == 'K') {
    aceptarEntrada();
  }
}

void procesarTecla(char k) {
  redibujar = true;

  switch (pantalla) {

    case P_INICIO:
      if      (k == 'A') { menuIdx = 0; entrarPantalla(P_MENU); }
      else if (k == 'B') { etapa = 0; entrarPantalla(P_HORA); }    // F1: ajustar hora
      else if (k == 'C') {              entrarPantalla(P_VELOC); } // F2: velocidad
      else if (k == 'D') { luz_cicloModo(); }
      else if (k == '1') moverCortina(true);
      else if (k == '2') moverCortina(false);
      else if (k == '0') detenerCortina();
      break;

    case P_MENU:
      if      (k == 'B') menuIdx = (menuIdx + 1) % N_MENU;
      else if (k == 'C') menuIdx = (menuIdx + N_MENU - 1) % N_MENU;
      else if (k >= '1' && k <= '0' + N_MENU) {
        menuIdx = (uint8_t)(k - '1');
        seleccionarMenu();
      }
      else if (k == 'K') seleccionarMenu();
      else if (k == 'X' || k == 'A') entrarPantalla(P_INICIO);
      break;

    case P_VENTANA:
    case P_UMBRAL:
    case P_HUMEDAD:
    case P_DURALARM:
    case P_HORA:
      teclaEntrada(k);
      break;

    case P_ARMADO:
      if (k == 'A') {
        if (alarma_armar()) {
          mostrarMsg(PSTR("Alarma armada"), PSTR(""), P_INICIO);
        } else {
          mostrarMsg(PSTR("Fuera de horario"), PSTR("ver ventana alarma"), P_ARMADO);
        }
      } else if (k == 'D') {
        alarma_desarmar();
        mostrarMsg(PSTR("Alarma desarmada"), PSTR(""), P_INICIO);
      } else if (k == 'X') {
        entrarPantalla(P_MENU);
      }
      break;

    case P_LOG:
      if (k == 'X' || k == 'A' || k == 'B') {
        entrarPantalla(P_MENU);
      }
      break;

    case P_VELOC:
      if (k >= '1' && k <= '4') {
        tiempo_setVelocidad((uint8_t)(k - '1'));
        Serial.print(F("[PROG] Velocidad del reloj: x"));
        Serial.println(tiempo_velocidad());
        mostrarMsg(PSTR("Velocidad cambiada"), PSTR(""), P_INICIO);
      } else if (k == 'X') {
        entrarPantalla(P_MENU);
      }
      break;

    default:
      break;
  }
}

/* ----------------------------------------------------------
 *  Dibujo en el LCD 20x4 (cuatro lineas).
 * ---------------------------------------------------------- */
void ui_dibujar() {
  char f[4][24];
  char campo[8];
  for (uint8_t i = 0; i < 4; i++) f[i][0] = '\0';

  switch (pantalla) {

    case P_INICIO: {
      const char* cort =
        (cortinaMov == 1)     ? "ABRIENDO" :
        (cortinaMov == 2)     ? "CERRANDO" :
        cortinaAbierta()      ? "ABIERTA"  : "CERRADA";
      const char* estAlarma =
        (alarma_getEstado() == ALARMA_DISPARADA) ? "DISPARO" :
        (alarma_getEstado() == ALARMA_ARMADA)    ? "ARMADA  " : "DESARMAD";
      calcularRefRiego();
      snprintf(f[0], 24, "CASA x%-3u   %02u:%02u:%02u",
               tiempo_velocidad(), ahoraG.h, ahoraG.m, ahoraG.s);
      snprintf(f[1], 24, "Luz:%3u%% Hum:%3u%% %c",
               ldr_ultimaLectura(), humedad_leer(),
               (modoLuz == 0) ? 'A' : 'M');
      snprintf(f[2], 24, "Cortina:%-8s Pta:%s",
               cort, puertaAbierta() ? "ABIERTA" : "CERRADA");
      if (refMin == 0xFFFF) {
        snprintf(f[3], 24, "Alm:%s Riego:%-3s",
                 estAlarma, bombaOn ? "ON" : "OFF");
      } else {
        snprintf(f[3], 24, "Alm:%s Rgo:%s%02u:%02u",
                 estAlarma,
                 bombaOn ? "ON " : "OFF",
                 (uint8_t)(refMin / 60), (uint8_t)(refMin % 60));
      }
      break;
    }

    case P_MENU: {
      uint8_t primero = (menuIdx > 3) ? (menuIdx - 3) : 0;
      for (uint8_t r = 0; r < 4; r++) {
        uint8_t idx = primero + r;
        if (idx < N_MENU) {
          snprintf_P(f[r], 24, PSTR("%c%u %S"),
                     (idx == menuIdx) ? '>' : ' ', idx + 1, nombreMenu(idx));
        }
      }
      break;
    }

    case P_VENTANA: {
      const Ventana& v = cfg.v[editIdx];
      const char* et;
      if (editIdx == V_CORTINA) et = (etapa == 0) ? PSTR("ABRIR") : PSTR("CERRAR");
      else                     et = (etapa == 0) ? PSTR("INICIO") : PSTR("FIN");
      snprintf_P(f[0], 24, PSTR("%S %S"), nombreMenu(editIdx), et);
      snprintf_P(f[1], 24, PSTR("Actual: %02u:%02u"),
                 (etapa == 0) ? v.hIni : v.hFin,
                 (etapa == 0) ? v.mIni : v.mFin);
      campoHora(campo);
      snprintf_P(f[2], 24, PSTR("Nuevo:  %s"), campo);
      snprintf_P(f[3], 24, PSTR("OK=ok  ON/C=borrar"));
      break;
    }

    case P_UMBRAL:
      snprintf_P(f[0], 24,
                 (etapa == 0) ? PSTR("Luz: ENCENDER bajo")
                              : PSTR("Luz: APAGAR sobre"));
      snprintf_P(f[1], 24, PSTR("Actual: %u%%"),
                 (etapa == 0) ? cfg.luzOn : cfg.luzOff);
      snprintf_P(f[2], 24, PSTR("Nuevo:  %s%s"),
                 entrada, (entradaLen < 3) ? "_" : "");
      snprintf_P(f[3], 24, PSTR("OK=ok  ON/C=borrar"));
      break;

    case P_HUMEDAD:
      snprintf_P(f[0], 24, PSTR("Humedad: SECO bajo"));
      snprintf_P(f[1], 24, PSTR("Actual: %u%%"), cfg.humedadSeco);
      snprintf_P(f[2], 24, PSTR("Nuevo:  %s%s"),
                 entrada, (entradaLen < 3) ? "_" : "");
      snprintf_P(f[3], 24, PSTR("OK=ok  ON/C=borrar"));
      break;

    case P_DURALARM:
      snprintf_P(f[0], 24, PSTR("Sirena: duracion"));
      snprintf_P(f[1], 24, PSTR("Actual: %u s"), cfg.alarmaDuracionSeg);
      snprintf_P(f[2], 24, PSTR("Nuevo:  %s%s"),
                 entrada, (entradaLen < 3) ? "_" : "");
      snprintf_P(f[3], 24, PSTR("OK=ok  ON/C=borrar"));
      break;

    case P_ARMADO: {
      const char* est =
        (alarma_getEstado() == ALARMA_DISPARADA) ? "DISPARADA" :
        (alarma_getEstado() == ALARMA_ARMADA)    ? "ARMADA"    : "DESARMADA";
      snprintf_P(f[0], 24, PSTR("Alarma: %s"), est);
      snprintf_P(f[1], 24, PSTR("Horario: %02u:%02u - %02u:%02u"),
                 cfg.v[V_ALARMA].hIni, cfg.v[V_ALARMA].mIni,
                 cfg.v[V_ALARMA].hFin, cfg.v[V_ALARMA].mFin);
      snprintf_P(f[2], 24, PSTR("A=Armar D=Desarmar"));
      snprintf_P(f[3], 24, PSTR("ON/C=atras"));
      break;
    }

    case P_LOG: {
      uint8_t n = alarma_logCount();
      snprintf_P(f[0], 24, PSTR("Log (%u eventos):"), n);
      /* Mostramos los 3 eventos mas recientes (slots 0, 1, 2). */
      EventoLog e;
      uint8_t r = 1;
      for (uint8_t i = 0; i < 3 && i < n; i++, r++) {
        e = alarma_getEvento(i);
        const char* t =
          (e.tipo == EVT_ALARMA_DISPAR) ? "DISPARO " :
          (e.tipo == EVT_ALARMA_ARMAR)  ? "ARMADA  " :
          (e.tipo == EVT_ALARMA_TIEMPO) ? "TIEMPO  " :
          (e.tipo == EVT_ALARMA_DESARM) ? "DESARMAD" : "???     ";
        snprintf_P(f[r], 24, PSTR("%02u:%02u:%02u %S"), e.h, e.m, e.s, t);
      }
      for (; r < 4; r++) f[r][0] = '\0';
      break;
    }

    case P_HORA:
      snprintf_P(f[0], 24, PSTR("Ajustar hora"));
      snprintf_P(f[1], 24, PSTR("Actual: %02u:%02u:%02u"),
                 ahoraG.h, ahoraG.m, ahoraG.s);
      campoHora(campo);
      snprintf_P(f[2], 24, PSTR("Nuevo:  %s"), campo);
      snprintf_P(f[3], 24, PSTR("OK=ok  ON/C=borrar"));
      break;

    case P_VELOC:
      snprintf_P(f[0], 24, PSTR("Velocidad del reloj"));
      snprintf_P(f[1], 24, PSTR("Actual: x%u"), tiempo_velocidad());
      snprintf_P(f[2], 24, PSTR("1=x1  2=x10  3=x60"));
      snprintf_P(f[3], 24, PSTR("4=x300  ON/C=atras"));
      break;

    case P_MSG:
      strcpy(f[1], msgL0);
      strcpy(f[2], msgL1);
      break;
  }

  for (uint8_t i = 0; i < 4; i++) linea(i, f[i]);
}
