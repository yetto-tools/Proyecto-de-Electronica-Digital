# Manual técnico — Sistema para Casa (Grupo 1)

Curso de Electrónica Digital, UMG. Documento para quien arme, simule, mantenga o modifique el sistema. El manejo cotidiano está en `MANUAL_USUARIO.md`.

## 1. Descripción general

Sistema domótico sobre un único **Arduino Nano v3** (ATmega328P, 16 MHz). Requisito central del curso: **reloj, eventos y acciones según el tiempo**.

| Módulo | Entrada | Salida | Lógica |
|---|---|---|---|
| Iluminación | LDR en A0 | LED/relé en D8 | Umbrales con histéresis; modo AUTO/ON/OFF |
| Riego | Hora del sistema, potenciómetro de humedad en A6 | Bomba en D7 | Dos ventanas horarias habilitan el riego; dentro de ellas, umbrales de humedad con histéresis deciden si se riega |
| Cortina | Hora del sistema y teclado | Motor DC por L293D en D9/D10 | Abre/cierra al cambiar la ventana; giro temporizado |
| Alarma | Hora del sistema, sensor de intrusión (expansor I2C) | Buzzer (expansor I2C) | Ventana horaria arma/desarma; dentro de ella, el sensor dispara el buzzer; clave por teclado para forzar armado/desarmado manual |
| Interfaz | Teclado 4×4 | LCD 20×4 por I2C | Máquina de estados de pantallas |

La simulación objetivo es **Proteus 8.13**; todo componente debe existir allí. No hay RTC: la hora corre en software sobre el Timer1 (ver sección 3).

## 2. Arquitectura de hardware

```
                       ┌───────────────────────┐
  Teclado 4x4 ─────────┤ D2 D4 D5 D6  D11 D12  │
                       │ A2 A1                 │
  LDR + 10k ───────────┤ A0                    │
  Potenciometro hum. ──┤ A6                    │
                       │                       │
  Bomba (transistor) ◄─┤ D7                    │
  Luces (LED) ◄────────┤ D8        Arduino     │
  L293D IN1 ◄──────────┤ D9         Nano v3    │
  L293D IN2 ◄──────────┤ D10                   │
                       │                       │
  I2C: LCD + alarma ◄─►┤ A4 (SDA)  A5 (SCL)    │
  Virtual Terminal ◄───┤ D1 (TX, 115200 baud)  │
                       └───────────────────────┘
```

Por el bus I2C (A4/A5) cuelgan tres dispositivos: el expansor del LCD 20x4 (`PCF8574A`, 0x38), el expansor de la alarma (`PCF8574`/`PCF8574A`, 0x20 u 0x27, con el buzzer y el sensor de intrusión) y, opcional, la pantalla 16x2 (`JHD-2X16-I2C`, 0x3E).

### 2.1 Mapa de pines

| Pin | Función | Pin | Función |
|---|---|---|---|
| D1 | TX serie (115200) | D3 | Display 7 seg: dato (SER) |
| A3 | Display 7 seg: reloj (SRCLK) | D13 | Display 7 seg: latch (RCLK) |
| D9 | L293D IN1 (abrir) | A6 | Humedad del suelo (entrada analógica) |
| D2 | Teclado fila 1 | D10 | L293D IN2 (cerrar) |
| D4 | Teclado fila 2 | D11 | Teclado columna 1 |
| D5 | Teclado fila 3 | D12 | Teclado columna 2 |
| D6 | Teclado fila 4 | A0 | LDR (entrada analógica) |
| D7 | Bomba de riego | A1 | Teclado columna 4 |
| D8 | Luces | A2 | Teclado columna 3 |
| A4 | SDA (I2C) | A5 | SCL (I2C) |

Ya no quedan pines digitales libres en el Nano (D0/RX sigue reservado para programar). El buzzer y el sensor de intrusión de la alarma van por el expansor I2C de la sección 2, no por pines propios.

