# Manual técnico — Sistema para Casa (Grupo 1)

Curso de Electrónica Digital, UMG. Documento para quien arme, simule, mantenga o modifique el sistema. El manejo cotidiano está en `MANUAL_USUARIO.md`.

## 1. Descripción general

Sistema domótico sobre un único **Arduino Nano v3** (ATmega328P, 16 MHz). Requisito central del curso: **reloj, eventos y acciones según el tiempo**.

| Módulo | Entrada | Salida | Lógica |
|---|---|---|---|
| Iluminación | LDR en A0 | LED/relé en D8 | Umbrales con histéresis; modo AUTO/ON/OFF |
| Riego | Hora del DS1307 | Bomba en D7 | Dos ventanas horarias por día |
| Cortina | Hora del DS1307 y teclado | Motor DC por L293D en D9/D10 | Abre/cierra al cambiar la ventana; giro temporizado |
| Interfaz | Teclado 4×4 | LCD 20×4 por I2C | Máquina de estados de pantallas |

La simulación objetivo es **Proteus 8.13**; todo componente debe existir allí (por eso el DS1307 y no el DS3231).

## 2. Arquitectura de hardware

```
                       ┌───────────────────────┐
  Teclado 4x4 ─────────┤ D2 D4 D5 D6  D11 D12  │
                       │ A2 A1                 │
  LDR + 10k ───────────┤ A0                    │
                       │                       │
  Bomba (transistor) ◄─┤ D7                    │
  Luces (LED) ◄────────┤ D8        Arduino     │
  L293D IN1 ◄──────────┤ D9         Nano v3    │
  L293D IN2 ◄──────────┤ D10                   │
                       │                       │
  DS1307 + PCF8574A ◄─►┤ A4 (SDA)  A5 (SCL)    │
  Virtual Terminal ◄───┤ D1 (TX, 9600 baud)    │
                       └───────────────────────┘
```

### 2.1 Mapa de pines

| Pin | Función | Pin | Función |
|---|---|---|---|
| D1 | TX serie (9600) | D9 | L293D IN1 (abrir) |
| D2 | Teclado fila 1 | D10 | L293D IN2 (cerrar) |
| D4 | Teclado fila 2 | D11 | Teclado columna 1 |
| D5 | Teclado fila 3 | D12 | Teclado columna 2 |
| D6 | Teclado fila 4 | A0 | LDR (entrada analógica) |
| D7 | Bomba de riego | A1 | Teclado columna 4 |
| D8 | Luces | A2 | Teclado columna 3 |
| A4 | SDA (I2C) | A5 | SCL (I2C) |

Libres: D0, D3, D13, A3, A6, A7. La columna 3 del teclado va en A2 porque D13 comparte pin con el LED interno del Nano.

El cableado pin a pin de cada chip (Nano, DS1307, PCF8574A, LM044L, L293D, teclado) está en `README.md` y `PROTEUS.md`. El diagrama pictórico es `diagrama_conexion.png` / `.svg`.

### 2.2 Bus I2C

| Dispositivo | Dirección | Notas |
|---|---|---|
| DS1307 | `0x68` (fija) | En el I2C Debugger aparece como `0xD0` (dirección << 1) |
| PCF8574A (LCD) | `0x38` con A0–A2 a GND | En el debugger: `0x70`. Con A0–A2 en alto sería `0x3F` |

El sketch detecta la dirección del LCD al arrancar probando `0x38, 0x3F, 0x27, 0x20`. Se necesitan pull-ups de 4.7 kΩ a +5 V en SDA y SCL (una sola pareja para todo el bus).

### 2.3 LCD por I2C

LM044L (20×4, HD44780) con expansor PCF8574A en modo 4 bits. Mapeo del expansor: P0 = RS, P1 = RW, P2 = E, P3 = backlight, P4–P7 = D4–D7. Las filas del 20×4 empiezan en las direcciones DDRAM `0x00, 0x40, 0x14, 0x54`; la librería lo resuelve en `setCursor`.

