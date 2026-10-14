/*
 * lcd_16x2.cpp - implementacion.
 */
#include "lcd_16x2.h"
#include <tiempo.h>
#include <Wire.h>
#include <LcdI2C.h>

static LcdI2C *lcd2    = nullptr;
static uint8_t lcd2Dir = 0;

#if LCD2_JHD
static void jhdComando(uint8_t c);
static void jhdTexto(uint8_t fila, const char* s);
#endif

uint8_t lcd16x2_dir() { return lcd2Dir; }

bool lcd16x2_iniciar(uint8_t lcdPrincipalDir) {
  lcd2Dir = 0;
  Wire.beginTransmission(LCD2_DIR);
  if (LCD2_DIR != lcdPrincipalDir && Wire.endTransmission() == 0) {
    lcd2Dir = LCD2_DIR;
  }
  for (uint8_t d = 0x20; !lcd2Dir && d <= 0x3F; d++) {
    if (d == lcdPrincipalDir || (d > 0x27 && d < 0x38)) continue;
    Wire.beginTransmission(d);
    if (Wire.endTransmission() == 0) lcd2Dir = d;
  }
  if (!lcd2Dir) return false;

#if LCD2_JHD
  delay(50);
  jhdComando(0x38); delay(5);
  jhdComando(0x38);
  jhdComando(0x0C);
  jhdComando(0x01); delay(3);
  jhdComando(0x06);
#else
  lcd2 = new LcdI2C(lcd2Dir, 16, 2);
  lcd2->begin();
  lcd2->backlight();
#endif
  lcd2Texto(0, "Hora del sistema");
  return true;
}

#if LCD2_JHD

static void jhdComando(uint8_t c) {
  Wire.beginTransmission(lcd2Dir);
  Wire.write((uint8_t)0x80);   // byte de control: comando
  Wire.write(c);
  Wire.endTransmission();
}

static void jhdTexto(uint8_t fila, const char* s) {
  jhdComando((uint8_t)0x80 | (fila ? 0x40 : 0x00));
  Wire.beginTransmission(lcd2Dir);
  Wire.write((uint8_t)0x40);   // byte de control: datos
  for (uint8_t i = 0; i < 16 && s[i]; i++) Wire.write((uint8_t)s[i]);
  Wire.endTransmission();
}

void lcd2Texto(uint8_t fila, const char* s) { jhdTexto(fila, s); }

#else

void lcd2Texto(uint8_t fila, const char* s) {
  if (!lcd2) return;
  lcd2->setCursor(0, fila);
  lcd2->print(s);
}

#endif

void actualizarLcd2() {
  if (!lcd2Dir) return;
  char buf[17];
  snprintf(buf, sizeof(buf), "    %02u:%02u:%02u    ",
           ahoraG.h, ahoraG.m, ahoraG.s);
  lcd2Texto(1, buf);
}
