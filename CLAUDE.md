# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

@MEMORY.md

## Proyecto

Proyecto del curso de Electrónica Digital (UMG). **Grupo 1 — Sistema para Casa**, todo en un único **Arduino Nano v3**:

- Iluminación automática por luz ambiente (LDR): enciende y apaga las luces; `F3` alterna AUTO/ON/OFF.
- Riego del jardín programado: ventana de mañana y de tarde.
- Cortinas con motor DC (L293D): abrir y cerrar por horario o manual (teclas 1/2/0 en la pantalla de estado).
- LCD 20x4 con estado y menú de configuración; teclado 4x4 (0–9, F0–F3, ON/C, OK).

(La versión anterior, con alarma, humedad y servo, está en `respaldo\sistema_casa_v1.ino`.)

Requisito central: demostrar el uso de **reloj, eventos y acciones según el tiempo**.

Entregables: implementación con **Arduino**, simulación/virtualización en **Proteus** y el **código** fuente.

**Prioridad máxima (según el usuario): todo debe funcionar en la simulación de Proteus, versión 8.13 (Proteus Professional / ISIS).** Cualquier cambio al sketch o al circuito debe seguir siendo simulable allí (componentes disponibles en Proteus, p. ej. DS1307 y no DS3231).

## Estructura

- `sistema_casa\sistema_casa.ino` — sketch único (Arduino Nano v3). Librerías: `Keypad`, `LcdI2C` (propia), `EEPROM`, `Wire`, `avr/sleep.h`. El DS1307 se maneja con funciones propias (`rtcLeer`, `rtcSetHora`, ...) sobre `Wire` y **no con `RTClib`**.
- `librerias\LcdI2C\` — librería propia para LCD HD44780 por I2C (`LcdI2C.h`, `LcdI2C.cpp`, `library.properties`, `examples\Prueba20x4`). Se instala copiando la carpeta a `C:\Users\Erick\Documents\Arduino\libraries\LcdI2C\`. Sustituye a `LiquidCrystal I2C`.
- `PROTEUS.md` — componentes, cableado, uso del teclado, guion de demostración y solución de problemas en Proteus.
- `README.md` — descripción general del proyecto para quien lo revise.
- `MEMORY.md` — decisiones tomadas, problemas ya resueltos y pendientes. **Leerlo antes de tocar el circuito, el LCD o la compilación.**
- `diagrama_conexion.svg` / `.png` — diagrama pictórico de conexión. Se genera con un script de Python (no está en el repo); si cambian pines o componentes, hay que actualizar también este diagrama y la tabla de `PROTEUS.md`.
- `build\` — salida de compilación; `build\sistema_casa.ino.hex` es lo que se carga en el Arduino de Proteus.

## Restricciones de compilación (VSM Studio de Proteus, Arduino IDE 1.8.5)

- No resuelve dependencias transitivas (p. ej. Adafruit BusIO de `RTClib`): no reintroducir librerías con dependencias.
- Su generador de prototipos (`mksketch`) pone todos los prototipos **al inicio del archivo, antes de cualquier `struct`/`enum`**. Por eso **ninguna función del `.ino` puede usar tipos propios (`Hora`, `Ventana`, `Pantalla`, ...) en su firma ni como tipo de retorno**: usar `uint8_t`/índices y variables globales (`ahoraG`, `cfg`). Tampoco métodos en línea dentro de `struct`/`class` del `.ino`. Si no, falla con `'X' does not name a type`. Esta restricción **no** aplica a las clases dentro de librerías (`LcdI2C`), porque esas no pasan por `mksketch`.
- `arduino-cli` no detecta el caso anterior; hay que probar con `mksketch` (en `C:\Program Files (x86)\Labcenter Electronics\Proteus 8 Professional\Tools\ARDUINO\`) y el `avr-gcc` de `C:\Program Files (x86)\Arduino\hardware\tools\avr\bin`, con la línea de comandos del log de VSM Studio y `-O0`.
- Las librerías se toman de `Documents\Arduino\libraries`. **No agregar archivos de librerías al árbol del proyecto de VSM Studio** (ver `MEMORY.md`): el proyecto solo contiene `main.ino`.
- Todo archivo con clases debe ser `.cpp`, nunca `.c` (VSM Studio compila `.c` como C).
- El `main.ino` debe incluir `#include <Wire.h>` explícitamente para que VSM Studio enlace `Wire`.
- Placa: **Arduino Nano v3** (ATmega328P, 16 MHz).

## Diseño del sketch

