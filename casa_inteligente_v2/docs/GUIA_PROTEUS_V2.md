# Guía de cambios en Proteus — Sistema Casa v2

> **Destino**: `C:\Users\erashon\umg\digital\Proyecto-de-Electronica-Digital\proteus\Sistema Casa Arduino Nano 3.pdsprj`
> **Fecha**: 2026-10-12 (migración inicial) — **actualizado 2026-10-14**
> **Alcance original**: agregar 4 subsistemas (keypad I²C, sensor de humedad, reed de puerta, buzzer + LED de alarma) y retirar el display 7 segmentos + cadena 74HC595.
>
> **Estado actual**: los pasos 1–9 de este documento ya se aplicaron al esquemático. Esta revisión actualiza la guía con los cambios hechos **después** de esa migración inicial — ver la sección **"7. Cambios posteriores a la migración"** al final. Si estás armando el esquemático desde cero, seguí los pasos 1–9 tal cual, pero usá el **Paso 4 actualizado** (DHT22, no potenciómetro) y la **tabla de pines de la sección 7**, no la de la sección 4 (quedó vieja).

---

## 0. Antes de empezar

| # | Acción | Por qué |
|---|---|---|
| 1 | **Cierre Proteus** por completo | Si está abierto bloquea el `.pdsprj` y los cambios se pierden al guardar. |
| 2 | Confirme que tiene **copia de seguridad externa** (USB / OneDrive / Drive) | Si algo se rompe, restaura desde ahí, no desde mi backup local. |
| 3 | Abra **solo cuando vaya a modificar** | Vuelva a cerrar al terminar cada paso para minimizar la ventana de riesgo. |

Backup local ya generado por el agente:

```
…\proteus\Sistema Casa Arduino Nano 3.BACKUP-20261012-144655.pdsprj
```

---

## 1. Estado actual del esquemático (auditado)

El firmware `FIRMWARE/ATMEGA328P_1/main.ino` que Proteus trae **interno** está alineado con la tabla de pines de AGENTS.md. Hoy el esquemático usa:

| Pin Nano | Función actual |
|---|---|
| `A0` | LDR |
| `A1`, `A2` | Keypad columnas 2 y 3 |
| `A3` | 74HC595 SRCLK |
| `A4`, `A5` | I²C hacia LCD 20x4 |
| `D2`, `D5`, `D6`, `D10` | Keypad filas 0, 2, 3, 1 |
| `D3` | 74HC595 SER |
| `D4` | Servo de cortina |
| `D7` | Bomba (relé/LED) |
| `D8` | Luces (LED) |
| `D11`, `D12` | Keypad columnas 0 y 1 |
| `D13` | 74HC595 RCLK + LED onboard |

**Pin libre hoy**: solo `D9`. `D0` y `D1` están reservados para Serial.

---

## 2. Decisión de hardware para v2

| Componente nuevo | Pin / bus | Pieza Proteus |
|---|---|---|
| Keypad 4x4 | I²C (PCF8574 en `0x20`) | `PCF8574` + `KEYPAD-SMALLCALC` |
| Sensor humedad (potenciómetro) *(superado, ver sección 7.3: ahora es DHT22 en `D2`)* | `A3` | `POT-HG` (o `POT-LOG`) |
| Reed switch puerta | `D3` con `INPUT_PULLUP` | `REED-SWITCH` o `SW-SPST` |
| Buzzer piezo | `D9` | `BUZZER` |
| LED estado alarma | `D10` (con R 330 Ω) | `LED-RED` + `RES 330` |
| ~~Display 7-seg + 74HC595~~ | **se retira** | Eliminar del esquemático |

**Resultado neto en pines del Nano**:

- **Liberados**: `A1`, `A2`, `D2`, `D5`, `D6`, `D10`, `D11`, `D12` (keypad → I²C).
- **Cambio de función**: `D3` (595 SER → puerta), `A3` (595 SRCLK → humedad).
- **Nuevos en uso**: `D9` (buzzer), `D10` (LED alarma).
- **Eliminado**: la cadena de 4×74HC595 y el display 7 segmentos (la MM:SS del riego pasa al LCD 20x4).

⇒ **Mismos 14 pines ocupados que la versión actual**, pero con 3 subsistemas nuevos.

---

## 3. Pasos en Proteus (con clics)

> **Convención**: `→` significa “conectar a”; las instrucciones asumen la GUI en español. Si está en inglés, los nombres equivalentes están entre paréntesis.

### Paso 1 — Agregar el expansor I²C para el keypad

