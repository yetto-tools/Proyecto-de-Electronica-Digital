# Manual de usuario — Sistema para Casa (Grupo 1)

Curso de Electrónica Digital, UMG.

## 1. ¿Qué hace el sistema?

El sistema controla tres cosas de una casa y muestra todo en una pantalla:

| Función | Qué hace |
|---|---|
| **Iluminación** | Mide la luz ambiente. Enciende las luces cuando hay poca luz y las apaga cuando hay mucha. También se pueden forzar manualmente. |
| **Riego del jardín** | Enciende la bomba de riego en dos horarios al día: uno en la mañana y otro en la tarde. |
| **Cortinas** | Un motor abre y cierra la cortina a una hora programada, o cuando el usuario lo pide con el teclado. |

Todo funciona según un **reloj de tiempo real**: los eventos ocurren a la hora configurada, sin intervención.

## 2. Partes que usa el usuario

- **Pantalla LCD de 20 columnas × 4 filas:** muestra el estado y los menús.
- **Teclado de 16 teclas:**

```
 1    2    3    F0
 4    5    6    F1
 7    8    9    F2
ON/C  0    OK   F3
```

- **Indicadores:** un LED o relé de riego, un LED de luces y el motor de la cortina.

## 3. Pantalla de estado

Es la pantalla que se ve normalmente:

```
04/10/26    05:55:12
Luz: 62% Luces:OFF A
Cortina:CERRADA
Riego:OFF Prox 06:00
```

| Fila | Contenido |
|---|---|
| 1 | Fecha (`DD/MM/AA`) y hora (`HH:MM:SS`) |
| 2 | Nivel de luz en %, estado de las luces (`ON`/`OFF`) y modo: `A` = automático, `M` = manual |
| 3 | Estado de la cortina: `ABIERTA`, `CERRADA`, `ABRIENDO` o `CERRANDO` |
| 4 | Riego `ON`/`OFF`. Si está apagado, muestra la hora del **próximo** riego (`Prox`); si está regando, la hora en que **termina** (`Fin`). |

## 4. Teclas en la pantalla de estado

| Tecla | Acción |
|---|---|
| `F0` | Abrir el menú de configuración |
| `F1` | Ajustar la hora |
| `F2` | Cambiar la velocidad del reloj (solo para demostraciones) |
| `F3` | Cambiar el modo de las luces: **AUTO → ON → OFF → AUTO…** |
| `1` | Abrir la cortina |
| `2` | Cerrar la cortina |
| `0` | Detener el motor de la cortina |
| `3` | Ajustar la fecha |

**Luces:** en AUTO el sistema decide según la luz ambiente. En ON quedan encendidas y en OFF apagadas, sin importar la luz. La `M` en la pantalla indica que está en manual.

**Cortina:** una orden manual (`1` o `2`) se respeta hasta el siguiente evento programado, que volverá a abrir o cerrar a su hora. El motor gira 4 segundos en cada movimiento.

## 5. Menú de configuración

Se abre con `F0`. Muestra 4 opciones a la vez y se desplaza al bajar.

```
>1 Riego manana
 2 Riego tarde
 3 Cortinas
 4 Umbral de luz
```

| Opción | Para qué sirve |
|---|---|
| 1 Riego manana | Hora de inicio y de fin del riego de la mañana |
| 2 Riego tarde | Hora de inicio y de fin del riego de la tarde |
| 3 Cortinas | Hora de abrir y hora de cerrar la cortina |
| 4 Umbral de luz | Porcentajes de luz para encender y apagar las luces |
| 5 Ajustar hora | Poner el reloj en hora |
| 6 Ajustar fecha | Poner la fecha |
| 7 Velocidad reloj | Acelerar el reloj para probar el sistema |

**Moverse en el menú:**

| Tecla | Acción |
|---|---|
| `F1` | Siguiente opción |
| `F2` | Opción anterior |
| `1` … `7` | Ir directamente a esa opción |
| `OK` | Entrar a la opción marcada con `>` |
| `ON/C` o `F0` | Volver a la pantalla de estado |

## 6. Cómo configurar

En todas las pantallas de entrada: se escriben los dígitos con el teclado numérico, `ON/C` borra el último dígito (o vuelve atrás si no hay nada escrito) y `OK` acepta. Los espacios sin escribir se ven como `_`.

### 6.1 Horarios de riego y cortina

