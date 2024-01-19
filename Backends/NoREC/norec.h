#ifndef _NOREC_H_
#define _NOREC_H_

#include <perfcounter.h>
#include <stdint.h>

#ifdef TX_IN_MRAM
#define TYPE __mram_ptr
#else
#define TYPE
#endif

#ifdef DATA_IN_MRAM
#define TYPE_ACC __mram_ptr
#else
#define TYPE_ACC
#endif

/* Initial size of read sets */
#define R_SET_SIZE 300

/* Initial size of write sets */
#define W_SET_SIZE 300

typedef int BitMap;

typedef struct r_entry
{
    volatile TYPE_ACC uintptr_t *Addr;
    uintptr_t Valu;
} r_entry_t;

typedef struct r_set
{
    r_entry_t entries[R_SET_SIZE]; /* Array of entries */
    unsigned int nb_entries;       /* Number of entries */
    unsigned int size;             /* Size of array */
} r_set_t;

typedef struct w_entry
{
    volatile TYPE_ACC uintptr_t *Addr;
    uintptr_t Valu;
    long Ordinal;
} w_entry_t;

typedef struct w_set
{
    w_entry_t entries[W_SET_SIZE]; /* Array of entries */
    unsigned int nb_entries;       /* Number of entries */
    unsigned int size;             /* Size of array */
    BitMap BloomFilter;            /* Address exclusion fast-path test */
} w_set_t;

typedef struct _Thread
{
    w_set_t wrSet;
    r_set_t rdSet;
    uint64_t rng;
    uint32_t Retries;
    long snapshot;
    long status;
    int UniqID;
    uint32_t Starts;
    uint64_t aborts;
    perfcounter_t time;
    perfcounter_t start_time;
    perfcounter_t start_read;
    perfcounter_t start_write;
    perfcounter_t start_validation;
    uint32_t process_cycles;
    uint32_t read_cycles;
    uint32_t write_cycles;
    uint32_t validation_cycles;
    uint32_t total_read_cycles;
    uint32_t total_write_cycles;
    uint32_t total_validation_cycles;
    uint32_t total_commit_validation_cycles;
    uint32_t commit_cycles;
    uint32_t total_cycles;
} Thread;

void
TxAbort(TYPE Thread *);

void
TxInit(TYPE Thread *t, int id);

void
TxStart(TYPE Thread *);

uintptr_t
TxLoad(TYPE Thread *, volatile TYPE_ACC uintptr_t *);

void
TxStore(TYPE Thread *, volatile TYPE_ACC uintptr_t *, uintptr_t);

int
TxCommit(TYPE Thread *);

#endif
