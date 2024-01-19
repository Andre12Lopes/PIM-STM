#include <alloc.h>
#include <assert.h>
#include <perfcounter.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "norec.h"

#include "backoff.h"
#include "utils.h"

#define FILTERHASH(a) ((UNS(a) >> 2) ^ (UNS(a) >> 5))
#define FILTERBITS(a) (1 << (FILTERHASH(a) & 0x1F))

enum
{
    TX_ACTIVE = 1,
    TX_COMMITTED = 2,
    TX_ABORTED = 4
};

volatile long LOCK;

void
TxAbort(TYPE Thread *Self)
{
    Self->aborts++;

#ifdef BACKOFF
    Self->Retries++;
    if (Self->Retries > 3)
    { /* TUNABLE */
        backoff(Self, Self->Retries);
    }
#endif

    Self->status = TX_ABORTED;
}

void
TxInit(TYPE Thread *t, int id)
{
    /* CCM: so we can access NOREC's thread metadata in signal handlers */
    // pthread_setspecific(global_key_self, (void*)t);

    memset(t, 0, sizeof(*t)); /* Default value for most members */

    t->UniqID = id;
    t->rng = id + 1;
    t->Starts = 0;
    t->aborts = 0;

    t->process_cycles = 0;
    t->commit_cycles = 0;

    t->rdSet.size = R_SET_SIZE;
    t->wrSet.size = W_SET_SIZE;

    t->process_cycles = 0;
    t->read_cycles = 0;
    t->write_cycles = 0;
    t->validation_cycles = 0;
    t->total_read_cycles = 0;
    t->total_write_cycles = 0;
    t->total_validation_cycles = 0;
    t->total_commit_validation_cycles = 0;
    t->commit_cycles = 0;
    t->total_cycles = 0;
    t->start_time = 0;
    t->start_read = 0;
    t->start_write = 0;
    t->start_validation = 0;
}

// --------------------------------------------------------------

static inline void
txReset(TYPE Thread *Self)
{
    Self->rdSet.nb_entries = 0;

    Self->wrSet.nb_entries = 0;
    Self->wrSet.BloomFilter = 0;

    Self->status = TX_ACTIVE;
}

void
TxStart(TYPE Thread *Self)
{
    // Self->envPtr = envPtr;
    Self->time = perfcounter_config(COUNT_CYCLES, false);
    if (Self->start_time == 0)
    {
        Self->start_time = perfcounter_config(COUNT_CYCLES, false);
    }

    txReset(Self);

    Self->read_cycles = 0;
    Self->write_cycles = 0;
    Self->validation_cycles = 0;

    MEMBARLDLD();

    Self->Starts++;

    do
    {
        Self->snapshot = LOCK;
    } while ((Self->snapshot & 1) != 0);
}

// --------------------------------------------------------------

// returns -1 if not coherent
static inline long
ReadSetCoherent(TYPE Thread *Self)
{
    long time;
    TYPE r_entry_t *r;

    while (1)
    {
        MEMBARSTLD();
        time = LOCK;
        if ((time & 1) != 0)
        {
            continue;
        }

        r = Self->rdSet.entries;
        for (int i = Self->rdSet.nb_entries; i > 0; i--, r++)
        {
            if (r->Valu != LDNF(r->Addr))
            {
                return -1;
            }
        }

        if (LOCK == time)
        {
            break;
        }
    }

    return time;
}

uintptr_t
TxLoad(TYPE Thread *Self, volatile TYPE_ACC uintptr_t *Addr)
{
    uintptr_t Valu;
    TYPE w_entry_t *w;
    TYPE r_entry_t *r;

    Self->start_read = perfcounter_config(COUNT_CYCLES, false);

    uintptr_t msk = FILTERBITS(Addr);
    if ((Self->wrSet.BloomFilter & msk) == msk)
    {
        w = Self->wrSet.entries + (Self->wrSet.nb_entries - 1);
        for (int i = Self->wrSet.nb_entries; i > 0; i--, w--)
        {
            if (w->Addr == Addr)
            {
                return w->Valu;
            }
        }
    }

    MEMBARLDLD();
    Valu = LDNF(Addr);
    while (LOCK != Self->snapshot)
    {
        Self->start_validation = perfcounter_config(COUNT_CYCLES, false);

        long newSnap = ReadSetCoherent(Self);

        Self->validation_cycles += perfcounter_get() - Self->start_validation;

        if (newSnap == -1)
        {
            TxAbort(Self);
            return 0;
        }

        Self->snapshot = newSnap;
        MEMBARLDLD();
        Valu = LDNF(Addr);
    }

    if (Self->rdSet.nb_entries == Self->rdSet.size)
    {
        printf("[WARNING] Reached RS extend\n");
        assert(0);
    }

    r = &Self->rdSet.entries[Self->rdSet.nb_entries++];
    r->Addr = Addr;
    r->Valu = Valu;

    Self->read_cycles += perfcounter_get() - Self->start_read;

    return Valu;
}

