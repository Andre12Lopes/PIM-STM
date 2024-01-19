#ifndef _RW_LOCKS_WTETL_H_
#define _RW_LOCKS_WTETL_H_

#include <assert.h>
#include <stdio.h>

static inline void
stm_wtetl_rollback(TYPE stm_tx *tx)
{
    TYPE w_entry_t *w;
    TYPE r_entry_t *r;

    w = tx->w_set.entries;
    for (uint16_t i = tx->w_set.nb_entries; i > 0; i--, w++)
    {
        *(w->addr) = w->value;

        if (w->next != NULL)
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
stm_wtetl_read(TYPE stm_tx *tx, volatile TYPE_ACC stm_word_t *addr)
{
    volatile TYPE_LT stm_word_t *lock = NULL;
    TYPE r_entry_t *r = NULL;
    TYPE w_entry_t *w = NULL;
    stm_word_t lock_value;

    lock = GET_LOCK_ADDR(addr);

    hardware_acquire_lock(lock);

    lock_value = *lock;
    if (LOCK_GET_OWNED_WRITE(lock_value))
    {
        hardware_release_lock(lock);

        w = (TYPE w_entry_t *)LOCK_GET_W_ENTRY(lock_value);
        if (tx->w_set.entries <= w && w < tx->w_set.entries + tx->w_set.nb_entries)
        {
            return *addr;
        }

        stm_rollback(tx);
        return 0;
    }
  
    tx->start_xyz = perfcounter_config(COUNT_CYCLES, false);
    if (LOCK_GET_OWNED_READ(lock_value))
    {
        // TODO...
        // if (stm_has_read_lock(tx, lock) != NULL)
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

    tx->xyz += perfcounter_get() - tx->start_xyz;

    return *addr;
}

static inline TYPE w_entry_t *
stm_wtetl_write(TYPE stm_tx *tx, volatile TYPE_ACC stm_word_t *addr, stm_word_t value)
{
    volatile TYPE_LT stm_word_t *lock = NULL;
    TYPE r_entry_t *r = NULL;
    TYPE w_entry_t *w = NULL;
    TYPE w_entry_t *prev = NULL;
    stm_word_t lock_value;

    lock = GET_LOCK_ADDR(addr);

    hardware_acquire_lock(lock);

    lock_value = *lock;
    if (LOCK_GET_OWNED_WRITE(lock_value))
    {
        hardware_release_lock(lock);

        w = (TYPE w_entry_t *)LOCK_GET_W_ENTRY(lock_value);
        if (tx->w_set.entries <= w && w < tx->w_set.entries + tx->w_set.nb_entries)
        {
            prev = w;
            while (1)
            {
                if (addr == prev->addr)
                {
                    // We onw the lock and thre is already
                    // an entry for the current addr
                    *addr = value;
                    return w;
                }

                if (prev->next == NULL)
                {
                    break;
                }
                prev = prev->next;
            }

            assert(tx->w_set.nb_entries < W_SET_SIZE);

            w = &tx->w_set.entries[tx->w_set.nb_entries++];
            goto do_write;
        }
        else
        {
            stm_rollback(tx);
            return NULL;
        }
    }

    if (LOCK_GET_OWNED_READ(lock_value))
    {
        if (LOCK_GET_N_READERS(lock_value) == 1)
        {
            r = stm_has_read_lock(tx, lock);
            if (r != NULL)
            {
                r->dropped = 1;
                *lock = 0x00;
            }
            else
            {
                hardware_release_lock(lock);
                stm_rollback(tx);
                return NULL;
            }
        }
        else
        {
            hardware_release_lock(lock);
            stm_rollback(tx);
            return NULL;
        }
    }

    assert(tx->w_set.nb_entries < W_SET_SIZE);

    w = &tx->w_set.entries[tx->w_set.nb_entries++];

    assert(((uintptr_t)w & 0x03) == 0x00);

    *lock = LOCK_SET_ADDR_WRITE((stm_word_t)w);

    hardware_release_lock(lock);

do_write:
    w->lock = lock;
    w->addr = addr;
    w->value = *addr;
    w->next = NULL;

    if (prev != NULL)
    {
        prev->next = w;
    }

    *addr = value;

    return w;
}

static inline void
stm_wtetl_commit(TYPE stm_tx *tx)
{
    TYPE w_entry_t *w;
    TYPE r_entry_t *r;
    perfcounter_t s_time;

    s_time = perfcounter_config(COUNT_CYCLES, false);
    tx->total_commit_validation_cycles += perfcounter_get() - s_time;

    w = tx->w_set.entries;
    for (uint16_t i = tx->w_set.nb_entries; i > 0; i--, w++)
    {
        if (w->next == NULL)
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

#endif /* _RW_LOCKS_WTETL_H_ */
