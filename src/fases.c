#include <string.h>

#include "ecosim.h"
#include "rng.h"

static inline void vecinos8(int idx, int W, int H, int out[8])
{
    const int x = idx % W;
    const int y = idx / W;
    int dx, dy, k = 0;

    for (dy = -1; dy <= 1; dy++) {
        const int ny = (y + dy + H) % H;
        for (dx = -1; dx <= 1; dx++) {
            if (dx == 0 && dy == 0) continue;
            out[k++] = ny * W + (x + dx + W) % W;
        }
    }
}

static inline int elegir_vecino(const Celda *A, const int v[8],
                                unsigned char t, uint32_t *st)
{
    int elegido = -1, vistos = 0, k;

    for (k = 0; k < 8; k++) {
        if (A[v[k]].tipo != t) continue;
        vistos++;
        *st = rng_siguiente(*st);
        if (*st % (uint32_t)vistos == 0u) elegido = v[k];
    }
    return elegido;
}

static inline int hay_vecino(const Celda *A, const int v[8], unsigned char t)
{
    int k;
    for (k = 0; k < 8; k++) if (A[v[k]].tipo == t) return 1;
    return 0;
}

static inline int cuenta_vecinos(const Celda *A, const int v[8], unsigned char t)
{
    int k, c = 0;
    for (k = 0; k < 8; k++) if (A[v[k]].tipo == t) c++;
    return c;
}

/* ------------------------------------------------------------------------ */
static inline Celda celda_vacia(void)
{
    Celda x; memset(&x, 0, sizeof x); x.tipo = VACIO; return x;
}

static inline Celda celda_nueva(unsigned char tipo)
{
    Celda x; memset(&x, 0, sizeof x); x.tipo = tipo; return x;
}

typedef struct {
    Celda ag;
    int   vivo;
    int   reproduce;
} Resultado;

static inline Resultado avanzar(Celda ag, int comio, int puede_reproducir,
                                const Config *c, int tipo)
{
    const int gan        = (tipo == HERBIVORO) ? c->gan_h        : c->gan_c;
    const int e_repro    = (tipo == HERBIVORO) ? c->e_repro_h    : c->e_repro_c;
    const int hambre_max = (tipo == HERBIVORO) ? c->hambre_max_h : c->hambre_max_c;
    const int edad_max   = (tipo == HERBIVORO) ? c->edad_max_h   : c->edad_max_c;
    Resultado r;

    ag.edad = (short)(ag.edad + 1);

    if (comio) {
        ag.energia = (short)(ag.energia + gan);
        ag.hambre  = 0;
    } else {
        ag.hambre = (short)(ag.hambre + 1);
    }

    r.vivo      = 1;
    r.reproduce = 0;

    if (ag.hambre > hambre_max || ag.edad > edad_max) {
        r.vivo = 0;                      /* inanicion o vejez */
    } else if (puede_reproducir && ag.energia >= e_repro) {
        r.reproduce = 1;                 /* deja una cria en la celda de origen */
        ag.energia  = (short)(ag.energia - e_repro);
    }

    r.ag = ag;
    return r;
}

/* ========================================================================
 * FASE 1: PLANTAS  (expansion vegetativa y muerte por hacinamiento)
 * ====================================================================== */
void fase_plantas(Mundo *m, const Config *c, int tick)
{
    const int        n = m->n, W = m->W, H = m->H;
    const Celda     *A = m->a;
    Celda           *B = m->b;
    Propuesta       *P = m->prop;
    int             *G = m->ganador;
    int i, v[8];

    /* --- 1. PROPONER ---------------------------------------------------- */
#ifdef _OPENMP
#pragma omp for schedule(runtime) private(v)
#endif
    for (i = 0; i < n; i++) {
        uint32_t st;

        P[i].dst   = -1;
        P[i].clave = 0;

        if (A[i].tipo != PLANTA) continue;
        if (rng_u01(rng32(c->semilla, tick, FASE_PLANTA, 0, i)) >= c->p_repro_planta)
            continue;

        vecinos8(i, W, H, v);
        st = rng32(c->semilla, tick, FASE_PLANTA, 1, i);

        P[i].dst   = elegir_vecino(A, v, VACIO, &st);
        P[i].clave = rng32(c->semilla, tick, FASE_PLANTA, 2, i) | 1u;
    }

    /* --- 2. RESOLVER (gather sobre la celda destino) --------------------- */
#ifdef _OPENMP
#pragma omp for schedule(runtime) private(v)
#endif
    for (i = 0; i < n; i++) {
        uint32_t mejor = 0;
        int      gan = -1, k;

        G[i] = -1;
        if (A[i].tipo != VACIO) continue;

        vecinos8(i, W, H, v);
        for (k = 0; k < 8; k++) {
            const int s = v[k];
            if (P[s].dst != i) continue;
            if (P[s].clave > mejor || (P[s].clave == mejor && (gan < 0 || s < gan))) {
                mejor = P[s].clave;
                gan   = s;
            }
        }
        G[i] = gan;
    }

    /* --- 3. APLICAR ------------------------------------------------------ */
#ifdef _OPENMP
#pragma omp for schedule(runtime) private(v)
#endif
    for (i = 0; i < n; i++) {
        const Celda cur = A[i];

        if (cur.tipo == VACIO) {
            B[i] = (G[i] >= 0) ? celda_nueva(PLANTA) : cur;
        } else if (cur.tipo == PLANTA) {
            vecinos8(i, W, H, v);
            if (cuenta_vecinos(A, v, PLANTA) == 8) {
                B[i] = celda_vacia();          /* muere por falta de espacio */
            } else {
                B[i] = cur;
                B[i].edad = (short)(cur.edad + 1);
            }
        } else {
            B[i] = cur;                        /* herbivoros y carnivoros intactos */
        }
    }
}

