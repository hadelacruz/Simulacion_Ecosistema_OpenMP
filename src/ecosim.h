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

enum { FASE_PLANTA = 0, FASE_HERB = 1, FASE_CARN = 2 };

typedef struct {
    unsigned char tipo;   
    unsigned char _pad;   
    short energia;        
    short edad;         
    short hambre;       
} Celda;

typedef struct {
    int      dst;    
    uint32_t clave;  
} Propuesta;

typedef struct {
    int    W, H;                  /* dimensiones de la cuadricula */
    int    ticks;                 /* numero de pasos de tiempo */

    double dens_planta;       
    double dens_herb;
    double dens_carn;

    double p_repro_planta;       

    int    gan_h, gan_c;         
    int    e_repro_h, e_repro_c; 
    int    hambre_max_h, hambre_max_c; /* ticks sin comer antes de morir */
    int    edad_max_h, edad_max_c;     /* muerte por vejez */

    unsigned semilla;            
    int    cada;                 
    int    mostrar_grid;         
    int    hilos;                
    const char *ruta_log;
} Config;

/* --- Mundo: doble buffer + estructuras auxiliares ------------------------ */
typedef struct {
    int W, H, n;         
    Celda    *a;         
    Celda    *b;         
    Propuesta *prop;     
    int      *ganador;   
    long      nP, nH, nC;
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
void fase_plantas(Mundo *m, const Config *c, int tick);
void fase_agentes(Mundo *m, const Config *c, int tick, int tipo);

double simular(Mundo *m, const Config *c, FILE *flog);

#endif /* ECOSIM_H */
