# Simulación de Ecosistema con OpenMP

Universidad del Valle de Guatemala — Computación Paralela y Distribuida, Sección 20

Autómata celular de tres especies (plantas, herbívoros, carnívoros) sobre una
cuadrícula toroidal, paralelizado con OpenMP mediante un esquema
**proponer → resolver → aplicar** que es **determinista**: el resultado es
idéntico bit a bit con 1 hilo o con 64, y con cualquier política de reparto.

---

## 1. Compilación

### Git Bash + MinGW en Windows

```bash
make
```

Si `make` no está instalado, una sola línea basta:

```bash
gcc -std=c11 -O2 -Wall -Wextra -fopenmp src/*.c -o ecosim.exe -lm
```

Otros objetivos:

```bash
make serial     # compila SIN -fopenmp -> ecosim_serial (línea base de referencia)
make verificar  # comprueba que 1, 4 y 8 hilos producen el mismo resultado
make tsan       # ThreadSanitizer (requiere Linux o WSL; no está en MinGW)
make limpiar
```

El código compila **con y sin** `-fopenmp`: todas las directivas están
protegidas con `#ifdef _OPENMP`, así que la versión serial no es un
*build* distinto sino el mismo código sin la bandera.

---

> **Todos los comandos de este README son de Git Bash** (o de cualquier shell
> POSIX: WSL, Linux, macOS). Para `cmd` y PowerShell, ver la sección 2.1.

---

## 2. Ejecución

```bash
./ecosim                                   # 200x200, 200 ticks, valores por defecto
./ecosim --w 40 --h 20 --ticks 30 --cada 5 --grid
OMP_NUM_THREADS=8 ./ecosim --w 1000 --h 1000 --ticks 100
OMP_SCHEDULE="dynamic,64" OMP_NUM_THREADS=8 ./ecosim --w 1000 --h 1000
./ecosim --ayuda                           # lista completa de opciones
```

Salidas:

- **Consola**: parámetros, censo de población cada *N* ticks y, si la
  cuadrícula es pequeña (≤ 80×40) o se pasa `--grid`, el mapa en ASCII
  (`.` vacío, `P` planta, `H` herbívoro, `C` carnívoro).
- **`resultados.log`**: exactamente lo mismo, para entregar como archivo de
  resultados.

La cuadrícula solo se imprime automáticamente en grids pequeños porque en
uno de 1000×1000 la E/S dominaría por completo el tiempo medido (ley de
Amdahl aplicada a la parte serial del programa).

---

### 2.1 Equivalencias para cmd y PowerShell

El ejecutable es el mismo; solo cambia la sintaxis del shell.

| | Git Bash | cmd | PowerShell |
|---|---|---|---|
| Ir a la carpeta | `cd "/c/Users/..."` | `cd /d "C:\Users\..."` | `cd "C:\Users\..."` |
| Ejecutar | `./ecosim.exe` | `ecosim.exe` | `.\ecosim.exe` |
| Variable puntual | `OMP_NUM_THREADS=8 ./ecosim.exe` | `set OMP_NUM_THREADS=8` (línea aparte) | `$env:OMP_NUM_THREADS=8` |
| Descartar salida | `--log /dev/null` | `--log NUL` | `--log NUL` |
| Núcleos disponibles | `nproc` | `echo %NUMBER_OF_PROCESSORS%` | `$env:NUMBER_OF_PROCESSORS` |
| Comodín en rutas | `src/*.c` funciona | listar cada `.c` | listar cada `.c` |

Compilación en cmd (sin comodín, con barras invertidas):

```bat
gcc -std=c11 -O2 -Wall -Wextra -fopenmp src\main.c src\mundo.c src\fases.c src\sim.c -o ecosim.exe -lm
```

`make verificar`, `make tsan` y `scripts/bench.sh` **solo funcionan en Git Bash
o WSL**: usan `diff`, tuberías y sustitución de procesos que cmd no tiene.

