/*
 * eeprom_cfg.h - Configuracion persistida en EEPROM (version 2).
 *
 * Cambios respecto a v1 (bocetos/main_modular):
 *   - Nueva ventana V_ALARMA (indice 3). N_VENTANAS pasa de 3 a 4.
 *   - Nuevos campos humedadSeco y alarmaDuracionSeg.
 *   - CONFIG_MAGIC sube de 0xA7 a 0xA8 para forzar recarga en EEPROMs
 *     que tengan la version vieja.
 *
 * Layout en EEPROM (16 bytes):
 *   [0]   magic
 *   [1..8]  v[0..3]  (2 bytes cada uno: hIni, mIni)
 *   [9..16] v[0..3]  (hFin, mFin)
 *   [17]   luzOn
 *   [18]   luzOff
 *   [19]   humedadSeco
 *   [20]   alarmaDuracionSeg
 */
#ifndef EEPROM_CFG_H
#define EEPROM_CFG_H

#include <stdint.h>

struct Ventana {
  uint8_t hIni, mIni;
  uint8_t hFin, mFin;
};

enum IndiceVentana : uint8_t {
  V_RIEGO_AM = 0,
  V_RIEGO_PM = 1,
  V_CORTINA  = 2,
  V_ALARMA   = 3,
  N_VENTANAS = 4
};

struct Config {
  uint8_t magic;
  Ventana v[N_VENTANAS];
  uint8_t luzOn;
  uint8_t luzOff;
  uint8_t humedadSeco;       // % por debajo del cual se considera SECO
  uint8_t alarmaDuracionSeg; // 10..120
};

extern const uint8_t CONFIG_MAGIC;
extern const Config  CONFIG_DEFECTO;
extern Config        cfg;

void cargarConfig();
void guardarConfig();

#endif