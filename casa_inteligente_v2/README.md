# Casa Inteligente v2 — Firmware

Versión 2 del Sistema para Casa (Grupo 1). Reemplaza el firmware de `bocetos/main_modular/` con tres subsistemas nuevos: **sensor de humedad**, **reed de puerta**, y **alarma armada por horario** (buzzer + LED).

> **MCU**: Arduino Nano v3 (ATmega328P @ 16 MHz).
> **Simulación**: Proteus 8.13. Para los pasos del esquemático, ver `docs/GUIA_PROTEUS_V2.md`.

---

## Estructura

```
bocetos/casa_inteligente_v2/
├── main.ino              # firmware completo en un unico archivo
├── README.md             # este archivo
└── modulos/               # LEGACY: ya no se usa para compilar
    └── ...                 # (ver nota abajo)
```

`main.ino` es ahora un **archivo único y autocontenido**: todo lo que antes vivía separado en `modulos/*.h` y `modulos/*.cpp` (tiempo, eeprom_cfg, ldr_luz, ventanas, bomba_riego, cortina_servo, lcd_20x4, lcd_16x2, teclado_i2c, humedad, puerta, alarma, ui) está dentro de `main.ino`, organizado en dos bloques grandes (declaraciones de tipos/funciones, e implementaciones) seguidos de `setup()`/`loop()`.

Esto fue necesario porque el generador automático de prototipos de Arduino/Proteus (`mksketch`/`ctags`) solo procesa el `.ino` principal; al tener los subsistemas repartidos en `.h`/`.cpp` sueltos, el compilador de Proteus no los integraba correctamente. La carpeta `modulos/` se conserva como referencia/backup, pero **no la toca el build** — para modificar el firmware hay que editar `main.ino` directamente.

---

## Tabla de pines (post-cambios)

| Pin Nano | Bit | Función v2 | Antes (v1) |
|---|---|---|---|
| `A0` | PC0 | LDR | igual |
| `A4` | PC4 (SDA) | Bus I²C → PCF8574 + LCD | igual |
| `A5` | PC5 (SCL) | Bus I²C → PCF8574 + LCD | igual |
| `D2` | PD2 | **DATA** sensor de humedad DHT22 (1 cable, con pull-up 4.7k-10k a +5V) | libre |
| `D3` | PD3 | Reed switch puerta | SER 595 |
| `D4` | PD4 | Servo cortina | igual |
| `D7` | PD7 | Bomba | igual |
| `D8` | PB0 | Luces | igual |
| `D9` | PB1 | Buzzer alarma | **libre** |
| `D10` | PB2 | LED alarma | fila 1 keypad |
| `D13` | PB5 | LED onboard (sin 595) | RCLK 595 |

Pines libres que quedan como reserva: `A1`, `A2`, `A3`, `D5`, `D6`, `D11`, `D12`. (`A3` quedó libre al pasar la humedad de un potenciómetro analógico al DHT22 digital en `D2`.)

El display 7 segmentos y la cadena de 4×74HC595 se retiraron. El contador MM:SS del riego ahora se ve en la fila 3 del LCD 20x4.

---

## Compilación con `arduino-builder`

Asumiendo Arduino IDE 1.5.5-r2 instalado en `C:\Program Files (x86)\Arduino\`:

```powershell
arduino-builder.exe -compile `
  -hardware "C:\Program Files (x86)\Arduino\hardware" `
  -tools    "C:\Program Files (x86)\Arduino\hardware\tools" `
  -tools    "C:\Program Files (x86)\Arduino\tools-builder" `
  -libraries "C:\Users\erashon\Documents\Arduino\libraries" `
  -fqbn "arduino:avr:nano:cpu=atmega328" `
  -build-path "<build>" `
  "C:\Users\erashon\umg\digital\CODIGO CASA NANOV3\bocetos\casa_inteligente_v2\main.ino"
```

