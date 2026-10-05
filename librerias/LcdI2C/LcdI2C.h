#ifndef LCD_I2C_H
#define LCD_I2C_H

#include <Arduino.h>
#include <Wire.h>
#include <Print.h>

// ---------- Mapeo PCF8574 / PCF8574A -> LCD ----------
// P0=RS, P1=RW, P2=E, P3=Backlight, P4..P7 = D4..D7
#define LCD_RS  0x01
#define LCD_RW  0x02
#define LCD_EN  0x04
#define LCD_BL  0x08

// ---------- Comandos HD44780 ----------
#define LCD_CLEAR         0x01
#define LCD_HOME          0x02
#define LCD_ENTRY_MODE    0x04
#define LCD_DISPLAY_CTRL  0x08
#define LCD_SHIFT         0x10
#define LCD_FUNCTION_SET  0x20
#define LCD_SET_CGRAM     0x40
#define LCD_SET_DDRAM     0x80

// ---------- Banderas ----------
#define LCD_ENTRY_LEFT    0x02   // escribir de izquierda a derecha
#define LCD_DISPLAY_ON    0x04
#define LCD_CURSOR_ON     0x02
#define LCD_BLINK_ON      0x01
#define LCD_DISPLAY_MOVE  0x08
#define LCD_MOVE_RIGHT    0x04
#define LCD_2LINE         0x08   // 20x4 y 16x2 usan modo "2 lineas"

class LcdI2C : public Print {
public:
  LcdI2C(uint8_t addr, uint8_t cols, uint8_t rows);

  void begin();
  void clear();
  void home();
  void setCursor(uint8_t col, uint8_t row);

  void display();     void noDisplay();
  void cursor();      void noCursor();
  void blink();       void noBlink();
  void backlight();   void noBacklight();
  void scrollLeft();  void scrollRight();

  void createChar(uint8_t slot, const uint8_t data[8]);
  void command(uint8_t value);

  // Necesario para heredar print(), println() de Arduino
  virtual size_t write(uint8_t value);
  using Print::write;

private:
  void send(uint8_t value, uint8_t mode);
  void write4(uint8_t nibble, uint8_t mode);
  void expanderWrite(uint8_t data);

  uint8_t _addr;
  uint8_t _cols;
  uint8_t _rows;
  uint8_t _bl;         // estado del backlight
  uint8_t _ctrl;       // display / cursor / blink
  uint8_t _rowOffsets[4];
};

#endif
