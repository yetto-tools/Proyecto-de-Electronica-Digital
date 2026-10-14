/*
 * teclado_i2c.cpp - Sondeo del keypad 4x4 via PCF8574.
 *
 * Mapa de teclas (orden fisico del keypad Proteus):
 *
 *     C0    C1    C2    C3
 *   +-----+-----+-----+-----+
 * R0|  1  |  2  |  3  |  A  |
 * R1|  4  |  5  |  6  |  B  |
 * R2|  7  |  8  |  9  |  C  |
 * R3|  *  |  0  |  #  |  D  |
 *   +-----+-----+-----+-----+
 *
 * Las teclas A,B,C,D se mapean a las funciones especiales del menu
 * (A = OK, B = cancelar, C = arriba, D = abajo) por la capa UI.
 *
 * I2C con PCF8574:
 *   - Escritura: un byte pone los 8 pines en HIGH o LOW.
 *   - Lectura: un byte lee el estado de los 8 pines.
 *   - Los pines son cuasi-bidireccionales: si se escriben HIGH y luego
 *     algo externo los pone LOW, la lectura devuelve LOW. Perfecto
 *     para keypad multiplexado.
 */
#include <teclado_i2c.h>
#include <Wire.h>

/* Mapa de teclas (orden filas x columnas). */
static const char TECLAS[4][4] = {
  {'1', '2', '3', 'A'},
  {'4', '5', '6', 'B'},
  {'7', '8', '9', 'C'},
  {'*', '0', '#', 'D'}
};

static char ultimaTecla    = 0;
static char teclaPendiente = 0;
static bool teclaConfirmada = false;

static void escribir(byte valor) {
  Wire.beginTransmission(I2C_KEYPAD_ADDR);
  Wire.write(valor);
  Wire.endTransmission();
}

static byte leer() {
  Wire.requestFrom(I2C_KEYPAD_ADDR, 1);
  if (Wire.available()) return Wire.read();
  return 0xFF;
}

/* Sondea una fila poniendo solo esa fila en LOW y leyendo. */
static char scanFila(uint8_t fila) {
  byte mask = 0xFF;
  // Pone solo la fila `fila` en LOW. Filas = bits 0..3.
  mask &= ~(1 << fila);

  escribir(mask);
  delayMicroseconds(50);  // pequena pausa para que las asen se estabilicen
  byte lectura = leer();

  // Columnas = bits 4..7. Devuelve la columna que este en LOW.
  for (uint8_t col = 0; col < 4; col++) {
    if ((lectura & (1 << (col + 4))) == 0) {
      return TECLAS[fila][col];
    }
  }
  return 0;
}

static char scan() {
  /* Primero: poner todas las filas HIGH para evitar residuos. */
  escribir(0xFF);
  delayMicroseconds(20);

  for (uint8_t f = 0; f < 4; f++) {
    char t = scanFila(f);
    if (t != 0) {
      /* Restaurar todas las filas en HIGH antes de salir. */
      escribir(0xFF);
      return t;
    }
  }
  escribir(0xFF);  // restaurar
  return 0;
}

void teclado_i2c_iniciar() {
  Wire.begin();
  /* Pull-ups en todas las lineas (escritura HIGH en PCF8574 = input). */
  escribir(0xFF);
  ultimaTecla     = 0;
  teclaPendiente  = 0;
  teclaConfirmada = false;
}

void teclado_i2c_tick() {
  char t = scan();

  if (t == ultimaTecla) {
    /* Lectura estable dos veces seguidas. Si no estaba ya confirmada,
       la publicamos. */
    if (t != 0 && !teclaConfirmada) {
      teclaPendiente  = t;
      teclaConfirmada = true;
    } else if (t == 0) {
      teclaConfirmada = false;
    }
  } else {
    /* Cambio entre sondeos: reiniciamos confirmacion. */
    ultimaTecla = t;
    /* Si veníamos de una tecla confirmada y ahora hay otra distinta,
       descartamos la primera y esperamos a que se confirme la nueva. */
    if (t == 0) {
      teclaConfirmada = false;
      teclaPendiente  = 0;
    }
  }
}

char teclado_i2c_leer() {
  if (!teclaConfirmada || teclaPendiente == 0) return 0;
  char t = teclaPendiente;
  teclaPendiente = 0;  // consume
  return t;
}

void teclado_i2c_debugScan() {
  /* Imprime las 16 teclas en formato matriz. */
  Serial.println(F("[KBD]"));
  for (uint8_t f = 0; f < 4; f++) {
    Serial.print(F("  "));
    for (uint8_t c = 0; c < 4; c++) {
      Serial.print(TECLAS[f][c]);
      Serial.print(' ');
    }
    Serial.println();
  }
  Serial.print(F("  ultima=")); Serial.print(ultimaTecla);
  Serial.print(F(" pendiente=")); Serial.print(teclaPendiente);
  Serial.print(F(" confirmada=")); Serial.println(teclaConfirmada ? 'S' : 'N');
}