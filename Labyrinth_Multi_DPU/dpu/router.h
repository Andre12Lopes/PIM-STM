#ifndef _ROUTER_H_
#define _ROUTER_H_

typedef enum momentum
{
    MOMENTUM_ZERO = 0,
    MOMENTUM_POSX = 1,
    MOMENTUM_POSY = 2,
    MOMENTUM_POSZ = 3,
    MOMENTUM_NEGX = 4,
    MOMENTUM_NEGY = 5,
    MOMENTUM_NEGZ = 6
} momentum_t;

typedef struct point
{
    long x;
    long y;
    long z;
    int value;
    momentum_t momentum;
} point_t;

point_t MOVE_POSX = {1, 0, 0, 0, MOMENTUM_POSX};
point_t MOVE_POSY = {0, 1, 0, 0, MOMENTUM_POSY};
point_t MOVE_POSZ = {0, 0, 1, 0, MOMENTUM_POSZ};
point_t MOVE_NEGX = {-1, 0, 0, 0, MOMENTUM_NEGX};
point_t MOVE_NEGY = {0, -1, 0, 0, MOMENTUM_NEGY};
point_t MOVE_NEGZ = {0, 0, -1, 0, MOMENTUM_NEGZ};

typedef struct router
{
    long x_cost;
    long y_cost;
    long z_cost;
    long bend_cost;
} router_t;

void
router_alloc(__mram_ptr router_t *r, long x_cost, long y_cost, long z_cost,
             long bend_cost)
{
    r->x_cost = x_cost;
    r->y_cost = y_cost;
    r->z_cost = z_cost;
    r->bend_cost = bend_cost;
}

static void
pexpand_neighbor(__mram_ptr grid_t *myGridPtr, long x, long y, long z, int v,
                    __mram_ptr queue_t *queuePtr)
{
    __mram_ptr grid_point_t *neighborGridPointPtr;
    int neighborValue;

    if (grid_is_point_valid(myGridPtr, x, y, z))
    {
        neighborGridPointPtr = grid_get_point_ref(myGridPtr, x, y, z);
        neighborValue = neighborGridPointPtr->value;

        if (neighborValue == GRID_POINT_EMPTY)
        {
            neighborGridPointPtr->value = v;
            queue_push(queuePtr, (__mram_ptr void *)neighborGridPointPtr);
        }
        else if (neighborValue != GRID_POINT_FULL)
        {
            /* We have expanded here before... is this new path better? */
            if (v < neighborValue)
            {
                neighborGridPointPtr->value = v;
                queue_push(queuePtr, (__mram_ptr void *)neighborGridPointPtr);
            }
        }
    }
}

static bool_t
pdo_expansion(__mram_ptr router_t *routerPtr, __mram_ptr grid_t *myGridPtr,
              __mram_ptr queue_t *queuePtr, __mram_ptr coordinate_t *srcPtr, 
              __mram_ptr coordinate_t *dstPtr)
{
    __mram_ptr grid_point_t *srcGridPointPtr;
    __mram_ptr grid_point_t *dstGridPointPtr;
    __mram_ptr grid_point_t *gridPointPtr;
    bool_t isPathFound;
    long x, y, z;
    int value;

    queue_clear(queuePtr);

    srcGridPointPtr = grid_get_point_ref(myGridPtr, srcPtr->x, srcPtr->y, srcPtr->z);
    queue_push(queuePtr, (__mram_ptr void *)srcGridPointPtr);

    grid_set_point(myGridPtr, srcPtr->x, srcPtr->y, srcPtr->z, 0);
    grid_set_point(myGridPtr, dstPtr->x, dstPtr->y, dstPtr->z, GRID_POINT_EMPTY);

    dstGridPointPtr = grid_get_point_ref(myGridPtr, dstPtr->x, dstPtr->y, dstPtr->z);
    isPathFound = FALSE;

    while (!queue_is_empty(queuePtr))
    {
        gridPointPtr = (__mram_ptr grid_point_t *)queue_pop(queuePtr);

        if (gridPointPtr == dstGridPointPtr)
        {
            isPathFound = TRUE;
            break;
        }

        grid_get_point_indices(myGridPtr, gridPointPtr, &x, &y, &z);
        value = gridPointPtr->value;

        pexpand_neighbor(myGridPtr, x + 1, y, z, (value + routerPtr->x_cost), queuePtr);
        pexpand_neighbor(myGridPtr, x - 1, y, z, (value + routerPtr->x_cost), queuePtr);
        pexpand_neighbor(myGridPtr, x, y + 1, z, (value + routerPtr->y_cost), queuePtr);
        pexpand_neighbor(myGridPtr, x, y - 1, z, (value + routerPtr->y_cost), queuePtr);
        pexpand_neighbor(myGridPtr, x, y, z + 1, (value + routerPtr->z_cost), queuePtr);
        pexpand_neighbor(myGridPtr, x, y, z - 1, (value + routerPtr->z_cost), queuePtr);
    }

    // printf("Expansion (%li, %li, %li) -> (%li, %li, %li):\n", srcPtr->x, srcPtr->y,
    //        srcPtr->z, dstPtr->x, dstPtr->y, dstPtr->z);
    // printf("Path found: %d\n", isPathFound);
    // grid_print(myGridPtr);

    return isPathFound;
}

