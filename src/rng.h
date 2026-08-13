#ifndef RNG_H
#define RNG_H

#include <stdint.h>

/* Mezclador de bits de splitmix32: avalancha completa, 4 operaciones. */
static inline uint32_t rng_mezcla(uint32_t x)
{
    x ^= x >> 16; x *= 0x7FEB352Du;
    x ^= x >> 15; x *= 0x846CA68Bu;
    x ^= x >> 16;
    return x;
}

static inline uint32_t rng32(unsigned semilla, int tick, int fase,
                             int flujo, int idx)
{
    uint32_t h = rng_mezcla((uint32_t)semilla ^ 0x9E3779B9u);
    h = rng_mezcla(h ^ ((uint32_t)tick  * 0x85EBCA6Bu));
    h = rng_mezcla(h ^ ((uint32_t)fase  * 0x000001F1u) ^ ((uint32_t)flujo * 0x00000B79u));
    h = rng_mezcla(h ^ ((uint32_t)idx   * 0xC2B2AE35u));
    return h;
}

/* Avance local barato, para consumir varios valores dentro de una celda. */
static inline uint32_t rng_siguiente(uint32_t x)
{
    x ^= x << 13; x ^= x >> 17; x ^= x << 5;
    return x ? x : 0x1234567u;   /* xorshift32 no puede pasar por 0 */
}

/* Uniforme en [0,1). */
static inline double rng_u01(uint32_t r)
{
    return (double)(r >> 8) * (1.0 / 16777216.0);
}

#endif /* RNG_H */
