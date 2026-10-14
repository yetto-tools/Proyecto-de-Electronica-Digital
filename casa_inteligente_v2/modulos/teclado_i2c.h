/*
 * teclado_i2c.h - Keypad 4x4 multiplexado por PCF8574 sobre I2C.
 *
 * Direccion del PCF8574: 0x20 (la del LCD 20x4 es 0x3F, no chocan).
 *
 * Conexion keypad -> PCF8574 (orden CRITICO, debe coincidir con Proteus):
 *   Fila 0   -> P0
 *   Fila 1   -> P1
 *   Fila 2   -> P2
 *   Fila 3   -> P3
 *   Columna 0 -> P4
 *   Columna 1 -> P5
 *   Columna 2 -> P6
 *   Columna 3 -> P7
 *
 * Algoritmo de sondeo:
 *   - Ponemos todas las filas en HIGH (inactivos) y todas las columnas
 *     en HIGH (pull-up). Resultado: leemos el puerto.
 *   - Para cada fila: la ponemos LOW y leemos. Si una columna lee LOW,
 *     la tecla (fila, columna) esta apretada.
 *   - Sondeo completo cada ~30 ms (32 ms para alinear con tick10ms).
 *
 * Antirrebote:
 *   - Si leemos la misma tecla dos sondeos seguidos, la publicamos.
 *   - Si la tecla cambia entre sondeos, esperamos un ciclo mas.
 *
 * Sin libreria externa: hablamos directo al PCF8574 con Wire.h.
 * Esto ahorra ~1 KB de ROM vs. la libreria Keypad_I2C de Joe Young.
 */
#ifndef TECLADO_I2C_H
#define TECLADO_I2C_H

#include <Arduino.h>

#define I2C_KEYPAD_ADDR 0x20

void teclado_i2c_iniciar();

/* Llamar cada 32 ms aprox desde el bucle principal (o desde el tick
   de 10 ms contando 3 ticks). */
void teclado_i2c_tick();

/* Devuelve la tecla presionada (como caracter) o 0 si no hay.
   Se consume la tecla: la siguiente llamada devuelve 0 hasta que
   el usuario suelte y vuelva a presionar. */
char teclado_i2c_leer();

/* Test/debug: imprime por Serial el estado de las 16 teclas. */
void teclado_i2c_debugScan();

#endif