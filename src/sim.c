/* ============================================================================
 * sim.c - Bucle de ticks.
 *
 * Detalle de rendimiento importante: la region `omp parallel` envuelve al
 * bucle de ticks COMPLETO, no a cada bucle interno. Con T ticks y 3 fases de
 * 3 barridos cada una, la version ingenua (`#pragma omp parallel for` en cada
 * bucle) pagaria 9*T fork/join; esta version paga exactamente 1. La
 * coordinacion entre etapas se consigue con las barreras implicitas de
 * `omp for` y `omp single`, que son mucho mas baratas.
 * ==========================================================================*/
#include <stdio.h>

#ifdef _OPENMP
#  include <omp.h>
#else
#  include <time.h>
#endif

#include "ecosim.h"

/* ------------------------------------------------------------------------ */
static double ahora(void)
{
#ifdef _OPENMP
    return omp_get_wtime();
#else
    return (double)clock() / (double)CLOCKS_PER_SEC;
#endif
}

/* ------------------------------------------------------------------------ */
static void reportar(FILE *f, int tick, const Mundo *m, int mostrar_grid)
{
    fprintf(f, "== Tick %d ==\n", tick);
    fprintf(f, "Plantas: %ld\n",    m->nP);
    fprintf(f, "Herbivoros: %ld\n", m->nH);
    fprintf(f, "Carnivoros: %ld\n", m->nC);
    if (mostrar_grid) {
        fprintf(f, "Distribucion:\n");
        mundo_imprimir(f, m);
    }
    fprintf(f, "\n");
}

/* ------------------------------------------------------------------------ */
double simular(Mundo *m, const Config *c, FILE *flog)
{
    const int mostrar = (c->mostrar_grid == 1) ||
                        (c->mostrar_grid < 0 && m->W <= 80 && m->H <= 40);
    double t0;

    /* Estado inicial (tick 0) antes de que empiece la simulacion. */
    m->nP = m->nH = m->nC = 0;
    {
        int i;
        for (i = 0; i < m->n; i++) {
            switch (m->a[i].tipo) {
                case PLANTA:    m->nP++; break;
                case HERBIVORO: m->nH++; break;
                case CARNIVORO: m->nC++; break;
                default: break;
            }
        }
    }
    reportar(stdout, 0, m, mostrar);
    if (flog) reportar(flog, 0, m, mostrar);

    t0 = ahora();

#ifdef _OPENMP
#pragma omp parallel
#endif
    {
        int  tick, i;
        long lP, lH, lC;   /* acumuladores privados: evitan el false sharing
                            * de un arreglo contador[hilo] sin padding */

        for (tick = 1; tick <= c->ticks; tick++) {

            /* ---- Fase 1: plantas ---------------------------------------- */
            fase_plantas(m, c, tick);
#ifdef _OPENMP
#pragma omp single
#endif
            mundo_swap(m);

            /* ---- Fase 2: herbivoros ------------------------------------- */
            fase_agentes(m, c, tick, HERBIVORO);
#ifdef _OPENMP
#pragma omp single
#endif
            mundo_swap(m);

            /* ---- Fase 3: carnivoros ------------------------------------- */
            fase_agentes(m, c, tick, CARNIVORO);
#ifdef _OPENMP
#pragma omp single
#endif
            {
                mundo_swap(m);
                m->nP = m->nH = m->nC = 0;
            }

            /* ---- Censo de poblacion (reduccion manual) ------------------ */
            lP = lH = lC = 0;
#ifdef _OPENMP
#pragma omp for schedule(static) nowait
#endif
            for (i = 0; i < m->n; i++) {
                switch (m->a[i].tipo) {
                    case PLANTA:    lP++; break;
                    case HERBIVORO: lH++; break;
                    case CARNIVORO: lC++; break;
                    default: break;
                }
            }
#ifdef _OPENMP
#pragma omp atomic
#endif
            m->nP += lP;
#ifdef _OPENMP
#pragma omp atomic
#endif
            m->nH += lH;
#ifdef _OPENMP
#pragma omp atomic
#endif
            m->nC += lC;

#ifdef _OPENMP
#pragma omp barrier
#pragma omp single
#endif
            {
                if (c->cada > 0 && (tick % c->cada == 0 || tick == c->ticks)) {
                    reportar(stdout, tick, m, mostrar);
                    if (flog) reportar(flog, tick, m, mostrar);
                }
            }
        }
    }

    return ahora() - t0;
}