---

## 3. El problema y por qué el pseudocódigo del enunciado no sirve

El enunciado propone:

```
#pragma omp parallel for
Para cada celda: actualizar plantas / herbívoros / carnívoros
Sincronizar datos de especies entre hilos
```

Tiene tres defectos:

1. **Condición de carrera.** Cada celda lee y escribe sus 8 vecinos. Dos
   hilos en celdas adyacentes escriben la misma memoria sin sincronización:
   dos herbívoros pueden comerse la *misma* planta o acabar en la misma celda.
2. **Dependencia de orden (read-write hazard).** Si se actualiza sobre la
   misma cuadrícula, un agente que se mueve "hacia adelante" vuelve a ser
   procesado en el mismo tick. En paralelo el orden depende del número de
   hilos, así que **el resultado cambiaría con `OMP_NUM_THREADS`**.
3. *"Sincronizar datos entre hilos"* no es una instrucción: es la parte
   difícil del proyecto enunciada como si fuera trivial.

---

## 4. Estrategia de paralelización

### 4.1 Doble buffer

Dos cuadrículas: `a` (**solo lectura** durante la fase) y `b` (**solo
escritura**). Al terminar se permutan los punteros — `O(1)`, sin copiar.
Todos los agentes ven el mismo estado inicial, lo que elimina el defecto (2).

### 4.2 Fases secuenciales por especie

Cada tick se divide en tres fases: **plantas → herbívoros → carnívoros**,
con un `swap` entre cada una. Cuesta tres pasadas en lugar de una, pero cada
fase tiene un patrón de dependencias mucho más simple: dentro de una fase
solo se mueve una especie.

### 4.3 Proponer → resolver → aplicar

Cada fase son tres barridos `omp for` separados por la barrera implícita del
work-sharing:

| Etapa | Lee | Escribe | Conflictos |
|---|---|---|---|
| **PROPONER** | `a` | `prop[i]` (su propia celda) | ninguno |
| **RESOLVER** | `prop` | `ganador[d]` (su propia celda) | ninguno |
| **APLICAR** | `a`, `prop`, `ganador` | `b[i]` (su propia celda) | ninguno |

**La idea central está en RESOLVER.** El problema natural es un *scatter*
(varios agentes escriben en la misma celda destino), que exigiría atomics o
locks. Se invierte a un ***gather***: como toda propuesta apunta a una celda
adyacente, los únicos candidatos a ocupar la celda `d` son sus 8 vecinos. Así
que es `d` quien mira a sus vecinos y decide quién entra.

Resultado: **el programa no contiene ni un solo `critical`, `atomic` o
`omp_lock_t` en el núcleo de la simulación.** Los únicos `atomic` del
proyecto están en el censo de población, y son 3 por hilo por tick.

### 4.4 Arbitraje determinista

Cada propuesta lleva una clave pseudoaleatoria
`clave = hash(semilla, tick, fase, origen)`. Gana la clave mayor; a igualdad,
el índice de origen menor. Como la clave no depende del hilo ni del orden de
iteración, el ganador es siempre el mismo.

### 4.5 RNG por celda, sin estado

`rand()` está descartado por dos motivos: no es *thread-safe* (en glibc
serializa mediante un lock interno) y, aunque se usara un estado por hilo, el
valor consumido por una celda dependería de qué hilo la procesó.

En su lugar (`src/rng.h`), el valor aleatorio de una celda es una **función
hash pura** de `(semilla, tick, fase, flujo, índice)`. No hay estado que
compartir, no hay contención, y la simulación es reproducible.

### 4.6 Una sola región paralela

`#pragma omp parallel` envuelve el **bucle de ticks completo** (`src/sim.c`),
no cada bucle interno. Con 200 ticks × 3 fases × 3 barridos, la versión
ingenua pagaría 1800 fork/join; esta paga **uno**. La coordinación se logra
con las barreras implícitas de `omp for` y `omp single`, mucho más baratas.

