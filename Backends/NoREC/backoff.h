#ifndef _BACKOFF_H_
#define _BACKOFF_H_

#include <stdint.h>

static inline uint64_t
MarsagliaXORV(uint64_t x)
{
    if (x == 0)
    {
        x = 1;
    }

    x ^= x << 6;
    x ^= x >> 21;
    x ^= x << 7;

    return x;
}

static inline uint64_t
MarsagliaXOR(TYPE uint64_t *seed)
{
    uint64_t x = MarsagliaXORV(*seed);
    *seed = x;

    return x;
}

static inline uint64_t
TSRandom(TYPE Thread *tx)
{
    return MarsagliaXOR(&tx->rng);
}

static inline void
backoff(TYPE Thread *tx, uint32_t attempt)
{
    uint64_t stall = TSRandom(tx) & 0xF;
    stall += attempt >> 2;
    stall *= 10;

    // stall = stall << attempt;

    volatile uint64_t i = 0;
    while (i++ < stall)
    {
    }
}

#endif /* _BACKOFF_H_ */
