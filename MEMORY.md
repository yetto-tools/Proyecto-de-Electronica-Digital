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
8. **`multiple definition of cfg`, `dibujar()`, `ahoraG`, ... al enlazar (`Debug.elf`).** Se separó el sketch en varios `.h`/`.cpp` (`config.cpp`, `reloj.cpp`, ...) y además se pegó la versión aplanada en `main.ino`; los archivos separados quedaron también dentro de la carpeta del proyecto de VSM Studio, que los compiló y enlazó junto con `main.ino` (que ya traía todo el mismo código). Cada símbolo quedó definido dos veces. Confirma la misma regla del punto 7: el proyecto de VSM Studio **solo** puede contener `main.ino`. Se volvió a un `sistema_casa.ino` único para no tener que mantener dos copias del código ni arriesgarse a este error (ver "Separación en archivos (y vuelta atrás)" más abajo).

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

## Eliminación del DS1307 (6 oct 2026)

El usuario pidió quitar toda dependencia del RTC DS1307 y usar solo el Timer1 físico del Nano (ya era la base del reloj en software) como única fuente de hora. Cambios en `sistema_casa.ino`:

- Se eliminaron `DS1307_DIR`, `bcdABin`, `binABcd`, `rtcBegin`, `rtcCorriendo`, `rtcLeer`, `rtcCargar`, `rtcSetHora`, `rtcEscribirTodo`, la variable `horaRtc` y `segSync` (ya no hay con qué sincronizar).
- `ahoraG` se fija a `HORA_ARRANQUE:MIN_ARRANQUE:00` directamente en `setup()` (sin batería que la conserve) y se corrige con F1 escribiendo solo la variable en RAM.
- `mostrarRtc()` (leía el DS1307 para la pantalla 16x2) se volvió `actualizarLcd2()`: solo formatea `ahoraG` y lo manda al LCD 16x2; ya no toca el DS1307 en el bus I2C.
- Se quitó `FIJAR_HORA_AL_ARRANCAR` (ya no tiene sentido: sin RTC, la hora **siempre** se fija al arrancar).
- Compila con `arduino-cli` (51 % flash, 54 % RAM). **Pendiente:** probar en Proteus y correr `mksketch` (arduino-cli no detecta el problema de prototipos con tipos propios, y aquí no se agregó ninguno).
- **Pendiente de actualizar** (todavía mencionan el DS1307 y no se tocaron en este cambio): `PROTEUS.md` (componentes y cableado), `README.md`, `MANUAL_USUARIO.md`, `MANUAL_TECNICO.md` y el diagrama de conexión — hay que quitar el DS1307 del esquema de Proteus y de estos documentos.

## Rendimiento, segunda pasada (6 oct 2026)

Con el DS1307 ya fuera, el mayor costo de CPU que quedaba era el escaneo del teclado: `Keypad::scanKeys()` hace `pinMode()`/`digitalWrite()`/lectura por cada combinación fila×columna (16 para un 4x4) **en cada llamada a `getKey()`**, y el `loop` lo llamaba en cada tick de 10 ms (100 Hz). `pinMode`/`digitalWrite` son de lo más caro de simular en Proteus.

| Cambio | Motivo |
|---|---|
| `teclado.getKey()` se llama cada 3 ticks (~33 Hz) en vez de cada tick (100 Hz), contador `tickTeclado` | 33 Hz sigue siendo instantáneo para una pulsación humana; corta ~2/3 del escaneo de teclado, el mayor gasto de CPU restante |
| `DEPURAR` vuelto a `0` | Dejó de ser necesario para depurar (el usuario ya no lo necesita por ahora); quita el `Serial.println` y el refresco del display de 7 segmentos que corrían cada segundo real aunque no hubiera riego |
| `Serial.begin(9600)` → `Serial.begin(115200)` | Cada carácter impreso cuesta ~1/baudrate de tiempo simulado; 115200 lo reduce ~12x cuando se vuelva a activar `DEPURAR` o se impriman eventos |

- [x] Baud rate del `VIRTUAL TERMINAL` cambiado a 115200 en el esquema de Proteus (por el usuario) y en `PROTEUS.md`/`MANUAL_TECNICO.md` (6 oct 2026). `README.md` y `MANUAL_USUARIO.md` no mencionaban el baud rate, no hizo falta tocarlos.

## Rendimiento, tercera pasada: el LCD 20x4 por I2C (6 oct 2026)

