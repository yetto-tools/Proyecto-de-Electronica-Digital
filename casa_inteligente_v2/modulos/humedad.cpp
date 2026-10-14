/*
 * humedad.cpp - Implementacion del sensor de humedad con promedio movil.
 *
 * Nota sobre el ADC en Arduino Nano:
 *   - analogRead() en A3 comparte el multiplexor ADC con A0 (LDR), A1
 *     y A2. Como el LDR usa A0 y los antiguos pines del keypad usaban
 *     A1/A2, hay que asegurarse de no leer dos ADCs a la vez. Esto ya
 *     esta cubierto porque humedad_tick() y ldr_leerYActualizar() se
 *     llaman desde el mismo bucle de control cada 500 ms, no en paralelo.
 *
 * Promedio movil:
 *   - 8 muestras. Suficiente para estabilizar sin retardar demasiado.
 *   - Una muestra por ciclo de 500 ms -> el filtro converge en ~4 s.
 *     Para alertas de "suelo seco" no hace falta reaccion sub-segundo.
 */
#include <humedad.h>
#include <eeprom_cfg.h>

#define PIN_HUMEDAD A3  // PC3/ADC3, antes ocupado por 74HC595 SRCLK

static uint8_t  buffer[8]   = {0,0,0,0,0,0,0,0};
static uint8_t  indice      = 0;
static uint8_t  ultimaLect  = 0;
static bool     lleno       = false;

void humedad_iniciar() {
  // No requiere pinMode: analogRead configura el pin automaticamente.
  // Cargamos el buffer con la lectura inicial para evitar "rampa" al
  // arrancar.
  uint16_t r0 = analogRead(PIN_HUMEDAD);
  for (uint8_t i = 0; i < 8; i++) buffer[i] = (uint8_t)(r0 >> 2); // 0..255 aprox
  indice     = 0;
  lleno      = true;
  ultimaLect = (uint8_t)(r0 >> 2);
}

void humedad_tick() {
  uint16_t raw   = analogRead(PIN_HUMEDAD);
  uint8_t  valor = (uint8_t)map(raw, 0, 1023, 0, 100);
  buffer[indice] = valor;
  indice = (indice + 1) & 0x07;
  if (indice == 0) lleno = true;

  uint16_t suma = 0;
  uint8_t  n    = lleno ? 8 : indice;
  if (n == 0) n = 1;
  for (uint8_t i = 0; i < n; i++) suma += buffer[i];
  ultimaLect = (uint8_t)(suma / n);
}

uint8_t humedad_leer() {
  return ultimaLect;
}

uint8_t humedad_ultimaLectura() {
  return humedad_leer();
}

bool humedad_esSeco() {
  return (ultimaLect < cfg.humedadSeco);
}