1. Entra al menú (`F0`) y elige la opción (1, 2 o 3).
2. Escribe la hora de **inicio** en formato `HHMM` de 24 horas (por ejemplo `0600` = 6:00 a. m., `1830` = 6:30 p. m.) y pulsa `OK`.
3. Escribe la hora de **fin** y pulsa `OK`. Aparece "Guardado".

Para las cortinas, el primer dato es la hora de **abrir** y el segundo la hora de **cerrar**.

Reglas:
- Si la hora de fin es menor que la de inicio, el horario cruza la medianoche (por ejemplo 22:00 a 02:00).
- Si inicio y fin son iguales, ese horario queda **desactivado**.
- Si la hora no es válida (por ejemplo `2575`), aparece "Hora invalida" y se vuelve a pedir.

Valores de fábrica:

| Evento | Horario |
|---|---|
| Riego de la mañana | 06:00 – 06:30 |
| Riego de la tarde | 18:00 – 18:30 |
| Cortina | Abre 07:00, cierra 19:00 |

### 6.2 Umbral de luz

1. Menú → opción 4.
2. Escribe el porcentaje **debajo** del cual se **encienden** las luces (0 a 99) y pulsa `OK`.
3. Escribe el porcentaje **encima** del cual se **apagan** (mayor que el anterior, hasta 100) y pulsa `OK`.

De fábrica: se encienden por debajo de 30 % y se apagan por encima de 45 %. Entre los dos valores las luces mantienen su estado, para que no parpadeen.

### 6.3 Ajustar la hora

1. Pulsa `F1` en la pantalla de estado (o menú → opción 5).
2. La pantalla muestra la hora actual con segundos.
3. Escribe la hora nueva como `HHMM` y pulsa `OK`. Los segundos empiezan en cero.

### 6.4 Ajustar la fecha

1. Pulsa `3` en la pantalla de estado (o menú → opción 6).
2. Escribe `DDMMAA` y pulsa `OK`. Por ejemplo, `041026` es 4 de octubre de 2026.
3. Una fecha que no existe (como 31/02) se rechaza con "Fecha invalida".

La fecha avanza sola cada día y respeta los meses y los años bisiestos.

### 6.5 Velocidad del reloj (demostraciones)

Pulsa `F2` (o menú → opción 7) y elige:

| Tecla | Velocidad | Significado |
|---|---|---|
| `1` | x1 | Tiempo real |
| `2` | x10 | 1 segundo real = 10 segundos del reloj |
| `3` | x60 | 1 segundo real = 1 minuto del reloj |
| `4` | x300 | 1 segundo real = 5 minutos del reloj |

Al arrancar, el sistema usa **x60**. Para uso normal elige **x1**.

Los horarios de riego y cortina y los umbrales de luz **se guardan en memoria** y se conservan al apagar el sistema. La hora y la fecha las mantiene el reloj DS1307. La velocidad del reloj vuelve a x60 al reiniciar.

## 7. Mensajes de la pantalla

| Mensaje | Significado | Qué hacer |
|---|---|---|
| `Guardado` | La configuración se guardó | Nada |
| `Hora invalida – Use HHMM (24h)` | La hora escrita no existe | Escribirla de nuevo |
| `Fecha invalida – Use DDMMAA` | La fecha escrita no existe | Escribirla de nuevo |
| `Valor invalido – 0 a 99` | Porcentaje fuera de rango | Escribirlo de nuevo |
| `Apagar debe ser mayor que encender` | Los umbrales están al revés | Poner un valor de apagado más alto |
| `ERROR: sin RTC – Revise I2C` | No se detecta el reloj | Revisar la conexión del reloj DS1307 |

## 8. Preguntas frecuentes

**Las luces no cambian aunque cambie la luz.** Revisa si están en modo manual (`M` en la fila 2). Pulsa `F3` hasta que aparezca `A`.

**El riego o la cortina no se activan a la hora esperada.** Comprueba la hora y la fecha del reloj (`F1` / `3`) y que el horario no tenga inicio igual a fin. Con la velocidad en x1 el cambio puede tardar horas; usa x60 para probar.

**Al reiniciar vuelve a la hora de las 05:55.** Es el comportamiento de demostración. Un técnico puede desactivarlo (ver el manual técnico, sección 7).

**La pantalla se ve encendida pero sin texto.** Es un problema de conexión del LCD, no de uso. Consulta `PROTEUS.md`, sección "Solución de problemas".
