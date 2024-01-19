#ifndef _TM_H_
#define _TM_H_

#include <tiny.h>

typedef stm_tx_t Tx;

#define TM_INIT(tx, tid) stm_init(tx, tid)

#define TM_START(tx)                                                                     \
    do                                                                                   \
    {                                                                                    \
        stm_start(tx);

#define TM_LOAD(tx, addr)                                                                \
    stm_load(tx, addr);                                                                  \
    if ((tx)->status == 4)                                                               \
    {                                                                                    \
        continue;                                                                        \
    }

#define TM_LOAD_LOOP(tx, addr)                                                           \
    stm_load(tx, addr);                                                                  \
    if ((tx)->status == 4)                                                               \
    {                                                                                    \
        break;                                                                           \
    }

#define TM_STORE(tx, addr, value)                                                        \
    stm_store(tx, addr, value);                                                          \
    if ((tx)->status == 4)                                                               \
    {                                                                                    \
        continue;                                                                        \
    }

#define TM_STORE_LOOP(tx, addr, value)                                                   \
    stm_store(tx, addr, value);                                                          \
    if ((tx)->status == 4)                                                               \
    {                                                                                    \
        break;                                                                           \
    }

#define TM_RESTART(tx)                                                                   \
    stm_abort(tx);                                                                       \
    continue;

#define TM_RESTART_LOOP(tx)                                                              \
    stm_abort(tx);                                                                       \
    break;

#define TM_COMMIT(tx)                                                                    \
    stm_commit(tx);                                                                      \
    if ((tx)->status != 4)                                                               \
    {                                                                                    \
        break;                                                                           \
    }                                                                                    \
    }                                                                                    \
    while (1)

#endif /* _TM_H_ */