**Librerías externas obligatorias** (en `Documents\Arduino\libraries\`):

- `Keypad` (Library Manager) — se conserva la firma `Keypad.h` aunque el sondeo real es por I²C en la sección `teclado_i2c` de `main.ino`. **Si quiere eliminarla del todo, hay que quitar también la línea `#include <Keypad.h>` en `main.ino` y confirmar que no quede ninguna referencia suelta.**
- `LcdI2C` — propia del curso.

---

## EEPROM (versión 2)

`struct Config` ocupa **21 bytes**. La EEPROM del ATmega328P tiene 1024 bytes, así que cabe.

| Byte | Campo |
|---|---|
| 0 | `magic` (`0xA8` — versión 2) |
| 1–4 | `v[0]` (V_RIEGO_AM): `hIni, mIni, hFin, mFin` |
| 5–8 | `v[1]` (V_RIEGO_PM) |
| 9–12 | `v[2]` (V_CORTINA) |
| 13–16 | `v[3]` (V_ALARMA) |
| 17 | `luzOn` |
| 18 | `luzOff` |
| 19 | `humedadSeco` |
| 20 | `alarmaDuracionSeg` |

**Importante**: las EEPROMs con la versión vieja (`0xA7`, 16 bytes) **no son compatibles**. Al arrancar con un firmware v2 encima de una EEPROM v1, `cargaConfig()` detecta el magic incorrecto y restaura los valores por defecto de v2. Perderá las programaciones que tenía.

Si modifica `Config` o `CONFIG_DEFECTO`, **suba `CONFIG_MAGIC`** en `eeprom_cfg.cpp`.

---

## Menú principal

Con `A` desde la pantalla de inicio se entra al menú. Con `B` se avanza, con `C` se retrocede, con `1..0` se selecciona.

| # | Entrada | Acción |
|---|---|---|
| 1 | Riego mañana | Editar ventana HH:MM inicio / HH:MM fin |
| 2 | Riego tarde | Idem |
| 3 | Cortinas | Editar ventana ABRIR / CERRAR |
| 4 | Umbral de luz | Editar `luzOn` (enciende bajo) y `luzOff` (apaga sobre) |
| 5 | Umbral humedad | Editar `humedadSeco` (0..100 %) |
| 6 | Duración sirena | Editar `alarmaDuracionSeg` (10..120 s) |
| 7 | Ajustar hora | HH:MM |
| 8 | Velocidad reloj | `1=x1`, `2=x10`, `3=x60`, `4=x300` |
| 9 | Armar / Desarmar | `A` arma, `D` desarma, `X` vuelve |
| 10 | Log eventos | Ver últimos 8 eventos de la alarma |

### Pantalla de inicio (P_INICIO)

```
CASA x1    18:42:35
Luz: 45% Hum: 62% A
Cortina:ABIERTA   Pta:CERRADA
Alm:DESARMAD Rgo:OFF
```

- `CASA x<n>`: n = velocidad del reloj (1 = tiempo real por defecto).
- `Luz:` porcentaje de luz ambiente (0..100).
- `Hum:` porcentaje de humedad relativa del aire (sensor DHT22 en D2).
- `Cortina:` estado / movimiento.
- `Pta:` estado del reed (puerta).
- `Alm:` `DESARMAD` / `ARMADA` / `DISPARO`.
- `Rgo:` estado de la bomba + próximo riego programado.

---

## Lógica clave

### Riego

`controlRiego()` activa la bomba si:

- está dentro de la ventana de la mañana, **o**
- está dentro de la ventana de la tarde, **o**
- la humedad leída por el DHT22 está por debajo de `cfg.humedadSeco`.

El riego por humedad es **correctivo** (no reprograma el `refMin` que muestra la pantalla de inicio).

> **Nota conceptual**: el DHT22 mide humedad relativa del **aire**, no humedad de **tierra**. Se eligió igual como variable de control del riego (reemplazando al potenciómetro que simulaba un sensor de suelo), así que el umbral `cfg.humedadSeco` hoy representa "aire seco", no "tierra seca". Si la consigna exige específicamente humedad de suelo, hay que volver a un sensor analógico (potenciómetro en Proteus, o un sensor resistivo/capacitivo real) en vez del DHT22.

### Alarma

Máquina de estados:

```
DESARMADA --(armar() && enHorario)--> ARMADA
ARMADA    --(puertaAbierta() && puertaEstuvoCerrada)--> DISPARADA
DISPARADA --(transcurrido alarmaDuracionSeg)--> DESARMADA
*         --(desarmar())--> DESARMADA
```

- Solo se puede armar dentro de `cfg.v[V_ALARMA]`. Si la ventana tiene inicio == fin, queda deshabilitada.
- Para evitar disparo inmediato al armar con la puerta ya abierta, se exige que la puerta haya estado cerrada al menos una vez desde el armado.
- Al disparar: buzzer (D9) y LED (D10) en HIGH.
- Alarma autoapagada tras `alarmaDuracionSeg` segundos. También se apaga al desarmar manualmente.
- Log en ring buffer de 8 eventos con timestamp.

### Reloj

- Tiempo real por defecto (`velocidad = 1`).
- `tiempo_setVelocidad(0..3)` selecciona entre {1, 10, 60, 300}.
- Si vuelve a demo, edite `VELOCIDAD_INICIAL` en la sección `tiempo` de `main.ino`.

### Teclado I²C

- PCF8574 en `0x20` (no choca con el LCD en `0x3F`).
- 8 pines multiplexados: `P0..P3` filas, `P4..P7` columnas.
- Sondeo cada ~32 ms desde el `loop()` (cada 3 ticks de 10 ms).
- Antirrebote por confirmación doble.
- Las teclas físicas rotuladas **`ON/C`** y **`OK`** (última fila del keypad, posiciones que en un teclado genérico serían `*` y `#`) se traducen en la tabla `TECLAS` del firmware a los códigos `'X'` (borrar/cancelar) y `'K'` (confirmar) que espera la UI — sin esa traducción esos dos botones no hacen nada.

---

## Cosas a verificar antes de simular

- El esquemático de Proteus debe estar actualizado con los pasos de `docs/GUIA_PROTEUS_V2.md`.
- Confirmar que el PCF8574 del keypad está en `0x20` y conectado a `SDA`/`SCL`.
- Confirmar que el keypad nuevo tiene sus 8 pines en `P0..P7` del PCF8574 en el orden indicado.
- Confirmar la dirección I²C real del LCD 20x4 contra `LCD_CANDIDATAS = {0x3F, 0x38, 0x27}` (Proteus suele mostrar la dirección de 8 bits — hay que dividirla entre 2 / correrla 1 bit a la derecha para obtener la de 7 bits que usa el firmware).
- Confirmar que el DHT22 tiene su resistencia de **pull-up (4.7k-10k) entre DATA y +5V**, y que DATA va a `D2`. Sin el pull-up, `humedad_tick()` va a loguear timeouts por Serial y el riego correctivo nunca se activa.
- El `.ino` interno que Proteus trae por defecto (`Sistema Casa Arduino Nano 3.pdsprj/FIRMWARE/ATMEGA328P_1/main.ino`) es de v1. Hay que reemplazarlo por el `main.ino` de esta carpeta, o apuntar la propiedad `FIRMWARE` del esquemático al `.hex` que produce `arduino-builder`.

---

## Cambios respecto a v1 (resumen)

- ✅ **Riego por horario + humedad** (umbral configurable). Humedad leída con sensor **DHT22** en `D2` (protocolo digital de 1 cable, bit-banging propio sin librería externa) — reemplaza al potenciómetro analógico de A3; ver nota conceptual en "Lógica clave → Riego".
- ✅ **Manejo de luz y cortinas** igual que v1 (sin cambios).
- ✅ **Alarma armada por horario** con buzzer + LED, disparo por apertura de puerta, autoapagado temporizado, armado restringido por ventana horaria.
- ✅ **Keypad por I²C** (libera 8 pines).
- ⚠️ **Display 7-seg retirado** (la MM:SS del riego se ve en el LCD).
- ⚠️ **LCD 16x2 deshabilitado** por defecto (`LCD2_JHD 0`) para ahorrar ROM.
- ⚠️ **Velocidad del reloj por defecto = 1** (tiempo real).
- ⚠️ **Estructura `Config` cambia**: EEPROMs de v1 (`0xA7`) se invalidan y se reinician a defaults de v2.
- ⚠️ **Firmware en un único archivo**: `main.ino` ya no depende de `modulos/*.h`/`*.cpp` (ver sección "Estructura").