Libres: D0 (RX serie), A6, A7. La columna 3 del teclado va en A2 porque D13 comparte pin con el LED interno del Nano; D13 se usa ahora como latch del display de 7 segmentos, así que ese LED parpadea con sus pulsos (inofensivo).

El cableado pin a pin de cada chip (Nano, DS1307, PCF8574A, LM044L, L293D, teclado) está en `README.md` y `PROTEUS.md`. El diagrama pictórico es `diagrama_conexion.png` / `.svg`.

### 2.2 Bus I2C

| Dispositivo | Dirección | Notas |
|---|---|---|
| PCF8574A (LCD 20x4) | `0x38` con A0–A2 a GND | En el debugger: `0x70`. Con A0–A2 en alto sería `0x3F` |
| PCF8574/PCF8574A (alarma: buzzer + sensor de intrusión) | `0x20` (A0–A2 a GND) o `0x27` (A0–A2 a +5 V) | El sketch prueba las dos, excluyendo la que ya usa el LCD |
| Módulo LCD 16x2 `JHD-2X16-I2C` (hora del sistema, opcional) | `0x3E` | En el debugger: `$7C`. El sketch busca otra dirección si no responde 0x3E, excluyendo la del LCD 20x4 y la de la alarma |

El sketch detecta la dirección del LCD principal al arrancar probando `0x38, 0x3F, 0x27, 0x20`. Se necesitan pull-ups de 4.7 kΩ a +5 V en SDA y SCL (una sola pareja para todo el bus). Tras detectar el LCD, el sketch sube el reloj I2C a 400 kHz (`Wire.setClock(400000)`) para reducir el costo de simulación en Proteus (ver `MEMORY.md`, "Rendimiento, tercera pasada").

### 2.2.0 Pantalla 16x2 (opcional): hora del sistema

Segundo LCD: módulo `JHD-2X16-I2C` de Proteus (16x2 con expansor integrado, 4 pines: VDD→+5 V, VSS→GND, SCL→A5, SDA→A4) en el mismo bus I2C. Con `LCD2_JHD 1` (por defecto) el sketch lo maneja con un controlador propio de comandos (`jhdComando`, `jhdTexto`, `jhdInicio`): cada transmisión lleva un byte de control (`0x80` = comando, `0x40` = datos); inicio `0x38, 0x38, 0x0C, 0x01, 0x06`; fila 1 en la posición DDRAM `0x40`. Con `LCD2_JHD 0` usa `LcdI2C` (protocolo del PCF8574, como la 20x4), para una 16x2 con expansor PCF8574 suelto. Fila 1: `Hora del sistema`. Fila 2: `HH:MM:SS` (`actualizarLcd2`, formatea `ahoraG` y lo manda por I2C una vez por segundo real; no lee ningún reloj externo, solo refleja la variable del sistema). Es opcional: si no hay un segundo expansor, el sketch lo omite.

### 2.2.1 Alarma: expansor I2C, buzzer y sensor de intrusión

Un `PCF8574`/`PCF8574A` dedicado (0x20 u 0x27) porque no quedaba ningún pin digital libre del Nano. P0 = buzzer (salida, el sketch lo controla con `alarmaExpWrite()`); P1 = sensor de intrusión (entrada cuasi-bidireccional: se escribe en 1 para dejarlo en alto por el pull-up interno del PCF8574 — unos ~100 µA —, y un botón/PIR a GND lo lleva a bajo; `leerIntrusion()` hace `Wire.requestFrom` y comprueba ese bit). Si el expansor no responde al arrancar, `alarmaDir` queda en 0 y toda la lógica de alarma (`controlAlarma()`) se desactiva sin afectar al resto del sistema.

### 2.2.1 Display de 7 segmentos (tiempo de riego, MM:SS)

