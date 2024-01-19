#include <stdio.h>
#include <assert.h>
#include <mram.h>

#include <defs.h>

#include "dpu_alloc.h"

uintptr_t memory_ptr[24];

__mram_ptr void *
mram_malloc(size_t size)
{
    assert(DPU_MRAM_HEAP_POINTER < (__mram_ptr void *)0x8FFFFF);

    int tid = me();
    uintptr_t block;

    if (size <= 0)
    {
        assert(0);
        return NULL;
    }

    if (memory_ptr[tid] + size >=
        (uintptr_t)((((((tid + 1) * 2) + 8) << 20) | 0xFFFFF) + 1))
    {
        assert(0);
        return NULL;
    }

    if (!memory_ptr[tid])
    {
        memory_ptr[tid] = (uintptr_t)(((((tid * 2) + 8) << 20) | 0xFFFFF) + 1);
    }

    block = memory_ptr[tid];

    memory_ptr[tid] += size;
    
    return (__mram_ptr void *)block;
}

void
mram_free(__mram_ptr void *ptr)
{
    (void)ptr;
}
