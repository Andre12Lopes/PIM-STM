#ifndef _QUEUE_H_
#define _QUEUE_H_

#include <dpu_alloc.h>

enum config
{
    QUEUE_GROWTH_FACTOR = 2,
};

typedef struct data_item
{
    uintptr_t ptr;      // point value
    int padding;    // 4 byte padding
} data_item_t;

typedef struct queue
{
    int pop;
    // int padding_1;
    int push;
    // int padding_2;
    int capacity;
    // int padding_3;
    __mram_ptr data_item_t *elements;
} queue_t;

__mram_ptr queue_t *
queue_alloc(long initCapacity)
{
    __mram_ptr queue_t *queuePtr = (__mram_ptr queue_t *)mram_malloc(sizeof(queue_t));

    if (queuePtr)
    {
        int capacity = ((initCapacity < 2) ? 2 : initCapacity);
        queuePtr->elements =
            (__mram_ptr data_item_t *)mram_malloc(capacity * sizeof(data_item_t));

        if (queuePtr->elements == NULL)
        {
            // mram_free(queuePtr);
            return NULL;
        }

        queuePtr->pop = capacity - 1;
        queuePtr->push = 0;
        queuePtr->capacity = capacity;
    }

    return queuePtr;
}

void
queue_clear(__mram_ptr queue_t *queuePtr)
{
    queuePtr->pop = queuePtr->capacity - 1;
    queuePtr->push = 0;
}

bool_t
queue_is_empty(__mram_ptr queue_t *queuePtr)
{
    int pop = queuePtr->pop;
    int push = queuePtr->push;
    int capacity = queuePtr->capacity;

    return (((pop + 1) % capacity == push) ? TRUE : FALSE);
}

bool_t
queue_push(__mram_ptr queue_t *queuePtr, __mram_ptr void *dataPtr)
{
    int pop = queuePtr->pop;
    int push = queuePtr->push;
    int capacity = queuePtr->capacity;

    assert(pop != push);

    /* Need to resize */
    int newPush = (push + 1) % capacity;

    if (newPush == pop)
    {
        int newCapacity = capacity * QUEUE_GROWTH_FACTOR;
        __mram_ptr data_item_t *newElements =
            (__mram_ptr data_item_t *)mram_malloc(newCapacity * sizeof(data_item_t));

        if (newElements == NULL)
        {
            return FALSE;
        }

        long dst = 0;
        __mram_ptr data_item_t *elements = queuePtr->elements;
        if (pop < push)
        {
            long src;
            for (src = (pop + 1); src < push; src++, dst++)
            {
                newElements[dst] = elements[src];
            }
        }
        else
        {
            long src;
            for (src = (pop + 1); src < capacity; src++, dst++)
            {
                newElements[dst] = elements[src];
            }
            for (src = 0; src < push; src++, dst++)
            {
                newElements[dst] = elements[src];
            }
        }

        // mram_free(elements);

        queuePtr->elements = newElements;
        queuePtr->pop = newCapacity - 1;
        queuePtr->capacity = newCapacity;
        push = dst;
        newPush = push + 1;
    }

    data_item_t data = { .ptr = (uintptr_t)dataPtr, .padding = 0 };
    queuePtr->elements[push] = data;
    queuePtr->push = newPush;

    return TRUE;
}

__mram_ptr void *
queue_pop(__mram_ptr queue_t *queuePtr)
{
    int pop = queuePtr->pop;
    int push = queuePtr->push;
    int capacity = queuePtr->capacity;

    int newPop = (pop + 1) % capacity;

    if (newPop == push)
    {
        return NULL;
    }

    data_item_t data = queuePtr->elements[newPop];
    queuePtr->pop = newPop;

    return (__mram_ptr void *)data.ptr;
}

#endif /* _QUEUE_H_ */
