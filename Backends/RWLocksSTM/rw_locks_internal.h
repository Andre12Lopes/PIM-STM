#ifndef _RW_LOCKS_INTERNAL_H_
#define _RW_LOCKS_INTERNAL_H_

#include <stdio.h>
#include <stdlib.h>

#include <perfcounter.h>

#include "atomic.h"

#define LOCK_ARRAY_LOG_SIZE 20

enum
{
    TX_IDLE = 0,
    TX_ACTIVE = 1,
    TX_COMMITTED = (1 << 1),
    TX_ABORTED = (2 << 1),
    TX_COMMITTING = (1 << 1) | TX_ACTIVE,
    TX_ABORTING = (2 << 1) | TX_ACTIVE,
    TX_KILLED = (3 << 1) | TX_ACTIVE,
};

#define SET_STATUS(s, v) ((s) = (v))
#define UPDATE_STATUS(s, v) ((s) = (v))
#define GET_STATUS(s) ((s))
#define IS_ACTIVE(s) ((GET_STATUS(s) & 0x01) == TX_ACTIVE)
#define IS_ABORTED(s) ((GET_STATUS(s) & 0x04) == TX_ABORTED)

#define LOCK_ARRAY_SIZE (1 << LOCK_ARRAY_LOG_SIZE)

#define LOCK_MASK (LOCK_ARRAY_SIZE - 1)
#define LOCK_SHIFT_EXTRA 0
#define LOCK_SHIFT ((sizeof(stm_word_t) == 4 ? 2 : 3) + LOCK_SHIFT_EXTRA)
#define LOCK_IDX(a) (((stm_word_t)(a) >> LOCK_SHIFT) & LOCK_MASK)
#define GET_LOCK_ADDR(a) (&(lock_table[LOCK_IDX(a)].lock))

#define READ_BITS 1
#define WRITE_BITS 1
#define FALG_BITS 24
#define LOCK_BITS (READ_BITS + WRITE_BITS)
#define LOCK_FLAG_BITS (READ_BITS + WRITE_BITS + FALG_BITS)

#define READ_MASK 0x01
#define WRITE_MASK 0x02

#define LOCK_GET_OWNED_READ(l) ((l)&READ_MASK)

#define LOCK_GET_ME_OWNED_READ(l, id) ((l) & (0x01 << (LOCK_BITS + id)))

#define LOCK_SET_READ(id)                                                                \
    ((0x01 << LOCK_FLAG_BITS) | (0x01 << (LOCK_BITS + id)) | READ_MASK)
#define LOCK_GET_N_READERS(l) ((l) >> LOCK_FLAG_BITS)
#define LOCK_INC_N_READERS(l, id)                                                        \
    (((LOCK_GET_N_READERS(l) + 1) << LOCK_FLAG_BITS) |                                   \
     ((l & 0x3FFFFFC) | (0x01 << (LOCK_BITS + id))) | READ_MASK)
#define LOCK_DEC_N_READERS(l, id)                                                        \
    (((LOCK_GET_N_READERS(l) - 1) << LOCK_FLAG_BITS) |                                   \
     ((l & 0x3FFFFFC) & ~(0x01 << (LOCK_BITS + id))) | READ_MASK)

#define LOCK_GET_OWNED_WRITE(l) ((l) & WRITE_MASK)
#define LOCK_SET_ADDR_WRITE(a) ((a) | WRITE_MASK)

#define LOCK_GET_W_ENTRY(l) ((l) & ~(stm_word_t)(READ_MASK | WRITE_MASK))

typedef struct 
{
    stm_word_t lock;
    uint32_t padding;  
} lock_entry_t;

lock_entry_t TYPE_LT_DEF lock_table[LOCK_ARRAY_SIZE];

static void
stm_rollback(TYPE stm_tx *tx);

/*
 * Check if address has been written previously.
 */
static inline TYPE w_entry_t *
stm_has_written(TYPE stm_tx *tx, volatile TYPE_ACC stm_word_t *addr)
{
    TYPE w_entry_t *w;

    /* Look for write */
    w = tx->w_set.entries;
    for (int i = tx->w_set.nb_entries; i > 0; i--, w++)
    {
        if (w->addr == addr)
        {
            return w;
        }
    }

    return NULL;
}

#ifdef WRITE_BACK_CTL
#include "rw_locks_wbctl.h"
#endif

#ifdef WRITE_BACK_ETL
#include "rw_locks_wbetl.h"
#endif

#ifdef WRITE_THROUGH_ETL
#include "rw_locks_wtetl.h"
#endif

#include "backoff.h"

