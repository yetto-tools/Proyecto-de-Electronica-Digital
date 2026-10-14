/*
 * lcd_20x4.h - Pantalla principal LM044L via I2C (PCF8574A).
 *
 * Detecta automaticamente la direccion del expansor probando la tabla
 * `LCD_CANDIDATAS`. Usa la libreria `LcdI2C` del curso (orden de args:
 * `LcdI2C(dir, cols, filas)`).
 *
 * Para minimizar escrituras al LCD, `linea()` mantiene un espejo de
 * 4x20 (`pantallaLcd`) y solo envia al LCD los tramos que cambiaron.
 */
#ifndef LCD_20X4_H
#define LCD_20X4_H

#include <Arduino.h>
#include <stdint.h>
#include <LcdI2C.h>

extern const uint8_t LCD_CANDIDATAS[3];

uint8_t lcd20x4CandidatasCount();             // 3
const uint8_t* lcd20x4Candidatas();           // puntero a la tabla

void lcd20x4_iniciar(uint8_t dir);            // crea el objeto, begin(), backlight()
uint8_t detectarI2C(const uint8_t* tabla, uint8_t n);

void linea(uint8_t fila, const char* s);      // reescribe solo lo que cambia
void campoHora(char* out);                    // HH:MM con '_' en los huecos

#endif
