#ifndef _TINY_WTETL_H_
#define _TINY_WTETL_H_

#include <assert.h>
#include <stdio.h>

static inline void
stm_wtetl_add_to_rs(TYPE stm_tx_t *tx, stm_word_t version, volatile TYPE_LT stm_word_t *lock)
{
    TYPE r_entry_t *r;

    /* No need to add to read set for read-only transaction */
    if (tx->read_only)
    {
        return;
    }

    /* Add address and version to read set */
    if (tx->r_set.nb_entries == tx->r_set.size)
    {
        stm_allocate_rs_entries(tx, 1);
    }

    r = &tx->r_set.entries[tx->r_set.nb_entries++];
    r->version = version;
    r->lock = lock;
}

static inline int
stm_wtetl_validate(TYPE stm_tx_t *tx)
{
    TYPE r_entry_t *r;
    int i;
    stm_word_t l;

    /* Validate reads */
    r = tx->r_set.entries;
    for (i = tx->r_set.nb_entries; i > 0; i--, r++)
    {
        /* Read lock */
        l = ATOMIC_LOAD_LOCK(r->lock);

        if (LOCK_GET_WRITE(l))
        {
            /* Entry is locked */
            w_entry_t *w = (w_entry_t *)LOCK_GET_ADDR(l);
            /* Simply check if address falls inside our write set (avoids
             * non-faulting load) */
            if (!(tx->w_set.entries <= w && w < tx->w_set.entries + tx->w_set.nb_entries))
            {
                return 0;
            }

            /* We own the lock: OK */
        }
        else
        {
            if (LOCK_GET_TIMESTAMP(l) != r->version)
            {
                /* Other version: cannot validate */
                return 0;
            }

            /* Same version: OK */
        }
    }

    return 1;
}

/*
 * Extend snapshot range.
 */
static inline int
stm_wtetl_extend(TYPE stm_tx_t *tx)
{
    stm_word_t now;

    /* Get current time */
    now = GET_CLOCK;
    /* No need to check clock overflow here. The clock can exceed up to
     * MAX_THREADS and it will be reset when the quiescence is reached. */

    tx->start_validation = perfcounter_config(COUNT_CYCLES, false);
    /* Try to validate read set */
    if (stm_wtetl_validate(tx))
    {
        /* It works: we can extend until now */
        tx->validation_cycles += perfcounter_get() - tx->start_validation;

        tx->end = now;
        return 1;
    }

    return 0;
}

static inline void
stm_wtetl_rollback(TYPE stm_tx_t *tx)
{
    TYPE w_entry_t *w;
    stm_word_t t;

    assert(IS_ACTIVE(tx->status));

    t = 0;
    /* Undo writes and drop locks */
    w = tx->w_set.entries;
    for (int i = tx->w_set.nb_entries; i > 0; i--, w++)
    {
        stm_word_t j;
        /* Restore previous value */
        if (w->mask != 0)
        {
            ATOMIC_STORE_VALUE(w->addr, w->value);
        }

        if (w->next != NULL)
        {
            continue;
        }

        /* Incarnation numbers allow readers to detect dirty reads */
        j = LOCK_GET_INCARNATION(w->version) + 1;
        if (j > INCARNATION_MAX)
        {
            /* Simple approach: write new version (might trigger unnecessary aborts) */
            if (t == 0)
            {
                /* Get new version (may exceed VERSION_MAX by up to MAX_THREADS) */
                t = FETCH_INC_CLOCK;
            }

            ATOMIC_STORE_REL(w->lock, LOCK_SET_TIMESTAMP(t));
        }
        else
        {
            /* Use new incarnation number */
            ATOMIC_STORE_REL(w->lock, LOCK_UPD_INCARNATION(w->version, j));
        }
    }

    /* Make sure that all lock releases become visible */
    ATOMIC_B_WRITE;
}