void
drop_read_locks(TYPE stm_tx *tx)
{
    TYPE r_entry_t *r;
    
    r = tx->r_set.entries;
    for (uint16_t i = tx->r_set.nb_entries; i > 0; i--, r++)
    {
        if (r->dropped)
        {
            continue;
        }

        hardware_acquire_lock(r->lock);

        if (LOCK_GET_N_READERS(*(r->lock)) == 1)
        {
            *(r->lock) = 0x00;
        }
        else
        {
            *(r->lock) = LOCK_DEC_N_READERS((*(r->lock)), tx->tid);
        }

        hardware_release_lock(r->lock);
    }
}

static inline void
int_stm_start(TYPE stm_tx *tx)
{
    if (tx->start_time == 0)
    {
        tx->start_time = perfcounter_config(COUNT_CYCLES, false);
    }
    tx->time = perfcounter_config(COUNT_CYCLES, false);

    tx->r_set.nb_entries = 0;
    tx->w_set.nb_entries = 0;

    tx->read_cycles = 0;
    tx->write_cycles = 0;
    tx->validation_cycles = 0;
    tx->xyz = 0;

    UPDATE_STATUS(tx->status, TX_ACTIVE);
}

static void
stm_rollback(TYPE stm_tx *tx)
{
    assert(IS_ACTIVE(tx->status));

    // tx->abort_cycles += perfcounter_get() - tx->time;

#if defined(WRITE_BACK_CTL)
    stm_wbctl_rollback(tx);
#elif defined(WRITE_BACK_ETL)
    stm_wbetl_rollback(tx);
#elif defined(WRITE_THROUGH_ETL)
    stm_wtetl_rollback(tx);
#endif

    tx->aborts++;

#ifdef BACKOFF
    tx->retries++;
    if (tx->retries > 3)
    { /* TUNABLE */
        backoff(tx, tx->retries);
    }
#endif

    /* Set status to ABORTED */
    SET_STATUS(tx->status, TX_ABORTED);
}

static inline stm_word_t
int_stm_load(TYPE stm_tx *tx, volatile TYPE_ACC stm_word_t *addr)
{
    stm_word_t val = -1;

    tx->start_read = perfcounter_config(COUNT_CYCLES, false);

#if defined(WRITE_BACK_CTL)
    val = stm_wbctl_read(tx, addr);
#elif defined(WRITE_BACK_ETL)
    val = stm_wbetl_read(tx, addr);
#elif defined(WRITE_THROUGH_ETL)
    val = stm_wtetl_read(tx, addr);
#endif

    tx->read_cycles += perfcounter_get() - tx->start_read;

    return val;
}

static inline void
int_stm_store(TYPE stm_tx *tx, volatile TYPE_ACC stm_word_t *addr, stm_word_t value)
{
    tx->start_write = perfcounter_config(COUNT_CYCLES, false);

#if defined(WRITE_BACK_CTL)
    stm_wbctl_write(tx, addr, value);
#elif defined(WRITE_BACK_ETL)
    stm_wbetl_write(tx, addr, value);
#elif defined(WRITE_THROUGH_ETL)
    stm_wtetl_write(tx, addr, value);
#endif

    tx->write_cycles += perfcounter_get() - tx->start_write;
}

static inline int
int_stm_commit(TYPE stm_tx *tx)
{
    uint64_t t_process_cycles;

    assert(IS_ACTIVE(tx->status));

    t_process_cycles = perfcounter_get() - tx->time;
    tx->time = perfcounter_config(COUNT_CYCLES, false);

    if (tx->w_set.nb_entries == 0)
    {
        drop_read_locks(tx);
    }
    else
    {
#if defined(WRITE_BACK_CTL)
        stm_wbctl_commit(tx);
#elif defined(WRITE_BACK_ETL)
        stm_wbetl_commit(tx);
#elif defined(WRITE_THROUGH_ETL)
        stm_wtetl_commit(tx);
#endif
    }

    if (IS_ABORTED(tx->status))
    {
        return 0;
    }

    /* Set status to COMMITTED */
    SET_STATUS(tx->status, TX_COMMITTED);

    tx->process_cycles += t_process_cycles;
    tx->commit_cycles += perfcounter_get() - tx->time;
    tx->total_cycles += perfcounter_get() - tx->start_time;
    tx->total_read_cycles += tx->read_cycles;
    tx->total_write_cycles += tx->write_cycles;
    tx->total_validation_cycles += tx->validation_cycles;
    tx->total_xyz += tx->xyz;

    tx->start_time = 0;
    tx->retries = 0;

    return 1;
}

#endif /* _RW_LOCKS_INTERNAL_H_ */