1. **Component Mode** (icono con dos diodos `D` en la barra izquierda).
2. Click en **P** (`Pick Devices`). En el buscador escriba `PCF8574`.
3. **Importante**: elija el chip **`PCF8574`** (NO el `PCF8574A`; ese está ocupado por el LCD 20x4 en `0x3F`).
4. Click **OK** y coloque el chip en una zona libre del esquemático, lejos del Nano para no enredar visualmente.
5. **Conexiones**:
   - Pin `SDA` → etiqueta `SDA` (la misma net donde está colgado el LCD 20x4).
   - Pin `SCL` → etiqueta `SCL` (idem).
   - Pin `VCC` → terminal `+5V`.
   - Pin `GND` → terminal `GROUND`.
6. **Doble-click** sobre el `PCF8574` → campo **Address** → escribir `0x20`. Aceptar.

> **Tip de Proteus**: para conectar dos pines a la misma net, basta con que ambos tengan el mismo `Wire Label`. Si los cables no se ven unidos, click derecho → **Place Wire Label** → mismo nombre en ambos extremos.

### Paso 2 — Eliminar el keypad viejo

1. **Click derecho** sobre el `KEYPAD-SMALLCALC` actual → **Delete Object** (o selecciónelo y pulse `Supr`).
2. **Elimine las 8 nets** que iban del keypad al Nano:
   - los cables conectados a `A1`, `A2`, `D2`, `D5`, `D6`, `D10`, `D11`, `D12`.
   - Click derecho sobre cada cable → **Delete Wire**.
3. Si Proteus le pregunta si quiere borrar los `Wire Label`s relacionados, diga **Sí**.

### Paso 3 — Colocar el nuevo keypad conectado al PCF8574

1. **Pick Devices** → `KEYPAD-SMALLCALC` → **OK**.
2. Coloque el keypad donde estaba el anterior (o donde le quede cómodo).
3. **Conecte los 8 pines del keypad a `P0..P7` del PCF8574** en este orden (debe coincidir con el firmware):

   | Pin keypad | → PCF8574 |
   |---|---|
   | Fila 0 | `P0` |
   | Fila 1 | `P1` |
   | Fila 2 | `P2` |
   | Fila 3 | `P3` |
   | Columna 0 | `P4` |
   | Columna 1 | `P5` |
   | Columna 2 | `P6` |
   | Columna 3 | `P7` |

> El keypad en Proteus tiene los pines rotulados `R0..R3` (filas) y `C0..C3` (columnas). Conéctelos según la tabla.

> **Nota sobre las teclas `ON/C` y `OK`**: la última fila del keypad (la que trae botones rotulados `ON/C` y `OK` en vez de `*` y `#`) se traduce **adentro del firmware** (no en el cableado): el botón `ON/C` manda el código `'X'` (borrar/cancelar) y el botón `OK` manda `'K'` (confirmar). Si en algún momento cambiás de keypad y el nuevo trae literalmente `*` y `#` en vez de esos rótulos, hay que ajustar la tabla `TECLAS` en `main.ino` (sección `teclado_i2c`) para que siga mandando `'X'`/`'K'` en esas dos posiciones — si no, esos dos botones dejan de hacer algo.

### Paso 4 — Sensor de humedad ⚠️ SUPERADO, ver sección 7

> Este paso (potenciómetro en `A3`) es el que se aplicó en la migración original. **Ya no está vigente**: se reemplazó por un sensor **DHT22 en `D2`**. Ver la sección **7.3** más abajo para la conexión actual. Se deja este texto solo como registro histórico de lo que se hizo primero.

1. ~~**Pick Devices** → `POT-HG` (potenciómetro horizontal). Si no aparece, use `POT-LOG` o cualquier potenciómetro genérico.~~
2. ~~Colóquelo cerca del Nano. **Click derecho → Place Text** y escriba `SENSOR HUMEDAD` como etiqueta visual.~~
3. ~~**Conexiones**:~~
   - ~~Terminal **izquierdo** (fijo) → `+5V`.~~
   - ~~Terminal **derecho** (fijo) → `GROUND`.~~
   - ~~Terminal **central** (wiper / cursor) → pin `A3` del Nano.~~

### Paso 5 — Reed switch de la puerta

1. **Pick Devices** → `REED-SWITCH`. Si no está en su librería, use `SW-SPST` (interruptor simple).
2. Colóquelo en un lateral. Agregue text-script con la etiqueta `PUERTA`.
3. **Conexiones**:
   - **Un** terminal → pin `D3` del Nano.
   - **El otro** terminal → `GROUND`.
