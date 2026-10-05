# Sistema para Casa — Grupo 1

Proyecto del curso de **Electrónica Digital (UMG)**. Sistema domótico basado en **Arduino Nano v3** que ejecuta acciones según la hora de un reloj de tiempo real, simulado en **Proteus 8.13**.

## Funciones

- **Iluminación automática**: un LDR mide la luz ambiente; las luces se encienden con poca luz y se apagan con mucha (con histéresis). `F3` alterna AUTO / ON / OFF manual.
- **Riego del jardín programado**: una ventana de mañana y una de tarde, con hora de inicio y fin configurables.
- **Cortinas con motor DC** (puente H L293D): se abren y cierran por horario o manualmente con el teclado.
- **Configuración desde teclado 4x4** y pantalla LCD 20x4; los valores se guardan en EEPROM.
- **Reloj editable**: hora y fecha se ajustan desde el teclado; la velocidad (x1, x10, x60, x300) se cambia sin recompilar para las demostraciones.

Todo corre en **un solo Arduino Nano v3**.

## Hardware

| Bloque | Componente |
|---|---|
| Microcontrolador | Arduino Nano v3 (ATmega328P, 16 MHz) |
| Reloj de tiempo real | DS1307 (I2C, 0x68) con cristal de 32.768 kHz |
| Pantalla | LCD 20x4 (LM044L) mediante expansor PCF8574A (I2C, 0x38) |
| Entrada | Teclado matricial 4x4, LDR (luz) |
| Salidas | Riego (D7), luces (D8), motor de cortina por L293D (D9 abrir, D10 cerrar) |

El diagrama pictórico está en [`diagrama_conexion.png`](diagrama_conexion.png); la asignación de pines, el guion de demostración y la solución de problemas están en [`PROTEUS.md`](PROTEUS.md).

## Conexión pin a pin

Pines del Nano según el conector de 30 pines (`TX1`=1 … `D2`=5 … `A0`=19 … `5V`=27, `GND`=29). Pines de los chips según su encapsulado DIP.

### Arduino Nano v3

| Pin Nano | Nº | Función | Se conecta a |
|---|---|---|---|
| D2 | 5 | Fila 1 teclado | KEYPAD `ROW1` |
| D4 | 7 | Fila 2 teclado | KEYPAD `ROW2` |
| D5 | 8 | Fila 3 teclado | KEYPAD `ROW3` |
| D6 | 9 | Fila 4 teclado | KEYPAD `ROW4` |
| D7 | 10 | Riego (bomba) | Ánodo del LED azul vía 330 Ω (o entrada del transistor/relé) |
| D8 | 11 | Luces | Ánodo del LED amarillo vía 330 Ω |
| D9 | 12 | Cortina: abrir | L293D pin 2 (`1A`, IN1) |
| D10 | 13 | Cortina: cerrar | L293D pin 7 (`2A`, IN2) |
| D11 | 14 | Columna 1 teclado | KEYPAD `COL1` |
| D12 | 15 | Columna 2 teclado | KEYPAD `COL2` |
| A2 | 21 | Columna 3 teclado | KEYPAD `COL3` |
| A1 | 20 | Columna 4 teclado | KEYPAD `COL4` |
| A0 | 19 | Luz ambiente | Unión LDR – resistencia de 10 kΩ |
| A4 | 23 | SDA (I2C) | DS1307 pin 5, PCF8574A pin 15, pull-up 4.7 kΩ a +5 V |
| A5 | 24 | SCL (I2C) | DS1307 pin 6, PCF8574A pin 14, pull-up 4.7 kΩ a +5 V |
| D1 / TX1 | 1 | Salida serie | Virtual Terminal `RXD` |
| 5V | 27 | Alimentación | Todos los puntos +5 V |
| GND | 29 | Masa | Todos los puntos GND |

Los pines D0, D3, D13, A3, A6 y A7 quedan libres.

### Teclado 4x4 (KEYPAD-SMALLCALC)

En el componente de Proteus las filas son las terminales de la izquierda (`A`–`D`) y las columnas las de abajo (`1`–`4`).

| Terminal KEYPAD | Fila / columna | Teclas | Se conecta a |
|---|---|---|---|
| A | Fila 1 | `1` `2` `3` `F0` | Nano D2 |
| B | Fila 2 | `4` `5` `6` `F1` | Nano D4 |
| C | Fila 3 | `7` `8` `9` `F2` | Nano D5 |
| D | Fila 4 | `ON/C` `0` `ok` `F3` | Nano D6 |
| 1 | Columna 1 | `1` `4` `7` `ON/C` | Nano D11 |
| 2 | Columna 2 | `2` `5` `8` `0` | Nano D12 |
| 3 | Columna 3 | `3` `6` `9` `ok` | Nano A2 |
| 4 | Columna 4 | `F0` `F1` `F2` `F3` | Nano A1 |

Sin resistencias externas: el sketch usa los pull-up internos del ATmega (librería `Keypad`).

### Luz ambiente (LDR)

| De | A |
|---|---|
| +5 V | LDR terminal 1 |
| LDR terminal 2 | Nano A0 y resistencia de 10 kΩ (terminal 1) |
| Resistencia de 10 kΩ (terminal 2) | GND |

### Motor de cortina (L293D)

| Pin L293D | Nombre | Se conecta a |
|---|---|---|
| 1 | EN1 | +5 V |
| 2 | 1A | Nano D9 |
| 3 | 1Y | Motor, terminal 1 |
| 4, 5 | GND | GND |
| 6 | 2Y | Motor, terminal 2 |
| 7 | 2A | Nano D10 |
| 8 | VCC2 | +5 V (alimentación del motor) |
| 16 | VCC1 | +5 V |
| 9–15 | EN2, 3A, 3Y, 12-13 GND, 4Y, 4A | Sin conectar (pines 12 y 13 a GND) |

