#ifndef _RW_LOCKS_H_
#define _RW_LOCKS_H_

#include <perfcounter.h>
#include <stdint.h>

#ifdef TX_IN_MRAM
#define TYPE __mram_ptr
#else
#define TYPE
#endif

#ifdef LT_IN_MRAM
#define TYPE_LT __mram_ptr
#define TYPE_LT_DEF __mram
#else
#define TYPE_LT
#define TYPE_LT_DEF
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

typedef uintptr_t stm_word_t;

typedef struct r_entry
{
    volatile TYPE_LT stm_word_t *lock; /* Pointer to lock (for fast access) */
    uint8_t dropped;
} r_entry_t;

typedef struct r_set
{
    r_entry_t entries[R_SET_SIZE]; /* Array of entries */
    uint16_t nb_entries;           /* Number of entries */
} r_set_t;

typedef struct w_entry
{
    volatile TYPE_LT stm_word_t *lock;          /* Pointer to lock (for fast access) */
    volatile TYPE_ACC stm_word_t *addr; /* Address written */
    stm_word_t value;                   /* New (write-back) value */
    TYPE struct w_entry *next;          /* Next address covered by same lock */
    uint8_t no_drop;                    /* Should we drop lock upon abort? */
} w_entry_t;

typedef struct w_set
{                                  /* Write set */
    w_entry_t entries[W_SET_SIZE]; /* Array of entries */
    uint16_t nb_entries;           /* Number of entries */
} w_set_t;

typedef struct _stm_tx
{
    volatile stm_word_t status; /* Transaction status */
    r_set_t r_set;              /* Read set */
    w_set_t w_set;              /* Write set */
    uint8_t tid;
    uint64_t rng;
    uint32_t retries;
    uint64_t aborts;
    // Variables to measure performance
    perfcounter_t time;
    perfcounter_t start_time;
    perfcounter_t start_read;
    perfcounter_t start_write;
    perfcounter_t start_validation;
    perfcounter_t start_xyz;
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
    uint32_t xyz;
    uint32_t total_xyz;
} stm_tx;

void
stm_init(TYPE stm_tx *tx, int tid);

void
stm_start(TYPE stm_tx *tx);

stm_word_t
stm_load(TYPE stm_tx *tx, TYPE_ACC stm_word_t *addr);

void
stm_store(TYPE stm_tx *tx, TYPE_ACC stm_word_t *addr, stm_word_t value);

void
stm_abort(TYPE stm_tx *tx);

int
stm_commit(TYPE stm_tx *tx);

#endif /* _RW_LOCKS_H_ */