4. El firmware usará `pinMode(D3, INPUT_PULLUP)`; en Proteus el pull-up interno del AVR se simula correctamente, así que no hace falta resistencia externa.

### Paso 6 — Buzzer piezo

1. **Pick Devices** → `BUZZER` (o `SOUNDER`). Si hay varios, elija el estándar.
2. **Conexiones**:
   - Pin `+` (positivo) → pin `D9` del Nano.
   - Pin `−` (negativo) → `GROUND`.
3. Doble-click → propiedad **Operating Point** → deje el default (5 V).

> En Proteus, el `BUZZER` suena cuando su pin de control está en HIGH. No requiere transistor para la simulación.

### Paso 7 — LED de estado de alarma

1. **Pick Devices** → `RES` → valor **`330`** (ohms). **OK**.
2. **Pick Devices** → `LED-RED`. **OK**.
3. Coloque ambos en serie, conectados así:

   ```
   D10 (Nano)  →  Resistencia 330 Ω  →  Ánodo del LED
                                         Cátodo del LED  →  GROUND
   ```

4. **Click derecho → Place Text** y escriba `LED ALARMA` como etiqueta visual.

> El ánodo del LED en Proteus suele ser el pin marcado `A` o el lado con flecha; el cátodo es `K`. Si el LED no enciende, invierta la polaridad.

### Paso 8 — Retirar el display 7-seg y la cadena 74HC595

1. Identifique en el esquemático los 4 chips `74HC595` y el/los display 7 segmentos.
2. **Supr** sobre cada uno. También las nets que los unían (`SER`, `SRCLK`, `RCLK`).
3. **No es estrictamente obligatorio borrarlos**: si los deja sin conectar, Proteus los ignora en la simulación. Pero hacerlo deja el esquemático más limpio.

### Paso 9 — Verificación final

Antes de guardar y dar por buena la sesión:

1. Menú **Tools → Electrical Rule Check…** (`Tools → ERC…`).
2. Revise el reporte. **Errores tolerables** (puede ignorarlos): pines no usados en componentes pasivos, `VCC`/`GND` de integrados que ya están conectados.
3. **Errores NO tolerables**: nets sueltas en pines críticos, corto entre `+5V` y `GROUND`, conflicto de buses I²C.
4. Menú **File → Save Design** (`Ctrl+S`).
5. Cierre Proteus por completo para liberar el archivo.

---

## 4. Tabla final de pines (post-cambios)

| Pin Nano | Bit ATmega | Función v2 | Notas |
|---|---|---|---|
| `A0` | PC0 | LDR | sin cambios |
| `A3` | PC3 / ADC3 | Sensor humedad (potenciómetro) | antes SRCLK 595 |
| `A4` (D18) | PC4 (SDA) | Bus I²C → PCF8574 + LCD | sin cambios |
| `A5` (D19) | PC5 (SCL) | Bus I²C → PCF8574 + LCD | sin cambios |
| `D3` | PD3 | Reed switch puerta (INPUT_PULLUP) | antes SER 595 |
| `D4` | PD4 | Servo cortina | sin cambios |
| `D7` | PD7 | Bomba | sin cambios |
| `D8` | PB0 | Luces | sin cambios |
| `D9` | PB1 / OC1A | Buzzer alarma | **nuevo** |
| `D10` | PB2 / OC1B | LED alarma | antes fila 1 keypad |
| `D11` | PB3 / MOSI | (libre en Nano, pero el keypad ya está en I²C) | — |
| `D12` | PB4 / MISO | (idem) | — |
| `D13` | PB5 / SCK | LED onboard (sin 595) | 595 retirado |

Pines que se **liberaron al pasar el keypad a I²C** y quedan como reserva para futuras ampliaciones: `A1`, `A2`, `D2`, `D5`, `D6`, `D11`, `D12`.

---

## 5. Validación con el firmware v2

Una vez aplicados los pasos 1–9:

1. Compile el firmware nuevo (`bocetos/casa_inteligente_v2/`) en VSM Studio.
2. Cargue el `.hex` en el esquemático: doble-click sobre el ATmega → propiedad **Firmware File** → apunte al nuevo `.hex`.
3. Ejecute la simulación. Debería ver:
   - LCD 20x4 mostrando el menú principal.
   - Teclado respondiendo (pulsar una tecla debe cambiar de pantalla).
   - Al cambiar el valor de **Actual Humidity (%RH)** del DHT22 (ver sección 7.3), el LCD debe mostrar el nuevo porcentaje de humedad dentro de los ~2 s siguientes.
   - Al pulsar el reed, debe verse cambio de estado en pantalla (debug serial con `DEPURAR 1`).
   - Al armar la alarma y abrir el reed, el buzzer suena y el LED enciende durante `alarmaDuracionSeg` segundos.

