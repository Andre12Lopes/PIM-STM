#ifndef _ATOMIC_H_
#define _ATOMIC_H_

static inline void
hardware_acquire_lock(volatile TYPE_LT stm_word_t *addr)
{
    __asm__ __volatile__("acquire %[p], 0, nz, ." : : [p] "r"(addr) :);
}

static inline void
hardware_release_lock(volatile TYPE_LT stm_word_t *addr)
{
    __asm__ __volatile__("release %[p], 0, nz, .+1" : : [p] "r"(addr) :);
}

#endif /* _ATOMIC_H_ */
