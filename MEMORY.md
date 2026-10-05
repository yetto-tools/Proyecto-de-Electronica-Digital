# MEMORY.md — Decisiones, lecciones aprendidas y pendientes

Registro de lo que ya se probó en Proteus 8.13 y VSM Studio. Antes de proponer cambios al circuito, al LCD o a la compilación, revisar aquí si ya se resolvió o se descartó.

## Decisiones vigentes

| Tema | Decisión | Motivo |
|---|---|---|
| Expansor I2C del LCD | `PCF8574A` con A0, A1, A2 a GND → **0x38** | Es el componente real del esquema. Verificado con I2C Debugger (`S 70 A`). |
| Pantalla | `LM044L` (20x4) | Más espacio para hora, humedad y estados. Reemplaza al `LM016L` (16x2). |
| Librería del LCD | `LcdI2C` propia | `LiquidCrystal I2C` instalada es la de Frank de Brabander (sin `POSITIVE` ni constructor largo) y no soporta bien 20x4 sin ajustes. La propia evita dependencias y funciona en VSM Studio. |
| Ubicación de la librería | Solo en `Documents\Arduino\libraries\LcdI2C\` (copia versionada en `librerias\LcdI2C\`) | Si además se agrega al proyecto de VSM Studio se compila dos veces (ver problemas resueltos). |
| RTC | `DS1307` con funciones propias | `RTClib` arrastra Adafruit BusIO, que VSM Studio no resuelve. |

## Tabla de direcciones I2C

| Chip | A2 A1 A0 = 000 | = 111 | Byte en el I2C Debugger (escritura) |
|---|---|---|---|
| PCF8574 | 0x20 | 0x27 | 0x40 … 0x4E |
| PCF8574A (sufijo M = solo encapsulado) | **0x38** | 0x3F | **0x70** … 0x7E |
| DS1307 | 0x68 (fija) | — | 0xD0 |

El I2C Debugger muestra la dirección **multiplicada por 2** (dirección << 1 | R/W). `A` = ACK (responde), `N` = NACK (nadie responde).

## Problemas ya resueltos (no repetir)

1. **Etiquetas que no unen.** En Proteus las terminales solo se conectan si tienen exactamente el mismo nombre. El LCD tenía `RS`, `RW`, `E`, `D4…D7` y el PCF tenía `P0…P7`: no había conexión. Se renombraron las terminales del lado del LCD a `P0`, `P1`, `P2`, `P4`–`P7`.
2. **P4–P7 corridos dos pines.** Las terminales llegaban a D2–D5 en vez de D4–D7, y D6/D7 quedaban sueltos. El LCD se veía iluminado pero sin texto. **Verificar siempre con clic derecho → Highlight net.**
3. **I2C Debugger con SDA y SCL cruzados.** En el Nano A4 = SDA y A5 = SCL. Con el debugger cruzado aparecía `Noise` y `? ? S P`.
4. **Dirección equivocada.** Se usaba 0x27 (PCF8574 con A0–A2 en alto). Con PCF8574A y A0–A2 a GND es 0x38.
5. **`'POSITIVE' was not declared`.** La librería instalada es la de Brabander: usa `LiquidCrystal_I2C lcd(0x38, 16, 2); lcd.init();`. Ya no aplica porque se usa `LcdI2C`.
6. **`unknown type name 'class'`.** Se había agregado al proyecto un archivo `LcdI2c.c`; VSM Studio compila `.c` como C. Debe ser `.cpp`.
7. **`multiple definition of LcdI2C::...`** y aviso `overriding commands for target 'LcdI2c.o'`. La librería estaba agregada dos veces al proyecto (`LcdI2c.cpp` y `LcdI2C.cpp`; Windows no distingue mayúsculas) además de en `Documents\Arduino\libraries`. Solución: el proyecto de VSM Studio contiene **solo `main.ino`**.

## Cómo diagnosticar el bus I2C en Proteus

- Instrumento **I2C DEBUGGER**: SDA → AD4, SCL → AD5.
- Si se cierra su ventana: con la simulación corriendo o en pausa, menú **Debug → I2C Debug – (nombre)**. Si no aparece, **Debug → Reset Popup Windows**.
- `(WHI)` en los bits = alto débil → faltan pull-ups de 4.7 kΩ a +5 V en SDA y SCL.
- Prueba de cableado sin librería: escribir `0xFF` y `0x00` alternados al PCF cada segundo y ver que los pines 4, 5, 6, 11–14 del LCD cambien entre rojo y azul.
- La librería de LCD espera ~1 s antes de escribir: dejar correr más de 2 s de **tiempo simulado** antes de concluir que no funciona.

## Avisos conocidos (inofensivos)

- `Compiler optimizations disabled; functions from <util/delay.h> won't work as designed`: VSM Studio compila `main.ino` en Debug (`-O0`). `delay()` de Arduino funciona igual. Cambiar a **Release** lo elimina.