Si alguno de estos pasos falla, lo más probable es:

| Síntoma | Causa probable |
|---|---|
| Teclado no responde | Orden `P0..P7` invertido en el PCF8574 (revisar Paso 3). |
| Las teclas `ON/C` u `OK` no hacen nada | Falta la traducción a `'X'`/`'K'` en la tabla `TECLAS` del firmware (ver nota en Paso 3). |
| Humedad siempre en 0% | Ver sección **7.3** — lista de causas específicas del DHT22 (falta pull-up, pulso de arranque muy corto, etc.). |
| Reed siempre “abierto” o “cerrado” | Falta `pinMode(D3, INPUT_PULLUP)` (lo pone el firmware) **o** la net no está conectada a `D3`. |
| Buzzer no suena | Pin `+` invertido (revisar Paso 6). |
| LED alarma no enciende | Resistencia 330 Ω en serie con la polaridad invertida (revisar Paso 7). |

---

## 6. Resumen ejecutivo

| Acción | Cantidad |
|---|---|
| Componentes nuevos a agregar | 4 (PCF8574, POT, REED, BUZZER, LED, RES) — 6 piezas en total *(el POT luego se reemplazó por DHT22, ver sección 7.3)* |
| Componentes a eliminar | 1 keypad + 4×74HC595 + display 7-seg |
| Nets nuevas a cablear | ~15 |
| Tiempo estimado de aplicación | 20–30 minutos con calma |

Cuando haya aplicado los pasos y Proteus haya guardado sin errores, dígame y seguimos con la **Parte 2: el firmware v2** en `bocetos/casa_inteligente_v2/`.

---

## 7. Cambios posteriores a la migración

Todo lo de acá se hizo **después** de aplicar los pasos 1–9. Reemplaza lo que haya quedado viejo en las secciones anteriores.

### 7.1 Firmware: todo en un único `main.ino`

El firmware de `bocetos/casa_inteligente_v2/` dejó de depender de `modulos/*.h`/`*.cpp`: ahora es un **único archivo autocontenido** (`main.ino`), con dos bloques grandes (declaraciones de tipos/funciones, e implementaciones) antes de `setup()`/`loop()`.

**Por qué**: el generador automático de prototipos de Arduino/Proteus (`mksketch`/`ctags`) solo procesa el `.ino` principal. Con los subsistemas repartidos en `.h`/`.cpp` sueltos, el compilador de Proteus no los integraba bien. La carpeta `modulos/` se conserva como referencia/backup, pero **no la toca el build**.

Dos reglas que hay que respetar si se edita `main.ino` a mano:

1. **Todo `struct`/`enum` propio tiene que ir ANTES del primer `#include`** (bloque "TIPOS" al principio del archivo). El generador de prototipos inserta sus forward-declarations justo después del último `#include`, y si un tipo propio se usa en una firma de función antes de existir, no compila.
2. **Ninguna función puede devolver un `struct`/`enum` propio directamente.** El mismo generador de prototipos no reconoce bien un tipo propio como retorno y genera un prototipo sin tipo, rompiendo la compilación. La solución usada: funciones como `alarma_getEstado()` devuelven `uint8_t` en vez de `EstadoAlarma`, y `alarma_getEvento()` usa un parámetro de salida por referencia (`EventoLog&`) en vez de devolver el struct.

### 7.2 LCD 20x4: detección automática de dirección I²C

`main.ino` ya **no** tiene hardcodeada la dirección del LCD. Prueba en orden `0x3F`, `0x38`, `0x27` (función `detectarI2C()`) y usa la primera que responda.

**Ojo con un detalle de Proteus**: la dirección que muestra la ventana de propiedades del componente LCD suele ser la de **8 bits** (incluye el bit R/W). Para pasarla a la de 7 bits que usa el firmware, hay que correrla 1 bit a la derecha (dividir entre 2). Ejemplo: `$7C` (8 bits) → `0x3E` (7 bits).

### 7.3 Sensor de humedad: DHT22 en `D2` (reemplaza al potenciómetro de `A3`)

**Decisión**: la humedad ahora se mide con un sensor **DHT22** en lugar del potenciómetro de los pasos 2/4 originales. `A3` quedó libre.