Para conseguirlo sin renunciar a la modularidad, las funciones de `fases.c`
usan **directivas `omp for` huérfanas**, que se ligan en tiempo de ejecución
a la región paralela abierta en `sim.c`.

### 4.7 Otros detalles de rendimiento

- `Celda` ocupa 8 bytes → 8 celdas por línea de caché.
- Cuadrícula aplanada a 1D y recorrida en orden de memoria.
- Censo con acumuladores **privados** por hilo y 3 `atomic` al final, en
  lugar de un arreglo `contador[hilo]` que sufriría *false sharing*.
- `schedule(runtime)` en los barridos: la política se cambia con
  `OMP_SCHEDULE` **sin recompilar**.

---

## 5. Reglas implementadas

Vecindario de Moore (8 vecinos) con frontera **toroidal**: no hay casos
especiales de borde y todas las celdas tienen exactamente 8 vecinos.

| | Regla |
|---|---|
| **Plantas** | Se expanden a una celda vecina vacía con probabilidad `--prepro` por tick. No se mueven. Mueren si sus 8 vecinos son plantas (hacinamiento) o si un herbívoro las consume. |
| **Herbívoros** | Prioridad 1: si hay un carnívoro adyacente, huyen a una celda vacía. Prioridad 2: se mueven a una celda con planta y la comen (`+1` energía, hambre a 0). Prioridad 3: exploran a una celda vacía. Mueren por inanición (`--hamh` ticks sin comer) o vejez (`--edah`). |
| **Carnívoros** | Se mueven a una celda con herbívoro y lo comen (`+2` energía). Si no hay, exploran. Mueren por inanición (`--hamc`) o vejez (`--edac`). |
| **Reproducción** | Un agente que **se mueve** y tiene energía ≥ umbral deja una cría en la celda que acaba de desocupar, y gasta esa energía. Es la interpretación de "suficiente comida y espacio disponible" del enunciado. |

### Supuestos declarados

Estos puntos no estaban especificados en el enunciado y se resolvieron así:

1. **Una entidad por celda.** El ejemplo de salida muestra una letra por
   celda.
2. **Destinos válidos**: solo celdas que al inicio de la fase estuvieran
   vacías o contuvieran a la presa. Consecuencia útil: una celda ocupada por
   un agente de la fase actual nunca es destino válido, así que *"recibir a
   alguien"* y *"marcharse"* son mutuamente excluyentes.
3. **Comer implica moverse.** Un agente que se queda quieto no come.
4. **Reproducción solo al moverse**, para que la cría ocupe la celda de
   origen, que está garantizada libre.
5. **Vejez y umbrales de energía** no estaban cuantificados; son parámetros
   de línea de comandos.

Los valores por defecto están calibrados para producir **coexistencia estable
con oscilaciones de tipo Lotka–Volterra** durante más de 2000 ticks, en lugar
de la extinción que sugiere el ejemplo de salida del enunciado.

---

## 6. Verificación

### 6.1 Determinismo

```bash
make verificar
```

Comprueba que 1, 4 y 8 hilos generan una salida idéntica. Se verificó además
que **90 combinaciones** de {1,2,3,5,8,16 hilos} × {static, static,1,
dynamic,1, dynamic,64, guided} × {3 semillas} producen el mismo hash, y que
coinciden con la compilación **sin OpenMP**.

Esta es la prueba de corrección más fuerte disponible: si hubiera una carrera
en el núcleo, sobrevivir a 16 hilos con `dynamic,1` durante miles de ticks
sin una sola divergencia sería estadísticamente imposible.

### 6.2 ThreadSanitizer y sus falsos positivos

```bash
make tsan && OMP_NUM_THREADS=4 ./ecosim_tsan --w 60 --h 60 --ticks 15
```

