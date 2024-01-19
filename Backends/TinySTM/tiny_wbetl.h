#ifndef _TINY_WBETL_H_
#define _TINY_WBETL_H_

static inline int
stm_wbetl_validate(TYPE stm_tx_t *tx)
{
    TYPE r_entry_t *r;
    stm_word_t l;

    /* Validate reads */
    r = tx->r_set.entries;
    for (int i = tx->r_set.nb_entries; i > 0; i--, r++)
    {
        /* Read lock */
        l = ATOMIC_LOAD_LOCK(r->lock);

        /* Unlocked and still the same version? */
        if (LOCK_GET_OWNED(l))
        {
            /* Do we own the lock? */
            w_entry_t *w = (w_entry_t *)LOCK_GET_ADDR(l);
            /* Simply check if address falls inside our write set (avoids non-faulting
             * load) */
            if (!(tx->w_set.entries <= w && w < tx->w_set.entries + tx->w_set.nb_entries))
            {
                /* Locked by another transaction: cannot validate */
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
stm_wbetl_extend(TYPE stm_tx_t *tx)
{
    stm_word_t now;

    /* Get current time */
    now = GET_CLOCK;
    /* No need to check clock overflow here. The clock can exceed up to MAX_THREADS and it
     * will be reset when the quiescence is reached. */

    tx->start_validation = perfcounter_config(COUNT_CYCLES, false);
    /* Try to validate read set */
    if (stm_wbetl_validate(tx))
    {
        /* It works: we can extend until now */
        tx->validation_cycles += perfcounter_get() - tx->start_validation;

        tx->end = now;
        return 1;
    }
    return 0;
}

static inline void
stm_wbetl_rollback(TYPE stm_tx_t *tx)
{
    TYPE w_entry_t *w;
    int i;

    assert(IS_ACTIVE(tx->status));

    /* Drop locks */
    i = tx->w_set.nb_entries;
    if (i > 0)
    {
        w = tx->w_set.entries;
        for (; i > 0; i--, w++)
        {
            if (w->next == NULL)
            {
                /* Only drop lock for last covered address in write set */
                ATOMIC_STORE_LOCK(w->lock, LOCK_SET_TIMESTAMP(w->version));
            }
        }
        /* Make sure that all lock releases become visible */
        ATOMIC_B_WRITE;
    }
}

/*
 * Load a word-sized value (invisible read).
 */
static inline stm_word_t
stm_wbetl_read_invisible(TYPE stm_tx_t *tx, volatile TYPE_ACC stm_word_t *addr)
{
    volatile TYPE_LT stm_word_t *lock;
    stm_word_t l, l2, value, version;
    TYPE r_entry_t *r;
    TYPE w_entry_t *w;

    /* Get reference to lock */
    lock = GET_LOCK(addr);

    /* Note: we could check for duplicate reads and get value from read set */

    /* Read lock, value, lock */
restart:
    l = ATOMIC_LOAD_LOCK(lock);

restart_no_load:
    if (LOCK_GET_WRITE(l))
    {
        /* Locked */
        /* Do we own the lock? */
        w = (TYPE w_entry_t *)LOCK_GET_ADDR(l);
        /* Simply check if address falls inside our write set (avoids non-faulting load)
         */
        if (tx->w_set.entries <= w && w < tx->w_set.entries + tx->w_set.nb_entries)
        {
            /* Yes: did we previously write the same address? */
            while (1)
            {
                if (addr == w->addr)
                {
                    /* Yes: get value from write set (or from memory if mask was empty) */
                    value = (w->mask == 0 ? ATOMIC_LOAD_VALUE_MRAM(addr) : w->value);
                    break;
                }

                if (w->next == NULL)
                {
                    /* No: get value from memory */
                    value = ATOMIC_LOAD_VALUE_MRAM(addr);

                    break;
                }

                w = w->next;
            }
            /* No need to add to read set (will remain valid) */
            return value;
        }

        stm_rollback(tx, STM_ABORT_RW_CONFLICT);

        return 0;
    }
    else
    {
        /* Not locked */
        value = ATOMIC_LOAD_VALUE_MRAM(addr);
        l2 = ATOMIC_LOAD_LOCK(lock);
        if (l != l2)
        {
            l = l2;
            goto restart_no_load;
        }

        version = LOCK_GET_TIMESTAMP(l);

        /* Valid version? */
        if (version > tx->end)
        {
            /* No: try to extend first (except for read-only transactions: no read set) */
            if (tx->read_only || !stm_wbetl_extend(tx))
            {
                /* Not much we can do: abort */
                stm_rollback(tx, STM_ABORT_VAL_READ);

                return 0;
            }

            /* Verify that version has not been overwritten (read value has not
             * yet been added to read set and may have not been checked during
             * extend) */
            l2 = ATOMIC_LOAD_LOCK(lock);
            if (l != l2)
            {
                l = l2;
                goto restart_no_load;
            }
            /* Worked: we now have a good version (version <= tx->end) */
        }
    }

    /* We have a good version: add to read set (update transactions) and return value */

    if (!tx->read_only)
    {
        /* Add address and version to read set */
        if (tx->r_set.nb_entries == tx->r_set.size)
        {
            stm_allocate_rs_entries(tx, 1);
        }

        r = &tx->r_set.entries[tx->r_set.nb_entries++];
        r->version = version;
        r->lock = lock;
    }

return_value:
    return value;
}

static inline stm_word_t
stm_wbetl_read(TYPE stm_tx_t *tx, volatile TYPE_ACC stm_word_t *addr)
{

    return stm_wbetl_read_invisible(tx, addr);
}

static inline TYPE w_entry_t *
stm_wbetl_write(TYPE stm_tx_t *tx, volatile TYPE_ACC stm_word_t *addr, stm_word_t value,
                stm_word_t mask)
{
    volatile TYPE_LT stm_word_t *lock;
    stm_word_t l, l1, version;
    TYPE w_entry_t *w;
    TYPE w_entry_t *prev = NULL;

    /* Get reference to lock */
    lock = GET_LOCK(addr);

    /* Try to acquire lock */
restart:
    l = ATOMIC_LOAD_LOCK(lock);
restart_no_load:
    if (LOCK_GET_OWNED(l))
    {
        /* Locked */
        /* Do we own the lock? */
        w = (TYPE w_entry_t *)LOCK_GET_ADDR(l);
        /* Simply check if address falls inside our write set (avoids non-faulting load)
         */
        if (tx->w_set.entries <= w && w < tx->w_set.entries + tx->w_set.nb_entries)
        {
            /* Yes */
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
                    /* No need to add to write set */
                    if (mask != ~(stm_word_t)0)
                    {
                        if (prev->mask == 0)
                        {
                            prev->value = ATOMIC_LOAD(addr);
                        }

                        value = (prev->value & ~mask) | (value & mask);
                    }

                    prev->value = value;
                    prev->mask |= mask;
                    return prev;
                }

                if (prev->next == NULL)
                {
                    /* Remember last entry in linked list (for adding new entry) */
                    break;
                }
                prev = prev->next;
            }

            /* Get version from previous write set entry (all entries in linked list have
             * same version) */
            version = prev->version;

            /* Must add to write set */
            if (tx->w_set.nb_entries == tx->w_set.size)
            {
                stm_rollback(tx, STM_ABORT_EXTEND_WS);
            }

            w = &tx->w_set.entries[tx->w_set.nb_entries];
            goto do_write;
        }

        stm_rollback(tx, STM_ABORT_WW_CONFLICT);
        return NULL;
    }
    /* Not locked */
    /* Handle write after reads (before CAS) */
    version = LOCK_GET_TIMESTAMP(l);
    ATOMIC_B_WRITE;
acquire:
    if (version > tx->end)
    {
        /* We might have read an older version previously */
        if (stm_has_read(tx, lock) != NULL)
        {
            /* Read version must be older (otherwise, tx->end >= version) */
            /* Not much we can do: abort */
            stm_rollback(tx, STM_ABORT_VAL_WRITE);
            return NULL;
        }
    }

    /* Acquire lock (ETL) */
    if (tx->w_set.nb_entries == tx->w_set.size)
    {
        stm_rollback(tx, STM_ABORT_EXTEND_WS);
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
        /* Remember new value */
        if (mask != ~(stm_word_t)0)
        {
            value = (ATOMIC_LOAD(addr) & ~mask) | (value & mask);
        }

        w->value = value;
    }

    w->version = version;
    w->next = NULL;

    if (prev != NULL)
    {
        /* Link new entry in list */
        prev->next = w;
    }

    tx->w_set.nb_entries++;
    tx->w_set.has_writes++;

    return w;
}

static inline int
stm_wbetl_commit(TYPE stm_tx_t *tx)
{
    TYPE w_entry_t *w;
    stm_word_t t;
    int i;
    perfcounter_t s_time;

    /* Update transaction */
    /* Get commit timestamp (may exceed VERSION_MAX by up to MAX_THREADS) */
    t = FETCH_INC_CLOCK;

    s_time = perfcounter_config(COUNT_CYCLES, false);
    /* Try to validate (only if a concurrent transaction has committed since tx->start) */
    if (tx->start != t - 1 && !stm_wbetl_validate(tx))
    {
        /* Cannot commit */
        stm_rollback(tx, STM_ABORT_VALIDATE);
        return 0;
    }
    tx->total_commit_validation_cycles += perfcounter_get() - s_time;

    /* Install new versions, drop locks and set new timestamp */
    w = tx->w_set.entries;
    for (i = tx->w_set.nb_entries; i > 0; i--, w++)
    {
        if (w->mask != 0)
        {
            ATOMIC_STORE_VALUE(w->addr, w->value);
        }

        /* Only drop lock for last covered address in write set */
        if (w->next == NULL)
        {
            ATOMIC_STORE_REL(w->lock, LOCK_SET_TIMESTAMP(t));
        }
    }

end:
    return 1;
}

#endif /* _TINY_WBETL_H_ */
