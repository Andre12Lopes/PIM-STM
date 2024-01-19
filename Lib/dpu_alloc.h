#ifndef _DPU_ALLOC_H_
#define _DPU_ALLOC_H_

#include <stddef.h>

__mram_ptr void *
mram_malloc(size_t size);

void
mram_free(__mram_ptr void *ptr);

#endif /* _DPU_ALLOC_H_ */