static inline stm_word_t
stm_wtetl_read(TYPE stm_tx_t *tx, volatile TYPE_ACC stm_word_t *addr)
{
    volatile TYPE_LT stm_word_t *lock_addr;
    stm_word_t l1;
    stm_word_t l2;
    stm_word_t value, version;
    w_entry_t *w;

    // Read lock
    lock_addr = GET_LOCK(addr);
    l1 = ATOMIC_LOAD_LOCK(lock_addr);

restart_no_load:
    if (!LOCK_GET_WRITE(l1))
    {
    	tx->start_xyz = perfcounter_config(COUNT_CYCLES, false);
        /* Address not locked */
        value = ATOMIC_LOAD_VALUE_MRAM(addr);
        l2 = ATOMIC_LOAD_LOCK(lock_addr);

        if (l1 != l2)
        {
            l1 = l2;
            goto restart_no_load;
        }
 
	
	version = LOCK_GET_TIMESTAMP(l1);

        stm_wtetl_add_to_rs(tx, version, lock_addr);

        if (version > tx->end)
        {
            /* The version read is invalid, we can try to extend the snapshot*/
            if (tx->read_only || !stm_wtetl_extend(tx))
            {
                /* If the transaction is read only, or the snapshot can not be
                extended we have to abort */
                stm_rollback(tx, STM_ABORT_VAL_READ);
                return 0;
            }
            /* Worked: we now have a good version (version <= tx->end) */
        }

    	tx->xyz += perfcounter_get() - tx->start_xyz;

        return value;
    }
    else
    {
        /* Address locked */
        /* Do we own the lock? */
        w = (w_entry_t *)LOCK_GET_ADDR(l1);

        /* Simply check if address falls inside our write set (avoids
         * non-faulting load) */
        if (tx->w_set.entries <= w && w < tx->w_set.entries + tx->w_set.nb_entries)
        {
            /* Yes: we have a version locked by us that was valid at write time
             */
            value = ATOMIC_LOAD_VALUE_MRAM(addr);

            /* No need to add to read set (will remain valid) */
            return value;
        }

        stm_rollback(tx, STM_ABORT_RW_CONFLICT);

        return 0;
    }
}

