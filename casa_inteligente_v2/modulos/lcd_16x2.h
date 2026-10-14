/*
 * lcd_16x2.h - Pantalla secundaria opcional. Muestra la hora del sistema.
 *
 * Dos modos segun `LCD2_JHD`:
 *   1 -> modulo JHD-2X16-I2C de Proteus (protocolo con byte de control
 *        0x80 = comando, 0x40 = datos). Implementacion propia.
 *   0 -> pantalla 16x2 con expansor PCF8574 (reusa la lib LcdI2C).
 *
 * Si no se detecta ningun expansor en el bus, se considera "no
 * conectada" y la pantalla simplemente no se actualiza.
 */
#ifndef LCD_16X2_H
#define LCD_16X2_H

#include <Arduino.h>
#include <stdint.h>

/* LCD2_JHD = 1: LCD 16x2 secundario (modulo JHD Proteus).
   LCD2_JHD = 0: deshabilitado (ahorra ROM). */
#define LCD2_JHD 0

const uint8_t LCD2_DIR = 0x3E;   // 7 bits; si no responde, se busca otro

/* Devuelve true si encontro un LCD 16x2 y lo inicializo. */
bool lcd16x2_iniciar(uint8_t lcdPrincipalDir);

/* Direccion I2C detectada (0 = no presente). */
uint8_t lcd16x2_dir();

/* Escribe una fila (0 o 1) segun el modo activo. No hace nada si no hay LCD. */
void lcd2Texto(uint8_t fila, const char* s);

/* Actualiza la fila 1 con la hora del sistema (formato HH:MM:SS). */
void actualizarLcd2();

#endif
