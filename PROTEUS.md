# Simulación en Proteus (versión 8.13)

Diagrama pictórico de conexión: `diagrama_conexion.png` (editable: `diagrama_conexion.svg`).

> Proteus 8.13 no trae el Arduino Nano v3 de fábrica: hay que instalar una librería de Arduino para Proteus (por ejemplo, la de The Engineering Projects). El resto de componentes (`DS1307`, `PCF8574A`, `LM044L`, `KEYPAD-SMALLCALC`, `L293D`, `LDR`) son de la librería estándar; conviene confirmarlo buscándolos con la tecla `P` en el selector de dispositivos.

## Firmware

**Opción A: compilar desde Proteus (VSM Studio).** Pegar el contenido de `sistema_casa\sistema_casa.ino` en el `main.ino` del proyecto. **El proyecto de VSM Studio debe contener solo `main.ino`**: no agregar ahí archivos de librerías.

Librerías necesarias en `C:\Users\Erick\Documents\Arduino\libraries\`:

- `Keypad`
- `LcdI2C` (propia): copiar la carpeta `librerias\LcdI2C\` del repositorio.

`Wire`, `EEPROM` y `avr/sleep.h` ya vienen con el Arduino IDE. El sketch no usa `RTClib` ni `LiquidCrystal I2C`. El `main.ino` debe tener `#include <Wire.h>`.

**Opción B: cargar el `.hex`.** En el Arduino Nano v3 de Proteus (doble clic > *Program File*):
`build\sistema_casa.ino.hex` (se regenera con el comando de compilación de `CLAUDE.md`). Si se exporta desde el Arduino IDE, usar el `.ino.hex`, no el `with_bootloader`.

## Componentes (Proteus)

| Componente | Nombre en Proteus | Conexión |
|---|---|---|
| Microcontrolador | Arduino Nano v3 (librería Arduino para Proteus) | — |
| Reloj | `DS1307` | SDA→A4, SCL→A5, cristal `CRYSTAL` 32.768 kHz en X1/X2, VBAT a pila 3 V (o 3 V fijo) |
| Expansor del LCD | `PCF8574A` | SDA→A4, SCL→A5, **A0, A1, A2 a GND → dirección 0x38**; salidas al LCD según la tabla de abajo |
| Pantalla | `LM044L` (20x4) | Ver tabla PCF8574A → LM044L |
| Pantalla 2 (hora del DS1307) | `JHD-2X16-I2C` (módulo 16x2 con expansor integrado) | Solo 4 pines: VDD→+5 V, VSS→GND, SCL→A5, SDA→A4 (mismo bus que el resto). Dirección **0x3E** (`$7C` en el I2C Debugger); si usa otra, el sketch la busca solo (0x20–0x27 o 0x38–0x3F). Usa un protocolo de comandos propio (no el del PCF8574): el sketch lo maneja con `LCD2_JHD 1`. Verificado: muestra la hora del DS1307 |
| Teclado | Teclado 4x4 con `0`–`9`, `F0`–`F3`, `ON/C`, `ok` | filas→D2, D4, D5, D6 · columnas→D11, D12, A2, A1 |
| Luz ambiente | `LDR` + resistencia 10 kΩ | +5 V → LDR → A0 → 10 kΩ → GND (más luz = más voltaje) |
| Bomba de riego | `MOTOR` + transistor/`RELAY`, o `LED-BLUE` con 330 Ω | D7 |
| Luces | `LED-YELLOW` + 330 Ω | D8 |
| Motor de cortina | `L293D` + `MOTOR` DC | IN1→D9 (abrir), IN2→D10 (cerrar), EN1 y VCC1 (`VSS`) a +5 V, VCC2 (`VS`) a +12 V (o +5 V según el motor), motor entre OUT1 y OUT2, GND a masa |
| Tiempo de riego (MM:SS) | 4 × `7SEG-COM-CATHODE` + 4 × `74HC595` en cadena | D3 → SER del 1.º; A3 → SRCLK de los cuatro; D13 → RCLK de los cuatro; QH' de cada 595 → SER del siguiente; OE a GND, SRCLR a +5 V; QA–QG → segmentos a–g, QH → punto; cátodo común a GND |
| Salida de texto | `VIRTUAL TERMINAL` | RXD del terminal → TX (D1), 9600 baud |
| Diagnóstico I2C (opcional) | `I2C DEBUGGER` (Instruments) | **SDA→A4, SCL→A5** |

Pull-ups de 4.7 kΩ a +5 V en SDA y SCL (una sola pareja para todo el bus I2C).

No conectar nada al pin RESET del Nano salvo su pulsador interno (un LED a GND en RESET lo deja en un nivel dudoso y puede reiniciar el microcontrolador).

