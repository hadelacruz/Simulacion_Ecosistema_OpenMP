/* ============================================================================
 * ecosim.h - Simulacion de ecosistema paralelizada con OpenMP
 *
 * Universidad del Valle de Guatemala
 * Computacion Paralela y Distribuida - Seccion 20
 *
 * Estrategia de paralelizacion: PROPONER -> RESOLVER -> APLICAR (dos fases
 * con arbitraje determinista). Ver README.md para la justificacion.
 * ==========================================================================*/
#ifndef ECOSIM_H
#define ECOSIM_H

#include <stdint.h>
#include <stdio.h>

/* --- Tipos de ocupante de una celda ------------------------------------- */
typedef enum {
    VACIO      = 0,
    PLANTA     = 1,
    HERBIVORO  = 2,
    CARNIVORO  = 3
} Tipo;

/* Identificadores de fase; se usan tambien como flujo del RNG para que cada
 * fase tenga una secuencia pseudoaleatoria independiente. */
enum { FASE_PLANTA = 0, FASE_HERB = 1, FASE_CARN = 2 };

/* --- Estado de una celda -------------------------------------------------
 * Struct plano y pequeno (8 bytes) para maximizar celdas por linea de cache.
 * Sin punteros: el grid es un arreglo contiguo, no una lista de agentes.    */
typedef struct {
    unsigned char tipo;   /* Tipo */
    unsigned char _pad;   /* alineacion explicita */
    short energia;        /* acumulada al comer; se gasta al reproducirse */
    short edad;           /* ticks vividos */
    short hambre;         /* ticks consecutivos sin comer */
} Celda;

/* --- Propuesta de movimiento --------------------------------------------
 * Emitida en la fase PROPONER por el ocupante de cada celda.               */
typedef struct {
    int      dst;    /* indice de la celda destino, o -1 si no se mueve */
    uint32_t clave;  /* prioridad pseudoaleatoria para desempatar (siempre > 0) */
} Propuesta;

/* --- Parametros de la simulacion ---------------------------------------- */
typedef struct {
    int    W, H;                  /* dimensiones de la cuadricula */
    int    ticks;                 /* numero de pasos de tiempo */

    double dens_planta;           /* densidades iniciales (fraccion de celdas) */
    double dens_herb;
    double dens_carn;

    double p_repro_planta;        /* prob. de expansion de una planta por tick */

    int    gan_h, gan_c;          /* energia ganada al comer */
    int    e_repro_h, e_repro_c;  /* energia necesaria para dejar cria */
    int    hambre_max_h, hambre_max_c; /* ticks sin comer antes de morir */
    int    edad_max_h, edad_max_c;     /* muerte por vejez */

    unsigned semilla;             /* semilla global; fija => corrida reproducible */
    int    cada;                  /* imprimir estado cada N ticks */
    int    mostrar_grid;          /* -1 = auto, 0 = nunca, 1 = siempre */
    int    hilos;                 /* 0 = dejar que OpenMP decida */
    const char *ruta_log;
} Config;

/* --- Mundo: doble buffer + estructuras auxiliares ------------------------ */
typedef struct {
    int W, H, n;         /* n = W*H */
    Celda    *a;         /* buffer ACTUAL   (solo lectura durante un tick) */
    Celda    *b;         /* buffer SIGUIENTE (solo escritura)              */
    Propuesta *prop;     /* prop[i] = propuesta emitida por la celda i     */
    int      *ganador;   /* ganador[d] = celda origen que ocupara d, o -1  */
    long      nP, nH, nC;/* contadores de poblacion del tick actual        */
} Mundo;

/* --- API ----------------------------------------------------------------- */
void  config_por_defecto(Config *c);
int   config_parsear(Config *c, int argc, char **argv);  /* 0 = ok */
void  config_imprimir(FILE *f, const Config *c);
void  uso(const char *prog);

Mundo *mundo_crear(const Config *c);
void   mundo_liberar(Mundo *m);
void   mundo_inicializar(Mundo *m, const Config *c);
void   mundo_swap(Mundo *m);
void   mundo_imprimir(FILE *f, const Mundo *m);

/* Fases. Contienen directivas `omp for` huerfanas: DEBEN invocarse desde
 * dentro de una region paralela (ver sim.c). */
void fase_plantas(Mundo *m, const Config *c, int tick);
void fase_agentes(Mundo *m, const Config *c, int tick, int tipo);

double simular(Mundo *m, const Config *c, FILE *flog);

#endif /* ECOSIM_H */
