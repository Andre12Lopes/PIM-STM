#include <assert.h>
#include <barrier.h>
#include <defs.h>
#include <mram.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

#include <random.h>
#include <tm.h>

BARRIER_INIT(my_barrier, NR_TASKLETS);

#define TRANSFER 2
#define ACCOUNT_V 1000
#define N_TRANSACTIONS 1000

#ifndef N_ACCOUNTS
#define N_ACCOUNTS 1000
#endif

#include "metrics.h"

typedef struct
{
    uint32_t number;
    uint32_t balance;
} account_t;

#ifdef DATA_IN_MRAM
account_t __mram_noinit bank[N_ACCOUNTS];
uint32_t __mram_noinit arr[2500];
#else
account_t bank[N_ACCOUNTS];
uint32_t arr[2500];
#endif

#ifdef TX_IN_MRAM
Tx __mram_noinit tx_mram[NR_TASKLETS];
#else
Tx tx_wram[NR_TASKLETS];
#endif

void
initialize_accounts();
void
check_total();

int
main()
{
    TYPE Tx *tx;
    int tid;
    uint32_t ra, rb;
    uint32_t a, b;
    uint64_t s;

    s = (uint64_t)me();
    tid = me();

#ifndef TX_IN_MRAM
    tx = &tx_wram[tid];
#else
    tx = &tx_mram[tid];
#endif

    TM_INIT(tx, tid);

    initialize_accounts();

    start_count(tid);

    for (int i = 0; i < N_TRANSACTIONS; ++i)
    {
        TM_START(tx);

        for (int j = 0; j < 0; ++j)
        {
            TM_LOAD_LOOP(tx, &arr[(tid * 100) + i]);
        }

        if (tx->status == 4)
        {
            continue;
        }

        for (int j = 0; j < 1; ++j)
        {
            ra = RAND_R_FNC(s) % N_ACCOUNTS;
            rb = RAND_R_FNC(s) % N_ACCOUNTS;

            a = TM_LOAD_LOOP(tx, &bank[ra].balance);
            a -= TRANSFER;
            TM_STORE_LOOP(tx, &bank[ra].balance, a);

            b = TM_LOAD_LOOP(tx, &bank[rb].balance);
            b += TRANSFER;
            TM_STORE_LOOP(tx, &bank[rb].balance, b);

        }

        if (tx->status == 4)
        {
            continue;
        }
        
        TM_COMMIT(tx);
    }

    get_metrics(tx, tid);

    check_total();

    return 0;
}

void
initialize_accounts()
{
    if (me() == 0)
    {
        for (int i = 0; i < N_ACCOUNTS; ++i)
        {

            bank[i].number = i + 1;
            bank[i].balance = ACCOUNT_V;
        }
    }
}

void
check_total()
{
    if (me() == 0)
    {
        // printf("[");
        unsigned int total = 0;
        for (int i = 0; i < N_ACCOUNTS; ++i)
        {
            // printf("%u -> %u | ", bank[i].number, bank[i].balance);
            total += bank[i].balance;
        }
        // printf("]\n");

        // printf("TOTAL = %u\n", total);

        assert(total == (N_ACCOUNTS * ACCOUNT_V));
    }
}
