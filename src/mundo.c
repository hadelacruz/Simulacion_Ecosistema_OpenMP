#include <stdlib.h>
#include <string.h>

#include "ecosim.h"
#include "rng.h"

/* ------------------------------------------------------------------------ */
Mundo *mundo_crear(const Config *c)
{
    Mundo *m = (Mundo *)calloc(1, sizeof(Mundo));
    if (!m) return NULL;

    m->W = c->W;
    m->H = c->H;
    m->n = c->W * c->H;

    m->a       = (Celda *)    calloc((size_t)m->n, sizeof(Celda));
    m->b       = (Celda *)    calloc((size_t)m->n, sizeof(Celda));
    m->prop    = (Propuesta *)calloc((size_t)m->n, sizeof(Propuesta));
    m->ganador = (int *)      calloc((size_t)m->n, sizeof(int));

    if (!m->a || !m->b || !m->prop || !m->ganador) {
        mundo_liberar(m);
        return NULL;
    }
    return m;
}

void mundo_liberar(Mundo *m)
{
    if (!m) return;
    free(m->a); free(m->b); free(m->prop); free(m->ganador);
    free(m);
}

void mundo_inicializar(Mundo *m, const Config *c)
{
    const double u1 = c->dens_planta;
    const double u2 = u1 + c->dens_herb;
    const double u3 = u2 + c->dens_carn;
    int i;

#ifdef _OPENMP
#pragma omp parallel for schedule(static)
#endif
    for (i = 0; i < m->n; i++) {
        uint32_t r = rng32(c->semilla, 0, 9, 0, i);
        double   u = rng_u01(r);
        Celda    x;

        memset(&x, 0, sizeof x);

        if (u < u1) {
            x.tipo = PLANTA;
        } else if (u < u2) {
            x.tipo    = HERBIVORO;
            x.energia = 0;
            x.edad    = (short)(rng32(c->semilla, 0, 9, 1, i) % 5u);
        } else if (u < u3) {
            x.tipo    = CARNIVORO;
            x.energia = 0;
            x.edad    = (short)(rng32(c->semilla, 0, 9, 2, i) % 5u);
        } else {
            x.tipo = VACIO;
        }

        m->a[i] = x;
        m->b[i] = x;
        m->prop[i].dst   = -1;
        m->prop[i].clave = 0;
        m->ganador[i]    = -1;
    }
}


void mundo_swap(Mundo *m)
{
    Celda *t = m->a;
    m->a = m->b;
    m->b = t;
}

/* ------------------------------------------------------------------------ */
void mundo_imprimir(FILE *f, const Mundo *m)
{
    static const char SIMBOLO[4] = { '.', 'P', 'H', 'C' };
    int x, y;

    for (y = 0; y < m->H; y++) {
        for (x = 0; x < m->W; x++) {
            unsigned char t = m->a[y * m->W + x].tipo;
            fputc(SIMBOLO[t & 3], f);
        }
        fputc('\n', f);
    }
}
