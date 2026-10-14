// eeprom_cfg.cpp - Persistencia de la configuracion v2.
// Tamano total en EEPROM: 1 (magic) + 4 ventanas * 4 bytes + 4 = 21 bytes.
// Si modificas Config o CONFIG_DEFECTO, SUBE CONFIG_MAGIC.
#include <eeprom_cfg.h>
#include <EEPROM.h>

const uint8_t CONFIG_MAGIC = 0xA8;

const Config CONFIG_DEFECTO = {
  CONFIG_MAGIC,
  {
    { 6, 30, 7, 30 },    // V_RIEGO_AM
    { 17, 0, 18, 0 },   // V_RIEGO_PM
    { 20, 0, 6, 0 },    // V_CORTINA
    { 22, 0, 6, 0 }     // V_ALARMA (22:00 a 06:00, horario nocturno)
  },
  35,   // luzOn
  55,   // luzOff
  35,   // humedadSeco
  30    // alarmaDuracionSeg
};

Config cfg;

void cargarConfig() {
  uint8_t b;  // variable local sin uso, conservada por compatibilidad
  EEPROM.get(0, cfg);
  if (cfg.magic != CONFIG_MAGIC) {
    cfg = CONFIG_DEFECTO;
    EEPROM.put(0, cfg);
  }
  // Validacion defensiva de los valores criticos.
  if (cfg.luzOff <= cfg.luzOn) cfg.luzOff = cfg.luzOn + 10;
  if (cfg.humedadSeco > 100)   cfg.humedadSeco = 35;
  if (cfg.alarmaDuracionSeg < 10 || cfg.alarmaDuracionSeg > 120) {
    cfg.alarmaDuracionSeg = 30;
  }
}

void guardarConfig() {
  EEPROM.put(0, cfg);
}