Cuatro dígitos `7SEG-COM-CATHODE`, cada uno con su `74HC595`, en cadena y **sin multiplexar** (no hay que refrescarlos, así que no cuestan CPU en la simulación). Cadena: Nano D3 → 595 #1 (decenas de minuto) → #2 → #3 → #4 (unidades de segundo); SRCLK (pin 11) a A3 y RCLK (pin 12) a D13 en los cuatro; OE (13) a GND; SRCLR (10) a +5 V; `QH'` (9) de cada 595 al `SER` (14) del siguiente. Salidas QA…QG = segmentos a…g y QH = punto (el del 2.º dígito hace de dos puntos). El cátodo común de cada dígito a GND; en un circuito real, 330 Ω por segmento.

Software: `mostrar7seg(segundos)` envía 4 bytes con `shiftOut` (tabla `SEG7`) y activa el latch. `riegoSeg` cuenta los segundos simulados que lleva regando (`n × velocidad` cada segundo real, tope 99:59); se pone en 0 cuando empieza un riego (`setBomba`) y conserva el valor al terminar. Con `DEPURAR 1` los dos puntos parpadean cada segundo real; durante el arranque se muestra `88:88` (`prueba7seg`).

### 2.3 LCD por I2C

LM044L (20×4, HD44780) con expansor PCF8574A en modo 4 bits. Mapeo del expansor: P0 = RS, P1 = RW, P2 = E, P3 = backlight, P4–P7 = D4–D7. Las filas del 20×4 empiezan en las direcciones DDRAM `0x00, 0x40, 0x14, 0x54`; la librería lo resuelve en `setCursor`.

### 2.4 Etapa de potencia

Ningún pin del Arduino debe alimentar directamente una carga (máximo ~20 mA por pin).

- **Bomba:** transistor NPN (2N2222/TIP120) con 1 kΩ en la base, diodo 1N4007 en antiparalelo con la bomba y masa común con el Arduino; o módulo de relé. En Proteus basta un LED con 330 Ω.
- **Motor de cortina:** puente H **L293D**. EN1 (pin 1) y VCC1 (`VSS`, pin 16) a +5 V; VCC2 (`VS`, pin 8) a la tensión del motor (**+12 V** en la simulación, porque el `MOTOR` de Proteus es de 12 V). Si EN1 queda sin conectar o VSS a GND, el motor no gira. D9 alto y D10 bajo = abrir; D9 bajo y D10 alto = cerrar; ambos bajo = parado. En un circuito real, alimentar VCC2 con una fuente aparte, con masa común. Un segundo motor en OUT3/OUT4 solo se mueve si se unen IN3 a D9, IN4 a D10 y EN2 a +5 V.
- **LDR:** divisor de voltaje `+5 V – LDR – A0 – 10 kΩ – GND`. Más luz → más voltaje → porcentaje mayor.

## 3. Arquitectura de software