### 2.4 Etapa de potencia

Ningún pin del Arduino debe alimentar directamente una carga (máximo ~20 mA por pin).

- **Bomba:** transistor NPN (2N2222/TIP120) con 1 kΩ en la base, diodo 1N4007 en antiparalelo con la bomba y masa común con el Arduino; o módulo de relé. En Proteus basta un LED con 330 Ω.
- **Motor de cortina:** puente H **L293D**. EN1 (pin 1) y ambos VCC a +5 V. D9 alto y D10 bajo = abrir; D9 bajo y D10 alto = cerrar; ambos bajo = parado. En un circuito real, alimentar VCC2 con una fuente aparte, con masa común.
- **LDR:** divisor de voltaje `+5 V – LDR – A0 – 10 kΩ – GND`. Más luz → más voltaje → porcentaje mayor.

## 3. Arquitectura de software

Archivo único: `sistema_casa\sistema_casa.ino`. Registros del ATmega usados directamente: Timer1 (`TCCR1A/B`, `OCR1A`, `TIMSK1`) y modo sleep. Librerías: `Wire`, `EEPROM`, `avr/sleep.h` (incluidas con el IDE), `Keypad` (instalada) y `LcdI2C` (propia, en `librerias\LcdI2C\`). El DS1307 se maneja con funciones propias sobre `Wire`, sin `RTClib`.

Uso de memoria (compilación actual): flash 53 % (16 348 de 30 720 bytes), RAM 52 % (1 085 de 2 048 bytes).

### 3.1 Bucle principal

`loop()` no bloquea. En cada iteración:

1. Duerme en modo **IDLE** hasta el siguiente tick de 10 ms del **Timer1** (100 Hz). El Timer0 puede despertarlo antes, y entonces vuelve a dormir sin hacer trabajo.
2. Aplica los segundos reales pendientes al reloj en software: `relojAvanzar(segundos × velocidad)`. Una vez por minuto real sincroniza con el DS1307.
3. Sondea el teclado (`getKey`) y procesa la tecla.
4. Cierra mensajes temporales y detiene el motor de la cortina si venció su tiempo.
5. Cada `PERIODO_MS = 500 ms`: lee el LDR, ejecuta `controlRiego`, `controlLuz`, `controlCortina` y, una vez por minuto simulado, imprime el estado por serie.
6. Si hace falta, repinta la pantalla (`dibujar`).

### 3.2 Reloj, eventos y acciones

- **Reloj en software + DS1307.** La hora y la fecha viven en la variable global `ahoraG` (`struct Hora`) y avanzan con el Timer1 (ver §3.3). El **DS1307** es el respaldo persistente: se lee al arrancar (7 registros: segundos, minutos, horas, día de la semana, día, mes, año) y se escribe al editar la hora o la fecha. El bit CH (bit 7 del registro 0) en 1 indica reloj detenido.
- Las ventanas horarias se evalúan con `enVentana(idx)`, en minutos del día. Una ventana es `[inicio, fin)`; si `fin < inicio` cruza la medianoche; si `inicio == fin` está deshabilitada.
- **Riego:** `setBomba(enVentana(AM) || enVentana(PM))`.
- **Cortina:** `controlCortina` solo actúa **cuando cambia** el estado de la ventana (`cortinaVentPrev`), de modo que una orden manual se respeta hasta el siguiente evento programado. `moverCortina` activa el pin correspondiente y fija `cortinaFin = millis() + CORTINA_MS` (4 s); `actualizarCortina` detiene el motor al vencer.
- **Luces:** modo AUTO: `luz < luzOn` → encender; `luz > luzOff` → apagar; entre ambos mantiene el estado (histéresis). Modo 1 = ON y 2 = OFF manual.
- Cada acción emite un evento por serie (`[EVENTO] ...`, `[PROG] ...`).

### 3.3 Base de tiempo (Timer1) y aceleración del reloj

El **Timer1** del ATmega328P está en modo CTC con prescaler 64 y `OCR1A = 2499`: genera una interrupción a 100 Hz (16 MHz / 64 / 2500). Su ISR (`TIMER1_COMPA_vect`) levanta el flag `tick10ms` y, cada 100 ticks, suma un segundo a `segPend`. El Timer0 no se toca, porque lo usan `millis()`, `delay()` y la librería `Keypad`. Los pines 9 y 10 (PWM del Timer1) se usan solo con `digitalWrite`, así que no hay conflicto.

El `loop()` consume `segPend` y llama a `relojAvanzar(n × velocidad)`, que suma los segundos a `ahoraG` y avanza la fecha (meses y años bisiestos) al cruzar la medianoche. Velocidades: x1, x10, x60 (valor inicial) y x300, en `VELOCIDADES`.

**Sincronización con el DS1307**, una vez por minuto real (`segSync`):
- a **x1**, se lee el DS1307 y manda él (corrige la deriva del reloj en software);
- a **velocidades aceleradas**, se escribe en el DS1307 la hora simulada (`rtcEscribirTodo`).

Así el bus I2C solo lo usa el LCD (cuando cambia un carácter) y una transferencia por minuto del reloj.

### 3.4 Configuración y EEPROM

```c
struct Ventana { uint8_t hIni, mIni, hFin, mFin; };
struct Config  { uint8_t magic; Ventana v[3]; uint8_t luzOn, luzOff; };   // 15 bytes
```

Se guarda con `EEPROM.put(0, cfg)` desde la dirección 0. Al arrancar, si `magic != CONFIG_MAGIC (0xA7)`, se cargan los valores de `CONFIG_DEFECTO` y se guardan.

> **Si cambia la estructura `Config` o sus valores por defecto, hay que subir `CONFIG_MAGIC`**; si no, el Arduino seguirá usando datos viejos de la EEPROM.

Valores por defecto: riego 06:00–06:30 y 18:00–18:30; cortina abre 07:00, cierra 19:00; luces encienden < 30 %, apagan > 45 %.

La hora, la fecha y la velocidad **no** se guardan en la EEPROM: la hora y la fecha viven en el DS1307.

### 3.5 Interfaz (máquina de estados)

| Pantalla | Función |
|---|---|
| `P_INICIO` | Estado (fecha, hora, luz, cortina, riego) |
| `P_MENU` | Lista de 7 opciones con desplazamiento |
| `P_VENTANA` | Edita inicio/fin de riego o cortina (2 etapas) |
| `P_UMBRAL` | Edita umbrales de luz (2 etapas) |
| `P_HORA` | Edita la hora (`HHMM`) |
| `P_FECHA` | Edita la fecha (`DDMMAA`) |
| `P_VELOC` | Elige la velocidad del reloj |
| `P_MSG` | Mensaje temporal (1.3 s) y luego pasa a otra pantalla |

`dibujar()` arma las 4 filas de texto y `linea()` las compara con lo que ya muestra el LCD (`pantallaLcd[4][20]`); solo escribe los tramos que cambiaron. Esto reduce mucho el tráfico I2C y acelera la simulación. `redibujar` se activa al pulsar una tecla, cambiar de pantalla y, cada 500 ms, en `P_INICIO` y `P_HORA`.

### 3.6 Teclado

Matriz 4×4 con la librería `Keypad` (pull-ups internos, sin resistencias externas). Mapa de teclas: `1 2 3 A / 4 5 6 B / 7 8 9 C / X 0 K D`, donde `A`–`D` = F0–F3, `X` = ON/C y `K` = OK.

### 3.7 Salida serie

9600 baud por D1 al Virtual Terminal. Una línea por minuto simulado: `DD/MM/AA HH:MM:SS | Luz NN% | Riego:.. Luces:.. Cortina:..`, más los eventos y los cambios de programación.

## 4. Restricciones de compilación (VSM Studio de Proteus, Arduino IDE 1.8.5)

- **Ninguna función del `.ino` puede usar tipos propios** (`Hora`, `Ventana`, `Pantalla`…) en su firma ni como retorno. El generador de prototipos (`mksketch`) pone los prototipos antes de las `struct`/`enum`. Se usan `uint8_t` y variables globales (`ahoraG`, `cfg`). Tampoco se usan métodos dentro de `struct`. Esto no aplica a las clases de librerías.
- `arduino-cli` **no detecta** ese problema; hay que probar con `mksketch` y `avr-gcc` con la línea de comandos del log de VSM Studio y `-O0`.
- No usar librerías con dependencias transitivas (VSM Studio no las resuelve; por eso no se usa `RTClib`).
- El proyecto de VSM Studio contiene **solo `main.ino`**; las librerías se toman de `Documents\Arduino\libraries`. Agregarlas al proyecto las compila dos veces (`multiple definition`).
- Todo archivo con clases debe ser `.cpp` (los `.c` se compilan como C).
- `main.ino` debe incluir `#include <Wire.h>`.
- Placa: Arduino Nano v3, ATmega328P, 16 MHz.

## 5. Compilación y carga

```powershell
& "C:\Program Files\Arduino IDE\resources\app\lib\backend\resources\arduino-cli.exe" compile --fqbn arduino:avr:nano:cpu=atmega328 --libraries librerias --output-dir build sistema_casa
```

Genera `build\sistema_casa.ino.hex` (no usar el `with_bootloader`). En Proteus: doble clic en el Arduino → *Program File* → ese `.hex`, o pegar el sketch en `main.ino` de VSM Studio. Antes, copiar `librerias\LcdI2C\` a `C:\Users\Erick\Documents\Arduino\libraries\LcdI2C\` e instalar `Keypad`.

## 6. Simulación en Proteus 8.13

Componentes: Arduino Nano v3 (librería externa), `DS1307` con `CRYSTAL` 32.768 kHz, `PCF8574A`, `LM044L`, `KEYPAD-SMALLCALC`, `L293D`, `MOTOR`, `LDR` + resistencia 10 kΩ, LEDs, `VIRTUAL TERMINAL` y, opcional, `I2C DEBUGGER`.

**Guion de demostración** (x60, desde las 05:55):

| Hora simulada | Tiempo real aprox. | Evento |
|---|---|---|
| 06:00 | 0:05 | Riego ON (hasta 06:30) |
| 07:00 | 1:05 | Cortina se abre (motor 4 s) |
| 18:00 | 12:05 | Riego ON (hasta 18:30) |
| 19:00 | 13:05 | Cortina se cierra |

Las luces se prueban moviendo el nivel de luz del LDR (o un potenciómetro de sustitución): se encienden por debajo del 30 % y se apagan por encima del 45 %.

**Rendimiento.** Si la simulación va lenta: cerrar el I2C Debugger y el Virtual Terminal, quitar temporalmente el motor y el L293D, subir *Frames per Second* en *System → Set Animation Options* y cargar el `.hex` generado por `arduino-cli` (optimizado) en lugar de compilar en Debug (`-O0`). El sketch ya reduce la carga con el repintado parcial del LCD, el reloj en software con Timer1 (casi sin tráfico I2C del DS1307) y el modo IDLE a 100 Hz.

## 7. Parámetros ajustables en el código

| Constante | Valor | Efecto |
|---|---|---|
| `velocidad` | 60 | Velocidad inicial del reloj |
| `FIJAR_HORA_AL_ARRANCAR` | 1 | 1 = fija fecha y hora al arrancar (demo repetible); 0 = respeta el DS1307 |
| `HORA_ARRANQUE`, `MIN_ARRANQUE` | 05:55 | Hora fijada al arrancar |
| `DIA/MES/ANIO_ARRANQUE` | 01/01/26 | Fecha fijada al arrancar |
| `CORTINA_MS` | 4000 | Duración del giro del motor |
| `PERIODO_MS` | 500 | Periodo del ciclo de control |
| `CONFIG_DEFECTO` | ver §3.4 | Horarios y umbrales de fábrica |
| `CONFIG_MAGIC` | 0xA7 | Subir al cambiar `Config` o sus defaults |

**Para operación real:** poner `FIJAR_HORA_AL_ARRANCAR 0`, `velocidad = 1`, programar la hora y la fecha desde el teclado y alimentar el DS1307 con su pila de respaldo.

## 8. Solución de problemas

| Síntoma | Causa probable | Solución |
|---|---|---|
| LCD iluminado, sin texto | Terminales P4–P7 corridas o nombres de red distintos | Clic derecho → *Highlight net* y comprobar cada línea hasta el PCF8574A |
| `? ? S P` / `Noise` en el I2C Debugger | SDA y SCL cruzados | En el Nano A4 = SDA, A5 = SCL |
| Bits `(WHI)` en el I2C Debugger | Faltan pull-ups | 4.7 kΩ a +5 V en SDA y SCL |
| `S 70 N` | Nadie responde en 0x38 | Revisar A0–A2 a GND y la alimentación del PCF8574A |
| `ERROR: sin RTC` | DS1307 sin cableado, sin cristal o sin pull-ups | Revisar I2C, cristal X1/X2 y VBAT |
| `'X' does not name a type` | Función con tipo propio en la firma | Ver §4; usar `uint8_t` y globales |
| `multiple definition of LcdI2C::...` | Librería agregada al proyecto de VSM Studio | Dejar solo `main.ino` |
| `unknown type name 'class'` | Archivo `.c` con clases | Renombrar a `.cpp` |
| `'POSITIVE' was not declared` | Librería `LiquidCrystal I2C` equivocada | Usar `LcdI2C` |
| Motor gira al revés | Terminales del motor o D9/D10 intercambiados | Invertir los cables del motor |
| Bomba actúa al revés | Módulo de relé activo en bajo | Cambiar la lógica en `setBomba` |
| Reinicios o bloqueos | LED con resistencia entre RESET y GND | Quitarlo (ver §9) |
| Simulación lenta | Ver §6 | — |

Aviso inofensivo: `Compiler optimizations disabled; functions from <util/delay.h> won't work as designed` (VSM Studio compila en Debug). `delay()` de Arduino funciona igual.

## 9. Limitaciones y pendientes

- Probar el circuito completo en Proteus con la versión actual del sketch y verificar la compilación con `mksketch`.
- Quitar D2/R5 de RESET en el esquema (deja el pin en ~2 V).
- Agregar pull-ups de 4.7 kΩ en SDA/SCL al esquema (hacen falta en el circuito real).
- La hora se acelera por software (lectura-modificación-escritura); no sirve para precisión en operación real.
- Solo hay dos ventanas de riego y una de cortina; la hora y la fecha no se guardan en EEPROM (las mantiene el DS1307).
- Comprobar en Proteus que el teclado responde bien con el barrido a 100 Hz y medir la mejora de velocidad; si hace falta, probar `Wire.setClock(400000)`.
- Con `FIJAR_HORA_AL_ARRANCAR 1`, cada arranque de la simulación fija 01/01/26 05:55 y pierde la fecha editada.

## 10. Estructura del repositorio

```
sistema_casa/sistema_casa.ino    Sketch principal
librerias/LcdI2C/                Librería propia para LCD I2C
build/sistema_casa.ino.hex       Firmware para cargar en Proteus
README.md                        Descripción general
PROTEUS.md                       Simulación, conexiones y problemas
MANUAL_USUARIO.md                Manual de usuario
MANUAL_TECNICO.md                Este documento
diagrama_conexion.svg / .png     Diagrama de conexión
MEMORY.md, CLAUDE.md             Decisiones, historial y contexto técnico
respaldo/sistema_casa_v1.ino     Versión anterior (alarma, humedad, servo)
```
