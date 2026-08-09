# ============================================================================
# Makefile - Simulacion de Ecosistema con OpenMP
#
#   make            compila la version paralela  -> ecosim
#   make serial     compila SIN OpenMP           -> ecosim_serial
#   make tsan       compila con ThreadSanitizer  -> ecosim_tsan
#   make verificar  comprueba que 1 hilo y N hilos dan el MISMO resultado
#   make limpiar
#
# En Windows (Git Bash + MinGW) los ejecutables llevan .exe automaticamente;
# si `make` no esta disponible, ver la seccion "Compilacion manual" del README.
# ============================================================================

CC      ?= gcc
CSTD     = -std=c11
WARN     = -Wall -Wextra -Wshadow -Wstrict-prototypes
OPT      = -O2
CFLAGS   = $(CSTD) $(WARN) $(OPT)
LDLIBS   = -lm

SRC      = src/main.c src/mundo.c src/fases.c src/sim.c
HDR      = src/ecosim.h src/rng.h

BIN      = ecosim
BIN_SER  = ecosim_serial
BIN_TSAN = ecosim_tsan

.PHONY: all serial tsan verificar limpiar

all: $(BIN)

$(BIN): $(SRC) $(HDR)
	$(CC) $(CFLAGS) -fopenmp $(SRC) -o $@ $(LDLIBS)

$(BIN_SER): $(SRC) $(HDR)
	$(CC) $(CFLAGS) $(SRC) -o $@ $(LDLIBS)

serial: $(BIN_SER)

# ThreadSanitizer: detector de condiciones de carrera. Es la herramienta
# actual para esto; Helgrind/DRD de Valgrind producen muchos falsos positivos
# con OpenMP por la espera activa del runtime.
# Requiere GCC/Clang en Linux o WSL (no disponible en MinGW).
$(BIN_TSAN): $(SRC) $(HDR)
	$(CC) $(CSTD) $(WARN) -O1 -g -fopenmp -fsanitize=thread $(SRC) -o $@ $(LDLIBS)

tsan: $(BIN_TSAN)

# Prueba de determinismo: la salida debe ser identica con 1 y con N hilos.
verificar: $(BIN)
	@echo "== Determinismo: 1 hilo vs 4 hilos vs 8 hilos =="
	@OMP_NUM_THREADS=1 ./$(BIN) --w 120 --h 120 --ticks 60 --cada 1 --sin-grid --log /dev/null > /tmp/eco_1.txt
	@OMP_NUM_THREADS=4 ./$(BIN) --w 120 --h 120 --ticks 60 --cada 1 --sin-grid --log /dev/null > /tmp/eco_4.txt
	@OMP_NUM_THREADS=8 ./$(BIN) --w 120 --h 120 --ticks 60 --cada 1 --sin-grid --log /dev/null > /tmp/eco_8.txt
	@grep -v -E "Tiempo|Celdas|Hilos" /tmp/eco_1.txt > /tmp/eco_1c.txt
	@grep -v -E "Tiempo|Celdas|Hilos" /tmp/eco_4.txt > /tmp/eco_4c.txt
	@grep -v -E "Tiempo|Celdas|Hilos" /tmp/eco_8.txt > /tmp/eco_8c.txt
	@diff -q /tmp/eco_1c.txt /tmp/eco_4c.txt && diff -q /tmp/eco_1c.txt /tmp/eco_8c.txt \
	  && echo "OK: resultados identicos con 1, 4 y 8 hilos." \
	  || (echo "FALLO: el resultado depende del numero de hilos."; exit 1)

limpiar:
	rm -f $(BIN) $(BIN_SER) $(BIN_TSAN) $(BIN).exe $(BIN_SER).exe $(BIN_TSAN).exe
	rm -f resultados.log bench_*.csv