Archivo único: `sistema_casa\sistema_casa.ino`. Es un solo archivo a propósito: el proyecto de VSM Studio (Proteus) solo admite un `main.ino`, así que separarlo en varios `.h`/`.cpp` rompe esa vía de compilación (ver `MEMORY.md`). Registros del ATmega usados directamente: Timer1 (`TCCR1A/B`, `OCR1A`, `TIMSK1`) y modo sleep. Librerías: `Wire`, `EEPROM`, `avr/sleep.h` (incluidas con el IDE), `Keypad` (instalada) y `LcdI2C` (propia, en `librerias\LcdI2C\`). No hay RTC: la hora corre en software sobre el Timer1.

Uso de memoria (compilación actual): flash 56 % (17 384 de 30 720 bytes), RAM 54 % (1 113 de 2 048 bytes).

### 3.1 Bucle principal

`loop()` no bloquea. En cada iteración:

1. Duerme en modo **IDLE** hasta el siguiente tick de 10 ms del **Timer1** (100 Hz). El Timer0 puede despertarlo antes, y entonces vuelve a dormir sin hacer trabajo.
2. Aplica los segundos reales pendientes al reloj en software: `relojAvanzar(segundos × velocidad)`.
3. Sondea el teclado a ~33 Hz (`sondearTeclado`, throttle sobre `getKey`) y procesa la tecla.
4. Cierra mensajes temporales y detiene el motor de la cortina si venció su tiempo.
5. Cada `PERIODO_MS = 500 ms`: lee el LDR y el sensor de humedad, ejecuta `controlRiego`, `controlLuz`, `controlCortina`, `controlAlarma` y, una vez por minuto simulado, imprime el estado por serie.
6. Si hace falta, repinta la pantalla (`dibujar`).

### 3.2 Reloj, eventos y acciones

- **Reloj en software, sin RTC.** El sistema trabaja solo con la **hora del día** (contador de 24 h, sin fecha, días ni años): vive en la variable global `ahoraG` (`struct Hora`: h, m, s) y avanza con el Timer1 (ver §3.3). Al arrancar se fija siempre a `HORA_ARRANQUE:MIN_ARRANQUE` (05:55), porque no hay batería que la conserve; se corrige desde el teclado con `F1`.
- Las ventanas horarias se evalúan con `enVentana(idx)`, en minutos del día. Una ventana es `[inicio, fin)`; si `fin < inicio` cruza la medianoche; si `inicio == fin` está deshabilitada. Hay cuatro: riego mañana, riego tarde, cortina y alarma.
- **Riego:** `controlRiego(humedad)` — `setBomba(false)` si no es horario de riego; dentro del horario, `humedad < humMin` enciende y `humedad > humMax` apaga (histéresis); entre ambos mantiene el estado. Es decir, el horario habilita la posibilidad de regar, pero la humedad decide si realmente hace falta.
- **Cortina:** `controlCortina` solo actúa **cuando cambia** el estado de la ventana (`cortinaVentPrev`), de modo que una orden manual se respeta hasta el siguiente evento programado. `moverCortina` activa el pin correspondiente y fija `cortinaFin = millis() + CORTINA_MS` (4 s); `actualizarCortina` detiene el motor al vencer.
- **Luces:** modo AUTO: `luz < luzOn` → encender; `luz > luzOff` → apagar; entre ambos mantiene el estado (histéresis). Modo 1 = ON y 2 = OFF manual.
- **Alarma:** `controlAlarma` arma/desarma al cambiar el estado de la ventana `V_ALARMA` (`alarmaArmadaPrev`); mientras está armada y no se desarmó a mano, si el sensor de intrusión se dispara (`leerIntrusion()`) enciende el buzzer (`setAlarmaSonando`). Al salir de la ventana se apaga el buzzer y se limpia el desarmado manual. Pulsando `3` en la pantalla de estado se pide una clave (`CLAVE_ALARMA`, fija en el código) para alternar el armado manual (`alternarAlarmaManual`); fuera de horario no tiene efecto.
- Cada acción emite un evento por serie (`[EVENTO] ...`, `[PROG] ...`).

### 3.3 Base de tiempo (Timer1)

El **Timer1** del ATmega328P está en modo CTC con prescaler 64 y `OCR1A = 2499`: genera una interrupción a 100 Hz (16 MHz / 64 / 2500). Su ISR (`TIMER1_COMPA_vect`) levanta el flag `tick10ms` y, cada 100 ticks, suma un segundo a `segPend`. El Timer0 no se toca, porque lo usan `millis()`, `delay()` y la librería `Keypad`. Los pines 9 y 10 (PWM del Timer1) se usan solo con `digitalWrite`, así que no hay conflicto.

El `loop()` consume `segPend` y llama a `relojAvanzar(n × velocidad)`, que suma los segundos a `ahoraG` módulo 86 400 (vuelve a 00:00:00 al pasar de 23:59:59). Velocidades: x1 (valor inicial), x10, x60 y x300, en `VELOCIDADES`.

### 3.4 Configuración y EEPROM

```c
struct Ventana { uint8_t hIni, mIni, hFin, mFin; };
enum IndiceVentana : uint8_t { V_RIEGO_AM, V_RIEGO_PM, V_CORTINA, V_ALARMA, N_VENTANAS };  // N_VENTANAS = 4
struct Config  {
  uint8_t magic;
  Ventana v[N_VENTANAS];          // 4 ventanas x 4 bytes
  uint8_t luzOn, luzOff;          // umbral de luz
  uint8_t humMin, humMax;         // umbral de humedad
};   // 21 bytes
```

Se guarda con `EEPROM.put(0, cfg)` desde la dirección 0. Al arrancar, si `magic != CONFIG_MAGIC (0xA8)`, se cargan los valores de `CONFIG_DEFECTO` y se guardan.

> **Si cambia la estructura `Config` o sus valores por defecto, hay que subir `CONFIG_MAGIC`**; si no, el Arduino seguirá usando datos viejos de la EEPROM. Por eso al agregar la alarma y la humedad se subió de `0xA7` a `0xA8`: la primera vez que corra este sketch en una EEPROM que tenía la versión anterior, detecta el magic distinto y recarga los valores de fábrica (no es un error).

Valores por defecto: riego 06:00–06:30 y 18:00–18:30; cortina abre 07:00, cierra 19:00; alarma arma 23:00, desarma 06:00; luces encienden < 30 %, apagan > 45 %; bomba enciende con humedad < 35 %, apaga > 60 %.

La clave de la alarma (`CLAVE_ALARMA`, por defecto `"1234"`) es una constante del programa, no se edita desde el teclado ni se guarda en EEPROM. La hora y la velocidad tampoco se guardan: la hora vuelve a 05:55 y la velocidad a x1 en cada arranque (no hay RTC con batería).

### 3.5 Interfaz (máquina de estados)

| Pantalla | Función |
|---|---|
| `P_INICIO` | Estado (velocidad, hora, luz, humedad, luces, cortina, riego, alarma) |
| `P_MENU` | Lista de 8 opciones con desplazamiento |
| `P_VENTANA` | Edita inicio/fin de riego, cortina o alarma (2 etapas) |
| `P_UMBRAL` | Edita umbrales de luz o de humedad (2 etapas; `tipoUmbral` elige cuál) |
| `P_HORA` | Edita la hora (`HHMM`) |
| `P_VELOC` | Elige la velocidad del reloj |
| `P_CLAVE` | Pide la clave de 4 dígitos para armar/desarmar la alarma a mano |
| `P_MSG` | Mensaje temporal (1.3 s) y luego pasa a otra pantalla |

`dibujar()` arma las 4 filas de texto y `linea()` las compara con lo que ya muestra el LCD (`pantallaLcd[4][20]`); solo escribe los tramos que cambiaron. Esto reduce mucho el tráfico I2C y acelera la simulación. `redibujar` se activa al pulsar una tecla, cambiar de pantalla y, cada 500 ms, en `P_INICIO` y `P_HORA`. La pantalla de estado usa códigos abreviados por falta de espacio en 20 columnas: cortina `ABRI/CIER/ABIE/CRDA`, alarma `ARM/SON/OFF`.

### 3.6 Teclado

Matriz 4×4 con la librería `Keypad` (pull-ups internos, sin resistencias externas), sondeada a ~33 Hz en vez de cada tick de 10 ms (ver `MEMORY.md`, rendimiento). Mapa de teclas: `1 2 3 A / 4 5 6 B / 7 8 9 C / X 0 K D`, donde `A`–`D` = F0–F3, `X` = ON/C y `K` = OK. En la pantalla de estado, `3` abre `P_CLAVE` para armar/desarmar la alarma.

### 3.7 Salida serie

115200 baud por D1 al Virtual Terminal. Una línea por minuto simulado: `HH:MM:SS | Luz NN% | Riego:.. Luces:.. Cortina:..`, más los eventos y los cambios de programación.

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

Genera `build\sistema_casa.ino.hex` (no usar el `with_bootloader`). En Proteus: doble clic en el Arduino → *Program File* → ese `.hex`, o pegar el sketch en `main.ino` de VSM Studio (solo ese archivo en el proyecto, nada más). Antes, copiar `librerias\LcdI2C\` a `C:\Users\Erick\Documents\Arduino\libraries\LcdI2C\` e instalar `Keypad`.

## 6. Simulación en Proteus 8.13

Componentes: Arduino Nano v3 (librería externa), `PCF8574A` (LCD), `PCF8574`/`PCF8574A` (alarma), `LM044L`, `KEYPAD-SMALLCALC`, `L293D`, `MOTOR`, `LDR` + resistencia 10 kΩ, potenciómetro (humedad), `BUZZER`/`SOUNDER`, `BUTTON` (sensor de intrusión), LEDs, `VIRTUAL TERMINAL` y, opcional, `I2C DEBUGGER`.

**Guion de demostración** (cambiar a x60 con `F2` desde los 05:55 iniciales):

| Hora simulada | Tiempo real aprox. (x60) | Evento |
|---|---|---|
| 06:00 | 0:05 | Riego ON si el suelo está seco (hasta 06:30) |
| 07:00 | 1:05 | Cortina se abre (motor 4 s) |
| 18:00 | 12:05 | Riego ON si el suelo está seco (hasta 18:30) |
| 19:00 | 13:05 | Cortina se cierra |
| 23:00 | 17:05 | Alarma se arma (`Al:ARM` en la pantalla) |
| 06:00 (día siguiente) | 24:05 | Alarma se desarma sola |

Las luces se prueban moviendo el nivel de luz del LDR: se encienden por debajo del 30 % y se apagan por encima del 45 %. El riego se prueba igual con el potenciómetro de humedad (enciende < 35 %, apaga > 60 %), pero **solo dentro de su horario**. La alarma se prueba presionando el botón de intrusión mientras esté armada (`Al:ARM`); para armarla/desarmarla a mano fuera de ese flujo, pulsar `3` y escribir `1234`.

**Rendimiento.** Si la simulación va lenta: cerrar el I2C Debugger y el Virtual Terminal, subir *Frames per Second* en *System → Set Animation Options* y cargar el `.hex` generado por `arduino-cli` (optimizado) en lugar de compilar en Debug (`-O0`). El sketch ya reduce la carga con el repintado parcial del LCD, el reloj en software con Timer1 (sin tráfico I2C de ningún RTC), el teclado sondeado a ~33 Hz y el I2C a 400 kHz (ver `MEMORY.md` para el detalle de cada optimización).

## 7. Parámetros ajustables en el código

| Constante | Valor | Efecto |
|---|---|---|
| `velocidad` | 1 | Velocidad inicial del reloj (tiempo real) |
| `HORA_ARRANQUE`, `MIN_ARRANQUE` | 05:55 | Hora fijada en cada arranque (no hay RTC con batería) |
| `LCD2_JHD` | 1 | 1 = pantalla 16x2 módulo JHD-2X16-I2C (protocolo de comandos); 0 = 16x2 con PCF8574 (`LcdI2C`) |
| `DEPURAR` | 0 | 1 = mensajes `[DBG]` por serie (incluida la causa del último reinicio) y dos puntos del display parpadeando; 0 para la entrega |
| `CORTINA_MS` | 4000 | Duración del giro del motor |
| `PERIODO_MS` | 500 | Periodo del ciclo de control |
| `CLAVE_ALARMA` | `"1234"` | Clave para armar/desarmar la alarma a mano |
| `CONFIG_DEFECTO` | ver §3.4 | Horarios y umbrales de fábrica |
| `CONFIG_MAGIC` | 0xA8 | Subir al cambiar `Config` o sus defaults |

**Para la entrega:** dejar `DEPURAR 0` y `velocidad = 1` (ya son los valores por defecto). La hora se fija sola a 05:55 en cada arranque; programarla desde el teclado con `F1` si se necesita otra.

## 8. Solución de problemas

| Síntoma | Causa probable | Solución |
|---|---|---|
| LCD iluminado, sin texto | Terminales P4–P7 corridas o nombres de red distintos | Clic derecho → *Highlight net* y comprobar cada línea hasta el PCF8574A |
| `? ? S P` / `Noise` en el I2C Debugger | SDA y SCL cruzados | En el Nano A4 = SDA, A5 = SCL |
| Bits `(WHI)` en el I2C Debugger | Faltan pull-ups | 4.7 kΩ a +5 V en SDA y SCL |
| `S 70 N` | Nadie responde en 0x38 | Revisar A0–A2 a GND y la alimentación del PCF8574A |
| `Expansor de alarma no encontrado (alarma deshabilitada)` | Nadie responde en 0x20 ni 0x27 | Revisar A0–A2, alimentación y cableado SDA/SCL del PCF8574/PCF8574A de la alarma |
| La alarma no se dispara con el botón | Botón no conectado a P1, o P1 conectado a algo más | P1 debe ir solo al botón y GND; el PCF8574 ya lo deja en alto con su pull-up interno — no agregar otra resistencia de pull-up ahí |
| `'X' does not name a type` | Función con tipo propio en la firma | Ver §4; usar `uint8_t` y globales |
| `multiple definition of ...` al enlazar en VSM Studio | Algún `.h`/`.cpp` quedó en la carpeta del proyecto de VSM Studio además de `main.ino` | Dejar solo `main.ino` en el proyecto (ver `MEMORY.md`) |
| `unknown type name 'class'` | Archivo `.c` con clases | Renombrar a `.cpp` |
| `'POSITIVE' was not declared` | Librería `LiquidCrystal I2C` equivocada | Usar `LcdI2C` |
| Motor no gira | EN1 sin conectar, VSS (pin 16) a GND, o VS (pin 8) sin tensión | EN1 y VSS a +5 V, VS a +12 V; masa común (ver `PROTEUS.md`) |
| Segundo motor quieto | IN3, IN4 y EN2 sin conectar | IN3→D9, IN4→D10, EN2→+5 V |
| Motor gira al revés | Terminales del motor o D9/D10 intercambiados | Invertir los cables del motor |
| Bomba actúa al revés | Módulo de relé activo en bajo | Cambiar la lógica en `setBomba` |
| Reinicios o bloqueos | Pin RESET a nivel bajo (LED entre RESET y GND), brown-out o fallo del programa | Leer la línea `[DBG] ... reinicio por:` del terminal (activar `DEPURAR 1`): `PIN-RESET` → revisar el esquema (ver §9); `BROWN-OUT` → alimentación; `SALTO-A-0` → fallo del programa |
| Display de 7 seg en blanco o con segmentos erróneos | Cadena de 595 mal unida u orden QA–QG equivocado | Revisar SRCLK/RCLK comunes, `QH'` → `SER`, OE a GND y QA…QG = a…g |
| Simulación lenta | Ver §6 | — |

Aviso inofensivo: `Compiler optimizations disabled; functions from <util/delay.h> won't work as designed` (VSM Studio compila en Debug). `delay()` de Arduino funciona igual.

## 9. Limitaciones y pendientes

- Probar en Proteus el expansor de la alarma (buzzer + botón de intrusión) y el potenciómetro de humedad — agregados en esta revisión, aún no verificados en simulación real.
- Verificar la compilación del sketch con `mksketch` (arduino-cli no detecta el problema de prototipos con tipos propios).
- Quitar D2/R5 de RESET en el esquema (deja el pin en ~2 V).
- Agregar pull-ups de 4.7 kΩ en SDA/SCL al esquema (hacen falta en el circuito real).
- No hay RTC: la hora se fija a 05:55 en cada arranque y no hay fecha, así que los eventos se repiten todos los días a su hora programada.
- Actualizar el diagrama pictórico (`diagrama_conexion.svg`/`.png`) con el potenciómetro de humedad (A6) y el expansor de la alarma (buzzer + botón) — no se regeneró en este cambio.

## 10. Estructura del repositorio

```
sistema_casa/sistema_casa.ino    Sketch principal (archivo único, ver sección 3)
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