**TSan reportará avisos, y son falsos positivos.** La `libgomp` de GCC no
está anotada para TSan, que por tanto no reconoce las barreras implícitas de
`omp for` ni el *join* al cerrar la región paralela: interpreta como carrera
toda escritura de un hilo seguida de una lectura de otro a través de una
barrera. Es el mismo problema, agravado, que hace inutilizables a Helgrind y
DRD de Valgrind con OpenMP.

La herramienta correcta es **Archer**, la capa que anota el runtime de OpenMP
para TSan, disponible con Clang y `libomp` de LLVM:

```bash
clang -fopenmp -fsanitize=thread -O1 -g src/*.c -o ecosim_tsan   # con libarcher
```

Vale la pena documentar esto en el informe: es exactamente la clase de detalle
que separa "usé la herramienta" de "entendí la herramienta".

---

## 7. Experimentos de rendimiento

```bash
./scripts/bench.sh              # 800x800, 60 ticks
./scripts/bench.sh 1500 1500 40
```

Genera `bench_escalabilidad.csv` (hilos, tiempo, speedup, eficiencia) y
`bench_schedule.csv` (política de reparto vs tiempo).

Qué buscar al analizarlos:

| Experimento | Qué debería observarse |
|---|---|
| Escalabilidad fuerte | Speedup casi lineal al inicio y eficiencia decreciente al acercarse al número de núcleos físicos (*hyper-threading* aporta poco en código limitado por memoria). |
| Tamaño de la cuadrícula | Con grids pequeños el overhead de las barreras domina; el speedup mejora al crecer `n`. |
| `static` vs `dynamic` | La carga por celda es **desigual** (una celda vacía es casi gratis; un carnívoro rodeado de presas recorre sus vecinos varias veces), así que `dynamic`/`guided` pueden ganar. Pero `dynamic,1` suele ser **peor que la versión serial** por el coste de coordinación: mídelo, es el resultado más instructivo del laboratorio. |
| Fracción serial | Compara `--cada 1` contra `--cada 0`: la impresión es 100 % serial y fija el techo de Amdahl. |

Metodología: mediana de 3 corridas (el script ya lo hace), máquina sin otras
cargas, y reportar siempre el tamaño de la cuadrícula junto al tiempo.

---

## 8. Estructura del código

```
src/ecosim.h   tipos, Config, Mundo, prototipos
src/rng.h      generador determinista por celda (header-only)
src/mundo.c    memoria, inicialización (con first-touch NUMA), swap, impresión
src/fases.c    núcleo: proponer / resolver / aplicar de las tres fases
src/sim.c      bucle de ticks dentro de una única región paralela
src/main.c     CLI, validación, medición de tiempos
scripts/bench.sh  experimentos de escalabilidad y scheduling
Makefile       objetivos: all, serial, tsan, verificar, limpiar
```

---

## 9. Limitaciones conocidas y posibles mejoras

- `vecinos8()` usa `%` y `/` para convertir índice ↔ coordenada. Es claro pero
  costoso; recorrer con `x`,`y` explícitos y usar una cuadrícula con *halo*
  eliminaría el módulo y permitiría vectorización.
- La estrategia elegida prioriza **determinismo** sobre rendimiento máximo.
  Una variante con *claim* atómico (`compare-and-swap` sobre la celda
  destino) ahorraría el barrido RESOLVER, a costa de que el resultado dependa
  del scheduling. Es la comparación natural para extender el proyecto.
- No hay descomposición explícita por bandas ni control de afinidad. En una
  máquina NUMA de varios sockets convendría fijar `OMP_PROC_BIND=close` y
  `OMP_PLACES=cores`; el `first-touch` de la inicialización ya prepara el
  terreno.
- El movimiento se limita a los 8 vecinos, así que los depredadores no
  "persiguen" a distancia. Un radio de percepción mayor haría la dinámica
  más rica sin cambiar la estrategia de paralelización.