## Pendientes

- [x] **Migrar el sketch a `LcdI2C`**: cambiar `#include <LiquidCrystal_I2C.h>` por `#include <LcdI2C.h>`, el constructor a `LcdI2C lcd(0x38, 20, 4);` y `lcd.init()` por `lcd.begin()`.
- [x] **Autodetección de dirección del LCD**: el sketch probaba 0x27 y 0x20; debe incluir **0x38** (y 0x3F). `LcdI2C` recibe la dirección en el constructor, así que la detección debe hacerse antes de construir o agregar un método para cambiarla.
- [x] **Rediseñar las pantallas para 20x4**: los textos del menú están pensados para 16 columnas y 2 filas.
- [ ] **Quitar D2/R5 de RESET** en el esquema: el LED verde con 330 Ω entre RESET y GND deja el pin en ~2 V y puede reiniciar o bloquear el ATmega.
- [x] **Conflicto en D13** (resuelto: columna 3 del teclado en A2): la columna del teclado comparte pin con el LED interno del Nano ("LED & Reset"); puede dar lecturas falsas. Candidato libre: A2 (A0 humedad, A1 teclado, A4/A5 I2C). Si se cambia, actualizar sketch, `PROTEUS.md` y diagrama.
- [ ] Agregar pull-ups de 4.7 kΩ en SDA/SCL al esquema (funcionó sin ellas en simulación, pero son necesarias en el circuito real).
- [x] Actualizar `diagrama_conexion.svg/.png` con PCF8574A + LM044L.

## Rediseño (4 oct 2026)

Se reescribió el sketch: LDR en A0 (luces automáticas), riego AM/PM, motor de cortina por L293D (D9/D10), sin alarma, humedad ni servo. Teclas: `A..D` = F0..F3, `X` = ON/C, `K` = OK. Compila con arduino-cli (47 % flash). **Sin probar aún en Proteus ni con `mksketch`.**
- [x] `diagrama_conexion.svg/.png` actualizado (LDR, L293D, A2 en el teclado, PCF8574A + LM044L).
- [ ] Probar el circuito completo en Proteus.

## Pantalla 16x2 de la hora del DS1307 (4 oct 2026)

El usuario añadió un LCD 16x2 por I2C (dirección `$7C` en el debugger = **0x3E** en 7 bits) para ver la hora que da el DS1307. El sketch crea `lcd2` (`LcdI2C(0x3E, 16, 2)`), con búsqueda automática de otro expansor si 0x3E no responde, y lee el DS1307 cada segundo real (`mostrarRtc`). Con el reloj acelerado escribe la hora simulada en el DS1307 cada segundo real. Cuesta 2 transferencias I2C cortas por segundo, a cambio de ver la lectura real.
- **Hallazgo:** el `Program File` del ATmega328P apuntaba a `AppData\Local\Temp\VSM Studio\...` (compilación de VSM Studio de las 19:10) con una **copia vieja** del sketch (con fecha, sin display de 7 seg ni causa de reinicio). Cargar `build\sistema_casa.ino.hex` o volver a pegar el sketch en `main.ino`. Reloj descartado como causa: CKDIV8 sin programar, cristal externo, 16 MHz.
- La pantalla es el módulo `JHD-2X16-I2C` de Proteus (solo VDD, VSS, SCL, SDA; sin pines A0–A2 ni RS/E/D4–D7). **No verificado** que hable el protocolo del PCF8574 que asume `LcdI2C`; si queda en blanco, mirar en el I2C Debugger los bytes que recibe `$7C` y escribir un controlador propio.
- El debugger mostró `S 70 A` (20x4) y `S 7C A` (16x2): las dos responden, pero la 16x2 salió en blanco con `LcdI2C`. Se añadió un controlador de comandos (`LCD2_JHD 1`: control 0x80/0x40, init `38,38,0C,01,06`). Si sigue en blanco, copiar los bytes que recibe `$7C` en el I2C Debugger.
- [x] **Verificado en Proteus (4 oct 2026):** con `LCD2_JHD 1` la 16x2 muestra la hora del DS1307. El módulo `JHD-2X16-I2C` usa el protocolo de comandos (control 0x80/0x40), no el del PCF8574.

