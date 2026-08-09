#!/usr/bin/env bash
# ============================================================================
# bench.sh - Experimentos de rendimiento para el informe.
#
#   ./scripts/bench.sh            experimentos por defecto
#   ./scripts/bench.sh 1000 1000 60   ancho alto ticks
#
# Genera dos CSV listos para graficar:
#   bench_escalabilidad.csv   hilos, tiempo, speedup, eficiencia
#   bench_schedule.csv        politica, hilos, tiempo
#
# Funciona en Git Bash (Windows) y en Linux/WSL.
# ============================================================================
set -u

W=${1:-800}
H=${2:-800}
TICKS=${3:-60}
BIN=./ecosim
[ -x "$BIN" ] || BIN=./ecosim.exe
[ -x "$BIN" ] || { echo "No se encontro el ejecutable. Ejecuta 'make' primero."; exit 1; }

COMUN="--w $W --h $H --ticks $TICKS --cada 0 --sin-grid --log /dev/null"

# Numero de nucleos logicos (portable entre Linux, macOS y Git Bash).
if   [ -n "${NUMBER_OF_PROCESSORS:-}" ]; then NUCLEOS=$NUMBER_OF_PROCESSORS
elif command -v nproc >/dev/null 2>&1;   then NUCLEOS=$(nproc)
else NUCLEOS=4; fi

tiempo() {  # $1 = hilos, $2 = OMP_SCHEDULE -> imprime segundos
    OMP_NUM_THREADS=$1 OMP_SCHEDULE="$2" $BIN $COMUN 2>/dev/null \
        | awk '/Tiempo de simulacion/ {print $5}'
}

mejor_de_tres() {  # mediana de 3 corridas
    local a b c
    a=$(tiempo "$1" "$2"); b=$(tiempo "$1" "$2"); c=$(tiempo "$1" "$2")
    printf '%s\n%s\n%s\n' "$a" "$b" "$c" | sort -g | sed -n 2p
}

echo "Cuadricula ${W}x${H}, ${TICKS} ticks, hasta ${NUCLEOS} hilos."
echo

# --- Experimento 1: escalabilidad fuerte ------------------------------------
echo "== Escalabilidad fuerte (schedule=static) =="
echo "hilos,tiempo_s,speedup,eficiencia" > bench_escalabilidad.csv
T1=$(mejor_de_tres 1 "static")
printf "%-6s %-12s %-10s %s\n" "hilos" "tiempo(s)" "speedup" "eficiencia"
h=1
while [ "$h" -le "$NUCLEOS" ]; do
    T=$(mejor_de_tres "$h" "static")
    S=$(awk -v a="$T1" -v b="$T" 'BEGIN{printf "%.3f", a/b}')
    E=$(awk -v s="$S"  -v h="$h" 'BEGIN{printf "%.3f", s/h}')
    printf "%-6s %-12s %-10s %s\n" "$h" "$T" "$S" "$E"
    echo "$h,$T,$S,$E" >> bench_escalabilidad.csv
    h=$((h * 2))
done
echo

# --- Experimento 2: politicas de reparto ------------------------------------
# La carga por celda es DESIGUAL (una celda vacia es casi gratis; un carnivoro
# rodeado de presas recorre sus 8 vecinos varias veces), asi que este es el
# escenario donde dynamic/guided pueden superar a static.
echo "== Politicas de reparto (con $NUCLEOS hilos) =="
echo "schedule,hilos,tiempo_s" > bench_schedule.csv
for S in "static" "static,64" "dynamic,1" "dynamic,64" "dynamic,512" "guided" "auto"; do
    T=$(mejor_de_tres "$NUCLEOS" "$S")
    printf "%-14s %s s\n" "$S" "$T"
    echo "\"$S\",$NUCLEOS,$T" >> bench_schedule.csv
done
echo
echo "CSV generados: bench_escalabilidad.csv, bench_schedule.csv"
