#ifndef _TINY_H_
#define _TINY_H_

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
{                              /* Read set entry */
    stm_word_t version;        /* Version read */
    volatile TYPE_LT stm_word_t *lock; /* Pointer to lock (for fast access) */
} r_entry_t;

typedef struct r_set
{ /* Read set */                   /* Array of entries */
    r_entry_t entries[R_SET_SIZE]; /* Array of entries */
    uint16_t nb_entries;            /* Number of entries */
    uint16_t size;                  /* Size of array */
} r_set_t;

typedef struct w_entry
{                                       /* Write set entry */
    volatile TYPE_ACC stm_word_t *addr; /* Address written */
    stm_word_t value;                   /* New/old value */
    stm_word_t mask;                    /* Write mask */
    stm_word_t version;                 /* Version overwritten */
    volatile TYPE_LT stm_word_t *lock;          /* Pointer to lock (for fast access) */
    TYPE struct w_entry *next;          /* Next address covered by same lock (if any) */
    uint8_t no_drop;                    /* Should we drop lock upon abort? */
} w_entry_t __attribute__((aligned(16)));

typedef struct w_set
{                                  /* Array of entries */
    w_entry_t entries[W_SET_SIZE]; /* Array of entries */
    uint16_t nb_entries;            /* Number of entries */
    uint16_t size;                  /* Size of array */
    uint8_t has_writes;
    uint8_t nb_acquired; /* WRITE_BACK_CTL: Number of locks acquired */
} w_set_t;

typedef struct stm_tx
{
    volatile stm_word_t status; /* Transaction status */
    stm_word_t start;           /* Start timestamp */
    stm_word_t end;             /* End timestamp (validity range) */
    r_set_t r_set;              /* Read set */
    w_set_t w_set;              /* Write set */
    uint8_t read_only;
    uint8_t TID;
    uint64_t rng;
    uint32_t retries;
    uint64_t aborts;
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
    uint32_t abort_cycles;
    uint32_t xyz;
    uint32_t total_xyz;
} stm_tx_t;

enum
{
    /**
     * Abort upon writing a memory location being written by another
     * transaction.
     */
    STM_ABORT_WW_CONFLICT = (1 << 6) | (0x04 << 8), // 1088
    /**
     * Abort upon read due to failed validation.
     */
    STM_ABORT_VAL_READ = (1 << 6) | (0x05 << 8), // 1344
    /**
     * Abort upon writing a memory location being read by another
     * transaction.
     */
    STM_ABORT_RW_CONFLICT = (1 << 6) | (0x02 << 8), // 576
    /**
     * Abort upon write due to failed validation.
     */
    STM_ABORT_VAL_WRITE = (1 << 6) | (0x06 << 8), // 1600
    /**
     * Abort upon commit due to failed validation.
     */
    STM_ABORT_VALIDATE = (1 << 6) | (0x07 << 8),
    /**
     * Abort due to reaching the write set size limit.
     */
    STM_ABORT_EXTEND_WS = (1 << 6) | (0x0C << 8)
};

void
stm_init(TYPE struct stm_tx *tx, int tid);

void
stm_start(TYPE struct stm_tx *tx);

/* TODO: Read does not deal well with floating point operations */
/* TODO: Change return type */
stm_word_t
stm_load(TYPE struct stm_tx *tx, volatile TYPE_ACC stm_word_t *addr);

void
stm_store(TYPE struct stm_tx *tx, volatile TYPE_ACC stm_word_t *addr, stm_word_t value);

void
stm_abort(TYPE stm_tx_t *tx);

int
stm_commit(TYPE struct stm_tx *tx);

#endif /* _TINY_H_ */
