#include "LcdI2C.h"

LcdI2C::LcdI2C(uint8_t addr, uint8_t cols, uint8_t rows)
  : _addr(addr), _cols(cols), _rows(rows),
    _bl(LCD_BL), _ctrl(LCD_DISPLAY_ON) {
  // Direccion DDRAM de inicio de cada fila.
  // 20x4 -> 0x00, 0x40, 0x14, 0x54
  // 16x4 -> 0x00, 0x40, 0x10, 0x50
  _rowOffsets[0] = 0x00;
  _rowOffsets[1] = 0x40;
  _rowOffsets[2] = 0x00 + cols;
  _rowOffsets[3] = 0x40 + cols;
}

// ================= Inicializacion =================
void LcdI2C::begin() {
  Wire.begin();
  delay(50);                         // esperar a que el LCD encienda
  expanderWrite(0);
  delay(10);

  // Secuencia de reset del HD44780 (datasheet, fig. 24)
  write4(0x30, 0); delayMicroseconds(4500);
  write4(0x30, 0); delayMicroseconds(4500);
  write4(0x30, 0); delayMicroseconds(150);
  write4(0x20, 0);                   // pasar a modo 4 bits

  command(LCD_FUNCTION_SET | LCD_2LINE);
  command(LCD_DISPLAY_CTRL | _ctrl);
  clear();
  command(LCD_ENTRY_MODE | LCD_ENTRY_LEFT);
}

// ================= Funciones basicas =================
void LcdI2C::clear() {
  command(LCD_CLEAR);
  delay(2);                          // este comando tarda ~1.5 ms
}

void LcdI2C::home() {
  command(LCD_HOME);
  delay(2);
}

void LcdI2C::setCursor(uint8_t col, uint8_t row) {
  if (row >= _rows) row = _rows - 1;
  if (col >= _cols) col = _cols - 1;
  command(LCD_SET_DDRAM | (col + _rowOffsets[row]));
}

// ================= Display / cursor =================
void LcdI2C::display()   { _ctrl |=  LCD_DISPLAY_ON; command(LCD_DISPLAY_CTRL | _ctrl); }
void LcdI2C::noDisplay() { _ctrl &= ~LCD_DISPLAY_ON; command(LCD_DISPLAY_CTRL | _ctrl); }
void LcdI2C::cursor()    { _ctrl |=  LCD_CURSOR_ON;  command(LCD_DISPLAY_CTRL | _ctrl); }
void LcdI2C::noCursor()  { _ctrl &= ~LCD_CURSOR_ON;  command(LCD_DISPLAY_CTRL | _ctrl); }
void LcdI2C::blink()     { _ctrl |=  LCD_BLINK_ON;   command(LCD_DISPLAY_CTRL | _ctrl); }
void LcdI2C::noBlink()   { _ctrl &= ~LCD_BLINK_ON;   command(LCD_DISPLAY_CTRL | _ctrl); }

void LcdI2C::backlight()   { _bl = LCD_BL; expanderWrite(0); }
void LcdI2C::noBacklight() { _bl = 0;      expanderWrite(0); }

void LcdI2C::scrollLeft()  { command(LCD_SHIFT | LCD_DISPLAY_MOVE); }
void LcdI2C::scrollRight() { command(LCD_SHIFT | LCD_DISPLAY_MOVE | LCD_MOVE_RIGHT); }

// ================= Caracteres personalizados =================
// slot: 0..7. Despues de llamar, usa setCursor() antes de escribir.
void LcdI2C::createChar(uint8_t slot, const uint8_t data[8]) {
  slot &= 0x07;
  command(LCD_SET_CGRAM | (slot << 3));
  for (uint8_t i = 0; i < 8; i++) {
    write(data[i]);
  }
}

// ================= Envio de datos =================
void LcdI2C::command(uint8_t value) {
  send(value, 0);                    // RS = 0 -> instruccion
}

size_t LcdI2C::write(uint8_t value) {
  send(value, LCD_RS);               // RS = 1 -> dato (caracter)
  return 1;
}

// Envia un byte como dos nibbles (alto y luego bajo)
void LcdI2C::send(uint8_t value, uint8_t mode) {
  write4(value & 0xF0, mode);
  write4((value << 4) & 0xF0, mode);
}

// nibble ya viene en los 4 bits altos (D4..D7 = P4..P7)
void LcdI2C::write4(uint8_t nibble, uint8_t mode) {
  uint8_t data = nibble | mode;      // RW siempre en 0 (escritura)
  expanderWrite(data | LCD_EN);      // E = 1
  delayMicroseconds(1);
  expanderWrite(data & ~LCD_EN);     // E = 0 -> el LCD captura el dato
  delayMicroseconds(50);
}

void LcdI2C::expanderWrite(uint8_t data) {
  Wire.beginTransmission(_addr);
  Wire.write(data | _bl);
  Wire.endTransmission();
}
