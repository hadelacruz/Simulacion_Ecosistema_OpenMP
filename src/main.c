#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifdef _OPENMP
#  include <omp.h>
#endif

#include "ecosim.h"

/* ------------------------------------------------------------------------ */
void config_por_defecto(Config *c)
{
    memset(c, 0, sizeof *c);
    c->W = 200;  c->H = 200;  c->ticks = 200;

    c->dens_planta = 0.40;
    c->dens_herb   = 0.10;
    c->dens_carn   = 0.02;

    c->p_repro_planta = 0.25;

    c->gan_h = 1;   c->gan_c = 2;      /* segun el enunciado */
    c->e_repro_h = 3;  c->e_repro_c = 3;
    c->hambre_max_h = 4;  c->hambre_max_c = 12;
    c->edad_max_h = 60;   c->edad_max_c = 90;

    c->semilla      = 42;
    c->cada         = 10;
    c->mostrar_grid = -1;      /* auto: solo si el grid es pequeno */
    c->hilos        = 0;       /* 0 = lo decide OpenMP / OMP_NUM_THREADS */
    c->ruta_log     = "resultados.log";
}

/* ------------------------------------------------------------------------ */
void uso(const char *prog)
{
    printf(
"Uso: %s [opciones]\n"
"\n"
"  Cuadricula y tiempo\n"
"    --w N              ancho  (def. 200, min. 3)\n"
"    --h N              alto   (def. 200, min. 3)\n"
"    --ticks N          numero de ticks (def. 200)\n"
"    --semilla N        semilla global; fija => corrida reproducible (def. 42)\n"
"\n"
"  Poblacion inicial (fraccion de celdas)\n"
"    --dp F             densidad de plantas    (def. 0.40)\n"
"    --dh F             densidad de herbivoros (def. 0.10)\n"
"    --dc F             densidad de carnivoros (def. 0.02)\n"
"\n"
"  Reglas\n"
"    --prepro F         prob. de expansion de una planta por tick (def. 0.25)\n"
"    --ganh N --ganc N  energia ganada al comer      (def. 1 / 2)\n"
"    --reph N --repc N  energia para dejar cria      (def. 3 / 3)\n"
"    --hamh N --hamc N  ticks sin comer tolerados    (def. 4 / 12)\n"
"    --edah N --edac N  edad maxima                  (def. 60 / 90)\n"
"\n"
"  Ejecucion y salida\n"
"    --hilos N          numero de hilos OpenMP (0 = automatico)\n"
"    --cada N           reportar cada N ticks; 0 = solo al final (def. 10)\n"
"    --grid             forzar impresion de la cuadricula\n"
"    --sin-grid         nunca imprimir la cuadricula\n"
"    --log RUTA         archivo de resultados (def. resultados.log)\n"
"    --ayuda            esta ayuda\n"
"\n"
"  La politica de reparto se controla sin recompilar, con OMP_SCHEDULE:\n"
"    OMP_SCHEDULE=\"dynamic,64\" OMP_NUM_THREADS=8 %s --w 1000 --h 1000\n",
    prog, prog);
}

/* ------------------------------------------------------------------------ */
#define ARG_INT(nombre, campo)                                    \
    if (strcmp(argv[i], nombre) == 0 && i + 1 < argc) {           \
        c->campo = atoi(argv[++i]); continue;                     \
    }
#define ARG_DBL(nombre, campo)                                    \
    if (strcmp(argv[i], nombre) == 0 && i + 1 < argc) {           \
        c->campo = atof(argv[++i]); continue;                     \
    }

int config_parsear(Config *c, int argc, char **argv)
{
    int i;

    for (i = 1; i < argc; i++) {
        if (strcmp(argv[i], "--ayuda") == 0 || strcmp(argv[i], "-h") == 0) {
            uso(argv[0]);
            return 1;
        }
        ARG_INT("--w",       W)
        ARG_INT("--h",       H)
        ARG_INT("--ticks",   ticks)
        ARG_INT("--ganh",    gan_h)
        ARG_INT("--ganc",    gan_c)
        ARG_INT("--reph",    e_repro_h)
        ARG_INT("--repc",    e_repro_c)
        ARG_INT("--hamh",    hambre_max_h)
        ARG_INT("--hamc",    hambre_max_c)
        ARG_INT("--edah",    edad_max_h)
        ARG_INT("--edac",    edad_max_c)
        ARG_INT("--hilos",   hilos)
        ARG_INT("--cada",    cada)
        ARG_DBL("--dp",      dens_planta)
        ARG_DBL("--dh",      dens_herb)
        ARG_DBL("--dc",      dens_carn)
        ARG_DBL("--prepro",  p_repro_planta)

        if (strcmp(argv[i], "--semilla") == 0 && i + 1 < argc) {
            c->semilla = (unsigned)strtoul(argv[++i], NULL, 10); continue;
        }
        if (strcmp(argv[i], "--log") == 0 && i + 1 < argc) {
            c->ruta_log = argv[++i]; continue;
        }
        if (strcmp(argv[i], "--grid")     == 0) { c->mostrar_grid = 1; continue; }
        if (strcmp(argv[i], "--sin-grid") == 0) { c->mostrar_grid = 0; continue; }

        fprintf(stderr, "Opcion desconocida: %s\n", argv[i]);
        return -1;
    }
    return 0;
}

