#include <assert.h>
#include <barrier.h>
#include <defs.h>
#include <perfcounter.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

#include <random.h>
#include <tm.h>

#include "linked_list.h"

#ifndef UPDATE_PERCENTAGE
#define UPDATE_PERCENTAGE 0
#endif

#define SET_INITIAL_SIZE 10
#define RAND_RANGE 100
#define N_TRANSACTIONS 100

#include "metrics.h"

__mram_ptr intset_t *set;

#ifdef TX_IN_MRAM
Tx __mram_noinit tx_mram[NR_TASKLETS];
#else
Tx tx_wram[NR_TASKLETS];
#endif

void
test(TYPE Tx *tx, __mram_ptr intset_t *set, uint64_t *seed, int *last);

int
main()
{
    TYPE Tx *tx;
    int val;
    int tid;
    uint64_t seed;
    int i = 0;
    int last = -1;

    seed = me();
    tid = me();

#ifndef TX_IN_MRAM
    tx = &tx_wram[tid];
#else
    tx = &tx_mram[tid];
#endif

    TM_INIT(tx, tid);

    if (tid == 0)
    {
        set = set_new(INIT_SET_PARAMETERS);

        while (i < SET_INITIAL_SIZE)
        {
            val = (RAND_R_FNC(seed) % RAND_RANGE) + 1;
            if (set_add(NULL, set, val, 0))
            {
                i++;
            }
        }
    }

    start_count(tid);

    for (int i = 0; i < N_TRANSACTIONS; ++i)
    {
        test(tx, set, &seed, &last);
    }

    get_metrics(tx, tid);

    return 0;
}

void
test(TYPE Tx *tx, __mram_ptr intset_t *set, uint64_t *seed, int *last)
{
    int val, op;

    op = RAND_R_FNC(*seed) % 100;
    if (op < UPDATE_PERCENTAGE)
    {
        if (*last < 0)
        {
            /* Add random value */
            val = (RAND_R_FNC(*seed) % RAND_RANGE) + 1;
            if (set_add(tx, set, val, 1))
            {
                *last = val;
            }
        }
        else
        {
            /* Remove last value */
            set_remove(tx, set, *last);
            *last = -1;
        }
    }
    else
    {
        /* Look for random value */
        val = (RAND_R_FNC(*seed) % RAND_RANGE) + 1;
        set_contains(tx, set, val);
        // if (set_contains(tx, set, val))
        // {
        //     printf("FOUND!!\n");
        // }
        // else
        // {
        //     printf("NOT FOUND!!\n");
        // }
    }
}