## Decisión: sin fecha (4 oct 2026)

El usuario aclaró que **no necesita días ni años**: todo es un contador de 24 h y eventos a la hora programada. Se eliminó del sketch y de la documentación la edición de fecha (`P_FECHA`, tecla `3`, `rtcSetFecha`, `diasMes`, opción de menú). `struct Hora` solo tiene h, m, s; `relojAvanzar` suma módulo 86 400; el DS1307 solo se lee/escribe en 3 registros. Menú de 6 opciones. Las secciones siguientes que mencionan fecha son historial.

## Hora, fecha y rendimiento (4 oct 2026)

La simulación iba lenta. Causa más probable (no medida): el LCD 20x4 por I2C, más las lecturas/escrituras al DS1307 cada 500 ms y cada segundo (acelerado), más un `loop` que giraba en vacío. Cambios en el sketch:

| Cambio | Motivo |
|---|---|
| `linea()` repinta solo los tramos que cambiaron (`pantallaLcd[4][20]`) | Menos tráfico I2C |
| Reloj en software con **Timer1** (CTC, 100 Hz); DS1307 solo al arrancar, al editar y 1 vez por minuto real (x1 lee, acelerado escribe) | Casi sin I2C de reloj; acelerar es una multiplicación |
| `loop` duerme en IDLE hasta cada tick de 10 ms | Proteus no simula instrucciones mientras duerme; el teclado se barre a 100 Hz |
| Se descartó apagar el Timer0 | `millis()`, `delay()` y `Keypad` dependen de él |
| `MODO_DEMO` reemplazado por `velocidad` en ejecución (x1/x10/x60/x300) | Cambiarla desde el teclado sin recompilar |
| Edición de hora (`F1`) y fecha (`3`), fecha DD/MM/AA en el DS1307 y en la pantalla | Pedido del usuario |

- Teclas en la pantalla de estado: `F0` menú, `F1` hora, `F2` velocidad, `F3` luces, `1`/`2`/`0` cortina, `3` fecha. Menú de 7 opciones.
- Con `FIJAR_HORA_AL_ARRANCAR 1`, cada arranque fija 01/01/26 05:55 (demo repetible); se pierde la fecha editada al reiniciar la simulación.
- Se crearon `MANUAL_USUARIO.md` y `MANUAL_TECNICO.md`; se actualizaron `README.md`, `PROTEUS.md` y el diagrama (etiquetas `F1`/`F2` y pantalla de ejemplo).
- El diagrama se editó a mano en el SVG y el PNG se generó con Chrome headless (el script de Python original no está en el repo).
- Revisión del esquema de Proteus (captura del 4 oct 2026): L293D con VS a +12 V (documentado), **EN1 sin conectar y VSS a GND** (hay que corregirlos: EN1 y VSS a +5 V), segundo motor en OUT3/OUT4 sin IN3/IN4/EN2, DS1307 con SOUT a +5 V directo (dejarlo libre o con pull-up), I2C Debugger y voltímetro abiertos (consumen CPU). El LCD se veía en "Iniciando..." al inicio: confirmar que pasa a la pantalla de estado.
- [x] Esquema corregido por el usuario: EN1, EN2 y VSS del L293D a +5 V; IN3/IN4 a IO9/IO10 (segundo motor en paralelo). Falta VS (pin 8) a +12 V (en la última captura seguía sin cable).
- [ ] SOUT del DS1307 libre (en la captura anterior aparecía a +5 V).
- Display de 7 segmentos con el tiempo de riego (MM:SS): 4 × 74HC595 en cadena + 4 × 7SEG-COM-CATHODE, estático; D3 = SER, A3 = SRCLK, D13 = RCLK (D13 antes se usaba para un LED de latido, que ahora son los dos puntos del display). Documentado en README, PROTEUS, manuales y diagrama.
- El usuario reportó que **el Arduino se reinicia solo**. Se agregó `[DBG] ... reinicio por: ...` (lee MCUSR). Pendiente: ver qué causa indica (PIN-RESET → esquema; BROWN-OUT → alimentación; SALTO-A-0 → fallo del programa).
- [ ] Probar el display de 7 segmentos en Proteus.
- [ ] Verificar con `mksketch` (arduino-cli no detecta los prototipos con tipos propios).
- [ ] Comprobar en Proteus que el teclado responde bien a 100 Hz y medir si mejoró la velocidad.
- [ ] Si sigue lenta: probar `Wire.setClock(400000)` (el modelo del DS1307/PCF de Proteus podría no tolerarlo).
