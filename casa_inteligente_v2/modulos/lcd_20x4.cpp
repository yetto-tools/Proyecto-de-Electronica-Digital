/*
 * lcd_20x4.cpp - implementacion.
 * Ver lcd_20x4.h para semantica.
 */
#include "lcd_20x4.h"
#include <Wire.h>

/* Forward declarations de variables de ui.h. Esto rompe el ciclo de
   inclusion: lcd_20x4.h expone `linea()` y `campoHora()` (que necesita
   entrada/entradaLen), y ui.h expone el resto. ui.h si incluye
   lcd_20x4.h, pero aqui solo necesitamos los nombres. */
extern char     entrada[5];
extern uint8_t  entradaLen;

/* Direcciones I2C que prueba el firmware para encontrar el LCD 20x4.
   El orden importa: las mas comunes primero. La 0x20 NO esta incluida
   porque esa direccion la usa el PCF8574 del keypad (teclado_i2c.h). */
const uint8_t LCD_CANDIDATAS[3] = { 0x3F, 0x38, 0x27 };

LcdI2C *lcd = nullptr;
char    pantallaLcd[4][20] = { {0}, {0}, {0}, {0} };

uint8_t lcd20x4CandidatasCount() { return 3; }
const uint8_t* lcd20x4Candidatas() { return LCD_CANDIDATAS; }

void lcd20x4_iniciar(uint8_t dir) {
  lcd = new LcdI2C(dir, 20, 4);
  lcd->begin();
  lcd->backlight();
}

/*
 * Devuelve la primera direccion de `tabla` que responda a ACK.
 * Si ninguna responde, devuelve tabla[0] (candidata por defecto).
 */
uint8_t detectarI2C(const uint8_t* tabla, uint8_t n) {
  for (uint8_t i = 0; i < n; i++) {
    Wire.beginTransmission(tabla[i]);
    if (Wire.endTransmission() == 0) return tabla[i];
  }
  return tabla[0];
}

/*
 * Escribe 20 caracteres en la fila, reusando `pantallaLcd` para solo
 * enviar al LCD los tramos que cambiaron. Rellena con espacios.
 */
void linea(uint8_t fila, const char* s) {
  if (!lcd) return;
  char b[21];
  uint8_t i = 0;
  while (i < 20 && s[i]) { b[i] = s[i]; i++; }
  while (i < 20) { b[i] = ' '; i++; }
  b[20] = '\0';

  i = 0;
  while (i < 20) {
    if (b[i] == pantallaLcd[fila][i]) { i++; continue; }
    uint8_t ini = i;
    char     tramo[21];
    uint8_t  n = 0;
    while (i < 20 &&
           (b[i] != pantallaLcd[fila][i] ||
            (i + 1 < 20 && b[i + 1] != pantallaLcd[fila][i + 1]))) {
      tramo[n++] = b[i];
      pantallaLcd[fila][i] = b[i];
      i++;
    }
    tramo[n] = '\0';
    lcd->setCursor(ini, fila);
    lcd->print(tramo);
  }
}

void campoHora(char* out) {
  out[0] = (entradaLen > 0) ? entrada[0] : '_';
  out[1] = (entradaLen > 1) ? entrada[1] : '_';
  out[2] = ':';
  out[3] = (entradaLen > 2) ? entrada[2] : '_';
  out[4] = (entradaLen > 3) ? entrada[3] : '_';
  out[5] = '\0';
}
