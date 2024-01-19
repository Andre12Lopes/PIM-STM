#ifndef _RANDOM_H_
#define _RANDOM_H_

#define RAND_R_FNC(seed)                                                                 \
    ({                                                                                   \
        uint64_t next = (seed);                                                          \
        uint64_t result;                                                                 \
        next *= 1103515245;                                                              \
        next += 12345;                                                                   \
        result = (uint64_t)(next >> 16) & (2048 - 1);                                    \
        next *= 1103515245;                                                              \
        next += 12345;                                                                   \
        result <<= 10;                                                                   \
        result ^= (uint64_t)(next >> 16) & (1024 - 1);                                   \
        next *= 1103515245;                                                              \
        next += 12345;                                                                   \
        result <<= 10;                                                                   \
        result ^= (uint64_t)(next >> 16) & (1024 - 1);                                   \
        (seed) = next;                                                                   \
        result; /* returns result */                                                     \
    })

#endif /* _RANDOM_H_ */