Con el teclado ya a 33 Hz y `DEPURAR` en 0, el usuario reportó que Proteus seguía exageradamente lento. Revisando `librerias\LcdI2C\LcdI2C.cpp`: el HD44780 en modo 4 bits sobre el PCF8574A manda cada byte como **2 nibbles** (`send()`), y cada nibble hace **2 transacciones I2C** (`expanderWrite` con E=1 y luego E=0 en `write4()`) más `delayMicroseconds(1)` y `delayMicroseconds(50)` de espera bloqueante. Es decir, **cada carácter que cambia en el LCD 20x4 cuesta ~4 transacciones I2C + ~100 µs de delay**, y Proteus simula esos flancos de reloj/datos instrucción por instrucción — este es, con diferencia, el costo de CPU más grande que queda (más que el teclado). `linea()` ya minimiza cuántos caracteres cambian, pero no puede evitar el costo por carácter: es inherente a usar un expansor I2C en modo 4 bits (la alternativa, LCD paralelo, no es viable por falta de pines con teclado 4x4 + I2C + salidas).

- Se agregó `Wire.setClock(400000)` (400 kHz en vez de los 100 kHz por defecto) justo después de `lcd->begin()` en `setup()` — **tiene que ir después**, porque `LcdI2C::begin()` llama a `Wire.begin()` internamente y eso resetea el reloj a 100 kHz. Reduce ~4x el tiempo de bus de cada transacción I2C (afecta también al LCD 16x2 y a la detección de direcciones).
- **Experimental:** no verificado aún si el modelo del `PCF8574A`/`JHD-2X16-I2C` de Proteus tolera 400 kHz (mismo riesgo que ya estaba anotado para el DS1307, que ya no aplica por estar eliminado). **Si el LCD muestra basura, parpadea mal o dejar de responder al correr en Proteus, volver `Wire.setClock(400000)` a `Wire.setClock(100000)` o quitar la línea.**
- Compila con `arduino-cli` (51 % flash, 52 % RAM).
- **Otra causa de lentitud que no es del código:** el circuito es de señal mixta (LDR analógica en A0 + motor DC por L293D), lo que obliga a Proteus a usar pasos de simulación más pequeños que un circuito puramente digital — es estructural, no se arregla desde el sketch.
- **Pendientes de rendimiento, no aplicadas todavía (por orden de impacto esperado):**
  - Cerrar/quitar del esquema los instrumentos que no se estén usando en el momento (I2C Debugger, voltímetro) — consumen CPU de Proteus aunque el código no los use.
  - Revisar en Proteus si hay alguna opción de "tiempo real" / sincronización activada que limite la velocidad a 1x a propósito (sería intencional, no un cuello de botella).
  - Si sigue lento: bajar el Timer1 de 100 Hz a 50 Hz (menos despertares de `sleep_mode()` y menos escaneos de teclado en el mismo tiempo real), ajustando `OCR1A`/prescaler y el umbral de `cuenta100`.
  - Pasar el 7 segmentos de `shiftOut()`/`digitalWrite()` a escritura directa de registros `PORTx` (impacto bajo: se llama 1x/seg con `DEPURAR 0`).

## Separación en archivos (y vuelta atrás) (6 oct 2026)

El usuario pidió separar `sistema_casa.ino` (antes un único archivo de ~930 líneas) en varios archivos, incluyendo un `config.h`. Como sigue pegando el código en VSM Studio (que solo admite un `main.ino`), había que conservar la posibilidad de "aplanar" todo a un solo archivo sin romper la restricción de `mksketch` (ninguna función puede usar `Hora`/`Ventana`/`Config`/`Pantalla` en su firma).

