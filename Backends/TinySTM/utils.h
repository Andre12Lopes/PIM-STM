#ifndef _UTILS_H_
#define _UTILS_H_

#include <stdio.h>
#include <stdlib.h>

#ifdef DEBUG
/* Note: stdio is thread-safe */
#define IO_FLUSH fflush(NULL)
#define PRINT_DEBUG(...)                                                                 \
    printf(__VA_ARGS__);                                                                 \
    fflush(NULL)
#else /* ! DEBUG */
#define IO_FLUSH
#define PRINT_DEBUG(...)
#endif /* ! DEBUG */

#ifndef CACHELINE_SIZE
/* It ensures efficient usage of cache and avoids false sharing.
 * It could be defined in an architecture specific file. */
#define CACHELINE_SIZE 64 /* TODO: Change to DPU */
#endif

#define PAUSE()

static inline void *
xrealloc(void *addr, size_t size)
{
    (void)addr;
    (void)size;
    // addr = realloc(addr, size);
    // if (addr == NULL)
    // {
    //     perror("realloc");
    //     exit(1);
    // }
    // return addr;
    return NULL;
}

static inline void *
xmalloc_aligned(size_t size)
{
    (void)size;
    // void *memptr;

    // if (posix_memalign(&memptr, CACHELINE_SIZE, size))
    // {
    //     memptr = NULL;
    // }

    // if (memptr == NULL)
    // {
    //     fprintf(stderr, "Error allocating aligned memory\n");
    //     exit(1);
    // }

    // return memptr;
    return NULL;
}

#endif /* !_UTILS_H_ */
