#ifndef _TINY_WBCTL_H_
#define _TINY_WBCTL_H_

static int
stm_wbctl_validate(TYPE stm_tx_t *tx)
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
        /* Unlocked and still the same version? */
        if (LOCK_GET_OWNED(l))
        {
            /* Do we own the lock? */
            TYPE w_entry_t *w = (TYPE w_entry_t *)LOCK_GET_ADDR(l);

            /* Simply check if address falls inside our write set (avoids non-faulting
             * load) */
            if (!(tx->w_set.entries <= w && w < tx->w_set.entries + tx->w_set.nb_entries))
            {
                /* Locked by another transaction: cannot validate */
                return 0;
            }

            /* We own the lock: OK */
            if (w->version != r->version)
            {
                /* Other version: cannot validate */
                return 0;
            }
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
static int
stm_wbctl_extend(TYPE stm_tx_t *tx)
{
    stm_word_t now;

    /* Get current time */
    now = GET_CLOCK;
    /* No need to check clock overflow here. The clock can exceed
       up to MAX_THREADS and it will be reset when the quiescence is reached. */

    tx->start_validation = perfcounter_config(COUNT_CYCLES, false);

    /* Try to validate read set */
    if (stm_wbctl_validate(tx))
    {
        /* It works: we can extend until now */
        tx->validation_cycles += perfcounter_get() - tx->start_validation;

        tx->end = now;
        return 1;
    }

    return 0;
}

static void
stm_wbctl_rollback(TYPE stm_tx_t *tx)
{
    TYPE w_entry_t *w;

    if (tx->w_set.nb_acquired > 0)
    {
        w = tx->w_set.entries + tx->w_set.nb_entries;
        do
        {
            w--;
            if (!w->no_drop)
            {
                if (--tx->w_set.nb_acquired == 0)
                {
                    /* Make sure that all lock releases become visible to other threads */
                    ATOMIC_STORE_REL(w->lock, LOCK_SET_TIMESTAMP(w->version));
                }
                else
                {
                    ATOMIC_STORE_LOCK(w->lock, LOCK_SET_TIMESTAMP(w->version));
                }
            }
        } while (tx->w_set.nb_acquired > 0);
    }
}

static stm_word_t
stm_wbctl_read(TYPE stm_tx_t *tx, volatile TYPE_ACC stm_word_t *addr)
{
    volatile TYPE_LT stm_word_t *lock;
    stm_word_t l, l2, value, version;
    TYPE r_entry_t *r;
    TYPE w_entry_t *written = NULL;

    /* Did we previously write the same address? */
    written = stm_has_written(tx, addr);
    if (written != NULL)
    {
        /* Yes: get value from write set if possible */
        if (written->mask == ~(stm_word_t)0)
        {
            value = written->value;
            /* No need to add to read set */
            return value;
        }
    }

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
        /* Spin while locked (should not last long) */
        goto restart;
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

        /* Check timestamp */
        version = LOCK_GET_TIMESTAMP(l);
        /* Valid version? */
        if (version > tx->end)
        {
            /* No: try to extend first (except for read-only transactions: no read set) */
            if (tx->read_only || !stm_wbctl_extend(tx))
            {
                /* Not much we can do: abort */
                stm_rollback(tx, STM_ABORT_VAL_READ);
                return 0;
            }
            /* Verify that version has not been overwritten (read value has not
             * yet been added to read set and may have not been checked during
             * extend) */
            l = ATOMIC_LOAD_LOCK(lock);
            if (l != l2)
            {
                l = l2;
                goto restart_no_load;
            }
            /* Worked: we now have a good version (version <= tx->end) */
        }
    }
    /* We have a good version: add to read set (update transactions) and return value */

    /* Did we previously write the same address? */
    if (written != NULL)
    {
        value = (value & ~written->mask) | (written->value & written->mask);
        /* Must still add to read set */
    }

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

    return value;
}

static TYPE w_entry_t *
stm_wbctl_write(TYPE stm_tx_t *tx, volatile TYPE_ACC stm_word_t *addr, stm_word_t value,
                stm_word_t mask)
{
    volatile TYPE_LT stm_word_t *lock;
    stm_word_t l, version;
    TYPE w_entry_t *w;

    /* Get reference to lock */
    lock = GET_LOCK(addr);

    /* Try to acquire lock */
restart:
    l = ATOMIC_LOAD_LOCK(lock);
restart_no_load:
    if (LOCK_GET_OWNED(l))
    {
        /* Locked */
        /* Spin while locked (should not last long) */
        goto restart;
    }

    /* Not locked */
    w = stm_has_written(tx, addr);
    if (w != NULL)
    {
        w->value = (w->value & ~mask) | (value & mask);
        w->mask |= mask;

        return w;
    }

    /* Handle write after reads (before CAS) */
    version = LOCK_GET_TIMESTAMP(l);

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
    /* We own the lock here (ETL) */
do_write:
    /* Add address to write set */
    if (tx->w_set.nb_entries == tx->w_set.size)
    {
        stm_allocate_ws_entries(tx, 1);
    }

    w = &tx->w_set.entries[tx->w_set.nb_entries++];
    w->addr = addr;
    w->mask = mask;
    w->lock = lock;

    if (mask != 0)
    {
        /* Remember new value */
        w->value = value;
    }

    w->no_drop = 1;

    return w;
}

static int
stm_wbctl_commit(TYPE stm_tx_t *tx)
{
    TYPE w_entry_t *w;
    stm_word_t t;
    int i;
    stm_word_t l, l1;
    perfcounter_t s_time;

    /* Acquire locks (in reverse order) */
    w = tx->w_set.entries + tx->w_set.nb_entries;
    do
    {
        w--;
        /* Try to acquire lock */
    restart:
        l = ATOMIC_LOAD_LOCK(w->lock);
        if (LOCK_GET_OWNED(l))
        {
            /* Do we already own the lock? */
            if (tx->w_set.entries <= (w_entry_t *)LOCK_GET_ADDR(l) &&
                (w_entry_t *)LOCK_GET_ADDR(l) < tx->w_set.entries + tx->w_set.nb_entries)
            {
                /* Yes: ignore */
                continue;
            }

            /* Abort self */
            stm_rollback(tx, STM_ABORT_WW_CONFLICT);
            return 0;
        }

        acquire(w->lock);

        l1 = ATOMIC_LOAD_LOCK(w->lock);

        if (l != l1)
        {
            release(w->lock);
            goto restart;
        }

        ATOMIC_STORE_LOCK(w->lock, LOCK_SET_ADDR_WRITE((stm_word_t)w));

        release(w->lock);

        /* We own the lock here */
        w->no_drop = 0;

        /* Store version for validation of read set */
        w->version = LOCK_GET_TIMESTAMP(l);
        tx->w_set.nb_acquired++;
    } while (w > tx->w_set.entries);

    /* Get commit timestamp (may exceed VERSION_MAX by up to MAX_THREADS) */
    t = FETCH_INC_CLOCK;

    s_time = perfcounter_config(COUNT_CYCLES, false);
    /* Try to validate (only if a concurrent transaction has committed since tx->start) */
    if (tx->start != t - 1 && !stm_wbctl_validate(tx))
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
        if (w->mask == ~(stm_word_t)0)
        {
            ATOMIC_STORE_VALUE(w->addr, w->value);
        }

        /* Only drop lock for last covered address in write set (cannot be "no drop") */
        if (!w->no_drop)
        {
            ATOMIC_STORE_REL(w->lock, LOCK_SET_TIMESTAMP(t));
        }
    }

end:
    return 1;
}

#endif /* _TINY_WBCTL_H_ */
