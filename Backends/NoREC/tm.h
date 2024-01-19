#ifndef _TM_H_
#define _TM_H_

#include <norec.h>

typedef Thread Tx;

#define TM_INIT(tx, tid) TxInit(tx, tid)

#define TM_START(tx)                                                                     \
    do                                                                                   \
    {                                                                                    \
        TxStart(tx);

#define TM_LOAD(tx, addr)                                                                \
    TxLoad(tx, addr);                                                                    \
    if ((tx)->status == 4)                                                               \
    {                                                                                    \
        continue;                                                                        \
    }

#define TM_LOAD_LOOP(tx, addr)                                                           \
    TxLoad(tx, addr);                                                                    \
    if ((tx)->status == 4)                                                               \
    {                                                                                    \
        break;                                                                           \
    }

#define TM_STORE(tx, addr, value)                                                        \
    TxStore(tx, addr, value);                                                            \
    if ((tx)->status == 4)                                                               \
    {                                                                                    \
        continue;                                                                        \
    }

#define TM_STORE_LOOP(tx, addr, value)                                                   \
    TxStore(tx, addr, value);                                                            \
    if ((tx)->status == 4)                                                               \
    {                                                                                    \
        break;                                                                           \
    }

#define TM_RESTART(tx)                                                                   \
    TxAbort(tx);                                                                         \
    continue;

#define TM_RESTART_LOOP(tx)                                                              \
    TxAbort(tx);                                                                         \
    break;

#define TM_COMMIT(tx)                                                                    \
    TxCommit(tx);                                                                        \
    if ((tx)->status != 4)                                                               \
    {                                                                                    \
        break;                                                                           \
    }                                                                                    \
    }                                                                                    \
    while (1)

#endif /* _TM_H_ */
