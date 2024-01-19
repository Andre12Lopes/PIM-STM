#ifndef _UTILS_H_
#define _UTILS_H_

#define UNS(a) ((uintptr_t)(a))

#define PAUSE()

/* =============================================================================
 * Non-faulting load
 * =============================================================================
 */
#define LDNF(a) (*((volatile TYPE_ACC uintptr_t *)a))

/* =============================================================================
 * Memory Barriers
 *
 * http://mail.nl.linux.org/kernelnewbies/2002-11/msg00127.html
 * =============================================================================
 */
#define MEMBARLDLD() /* nothing */
#define MEMBARSTST() /* nothing */
#define MEMBARSTLD() __asm__ __volatile__("" : : : "memory")

static inline void
acquire(volatile long *addr)
{
    __asm__ __volatile__("acquire %[p], 0, nz, ." : : [p] "r"(addr) :);
}

static inline void
release(volatile long *addr)
{
    __asm__ __volatile__("release %[p], 0, nz, .+1" : : [p] "r"(addr) :);
}

#endif