- Los horarios y umbrales viven en la struct `Config` (valores por defecto en `CONFIG_DEFECTO`) y se editan desde el teclado/LCD (máquina de estados `Pantalla`); se persisten en EEPROM. Si cambia `Config` o sus defaults, subir `CONFIG_MAGIC`.
- El LCD es **20x4 (`LM044L` en Proteus)** por I2C con **`PCF8574A`, A0–A2 a GND → dirección 0x38**. Se usa I2C porque el Nano no tiene pines para LCD paralelo + teclado 4x4 + salidas. El DS1307 (0x68) comparte el bus.
- Mapeo del expansor: P0=RS, P1=RW, P2=E, P3=backlight, P4–P7=D4–D7. Es el mismo que asume `LcdI2C`.
- En el 20x4 las filas empiezan en DDRAM 0x00, 0x40, 0x14, 0x54 (no son consecutivas); `LcdI2C::setCursor` ya lo resuelve.
- El loop no bloquea: duerme en modo IDLE hasta cada tick de 10 ms del **Timer1** (CTC, 100 Hz; el Timer0 se deja para `millis()`/`Keypad`); control cada 500 ms, teclado sondeado en cada tick.
- **Reloj en software (solo hora del día, contador de 24 h; sin fecha, días ni años):** la hora vive en `ahoraG` y avanzan con el Timer1 (`relojAvanzar`); el DS1307 se lee al arrancar, se escribe al editar y se sincroniza una vez por minuto real (a x1 se lee; acelerado se escribe). Así el bus I2C casi solo lo usa el LCD. No volver a leer el RTC en cada ciclo: ralentiza Proteus.
- **Pantalla 16x2 de la hora del DS1307** (`lcd2`, módulo `JHD-2X16-I2C` de Proteus con 4 pines en `0x3E` = `$7C` del I2C Debugger, opcional; si no responde se busca otra dirección; se maneja con controlador propio de comandos `jhd*` (byte de control 0x80/0x40) si `LCD2_JHD 1`, o con `LcdI2C` si 0; verificado en Proteus que ese módulo usa el protocolo de comandos y no el del PCF8574): `mostrarRtc()` lee el DS1307 cada segundo real. Con `velocidad > 1` se escribe el DS1307 cada segundo real para que coincida. `horaRtc` = lectura cruda; `ahoraG` = hora del sistema; `rtcCargar()` copia la primera en la segunda. Las direcciones I2C se escriben en 7 bits (el debugger muestra el valor × 2).
- **Display de 7 segmentos (tiempo de riego, MM:SS):** 4 × 74HC595 en cadena con 4 × 7SEG-COM-CATHODE, estático (sin multiplexar), por D3 (SER), A3 (SRCLK) y D13 (RCLK). `riegoSeg` cuenta los segundos simulados de riego (se reinicia al empezar cada riego y conserva el valor al terminar). `DEPURAR 1` (define) imprime `[DBG]`, la causa del reinicio (`MCUSR`) y hace parpadear los dos puntos; poner 0 para la entrega. D3, A3 y D13 ya no están libres.
- El LCD se repinta solo donde cambia (`pantallaLcd`, en `linea()`): evitar `lcd->clear()` o reescribir filas completas.
- La velocidad del reloj (`velocidad`, x1/x10/x60/x300) se cambia en ejecución con `F2` o desde el menú. En la pantalla de estado: `F1` = hora, `F2` = velocidad. `FIJAR_HORA_AL_ARRANCAR` (define) fija las 05:55 al arrancar para demos repetibles; poner 0 y `velocidad = 1` para operación real. (Reemplaza al antiguo `MODO_DEMO`.)
- Documentación: `README.md`, `PROTEUS.md`, `MANUAL_USUARIO.md` y `MANUAL_TECNICO.md`. Si cambian teclas, menú o pines, actualizar todos más el diagrama.

## Comandos

```powershell
# arduino-cli viene con el Arduino IDE; compilar y regenerar el .hex para Proteus
& "C:\Program Files\Arduino IDE\resources\app\lib\backend\resources\arduino-cli.exe" compile --fqbn arduino:avr:nano:cpu=atmega328 --libraries librerias --output-dir build sistema_casa
```

No hay tests automatizados; la verificación es la simulación en Proteus (salida por Virtual Terminal a 9600 baud, y el instrumento I2C Debugger para el bus).

Las reglas globales del usuario (en `C:\Users\Erick\CLAUDE.md`) aplican aquí, en especial: no agregar líneas de atribución a Claude en commits ni en descripciones de PR.
