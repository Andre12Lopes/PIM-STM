#include <assert.h>
#include <barrier.h>
#include <defs.h>
#include <limits.h>
#include <mram.h>
#include <perfcounter.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

#include <random.h>
#include <tm.h>

BARRIER_INIT(barr, NR_TASKLETS);
BARRIER_INIT(kmeans_barr, NR_TASKLETS);

#include "inputs/inputs_14.h"
#include "metrics.h"
#include "util.h"

#ifndef N_CLUSTERS
#define N_CLUSTERS 15
#endif

#define MIN_N_CLUSTERS N_CLUSTERS
#define MAX_N_CLUSTERS N_CLUSTERS
#define USE_ZSCORE_TRANSFORM 0
#define THRESHOLD 0.05
#define CHUNK 3

int loop;
float delta;
float delta_per_thread[NR_TASKLETS];
int membership[NUM_OBJECTS];

float new_centers[N_CLUSTERS][NUM_ATTRIBUTES];
uint32_t new_centers_len[N_CLUSTERS];

__dma_aligned float cluster_centres[N_CLUSTERS * NUM_ATTRIBUTES];

#ifdef TX_IN_MRAM
Tx __mram_noinit tx_mram[NR_TASKLETS];
#else
Tx tx_wram[NR_TASKLETS];
#endif

float
euclidian_distance(float *pt1, float *pt2);
int
find_nearest_center(float *pt, float *centers);

int
main()
{
    TYPE Tx *tx;
    uint64_t s;
    int tid;
    int index;
    int tmp_center_len;
    float tmp_center_attr;

    __dma_aligned float tmp_point[NUM_ATTRIBUTES];

    tid = me();
    s = (uint64_t)me();

#ifndef TX_IN_MRAM
    tx = &tx_wram[tid];
#else
    tx = &tx_mram[tid];
#endif

    TM_INIT(tx, tid);

    start_count(tid);

    // -------------------------------------------------------------------

    if (MIN_N_CLUSTERS != MAX_N_CLUSTERS || USE_ZSCORE_TRANSFORM != 0)
    {
        assert(0);
    }

    if (tid == 0)
    {
        /* Randomly pick cluster centers */
        for (int i = 0; i < N_CLUSTERS; ++i)
        {
            int n = (int)(RAND_R_FNC(s) % NUM_OBJECTS);
            mram_read(attributes[n], &cluster_centres[i * NUM_ATTRIBUTES],
                      sizeof(attributes[n]));
        }

        loop = 0;

        for (int i = 0; i < NUM_OBJECTS; ++i)
        {
            membership[i] = -1;
        }

        for (int i = 0; i < N_CLUSTERS; ++i)
        {
            new_centers_len[i] = 0;
            for (int j = 0; j < NUM_ATTRIBUTES; ++j)
            {
                new_centers[i][j] = 0;
            }
        }
    }
    barrier_wait(&kmeans_barr);

    do
    {
        // ==========================================================================

        delta_per_thread[tid] = 0;

        for (int i = tid; i < NUM_OBJECTS; i += NR_TASKLETS)
        {
            mram_read(attributes[i], tmp_point, sizeof(tmp_point));
            index = find_nearest_center(tmp_point, cluster_centres);

            if (membership[i] != index)
            {
                delta_per_thread[tid] += 1.0;
            }

            membership[i] = index;

            TM_START(tx);

            tmp_center_len = TM_LOAD(tx, &new_centers_len[index]);
            tmp_center_len++;
            TM_STORE(tx, &new_centers_len[index], tmp_center_len);

            for (int j = 0; j < NUM_ATTRIBUTES; ++j)
            {
                uintptr_t tmp = TM_LOAD_LOOP(tx, (uintptr_t *)&new_centers[index][j]);
                tmp_center_attr = intp2double(tmp) + tmp_point[j];
                TM_STORE_LOOP(tx, (uintptr_t *)&new_centers[index][j],
                              double2intp(tmp_center_attr));
            }

            if (tx->status == 4)
            {
                // We break out of previous loop because of an abort
                continue;
            }

            TM_COMMIT(tx);
        }
        barrier_wait(&kmeans_barr);

        // ==========================================================================

        if (tid == 0)
        {
            /* Replace old cluster centers with new_centers */
            for (int i = 0; i < N_CLUSTERS; ++i)
            {
                for (int j = 0; j < NUM_ATTRIBUTES; ++j)
                {
                    if (new_centers_len[i] > 0)
                    {
                        cluster_centres[(i * NUM_ATTRIBUTES) + j] =
                            new_centers[i][j] / new_centers_len[i];
                    }
                    new_centers[i][j] = 0.0;
                }
                new_centers_len[i] = 0;
            }

            delta = 0;
            for (int i = 0; i < NR_TASKLETS; ++i)
            {
                delta += delta_per_thread[i];
                delta_per_thread[i] = 0;
            }
            delta /= NUM_OBJECTS;

            loop++;
        }
        barrier_wait(&kmeans_barr);

    } while ((delta > THRESHOLD) && (loop < 500));

    get_metrics(tx, tid, loop);

    return 0;
}

float
euclidian_distance(float *pt1, float *pt2)
{
    float ans = 0.0F;

    for (int i = 0; i < NUM_ATTRIBUTES; ++i)
    {
        ans += (pt1[i] - pt2[i]) * (pt1[i] - pt2[i]);
    }

    return ans;
}

int
find_nearest_center(float *pt, float *centers)
{
    int index = -1;
    float max_dist = 3.402823466e+38F; // TODO: might be a bug

    /* Find the cluster center id with min distance to pt */
    for (int i = 0; i < N_CLUSTERS; ++i)
    {
        float dist;
        dist = euclidian_distance(pt, &centers[i * NUM_ATTRIBUTES]);
        if (dist < max_dist)
        {
            max_dist = dist;
            index = i;
            if (max_dist == 0)
            {
                break;
            }
        }
    }

    return index;
}
