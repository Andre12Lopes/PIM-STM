#ifndef _VECTOR_H_
#define _VECTOR_H_

#include <dpu_alloc.h>

typedef struct vector
{
    long size;
    long capacity;
    __mram_ptr intptr_t *elements;
} vector_t;

__mram_ptr vector_t *
vector_alloc(long initCapacity)
{
    __mram_ptr vector_t *vectorPtr;
    long capacity = (initCapacity < 1) ? 1 : initCapacity;

    vectorPtr = (__mram_ptr vector_t *)mram_malloc(sizeof(vector_t));

    if (vectorPtr != NULL)
    {
        vectorPtr->size = 0;
        vectorPtr->capacity = capacity;
        vectorPtr->elements =
            (__mram_ptr intptr_t *)mram_malloc(capacity * sizeof(intptr_t));

        if (vectorPtr->elements == NULL)
        {
            return NULL;
        }
    }

    return vectorPtr;
}

bool_t
vector_push_back(__mram_ptr vector_t *vectorPtr, __mram_ptr void *dataPtr)
{
    if (vectorPtr->size == vectorPtr->capacity)
    {
        long newCapacity = vectorPtr->capacity * 2;
        __mram_ptr intptr_t *newElements =
            (__mram_ptr intptr_t *)mram_malloc(newCapacity * sizeof(intptr_t));

        if (newElements == NULL)
        {
            return FALSE;
        }

        vectorPtr->capacity = newCapacity;
        for (long i = 0; i < vectorPtr->size; i++)
        {
            newElements[i] = vectorPtr->elements[i];
        }

        // mram_free(vectorPtr->elements);
        vectorPtr->elements = newElements;
    }

    vectorPtr->elements[vectorPtr->size++] = (intptr_t)dataPtr;

    return TRUE;
}

long
vector_get_size(__mram_ptr vector_t *vectorPtr)
{
    return vectorPtr->size;
}

__mram_ptr void *
vector_at(__mram_ptr vector_t *vectorPtr, long i)
{
    if ((i < 0) || (i >= vectorPtr->size)) 
    {
        return NULL;
    }

    return (__mram_ptr void *)vectorPtr->elements[i];
}

void
vector_free(__mram_ptr vector_t *vectorPtr)
{
    (void)vectorPtr;
    // mram_free(vectorPtr->elements);
    // mram_free(vectorPtr);
}

#endif /* _VECTOR_H_ */