static inline TYPE w_entry_t *
stm_wtetl_write(TYPE stm_tx_t *tx, volatile TYPE_ACC stm_word_t *addr, stm_word_t value,
                stm_word_t mask)
{
    volatile TYPE_LT stm_word_t *lock;
    stm_word_t l, l1, version;
    TYPE w_entry_t *w;
    TYPE w_entry_t *prev = NULL;

    assert(IS_ACTIVE(tx->status));

    /* Get reference to lock */
    lock = GET_LOCK(addr);

    /* Try to acquire lock */
restart:
    l = ATOMIC_LOAD_LOCK(lock);

    if (LOCK_GET_OWNED(l))
    {
        /* Locked */
        /* Do we own the lock? */
        w = (TYPE w_entry_t *)LOCK_GET_ADDR(l);
        /* Simply check if address falls inside our write set (avoids
         * non-faulting load) */
        if (tx->w_set.entries <= w && w < tx->w_set.entries + tx->w_set.nb_entries)
        {
            /* We own the lock */
            if (mask == 0)
            {
                /* No need to insert new entry or modify existing one */
                return w;
            }

            prev = w;
            /* Did we previously write the same address? */
            while (1)
            {
                if (addr == prev->addr)
                {
                    if (w->mask == 0)
                    {
                        /* Remember old value */
                        w->value = ATOMIC_LOAD(addr);
                        w->mask = mask;
                    }

                    /* Yes: only write to memory */
                    if (mask != ~(stm_word_t)0)
                    {
                        value = (ATOMIC_LOAD(addr) & ~mask) | (value & mask);
                    }

                    ATOMIC_STORE_VALUE(addr, value);
                    return w;
                }

                if (prev->next == NULL)
                {
                    /* Remember last entry in linked list (for adding new entry)
                     */
                    break;
                }
                prev = prev->next;
            }

            /* Must add to write set */
            if (tx->w_set.nb_entries == tx->w_set.size)
            {
                stm_rollback(tx, STM_ABORT_EXTEND_WS);
                exit(1);
            }

            w = &tx->w_set.entries[tx->w_set.nb_entries];
            /* Get version from previous write set entry (all entries in linked
             * list have same version) */
            w->version = prev->version;
            goto do_write;
        }

        /* Address is locked and we do not own the lock */
        stm_rollback(tx, STM_ABORT_WW_CONFLICT);
        return NULL;
    }

    /* Not locked */
    /* Handle write after reads (before CAS) */
    version = LOCK_GET_TIMESTAMP(l);
    if (version > tx->end)
    {
        if (stm_has_read(tx, lock) != NULL)
        {
            /* Read version must be older (otherwise, tx->end >= version) */
            /* Not much we can do: abort */
            stm_rollback(tx, STM_ABORT_VAL_WRITE);
            return NULL;
        }
    }

    if (tx->w_set.nb_entries == tx->w_set.size)
    {
        stm_rollback(tx, STM_ABORT_EXTEND_WS);
        exit(1);
    }

    w = &tx->w_set.entries[tx->w_set.nb_entries];

    acquire(lock);

    l1 = ATOMIC_LOAD_LOCK(lock);

    if (l != l1)
    {
        release(lock);
        goto restart;
    }

    ATOMIC_STORE_LOCK(lock, LOCK_SET_ADDR_WRITE((stm_word_t)w));

    release(lock);

    /* We store the old value of the lock (timestamp and incarnation) */
    // w->version = l;
    w->version = LOCK_GET_TIMESTAMP(l); // TODO: disregarding incarnation
    /* We own the lock here (ETL) */
do_write:
    /* Add address to write set */
    w->addr = addr;
    w->mask = mask;
    w->lock = lock;

    if (mask == 0)
    {
        /* Do not write anything */
    }
    else
    {
        /* Remember old value */
        w->value = ATOMIC_LOAD(addr);
    }

    if (mask != 0)
    {
        if (mask != ~(stm_word_t)0)
        {
            value = (w->value & ~mask) | (value & mask);
        }

        ATOMIC_STORE_VALUE(addr, value);
    }

    w->next = NULL;
    if (prev != NULL)
    {
        /* Link new entry in list */
        prev->next = w;
    }

    tx->w_set.nb_entries++;

    return w;
}

static inline int
stm_wtetl_commit(TYPE stm_tx_t *tx)
{
    TYPE w_entry_t *w;
    stm_word_t t;
    int i;
    perfcounter_t s_time;

    /* Get commit timestamp (may exceed VERSION_MAX by up to MAX_THREADS) */
    t = FETCH_INC_CLOCK;

    s_time = perfcounter_config(COUNT_CYCLES, false);
    /* Try to validate (only if a concurrent transaction has committed since tx->start) */
    if (tx->start != t - 1 && !stm_wtetl_validate(tx))
    {
        /* Cannot commit */
        stm_rollback(tx, STM_ABORT_VALIDATE);
        return 0;
    }
    tx->total_commit_validation_cycles += perfcounter_get() - s_time;

    /* Make sure that the updates become visible before releasing locks */
    ATOMIC_B_WRITE;
    /* Drop locks and set new timestamp */
    w = tx->w_set.entries;
    for (i = tx->w_set.nb_entries; i > 0; i--, w++)
    {
        if (w->next == NULL)
        {

            /* No need for CAS (can only be modified by owner transaction) */
            ATOMIC_STORE_LOCK(w->lock, LOCK_SET_TIMESTAMP(t));
        }
    }

    /* Make sure that all lock releases become visible */
    /* TODO: is ATOMIC_MB_WRITE required? */
    ATOMIC_B_WRITE;

    return 1;
}

#endif /* _TINY_WTETL_H_ */