## Tabla de conexión pin a pin

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
| D3 | 6 | Display 7 seg: dato | 74HC595 #1, pin 14 (`SER`) |
| A3 | 22 | Display 7 seg: reloj | `SRCLK` (pin 11) de los cuatro 74HC595 |
| D13 | 16 | Display 7 seg: latch | `RCLK` (pin 12) de los cuatro 74HC595 |
| A0 | 19 | Luz ambiente | Unión LDR – resistencia de 10 kΩ |
| A4 | 23 | SDA (I2C) | DS1307 pin 5, PCF8574A pin 15, pull-up 4.7 kΩ a +5 V |
| A5 | 24 | SCL (I2C) | DS1307 pin 6, PCF8574A pin 14, pull-up 4.7 kΩ a +5 V |
| D1 / TX1 | 1 | Salida serie | Virtual Terminal `RXD` |
| 5V | 27 | Alimentación | Todos los puntos +5 V |
| GND | 29 | Masa | Todos los puntos GND |

Los pines D0 (RX serie), A6 y A7 quedan libres. El LED `LED & Reset` del Nano de Proteus está en D13, así que parpadeará con los pulsos del latch del display: es normal.

### Display de 7 segmentos (tiempo de riego)

Cuatro dígitos `7SEG-COM-CATHODE`, cada uno con su `74HC595`, en cadena y sin multiplexar. Orden: Nano `D3` → 595 #1 (decenas de minuto) → #2 → #3 → #4 (unidades de segundo).

| Pin 74HC595 | Nombre | Se conecta a |
|---|---|---|
| 14 | SER | #1: Nano D3 · #2, #3, #4: `QH'` (pin 9) del 595 anterior |
| 11 | SRCLK | Nano A3 (los cuatro) |
| 12 | RCLK | Nano D13 (los cuatro) |
| 13 | OE | GND |
| 10 | SRCLR | +5 V |
| 16 / 8 | VCC / GND | +5 V / GND |
| 15, 1, 2, 3, 4, 5, 6 | QA … QG | Segmentos a, b, c, d, e, f, g |
| 7 | QH | Punto decimal (en el 2.º dígito hace de dos puntos) |

Cátodo común (`COM`) de cada dígito a GND. Si el display no muestra el número correcto, revisar que QA…QG estén en el orden a…g. Durante el arranque muestra `88:88` (prueba de segmentos); al entrar al bucle principal pasa a `00:00`.

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
| 8 | VCC2 (`VS` en Proteus) | **+12 V** (alimentación del motor; el `MOTOR` de Proteus es de 12 V. Con un motor de 5 V, usar +5 V) |
| 16 | VCC1 (`VSS` en Proteus) | **+5 V** (lógica; no conectarlo a GND) |
| 9–15 | EN2, 3A, 3Y, 12-13 GND, 4Y, 4A | Sin conectar (pines 12 y 13 a GND) |

Abrir = D9 en alto y D10 en bajo; cerrar = D9 en bajo y D10 en alto; detenido = ambos en bajo. **EN1 (pin 1) debe estar a +5 V**: sin eso el canal no se habilita y el motor no gira. La masa de la fuente de 12 V se une a la del Arduino.

**Segundo motor (opcional):** el sketch solo maneja el canal 1 (OUT1/OUT2). Si se dibuja un segundo motor en OUT3 (pin 11) y OUT4 (pin 14), para que gire junto con el primero: IN3 (pin 10) a D9, IN4 (pin 15) a D10 y EN2 (pin 9) a +5 V.

### DS1307

| Pin | Nombre | Se conecta a |
|---|---|---|
| 1 | X1 | Cristal 32.768 kHz |
| 2 | X2 | Cristal 32.768 kHz |
| 3 | VBAT | Pila 3 V |
| 4 | GND | GND |
| 5 | SDA | Nano A4 |
| 6 | SCL | Nano A5 |
| 7 | SQW (`SOUT`) | Sin conectar (no a +5 V directo: es salida de colector abierto y puede dar contención; si se usa, pull-up de 10 kΩ) |
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

### Direcciones I2C del bus

| Dispositivo | Dirección | En el I2C Debugger |
|---|---|---|
| PCF8574A (A0–A2 a GND) | 0x38 | `S 70 A …` |
| DS1307 | 0x68 | `S D0 A …` |

Si en lugar del PCF8574A se usa un `PCF8574` (sin A), con A0–A2 a GND la dirección es 0x20; hay que cambiarla en el sketch.

### PCF8574A → LM044L

En Proteus las terminales se unen **solo si tienen exactamente el mismo nombre**. Usar los mismos nombres (`P0`, `P1`, …) en ambos lados.