> **Nota conceptual importante**: el DHT22 mide humedad relativa del **aire**, no humedad de **tierra**. Se usa igual como variable de control del riego (mismo umbral `cfg.humedadSeco`), así que ese umbral hoy representa "aire seco", no "tierra seca". Si la consigna pide específicamente humedad de suelo, hay que volver a un sensor analógico (potenciómetro o sensor resistivo/capacitivo real) en vez del DHT22.

**Conexión** (variante de 3 pines del componente `DHT22` de Proteus — confirmar contra el símbolo real, puede venir con 4 pines en otras librerías):

| Pin DHT22 | Va a |
|---|---|
| VDD | `+5V` |
| DATA | **`D2`** del Nano + resistencia **pull-up 4.7k-10kΩ a +5V** (agregarla aunque el Nano ya ponga `INPUT_PULLUP` interno — el modelo de Proteus la necesita para simular bien el protocolo) |
| GND | `GROUND` |

**Firmware**: lectura por bit-banging propio del protocolo de 1 cable (sin librería externa, mismo criterio que el teclado I²C), en la sección `humedad` de `main.ino`.

**Gotcha de simulación encontrado y resuelto** — dejarlo documentado porque no es obvio:

- El modelo de Proteus (`LISA Model File: DHTXX`, genérico para DHT11 y DHT22) trae por defecto la propiedad avanzada **"Time Select Low" en 18 ms** — ese es el mínimo que exige el **DHT11** real, no el DHT22 (que solo pide ≥1 ms). Aunque el "Part Value" del componente diga `DHT22`, el timing configurado sigue siendo el de DHT11.
- El firmware soluciona esto manteniendo el pulso de arranque en **20 ms** (cubre ambos casos: no hay problema en mantener el pulso más tiempo del mínimo exigido).
- Si el DHT22 se queda siempre en `0%` y el Virtual Terminal muestra `[DHT22] timeout de lectura` sin parar, revisar en ese orden:
  1. ¿Está la resistencia pull-up entre `DATA` y `+5V`?
  2. ¿`DATA` está realmente en `D2` (no en otro pin)?
  3. Doble-click en el DHT22 → **Advanced Properties → Time Select Low** → confirmar que no quedó en un valor absurdamente alto.
- Si en cambio aparece `[DHT22] checksum invalido`, ya está respondiendo bien (el handshake funciona) y el problema es más fino — revisar los márgenes de tiempo en `dht22_esperarPin()` dentro de `main.ino`.

### 7.4 Reloj en 7 segmentos (`7SEG-MPX6-CA-BLUE`): probado y descartado

Se evaluó agregar un reloj físico HH:MM:SS en un display `7SEG-MPX6-CA-BLUE` (6 dígitos, ánodo común), multiplexado por software vía 2× `74HC595` en cascada (`D2`=SER, `D5`=SRCLK, `D6`=RCLK). **Se descartó** por un problema de multiplexado que no se pudo resolver de forma confiable dentro de la simulación de Proteus (los dígitos mostraban valores imposibles, p. ej. segundos > 59, consistentes con un artefacto de la simulación más que con un bug de datos). El firmware final **no** incluye este subsistema — si se retoma en el futuro, buscar en el historial de conversación la implementación completa (incluye tabla de conexión Nano↔595↔display y el modo de diagnóstico lento para aislar el problema).

### 7.5 Tabla de pines vigente (reemplaza a la de la sección 4)

| Pin Nano | Bit ATmega | Función actual | Notas |
|---|---|---|---|
| `A0` | PC0 | LDR | sin cambios |
| `A4` (D18) | PC4 (SDA) | Bus I²C → PCF8574 + LCD | sin cambios |
| `A5` (D19) | PC5 (SCL) | Bus I²C → PCF8574 + LCD | sin cambios |
| `D2` | PD2 | **DATA** del DHT22 (humedad), con pull-up 4.7k-10k a +5V | antes: libre (reserva keypad I²C) |
| `D3` | PD3 | Reed switch puerta (INPUT_PULLUP) | sin cambios |
| `D4` | PD4 | Servo cortina | sin cambios |
| `D7` | PD7 | Bomba | sin cambios |
| `D8` | PB0 | Luces | sin cambios |
| `D9` | PB1 | Buzzer alarma | sin cambios |
| `D10` | PB2 | LED alarma | sin cambios |
| `D13` | PB5 | LED onboard | sin cambios |

**Libres**: `A1`, `A2`, `A3` (liberado al pasar de potenciómetro a DHT22), `D5`, `D6`, `D11`, `D12`.