void fase_agentes(Mundo *m, const Config *c, int tick, int tipo)
{
    const int            n = m->n, W = m->W, H = m->H;
    const Celda         *A = m->a;
    Celda               *B = m->b;
    Propuesta           *P = m->prop;
    int                 *G = m->ganador;
    const unsigned char  presa = (tipo == HERBIVORO) ? PLANTA : HERBIVORO;
    const int            fase  = (tipo == HERBIVORO) ? FASE_HERB : FASE_CARN;
    int i, v[8];

    /* --- 1. PROPONER ---------------------------------------------------- */
#ifdef _OPENMP
#pragma omp for schedule(runtime) private(v)
#endif
    for (i = 0; i < n; i++) {
        uint32_t st;
        int      dst;

        P[i].dst   = -1;
        P[i].clave = 0;

        if (A[i].tipo != (unsigned char)tipo) continue;

        vecinos8(i, W, H, v);
        st = rng32(c->semilla, tick, fase, 1, i);

        if (tipo == HERBIVORO && hay_vecino(A, v, CARNIVORO)) {
            /* Huir tiene prioridad sobre comer. */
            dst = elegir_vecino(A, v, VACIO, &st);
        } else {
            dst = elegir_vecino(A, v, presa, &st);      /* cazar / pastar */
            if (dst < 0) dst = elegir_vecino(A, v, VACIO, &st);  /* explorar */
        }

        P[i].dst   = dst;
        P[i].clave = rng32(c->semilla, tick, fase, 2, i) | 1u;
    }

    /* --- 2. RESOLVER ----------------------------------------------------- */
#ifdef _OPENMP
#pragma omp for schedule(runtime) private(v)
#endif
    for (i = 0; i < n; i++) {
        const unsigned char td = A[i].tipo;
        uint32_t mejor = 0;
        int      gan = -1, k;

        G[i] = -1;
        if (td != VACIO && td != presa) continue;

        vecinos8(i, W, H, v);
        for (k = 0; k < 8; k++) {
            const int s = v[k];
            if (P[s].dst != i) continue;
            if (P[s].clave > mejor || (P[s].clave == mejor && (gan < 0 || s < gan))) {
                mejor = P[s].clave;
                gan   = s;
            }
        }
        G[i] = gan;
    }

    /* --- 3. APLICAR ------------------------------------------------------ */
#ifdef _OPENMP
#pragma omp for schedule(runtime)
#endif
    for (i = 0; i < n; i++) {
        const Celda cur = A[i];
        const int   gan = G[i];
        Resultado   r;

        if (gan >= 0) {
            /* Llega un agente desde `gan`. Si la celda tenia presa, se la come. */
            r = avanzar(A[gan], (cur.tipo == presa), 1, c, tipo);
            B[i] = r.vivo ? r.ag : celda_vacia();
            continue;
        }

        if (cur.tipo == (unsigned char)tipo) {
            const int dst = P[i].dst;

            if (dst >= 0 && G[dst] == i) {
                r = avanzar(cur, (A[dst].tipo == presa), 1, c, tipo);
                B[i] = (r.vivo && r.reproduce) ? celda_nueva((unsigned char)tipo)
                                               : celda_vacia();
            } else {
                /* Se queda: no come, envejece y pasa hambre. */
                r = avanzar(cur, 0, 0, c, tipo);
                B[i] = r.vivo ? r.ag : celda_vacia();
            }
            continue;
        }

        B[i] = cur;   /* celda vacia no reclamada, presa no depredada, otra especie */
    }
}