static void
traceToNeighbor(__mram_ptr grid_t *myGridPtr, point_t *currPtr, point_t *movePtr,
                bool_t useMomentum, long bendCost, point_t *nextPtr)
{
    int value;
    long b;
    long x = currPtr->x + movePtr->x;
    long y = currPtr->y + movePtr->y;
    long z = currPtr->z + movePtr->z;

    if (grid_is_point_valid(myGridPtr, x, y, z) &&
        !grid_is_point_empty(myGridPtr, x, y, z) &&
        !grid_is_point_full(myGridPtr, x, y, z))
    {
        value = grid_get_point_ref(myGridPtr, x, y, z)->value;
        b = 0;

        if (useMomentum && (currPtr->momentum != movePtr->momentum))
        {
            b = bendCost;
        }

        if ((value + b) <= nextPtr->value) /* '=' favors neighbors over current */
        {
            nextPtr->x = x;
            nextPtr->y = y;
            nextPtr->z = z;
            nextPtr->value = value;
            nextPtr->momentum = movePtr->momentum;
        }
    }
}

static __mram_ptr vector_t *
pdo_traceback(__mram_ptr grid_t *gridPtr, __mram_ptr grid_t *myGridPtr,
              __mram_ptr coordinate_t *dstPtr, long bendCost)
{
    __mram_ptr vector_t *pointVectorPtr;
    point_t next;
    point_t curr;

    pointVectorPtr = vector_alloc(1);
    assert(pointVectorPtr);

    next.x = dstPtr->x;
    next.y = dstPtr->y;
    next.z = dstPtr->z;
    next.value = grid_get_point_ref(myGridPtr, next.x, next.y, next.z)->value;
    next.momentum = MOMENTUM_ZERO;

    while (1)
    {
        // printf("(%li, %li, %li)\n", next.x, next.y, next.z);
        
        __mram_ptr grid_point_t *gridPointPtr =
            grid_get_point_ref(gridPtr, next.x, next.y, next.z);
        vector_push_back(pointVectorPtr, (__mram_ptr void *)gridPointPtr);
        grid_set_point(myGridPtr, next.x, next.y, next.z, GRID_POINT_FULL);

        /* Check if we are done */
        if (next.value == 0)
        {
            break;
        }
        curr = next;

        /*
         * Check 6 neighbors
         *
         * Potential Optimization: Only need to check 5 of these
         */
        traceToNeighbor(myGridPtr, &curr, &MOVE_POSX, TRUE, bendCost, &next);
        traceToNeighbor(myGridPtr, &curr, &MOVE_POSY, TRUE, bendCost, &next);
        traceToNeighbor(myGridPtr, &curr, &MOVE_POSZ, TRUE, bendCost, &next);
        traceToNeighbor(myGridPtr, &curr, &MOVE_NEGX, TRUE, bendCost, &next);
        traceToNeighbor(myGridPtr, &curr, &MOVE_NEGY, TRUE, bendCost, &next);
        traceToNeighbor(myGridPtr, &curr, &MOVE_NEGZ, TRUE, bendCost, &next);
    
        /*
         * Because of bend costs, none of the neighbors may appear to be closer.
         * In this case, pick a neighbor while ignoring momentum.
         */
        if ((curr.x == next.x) && (curr.y == next.y) && (curr.z == next.z))
        {
            next.value = curr.value;
            traceToNeighbor(myGridPtr, &curr, &MOVE_POSX, FALSE, bendCost, &next);
            traceToNeighbor(myGridPtr, &curr, &MOVE_POSY, FALSE, bendCost, &next);
            traceToNeighbor(myGridPtr, &curr, &MOVE_POSZ, FALSE, bendCost, &next);
            traceToNeighbor(myGridPtr, &curr, &MOVE_NEGX, FALSE, bendCost, &next);
            traceToNeighbor(myGridPtr, &curr, &MOVE_NEGY, FALSE, bendCost, &next);
            traceToNeighbor(myGridPtr, &curr, &MOVE_NEGZ, FALSE, bendCost, &next);

            if ((curr.x == next.x) && (curr.y == next.y) && (curr.z == next.z))
            {
                vector_free(pointVectorPtr);
                // puts("[dead]");

                return NULL; /* cannot find path */
            }
        }
    }

#if DEBUG
    puts("");
#endif /* DEBUG */

    return pointVectorPtr;
}

#endif /* _ROUTER_H_ */