// --------------------------------------------------------------

void
TxStore(TYPE Thread *Self, volatile TYPE_ACC uintptr_t *addr, uintptr_t valu)
{
    TYPE w_entry_t *w;

    Self->start_write = perfcounter_config(COUNT_CYCLES, false);

    Self->wrSet.BloomFilter |= FILTERBITS(addr);

    if (Self->wrSet.nb_entries == Self->wrSet.size)
    {
        printf("[WARNING] Reached WS extend\n");
        assert(0);
    }

    w = &Self->wrSet.entries[Self->wrSet.nb_entries++];
    w->Addr = addr;
    w->Valu = valu;

    Self->write_cycles += perfcounter_get() - Self->start_write;
}

// --------------------------------------------------------------

static inline void
txCommitReset(TYPE Thread *Self)
{
    txReset(Self);
    Self->Retries = 0;

    Self->status = TX_COMMITTED;
}

static inline void
WriteBackForward(TYPE Thread *Self)
{
    TYPE w_entry_t *w;

    w = Self->wrSet.entries;
    for (unsigned int i = Self->wrSet.nb_entries; i > 0; i--, w++)
    {
        *(w->Addr) = w->Valu;
    }
}

static inline long
TryFastUpdate(TYPE Thread *Self)
{
    perfcounter_t s_time;

    s_time = perfcounter_config(COUNT_CYCLES, false);

acquire:
    acquire(&LOCK);

    if (LOCK != Self->snapshot)
    {
        release(&LOCK);

        long newSnap = ReadSetCoherent(Self);
        if (newSnap == -1)
        {
            return 0; // TxAbort(Self);
        }

        Self->total_commit_validation_cycles += perfcounter_get() - s_time;

        Self->snapshot = newSnap;

        goto acquire;
    }

    LOCK = Self->snapshot + 1;

    release(&LOCK);

    {
        WriteBackForward(Self); /* write-back the deferred stores */
    }

    MEMBARSTST(); /* Ensure the above stores are visible  */
    LOCK = Self->snapshot + 2;
    MEMBARSTLD();

    return 1; /* success */
}

int
TxCommit(TYPE Thread *Self)
{
    uint64_t t_process_cycles;

    t_process_cycles = perfcounter_get() - Self->time;
    Self->time = perfcounter_config(COUNT_CYCLES, false);

    /* Fast-path: Optional optimization for pure-readers */
    if (Self->wrSet.nb_entries == 0)
    {
        txCommitReset(Self);
        Self->commit_cycles += perfcounter_get() - Self->time;
        Self->total_cycles += perfcounter_get() - Self->start_time;
        Self->process_cycles += t_process_cycles;
        Self->total_read_cycles += Self->read_cycles;
        Self->total_write_cycles += Self->write_cycles;
        Self->total_validation_cycles += Self->validation_cycles;

        Self->start_time = 0;
        return 1;
    }

    if (TryFastUpdate(Self))
    {
        txCommitReset(Self);
        Self->commit_cycles += perfcounter_get() - Self->time;
        Self->total_cycles += perfcounter_get() - Self->start_time;
        Self->process_cycles += t_process_cycles;
        Self->total_read_cycles += Self->read_cycles;
        Self->total_write_cycles += Self->write_cycles;
        Self->total_validation_cycles += Self->validation_cycles;

        Self->start_time = 0;
        return 1;
    }

    TxAbort(Self);

    return 0;
}