**Estructura nueva en `sistema_casa\`:**

| Archivo | Contenido |
|---|---|
| `config.h` / `.cpp` | Pines, `#define DEPURAR`/`LCD2_JHD`, structs (`Hora`, `Ventana`, `Config`, `IndiceVentana`), `CONFIG_DEFECTO`, `cfg`, `guardarConfig()`/`cargarConfig()` |
| `reloj.h` / `.cpp` | Timer1 (ISR, `iniciarTimer1()`), `ahoraG`/`velocidad`/`refMin`, `relojAvanzar()`, `enVentana()`, `calcularRefRiego()`, `avanzarSegundoReal()` (junta lo que antes estaba inline en `loop()`) |
| `display7seg.h` / `.cpp` | `mostrar7seg()`, `prueba7seg()`, `riegoSeg`, `dosPuntos` |
| `lcd2_jhd.h` / `.cpp` | Todo lo de la pantalla 16x2 (`jhd*`, `iniciarLcd2()`, `actualizarLcd2()`); `lcd2Dir`/`lcd2`/`LCD2_DIR` quedaron privados (`static`) del archivo |
| `actuadores.h` / `.cpp` | Bomba, luces, cortina: `setLuces()`, `moverCortina()`, `control*()`, `imprimirEstado()`, `leerLuzPct()` |
| `pantalla.h` / `.cpp` | LCD 20x4, menú, `enum Pantalla`: `dibujar()`, `entrarPantalla()`, `mostrarMsg()`, `iniciarPantalla()` (junta la detección de dirección I2C + init + `Wire.setClock(400000)` que antes estaba inline en `setup()`); `lcd`, `linea()`, `pantallaLcd[][]` quedaron privados |
| `teclado.h` / `.cpp` | Todo el teclado: expone solo `sondearTeclado()` (el throttling a 33 Hz + `procesarTecla()`); el resto (`aceptarEntrada`, `seleccionarMenu`, ...) quedó privado |
| `sistema_casa.ino` | Solo el comentario superior, los `#include`, `setup()` y `loop()` |

**Disciplina que se mantuvo en todos los `.cpp` nuevos** (para que la versión aplanada siga compilando con `mksketch`): ninguna función usa `Hora`/`Ventana`/`Config`/`Pantalla` como parámetro o retorno — todo por variable global, igual que ya hacía el archivo original. Esto es necesario a pesar de que `arduino-cli` compila los `.cpp` como unidades de traducción normales (sin la restricción, porque no pasan por ningún generador de prototipos) — se preservó solo pensando en `proteus\main_vsmstudio.ino`.

**`proteus\main_vsmstudio.ino`**: el mismo código ya aplanado en un solo archivo, para pegar en VSM Studio. Se generó a mano con el mismo orden que ya traía el archivo original (que sí se sabe que compila): todos los tipos, luego todas las variables globales, y solo al final las funciones — así ninguna función se ejecuta antes de que exista la variable global que usa, sin depender de que `mksketch` genere prototipos para variables (solo los genera para funciones). **No se edita a mano**: si cambian los fuentes separados, hay que regenerarlo (pedir que se regenere) antes de volver a pegarlo en VSM Studio.

