# Simulación de Ecosistema con OpenMP


Autómata celular de tres especies (plantas, herbívoros, carnívoros) sobre una
cuadrícula toroidal, paralelizado con OpenMP. El resultado es **determinista**:
idéntico con 1 hilo o con 8.

---

## 1. Compilación

Desde la carpeta del proyecto, en **PowerShell** o **cmd**:

```
gcc -std=c11 -O2 -Wall -Wextra -fopenmp src\main.c src\mundo.c src\fases.c src\sim.c -o ecosim.exe -lm
```

Si no imprime nada, compiló bien. Solo hay que repetirlo al cambiar el código.

> En PowerShell y cmd el comodín `src\*.c` **no** funciona: hay que listar los
> cuatro archivos. En Git Bash sí funciona (`src/*.c`).

---

## 2. Ejecución

| | PowerShell | cmd |
|---|---|---|
| Ejecutar | `.\ecosim.exe` | `ecosim.exe` |
| Fijar hilos | `$env:OMP_NUM_THREADS = 4` | `set OMP_NUM_THREADS=4` |
| Fijar scheduling | `$env:OMP_SCHEDULE = "dynamic,64"` | `set OMP_SCHEDULE=dynamic,64` |
| Descartar el log | `--log NUL` | `--log NUL` |
| Limpiar variable | `Remove-Item Env:\OMP_NUM_THREADS` | `set OMP_NUM_THREADS=` |

Las variables de entorno **quedan fijas para toda la sesión** de la terminal.
Si fijas 1 hilo para una prueba, recuerda cambiarla o limpiarla después.

Sin fijar `OMP_NUM_THREADS`, OpenMP usa todos los núcleos lógicos disponibles.
La primera línea de la salida confirma cuántos hilos se usaron:

```
# Hilos OpenMP : 8
```

Primera prueba:

```
.\ecosim.exe --w 40 --h 18 --ticks 20 --cada 5 --grid
```

---

## 3. Opciones

```
.\ecosim.exe --ayuda
```

### Cuadrícula y tiempo

| Opción | Def. | Descripción |
|---|---|---|
| `--w N` | 200 | Ancho (mínimo 3) |
| `--h N` | 200 | Alto (mínimo 3) |
| `--ticks N` | 200 | Pasos de tiempo. En cada tick, **cada agente actúa una vez** |
| `--semilla N` | 42 | Misma semilla ⇒ misma corrida, siempre |

### Población inicial

Se expresa como **densidad**: fracción de celdas ocupadas al inicio. Así los
parámetros no dependen del tamaño de la cuadrícula.

| Opción | Def. | Descripción |
|---|---|---|
| `--dp F` | 0.40 | Densidad de plantas |
| `--dh F` | 0.10 | Densidad de herbívoros |
| `--dc F` | 0.02 | Densidad de carnívoros |

Con 500×500 y los valores por defecto: ≈100 000 plantas, 25 000 herbívoros,
5 000 carnívoros. El resto (48 %) queda vacío. Las tres deben sumar ≤ 1.

La proporción 40 : 10 : 2 es una **pirámide trófica**: 20 plantas por
carnívoro. Densidades iguales (0.33 / 0.33 / 0.33) colapsan el ecosistema en
pocos ticks — ver experimento D.

### Reglas

| Opción | Def. | Descripción |
|---|---|---|
| `--prepro F` | 0.25 | Prob. de que una planta se expanda a una celda vecina vacía, por tick |
| `--ganh N` / `--ganc N` | 1 / 2 | Energía ganada al comer |
| `--reph N` / `--repc N` | 3 / 3 | Energía necesaria para dejar cría |
| `--hamh N` / `--hamc N` | 4 / 12 | Ticks sin comer tolerados antes de morir |
| `--edah N` / `--edac N` | 60 / 90 | Edad máxima |

### Ejecución y salida

