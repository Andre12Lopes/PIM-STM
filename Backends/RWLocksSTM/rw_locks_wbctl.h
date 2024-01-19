#ifndef _RW_LOCKS_WBCTL_H_
#define _RW_LOCKS_WBCTL_H_

#include <assert.h>
#include <stdio.h>

static inline void
stm_wbctl_rollback(TYPE stm_tx *tx)
{
    TYPE w_entry_t *w;
    TYPE r_entry_t *r;

    w = tx->w_set.entries;
    for (uint16_t i = tx->w_set.nb_entries; i > 0; i--, w++)
    {
        // *(w->addr) = w->value;

        if (w->no_drop)
        {
            continue;
        }

        *(w->lock) = 0x00;
    }

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

static inline TYPE r_entry_t *
stm_has_read_lock(TYPE stm_tx *tx, volatile TYPE_LT stm_word_t *lock)
{
    TYPE r_entry_t *r;

    r = tx->r_set.entries;
    for (uint16_t i = tx->r_set.nb_entries; i > 0; i--, r++)
    {
        if (r->lock == lock && r->dropped == 0)
        {
            return r;
        }
    }

    return NULL;
}

static inline stm_word_t
stm_wbctl_read(TYPE stm_tx *tx, volatile TYPE_ACC stm_word_t *addr)
{
    volatile TYPE_LT stm_word_t *lock = NULL;
    TYPE r_entry_t *r = NULL;
    TYPE w_entry_t *w = NULL;
    stm_word_t lock_value;
    stm_word_t value;

    /* Did we previously write the same address? */
    w = stm_has_written(tx, addr);
    if (w != NULL)
    {
        value = w->value;

        return value;
    }

    lock = GET_LOCK_ADDR(addr);

restart:
    hardware_acquire_lock(lock);
    lock_value = *lock;
    if (LOCK_GET_OWNED_WRITE(lock_value))
    {
        hardware_release_lock(lock);

        goto restart;
    }

    if (LOCK_GET_OWNED_READ(lock_value))
    {
        if (LOCK_GET_ME_OWNED_READ(lock_value, tx->tid))
        {
            hardware_release_lock(lock);
            return *addr;
        }

        *lock = LOCK_INC_N_READERS(lock_value, tx->tid);
    }
    else
    {
        *lock = LOCK_SET_READ(tx->tid);
    }

    hardware_release_lock(lock);

    assert(tx->r_set.nb_entries < R_SET_SIZE);

    r = &tx->r_set.entries[tx->r_set.nb_entries++];
    r->lock = lock;
    r->dropped = 0;

    return *addr;
}

static inline TYPE w_entry_t *
stm_wbctl_write(TYPE stm_tx *tx, volatile TYPE_ACC stm_word_t *addr, stm_word_t value)
{
    volatile TYPE_LT stm_word_t *lock = NULL;
    // TYPE r_entry_t *r = NULL;
    TYPE w_entry_t *w = NULL;
    // TYPE w_entry_t *prev = NULL;
    stm_word_t lock_value;

    lock = GET_LOCK_ADDR(addr);

restart:
    // hardware_acquire_lock(lock);
    lock_value = *lock;
    if (LOCK_GET_OWNED_WRITE(lock_value))
    {
        // hardware_release_lock(lock);
        goto restart;
    }
    // hardware_release_lock(lock);

    if (LOCK_GET_OWNED_READ(lock_value))
    {
        if (LOCK_GET_N_READERS(lock_value) > 1)
        {
            stm_rollback(tx);
            return NULL;
        }
    }

    w = stm_has_written(tx, addr);
    if (w != NULL)
    {
        w->value = value;

        return w;
    }

    assert(tx->w_set.nb_entries < W_SET_SIZE);

    w = &tx->w_set.entries[tx->w_set.nb_entries++];
    assert(((uintptr_t)w & 0x03) == 0x00);

    w->lock = lock;
    w->addr = addr;
    w->value = value;
    w->no_drop = 1;

    return w;
}

static inline void
stm_wbctl_commit(TYPE stm_tx *tx)
{
    TYPE w_entry_t *w;
    TYPE w_entry_t *lock_w;
    TYPE r_entry_t *r;
    perfcounter_t s_time;
    stm_word_t lock_value;

    w = tx->w_set.entries + tx->w_set.nb_entries;

    do
    {
        w--;
        /* Try to acquire lock */
        hardware_acquire_lock(w->lock);

        lock_value = *(w->lock);
        if (LOCK_GET_OWNED_WRITE(lock_value))
        {
            lock_w = (TYPE w_entry_t *)LOCK_GET_W_ENTRY(lock_value);
            if (tx->w_set.entries <= lock_w && 
                lock_w < tx->w_set.entries + tx->w_set.nb_entries)
            {
                hardware_release_lock(w->lock);
                continue;
            }

            hardware_release_lock(w->lock);
            stm_rollback(tx);
            return;
        }

        if (LOCK_GET_OWNED_READ(lock_value))
        {
            if (LOCK_GET_N_READERS(lock_value) == 1 &&
                LOCK_GET_ME_OWNED_READ(lock_value, tx->tid))
            {
                r = stm_has_read_lock(tx, w->lock);
                r->dropped = 1;
                *(w->lock) = 0x00;
            }
            else
            {
                hardware_release_lock(w->lock);
                // printf("# 2\n");
                stm_rollback(tx);
                return;
            }
        }

        *(w->lock) = LOCK_SET_ADDR_WRITE((stm_word_t)w);

        hardware_release_lock(w->lock);

        w->no_drop = 0;
    } while (w > tx->w_set.entries);

    s_time = perfcounter_config(COUNT_CYCLES, false);
    tx->total_commit_validation_cycles += perfcounter_get() - s_time;

    w = tx->w_set.entries;
    for (uint16_t i = tx->w_set.nb_entries; i > 0; i--, w++)
    {
        *(w->addr) = w->value;

        if (!w->no_drop)
        {
            *(w->lock) = 0x00;
        }
    }

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

#endif /* _RW_LOCKS_WBCTL_H_ */