- Compila con `arduino-cli` tanto `sistema_casa\` (multi-archivo, 50 % flash) como `proteus\main_vsmstudio.ino` aplanado como sketch independiente (50 % flash, tamaño casi idéntico) — buena señal de que el aplanado es equivalente, aunque **sigue sin probarse con `mksketch`/VSM Studio real** (pendiente de antes).
- Cambio de comportamiento menor: la línea `[DBG]` de `avanzarSegundoReal()` (antes en `loop()`) ya no imprime `pant=%u` (la pantalla activa), para no acoplar `reloj.cpp` con `pantalla.h`. Solo afecta con `DEPURAR 1`.
- El usuario cambió `velocidad` de `60` a `1` (tiempo real) como valor inicial; confirmado intencional.

**Vuelta atrás (mismo día):** al probar en VSM Studio, el usuario había agregado los 14 archivos separados (`config.h/.cpp`, `reloj.h/.cpp`, ...) a la carpeta del proyecto de Proteus, además de pegar la versión aplanada en `main.ino`. El enlazador falló con decenas de `multiple definition of cfg/dibujar()/ahoraG/...` porque cada símbolo quedó definido dos veces (ver "Problemas ya resueltos" #8). El usuario decidió **no mantener la separación**: mantener dos copias del mismo código (la separada para `arduino-cli` y la aplanada para VSM Studio) es frágil y ya causó este error en el primer intento real. Se volvió a `sistema_casa.ino` como archivo único, con el mismo contenido que tenía la versión aplanada (o sea: con todas las mejoras de rendimiento y la eliminación del DS1307 ya incluidas, más la línea `[DBG]` con `pant=%u` restaurada ya que al ser un solo archivo no hay motivo para quitarla). Se borraron los 14 archivos separados y `proteus\main_vsmstudio.ino`. Si en el futuro se vuelve a pedir separar en archivos, recordar este motivo antes de proponerlo de nuevo.
- Compila con `arduino-cli` (50 % flash, 52 % RAM), igual que antes de separar.

## Riego por humedad y alarma armada por horario (6 oct 2026)

El usuario pegó el requisito real del curso ("riego por horario y niveles de humedad", "manejo de luz (luces y cortinas)", "armado de alarmas por horario"), que resultó ser exactamente lo que ya resolvía `respaldo\sistema_casa_v1.ino` antes de la reescritura "Grupo 1". Se reintegró esa lógica al sketch actual (que ya no usa DS1307 ni servo, esos cambios de v1 no se tocaron):

- **Riego:** nuevo pin `PIN_HUMEDAD = A6` (potenciómetro, igual que el LDR pero en un pin analógico libre). `controlRiego(humedad)` combina horario **Y** humedad con histéresis (igual que v1): fuera de ventana la bomba siempre está apagada; dentro de ventana, `humedad < humMin` enciende y `humedad > humMax` apaga; entre ambos mantiene el estado. Nuevo umbral editable en el menú ("Umbral de humedad"), comparte la pantalla `P_UMBRAL` con el umbral de luz mediante la variable `tipoUmbral` (0 = luz, 1 = humedad) — se generalizó en vez de duplicar la pantalla.
- **Alarma:** armada por una quinta ventana horaria (`V_ALARMA`, de fábrica 23:00–06:00, cruza medianoche). Mientras está armada, un sensor de intrusión dispara un buzzer (`controlAlarma`/`setAlarmaSonando`); se puede armar/desarmar a mano con la tecla `3` en la pantalla de estado, que pide una clave de 4 dígitos (`CLAVE_ALARMA = "1234"`, fija en el código, no editable ni guardada en EEPROM) en una pantalla nueva `P_CLAVE`.
- **Sin pines digitales libres:** D2–D13 ya estaban todos usados (teclado, 7 segmentos, bomba, luces, cortina). Se eligió un **segundo expansor I2C** (`PCF8574`/`PCF8574A`, 0x20 u 0x27, detectado igual que el del LCD) en vez de quitar alguna función existente. P0 = buzzer (salida), P1 = sensor de intrusión: se escribe 1 para dejarlo en alto por el pull-up interno del PCF8574 (~100 µA) y un botón/PIR a GND lo dispara — **activo en BAJO**, al revés de v1 (que usaba un pin directo del Nano, activo en ALTO). Si el expansor no responde al arrancar, `alarmaDir` queda en 0 y `controlAlarma()` no hace nada (igual de "opcional" que la pantalla 16x2).
- **Conflicto evitado:** la detección de la pantalla 16x2 (`iniciarLcd2`) escaneaba 0x20–0x3F buscando cualquier expansor que respondiera; si el expansor de la alarma estuviera en 0x20 y no hubiera pantalla 16x2 física, esa búsqueda lo habría confundido con un LCD y le habría mandado bytes del protocolo JHD. Se detecta la alarma **antes** que la pantalla 16x2 en `setup()` y se excluye su dirección del escaneo (`iniciarLcd2(lcdDir, alarmaDir)`).
- **`Config` cambió de forma:** `N_VENTANAS` 3→4, se agregaron `humMin`/`humMax`. Se subió `CONFIG_MAGIC` de `0xA7` a `0xA8` (regla ya documentada): la primera vez que corra en una EEPROM con la versión anterior, detecta el magic distinto y recarga los valores de fábrica — normal, no es un bug.
- **Menú:** pasó de 6 a 8 opciones (se agregaron "Alarma" y "Umbral de humedad"); `N_MENU`, `M_UMBRAL`, `M_HUMEDAD`, `M_HORA`, `M_VELOC` se recalcularon.
- **Pantalla de estado rediseñada:** con 20×4 ya no alcanzaba el espacio para mostrar humedad y alarma además de lo que ya había. Se abreviaron cortina (`ABRI`/`CIER`/`ABIE`/`CRDA`) y alarma (`ARM`/`SON`/`OFF`) para que quepan en columnas de 20. **No verificado visualmente en Proteus todavía** — si algo se ve cortado o mal alineado, son solo cadenas de formato (`dibujar()`), fáciles de ajustar.
- `imprimirEstado()` ahora también imprime humedad y estado de la alarma por Serial.
- Compila con `arduino-cli`: 56 % flash (17 384 B), 54 % RAM (1 113 B).
- **Pendiente:** probar todo esto en Proteus (expansor de la alarma, botón de intrusión, potenciómetro de humedad, pantalla rediseñada) — nada de esto se ha corrido en simulación real todavía. Actualizar `diagrama_conexion.svg/.png` (no se tocó, requiere edición manual/herramienta externa).