| Opción | Def. | Descripción |
|---|---|---|
| `--hilos N` | 0 | Alternativa a `OMP_NUM_THREADS` (0 = automático) |
| `--cada N` | 10 | Reportar cada N ticks. **`0` desactiva la salida** |
| `--grid` | auto | Forzar impresión del mapa ASCII |
| `--sin-grid` | auto | Nunca imprimir el mapa |
| `--log RUTA` | `resultados.log` | Archivo de resultados |

Consola y log reciben **exactamente lo mismo**. Sin `--grid` ni `--sin-grid`,
el mapa se imprime solo si la cuadrícula es ≤ 80×40.

**La impresión es 100 % serial** y puede dominar el tiempo total: usa
`--cada 0` en toda medición de rendimiento.

---

## 4. Experimentos

### A. Escalabilidad fuerte

Mide el speedup. Anota la línea `Tiempo de simulacion` de cada corrida.

PowerShell:

```
$env:OMP_NUM_THREADS = 1
.\ecosim.exe --w 1000 --h 1000 --ticks 40 --cada 0 --log NUL
$env:OMP_NUM_THREADS = 2
.\ecosim.exe --w 1000 --h 1000 --ticks 40 --cada 0 --log NUL
$env:OMP_NUM_THREADS = 4
.\ecosim.exe --w 1000 --h 1000 --ticks 40 --cada 0 --log NUL
$env:OMP_NUM_THREADS = 6
.\ecosim.exe --w 1000 --h 1000 --ticks 40 --cada 0 --log NUL
$env:OMP_NUM_THREADS = 8
.\ecosim.exe --w 1000 --h 1000 --ticks 40 --cada 0 --log NUL
```

Corre cada una **3 veces** y usa la mediana. Luego calcula:

- Speedup = T₁ / Tₙ
- Eficiencia = Speedup / n

Es normal que la curva se aplane entre 4 y 8 hilos: el código está limitado
por ancho de banda de memoria, y los hilos lógicos de hyper-threading
comparten unidades de ejecución. Cuántos núcleos físicos tienes:

```
Get-CimInstance Win32_Processor | Select-Object NumberOfCores, NumberOfLogicalProcessors
```

### B. Políticas de reparto

La carga por celda es desigual (una celda vacía es casi gratis; un carnívoro
rodeado de presas recorre sus vecinos varias veces), así que es el escenario
donde `dynamic` y `guided` pueden superar a `static`.

```
$env:OMP_NUM_THREADS = 8
$env:OMP_SCHEDULE = "static"
.\ecosim.exe --w 1000 --h 1000 --ticks 40 --cada 0 --log NUL
$env:OMP_SCHEDULE = "dynamic,1"
.\ecosim.exe --w 1000 --h 1000 --ticks 40 --cada 0 --log NUL
$env:OMP_SCHEDULE = "dynamic,64"
.\ecosim.exe --w 1000 --h 1000 --ticks 40 --cada 0 --log NUL
$env:OMP_SCHEDULE = "guided"
.\ecosim.exe --w 1000 --h 1000 --ticks 40 --cada 0 --log NUL
```

`dynamic,1` suele salir **peor que static** por el coste de coordinación: es
el resultado más instructivo del experimento.

### C. Fracción serial (ley de Amdahl)

La diferencia entre estas dos corridas es el coste de la salida, que no se
paraleliza y fija el techo de speedup:

```
$env:OMP_NUM_THREADS = 8
$env:OMP_SCHEDULE = "static"
.\ecosim.exe --w 800 --h 800 --ticks 30 --cada 0 --log NUL
.\ecosim.exe --w 800 --h 800 --ticks 30 --cada 1 --grid --log NUL
```

### D. Sensibilidad a la población inicial

Pirámide trófica (por defecto) frente a densidades iguales:

```
.\ecosim.exe --w 200 --h 200 --ticks 300 --cada 25 --sin-grid --log NUL
.\ecosim.exe --w 200 --h 200 --ticks 300 --cada 25 --sin-grid --dp 0.33 --dh 0.33 --dc 0.33 --log NUL
```

La segunda colapsa: sobran depredadores, arrasan con las presas y luego
mueren de hambre.

### E. Determinismo

Misma configuración, distinto número de hilos ⇒ misma salida:

```
$env:OMP_NUM_THREADS = 1
.\ecosim.exe --w 120 --h 120 --ticks 60 --cada 1 --sin-grid --log h1.log
$env:OMP_NUM_THREADS = 8
.\ecosim.exe --w 120 --h 120 --ticks 60 --cada 1 --sin-grid --log h8.log
Compare-Object (Get-Content h1.log) (Get-Content h8.log)
```

Si `Compare-Object` no imprime nada, los archivos son idénticos (salvo la
línea de hilos y los tiempos).

### F. Sobresuscripción

Más hilos que núcleos degrada el rendimiento por cambios de contexto:

```
$env:OMP_NUM_THREADS = 32
.\ecosim.exe --w 1000 --h 1000 --ticks 40 --cada 0 --log NUL
```

---

## 6. Diseño (resumen)

Cada tick se divide en tres fases secuenciales — **plantas → herbívoros →
carnívoros** — y cada fase en tres barridos paralelos:

| Etapa | Lee | Escribe |
|---|---|---|
| **PROPONER** | cuadrícula actual | su propia celda: a dónde quiere moverse |
| **RESOLVER** | propuestas | su propia celda: quién la ocupa |
| **APLICAR** | todo lo anterior | su propia celda en la cuadrícula siguiente |

Como cada iteración escribe **solo su propia posición**, no hace falta ningún
`critical`, `atomic` ni lock en el núcleo de la simulación.

Tres decisiones que sostienen el diseño:

- **Doble buffer**: se lee de una cuadrícula y se escribe en otra; al final se
  permutan los punteros. Todos los agentes ven el mismo estado inicial del tick.
- **RESOLVER como *gather***: como toda propuesta apunta a una celda adyacente,
  los únicos candidatos a ocupar la celda `d` son sus 8 vecinos. Es `d` quien
  mira hacia afuera y decide, en vez de que varios agentes escriban sobre `d`.
- **RNG sin estado**: el valor aleatorio de una celda es un hash puro de
  `(semilla, tick, fase, índice)`. No se usa `rand()`, que no es thread-safe y
  además haría que el resultado dependiera de qué hilo procesó cada celda.

Estructura del código:

```
src/ecosim.h   tipos, Config, Mundo, prototipos
src/rng.h      generador determinista por celda
src/mundo.c    memoria, inicialización, swap, impresión
src/fases.c    núcleo: proponer / resolver / aplicar
src/sim.c      bucle de ticks
src/main.c     línea de comandos y medición de tiempos
```

---

## 7. Supuestos

El enunciado no especifica estos puntos; se resolvieron así:

1. **Una entidad por celda** (el ejemplo de salida muestra una letra por celda).
2. **Frontera toroidal**: los bordes se conectan entre sí, de modo que toda
   celda tiene exactamente 8 vecinos.
3. **Destinos válidos**: solo celdas que al inicio de la fase estuvieran vacías
   o contuvieran a la presa.
4. **Comer implica moverse**: un agente que se queda quieto no come.
5. **Reproducción solo al moverse**: la cría ocupa la celda de origen, que
   queda garantizadamente libre.
6. **Población inicial por densidad**, no por conteo absoluto.
7. **Vejez y umbrales de energía** no estaban cuantificados; son parámetros.

---

## 8. Nota sobre Git Bash

`make`, `make verificar` y `scripts/bench.sh` solo funcionan en Git Bash o WSL.
Automatizan lo mismo que las secciones 4 y 5, pero no son necesarios: todos los
experimentos se pueden correr a mano desde PowerShell o cmd.