Abrir = D9 en alto y D10 en bajo; cerrar = D9 en bajo y D10 en alto; detenido = ambos en bajo.

### DS1307

| Pin | Nombre | Se conecta a |
|---|---|---|
| 1 | X1 | Cristal 32.768 kHz |
| 2 | X2 | Cristal 32.768 kHz |
| 3 | VBAT | Pila 3 V |
| 4 | GND | GND |
| 5 | SDA | Nano A4 |
| 6 | SCL | Nano A5 |
| 7 | SQW | Sin conectar |
| 8 | VCC | +5 V |

### PCF8574A

| Pin | Nombre | Se conecta a |
|---|---|---|
| 1, 2, 3 | A0, A1, A2 | GND (dirección 0x38) |
| 4 | P0 | LM044L pin 4 (RS) |
| 5 | P1 | LM044L pin 5 (RW) |
| 6 | P2 | LM044L pin 6 (E) |
| 7 | P3 | Sin conectar |
| 8 | GND | GND |
| 9 | P4 | LM044L pin 11 (D4) |
| 10 | P5 | LM044L pin 12 (D5) |
| 11 | P6 | LM044L pin 13 (D6) |
| 12 | P7 | LM044L pin 14 (D7) |
| 13 | INT | Sin conectar |
| 14 | SCL | Nano A5 |
| 15 | SDA | Nano A4 |
| 16 | VCC | +5 V |

### LM044L (LCD 20x4)

| Pin | Nombre | Se conecta a |
|---|---|---|
| 1 | VSS | GND |
| 2 | VDD | +5 V |
| 3 | VEE | GND |
| 4 | RS | PCF8574A P0 |
| 5 | RW | PCF8574A P1 |
| 6 | E | PCF8574A P2 |
| 7–10 | D0–D3 | Sin conectar |
| 11 | D4 | PCF8574A P4 |
| 12 | D5 | PCF8574A P5 |
| 13 | D6 | PCF8574A P6 |
| 14 | D7 | PCF8574A P7 |

## Estructura del repositorio

```
sistema_casa/
  sistema_casa.ino        Sketch principal
librerias/
  LcdI2C/                 Librería propia para LCD I2C (16x2, 20x4)
build/
  sistema_casa.ino.hex    Firmware para cargar en Proteus
PROTEUS.md                Simulación: componentes, conexiones, uso y problemas comunes
MANUAL_USUARIO.md         Manual de usuario
MANUAL_TECNICO.md         Manual técnico
diagrama_conexion.svg/png Diagrama de conexión
CLAUDE.md, MEMORY.md      Contexto técnico del proyecto
```

## Cómo compilar

1. Copiar `librerias\LcdI2C\` a `Documents\Arduino\libraries\LcdI2C\` e instalar la librería `Keypad`.
2. Compilar con una de estas opciones:
   - **Arduino IDE / arduino-cli** (genera `build\sistema_casa.ino.hex`):
     ```powershell
     & "C:\Program Files\Arduino IDE\resources\app\lib\backend\resources\arduino-cli.exe" compile --fqbn arduino:avr:nano:cpu=atmega328 --libraries librerias --output-dir build sistema_casa
     ```
   - **VSM Studio de Proteus**: pegar el sketch en `main.ino`. El proyecto solo debe contener ese archivo.

## Cómo simular

1. Abrir el esquema en Proteus 8.13 (requiere la librería del Arduino Nano para Proteus).
2. Cargar el `.hex` en el Arduino (doble clic → *Program File*) o compilar desde VSM Studio.
3. Ejecutar la simulación y seguir el guion de [`PROTEUS.md`](PROTEUS.md#cómo-demostrar-los-eventos).

## Uso rápido del teclado

| Tecla | Función |
|---|---|
| `0`–`9` | Dígitos |
| `F0` | Menú |
| `F1` / `F2` | En el menú: siguiente / anterior opción |
| `F3` | Modo de luces AUTO / ON / OFF |
| `ON/C` | Borrar / volver |
| `ok` | Aceptar |

En la pantalla de estado: `1` abre la cortina, `2` la cierra, `0` detiene el motor, `F1` ajusta la hora, `3` ajusta la fecha y `F2` cambia la velocidad del reloj.

Los horarios se escriben en formato `HHMM` de 24 horas y la fecha como `DDMMAA`.

## Documentación

- [`MANUAL_USUARIO.md`](MANUAL_USUARIO.md): uso, teclas, configuración y mensajes.
- [`MANUAL_TECNICO.md`](MANUAL_TECNICO.md): arquitectura, pines, software, compilación, simulación y solución de problemas.

## Librería LcdI2C

Librería propia para pantallas HD44780 conectadas por PCF8574/PCF8574A, sin dependencias externas. Mapeo: P0 = RS, P1 = RW, P2 = E, P3 = backlight, P4–P7 = D4–D7.

```cpp
#include <Wire.h>
#include <LcdI2C.h>

LcdI2C lcd(0x38, 20, 4);

void setup() {
  lcd.begin();
  lcd.setCursor(0, 0);
  lcd.print("Sistema para Casa");
}
```

Funciones: `begin`, `clear`, `home`, `setCursor`, `print`/`println`, `display`/`noDisplay`, `cursor`/`noCursor`, `blink`/`noBlink`, `backlight`/`noBacklight`, `scrollLeft`/`scrollRight`, `createChar`, `command`.