/* ------------------------------------------------------------------------ */
static int config_validar(const Config *c)
{
    if (c->W < 3 || c->H < 3) {
        fprintf(stderr, "Error: --w y --h deben ser >= 3.\n");
        return -1;
    }
    if (c->ticks < 0) {
        fprintf(stderr, "Error: --ticks no puede ser negativo.\n");
        return -1;
    }
    if (c->dens_planta < 0 || c->dens_herb < 0 || c->dens_carn < 0 ||
        c->dens_planta + c->dens_herb + c->dens_carn > 1.0) {
        fprintf(stderr, "Error: las densidades deben ser >= 0 y sumar <= 1.\n");
        return -1;
    }
    return 0;
}

void config_imprimir(FILE *f, const Config *c)
{
    fprintf(f, "# Cuadricula   : %d x %d (%ld celdas)\n",
            c->W, c->H, (long)c->W * (long)c->H);
    fprintf(f, "# Ticks        : %d\n", c->ticks);
    fprintf(f, "# Semilla      : %u\n", c->semilla);
    fprintf(f, "# Densidades   : P=%.3f H=%.3f C=%.3f\n",
            c->dens_planta, c->dens_herb, c->dens_carn);
    fprintf(f, "# Plantas      : p_repro=%.3f\n", c->p_repro_planta);
    fprintf(f, "# Herbivoros   : +%d al comer, cria con %d, muere tras %d sin comer, edad max %d\n",
            c->gan_h, c->e_repro_h, c->hambre_max_h, c->edad_max_h);
    fprintf(f, "# Carnivoros   : +%d al comer, cria con %d, muere tras %d sin comer, edad max %d\n",
            c->gan_c, c->e_repro_c, c->hambre_max_c, c->edad_max_c);
#ifdef _OPENMP
    fprintf(f, "# Hilos OpenMP : %d\n", omp_get_max_threads());
#else
    fprintf(f, "# Hilos OpenMP : compilado SIN OpenMP (version serial)\n");
#endif
    fprintf(f, "\n");
}

/* ------------------------------------------------------------------------ */
int main(int argc, char **argv)
{
    Config  c;
    Mundo  *m;
    FILE   *flog = NULL;
    double  t;
    int     rc;

    config_por_defecto(&c);
    rc = config_parsear(&c, argc, argv);
    if (rc != 0) return (rc > 0) ? 0 : 1;
    if (config_validar(&c) != 0) return 1;

#ifdef _OPENMP
    if (c.hilos > 0) omp_set_num_threads(c.hilos);
    /* Si el usuario no fijo OMP_SCHEDULE, usamos `static` como linea base
     * conocida en lugar de depender del valor por defecto del runtime. */
    if (getenv("OMP_SCHEDULE") == NULL) omp_set_schedule(omp_sched_static, 0);
#endif

    m = mundo_crear(&c);
    if (!m) { fprintf(stderr, "Error: memoria insuficiente.\n"); return 1; }
    mundo_inicializar(m, &c);

    if (c.ruta_log && c.ruta_log[0]) {
        flog = fopen(c.ruta_log, "w");
        if (!flog) fprintf(stderr, "Aviso: no se pudo abrir %s\n", c.ruta_log);
    }

    config_imprimir(stdout, &c);
    if (flog) config_imprimir(flog, &c);

    t = simular(m, &c, flog);

    printf("--------------------------------------------------\n");
    printf("Tiempo de simulacion : %.4f s\n", t);
    if (c.ticks > 0)
        printf("Tiempo por tick      : %.4f ms\n", 1000.0 * t / c.ticks);
    printf("Celdas procesadas/s  : %.3e\n",
           c.ticks > 0 ? ((double)m->n * 3.0 * c.ticks) / (t > 0 ? t : 1e-9) : 0.0);
    if (flog) {
        fprintf(flog, "# Tiempo de simulacion: %.4f s\n", t);
        fclose(flog);
        printf("Resultados escritos en: %s\n", c.ruta_log);
    }

    mundo_liberar(m);
    return 0;
}
