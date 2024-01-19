#include <assert.h>
#include <barrier.h>
#include <defs.h>
#include <limits.h>
#include <mram.h>
#include <perfcounter.h>
#include <stdint.h>
#include <stdlib.h>

#include <tm.h>

BARRIER_INIT(labyrinth_barr, NR_TASKLETS);

#define PARAM_BENDCOST 1
#define PARAM_XCOST 1
#define PARAM_YCOST 1
#define PARAM_ZCOST 2

#define NUM_PATHS 100
#define RANGE_X 16
#define RANGE_Y 16
#define RANGE_Z 3

__mram int bach[NUM_PATHS * 6];

#include "types.h"
#include "vector.h"
#include "queue.h"
#include "maze.h"
#include "router.h"

#ifdef TX_IN_MRAM
Tx __mram_noinit tx_mram[NR_TASKLETS];
#else
Tx tx_wram[NR_TASKLETS];
#endif

__mram router_t router;
__mram maze_t maze;
__mram grid_t grid;
__mram grid_t thread_local_grids[NR_TASKLETS];

int
main()
{
    TYPE Tx *tx;
    uint64_t s;
    int tid;

    __mram_ptr coordinate_t *src;
    __mram_ptr coordinate_t *dest;
    __mram_ptr pair_t *coordinatePairPtr;
    __mram_ptr grid_t *my_grid;
    __mram_ptr queue_t *myExpansionQueue;
    __mram_ptr vector_t *point_vector;
    bool_t success;
    long n;

    tid = me();
    s = (uint64_t)me();

    my_grid = &thread_local_grids[tid];

#ifndef TX_IN_MRAM
    tx = &tx_wram[tid];
#else
    tx = &tx_mram[tid];
#endif

    TM_INIT(tx, tid);

    if (tid == 0)
    {
        grid_alloc(&grid);
        maze_read(&maze, &grid, bach);
        // router_alloc(&router, PARAM_XCOST, PARAM_YCOST, PARAM_ZCOST, PARAM_BENDCOST);
        router.x_cost = PARAM_XCOST;
        router.y_cost = PARAM_YCOST;
        router.z_cost = PARAM_ZCOST;
        router.bend_cost = PARAM_BENDCOST;
    }
    barrier_wait(&labyrinth_barr);

    myExpansionQueue = queue_alloc(-1);

    grid_alloc(my_grid);

    while(1)
    {
        TM_START(tx);

        int pop = (int)TM_LOAD(tx, (__mram_ptr uintptr_t *)&(maze.workQueuePtr->pop));
        int push = (int)TM_LOAD(tx, (__mram_ptr uintptr_t *)&(maze.workQueuePtr->push));
        int capacity = (int)TM_LOAD(tx, (__mram_ptr uintptr_t *)&(maze.workQueuePtr->capacity));

        // if (queue_is_empty(maze.workQueuePtr))
        if (((pop + 1) % capacity) == push)
        {
            coordinatePairPtr = NULL;
        }
        else 
        {
            pop = (int)TM_LOAD(tx, (__mram_ptr uintptr_t *)&(maze.workQueuePtr->pop));
            push = (int)TM_LOAD(tx, (__mram_ptr uintptr_t *)&(maze.workQueuePtr->push));
            capacity = (int)TM_LOAD(tx, (__mram_ptr uintptr_t *)&(maze.workQueuePtr->capacity));
            
            int newPop = (pop + 1) % capacity;
            if (newPop == push) 
            {
                coordinatePairPtr = NULL;
            }
            else
            {
                coordinatePairPtr = 
                    (__mram_ptr pair_t *)TM_LOAD(tx, &(maze.workQueuePtr->elements[newPop].ptr));

                TM_STORE(tx, (__mram_ptr uintptr_t *)&(maze.workQueuePtr->pop), newPop);
            }
        }

        TM_COMMIT(tx);

        if (coordinatePairPtr == NULL) 
        {
            break;
        }

        src = (__mram_ptr coordinate_t *)coordinatePairPtr->firstPtr;
        dest = (__mram_ptr coordinate_t *)coordinatePairPtr->secondPtr;

        // printf("Expansion (%ld, %ld, %ld) -> (%ld, %ld, %ld) | TID = %d:\n", 
        //        src->x, src->y, src->z, dest->x, dest->y, dest->z, tid);

        success = FALSE;
        point_vector = NULL;

        TM_START(tx);

        grid_copy(my_grid, &grid);
        if (pdo_expansion(&router, my_grid, myExpansionQueue, src, dest))
        {
            point_vector = pdo_traceback(&grid, my_grid, dest, PARAM_BENDCOST);
            
            if (point_vector)
            {
                // ================ ADD PATH TO GRID ====================
                n = vector_get_size(point_vector);
                
                for (long i = 1; i < (n - 1); i++)
                {
                    __mram_ptr grid_point_t *gridPointPtr = 
                        (__mram_ptr grid_point_t *)vector_at(point_vector, i);

                    int value = 
                        (int)TM_LOAD_LOOP(tx, (__mram_ptr uintptr_t *)&(gridPointPtr->value));

                    if (value != GRID_POINT_EMPTY)
                    {
                        TM_RESTART_LOOP(tx);
                    }

                    TM_STORE_LOOP(tx, (__mram_ptr uintptr_t *)&(gridPointPtr->value), GRID_POINT_FULL);
                }

                if (tx->status == 4)
                {
                    // We break out of previous loop because of an abort
                    continue;
                }
                else
                {
                    // Transaction did not abort
                    success = TRUE;                
                }  
                // ======================================================
            }
        }

        TM_COMMIT(tx);
    }

    return 0;
}