| PCF8574A (pin) | Etiqueta | LM044L (pin) |
|---|---|---|
| P0 (4) | `P0` | RS (4) |
| P1 (5) | `P1` | RW (5) |
| P2 (6) | `P2` | E (6) |
| P3 (7) | — | sin conectar (backlight) |
| P4 (9) | `P4` | D4 (11) |
| P5 (10) | `P5` | D5 (12) |
| P6 (11) | `P6` | D6 (13) |
| P7 (12) | `P7` | D7 (14) |

LM044L: VSS (1) y VEE (3) a GND, VDD (2) a +5 V. **D0–D3 (pines 7–10) sin conectar.** El LM044L tiene los mismos 14 pines y en el mismo orden que el LM016L.

Comprobar cada conexión con clic derecho sobre el cable → *Highlight net*: debe resaltarse hasta el pin correspondiente del PCF8574A.

Si usas motor o relé, agrega diodo de rueda libre y alimenta la carga aparte del Arduino.

## Uso del teclado

Disposición del teclado: `1 2 3 F0 / 4 5 6 F1 / 7 8 9 F2 / ON/C 0 OK F3`. Si se usa el `KEYPAD-SMALLCALC` (rotulación de calculadora), la 4.ª columna es `÷ × − +` y las teclas de dígitos van `7 8 9 / 4 5 6 / 1 2 3`: hay que ajustar la matriz `teclas[][]` del sketch.

| Tecla | Función |
|---|---|
| `0`–`9` | dígitos |
| `F0` | abrir menú |
| `F1` | en el menú: siguiente opción; en la pantalla de estado: ajustar la hora |
| `F2` | en el menú: opción anterior; en la pantalla de estado: velocidad del reloj |
| `F3` | modo de luces (AUTO → ON → OFF) |
| `ON/C` | borrar un dígito / volver atrás |
| `ok` | aceptar |

En la pantalla de estado: `1` abre la cortina, `2` la cierra, `0` detiene el motor, `F1` ajusta la hora y `F2` cambia la velocidad del reloj.

### Programar

1. En la pantalla de estado pulsar `F0`.
2. Elegir con `F1`/`F2` y `OK`, o pulsar el número: 1 Riego mañana, 2 Riego tarde, 3 Cortinas, 4 Umbral de luz, 5 Ajustar hora, 6 Velocidad reloj.
3. Horarios: escribir `HHMM` (24 h) de inicio y `OK`; luego el fin y `OK`. Para cortinas, inicio = abrir y fin = cerrar.
4. Umbral de luz: porcentaje por debajo del cual se encienden las luces y porcentaje por encima del cual se apagan (apagar > encender).
5. Todo se guarda en EEPROM. Inicio = fin deshabilita la ventana.

## Cómo demostrar los eventos

Al arrancar, el reloj se fija a las 05:55 y corre a **x60** (1 s real = 1 min simulado). Desde el teclado se puede cambiar sin recompilar:

- `F1` (en la pantalla de estado): ajustar la hora (`HHMM` + `OK`; la pantalla muestra la hora actual con segundos).
- `F2` (en la pantalla de estado): velocidad del reloj, `1`=x1, `2`=x10, `3`=x60, `4`=x300. También están en el menú (opciones 5 y 6). El reloj es un contador de 24 h: al llegar a 23:59:59 vuelve a 00:00:00 y los eventos se repiten cada día a su hora; no hay fecha.

Con los valores por defecto y x60:

| Hora simulada | Tiempo real aprox. | Evento |
|---|---|---|
| 06:00 | 0:05 | Riego ON (hasta 06:30) |
| 07:00 | 1:05 | Cortina se abre (motor 4 s) |
| 18:00 | 12:05 | Riego ON (hasta 18:30) |
| 19:00 | 13:05 | Cortina se cierra |

- **Luces:** mover el control de luz del `LDR` (propiedad *Light Level* o el potenciómetro de sustitución): por debajo de 30 % se encienden, por encima de 45 % se apagan. `F3` fuerza ON/OFF manual.
- **Cortina manual:** `1` abrir, `2` cerrar, `0` detener.
- Para una demostración más corta, programar ventanas cercanas a la hora del demo.
- Para operación real: elegir velocidad **x1** y, en el sketch, poner `FIJAR_HORA_AL_ARRANCAR 0` (el reloj respeta la hora del DS1307) y `velocidad = 1`.

## Rendimiento de la simulación

El sketch ya está pensado para simular rápido: el reloj corre en software con el **Timer1** (100 Hz) y solo consulta el DS1307 una vez por minuto, el LCD se repinta solo donde cambia y el `loop` duerme en modo IDLE entre ticks. Si aun así va lenta:

1. Cerrar el **I2C Debugger** y el **Virtual Terminal** (los instrumentos cuestan mucha CPU).
2. Revisar la barra inferior de Proteus: si la carga de CPU está al máximo, es el límite del equipo.
3. En **System → Set Animation Options**, subir *Frames per Second*.
4. Cargar el `.hex` de `build\` (compilado optimizado) en lugar de compilar en Debug (`-O0`) desde VSM Studio, o cambiar VSM Studio a *Release*.
5. Quitar temporalmente el motor y el L293D para comprobar si el análisis analógico es el cuello de botella.

## Solución de problemas

### El motor de la cortina no gira

1. Con *Highlight net*, comprobar que **EN1 (pin 1)** y **VSS (pin 16)** estén a +5 V (VSS a GND deja al chip sin alimentación lógica) y **VS (pin 8)** a la tensión del motor (+12 V para el `MOTOR` de Proteus).
2. Comprobar que la masa de la fuente del motor y la del Arduino sean la misma.
3. Ver en el Virtual Terminal que aparezca `[EVENTO] Cortina ABRIENDO`/`CERRANDO` (o pulsar `1`/`2` en la pantalla de estado). El giro dura 4 s.
4. Un segundo motor en OUT3/OUT4 no se mueve si IN3, IN4 y EN2 quedan sin conectar.

### Saber qué está haciendo el sketch (depuración)

Con `#define DEPURAR 1` (al inicio del sketch):

- **Por serie** salen mensajes `[DBG] setup: ...` en cada etapa del arranque (LCD, RTC y LDR, Timer1) y, ya en marcha, una línea por segundo real: `[DBG] HH:MM:SS xNN pant=N`. La última línea que aparece indica hasta dónde llegó.
- **Causa del último reinicio:** la primera línea dice `reinicio por: ...` con `ENCENDIDO` (arranque normal), `PIN-RESET` (se activó el pin RESET: revisar el esquema), `BROWN-OUT` (la tensión del chip bajó del umbral), `WATCHDOG` o `SALTO-A-0` (el programa se salió de control). Si el arranque se repite solo, esa línea dice por qué.
- **Display de 7 segmentos:** muestra `88:88` durante el arranque, pasa a `00:00` al entrar al bucle y **los dos puntos parpadean cada segundo real**. Si parpadean, el sketch corre aunque el terminal no muestre nada.

Para la entrega, poner `DEPURAR 0`.

### El Virtual Terminal no muestra texto

`RXD` del terminal va a **D1/TX** del Arduino (TXD del terminal puede ir a D0 o quedar libre); 9600 baud, 8 bits, sin paridad, 1 stop. Reabrirlo con **Debug → Virtual Terminal**. El sketch imprime una línea por minuto simulado (subir la velocidad con `F2`).

### El LCD se ve iluminado pero sin texto

1. Dejar correr la simulación más de **2 s de tiempo simulado** (barra inferior de Proteus): el LCD tarda ~1 s en inicializarse.
2. Con *Highlight net*, revisar que P4–P7 lleguen a **D4–D7 (pines 11–14)** y no a D2–D5.
3. Revisar que las etiquetas de ambos lados tengan el mismo nombre.
4. Revisar la dirección con el I2C Debugger (abajo).

### Revisar el bus con el I2C Debugger

- Conectar **SDA→A4 y SCL→A5** (si se cruzan, aparece `Noise` y `? ? S P`).
- Si se cierra la ventana: con la simulación corriendo o en pausa, menú **Debug → I2C Debug – (nombre)**; si no aparece, **Debug → Reset Popup Windows**.
- `S 70 A …` → el PCF8574A responde en 0x38. `S 70 N P` → no responde (revisar A0–A2 y cableado).
- `(WHI)` en los bits → faltan las pull-ups de SDA/SCL.

### Errores de compilación en VSM Studio

| Error | Causa | Solución |
|---|---|---|
| `unknown type name 'class'` | Archivo de librería con extensión `.c` en el proyecto | Quitarlo; las librerías van en `Documents\Arduino\libraries` y en `.cpp` |
| `multiple definition of ...` / `overriding commands for target` | Librería agregada al proyecto además de en `Documents\Arduino\libraries` | Dejar en el proyecto solo `main.ino` |
| `'X' does not name a type` | Función del `.ino` con un tipo propio en su firma | Ver restricciones de `mksketch` en `CLAUDE.md` |
| `Compiler optimizations disabled` (aviso) | Configuración Debug (`-O0`) | Inofensivo; se quita compilando en Release |

## Notas de simulación

- La simulación puede correr más lento que el tiempo real; el demo se estira en consecuencia.
- Si el DS1307 no responde, la pantalla muestra `ERROR: sin RTC` y el terminal `ERROR: no se encontró el RTC DS1307`: revisar pull-ups, cristal y cableado I2C.
- Los horarios guardados en EEPROM persisten entre simulaciones solo si Proteus conserva el estado; al cambiar `CONFIG_MAGIC` en el sketch se restauran los valores por defecto.
