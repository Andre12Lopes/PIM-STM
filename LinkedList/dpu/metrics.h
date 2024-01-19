#ifndef _METRICS_H_
#define _METRICS_H_

BARRIER_INIT(barr, NR_TASKLETS);

perfcounter_t initial_time;

__host uint32_t nb_cycles;
__host uint32_t nb_process_cycles;
__host uint32_t nb_process_read_cycles;
__host uint32_t nb_process_write_cycles;
__host uint32_t nb_process_validation_cycles;
__host uint32_t nb_commit_cycles;
__host uint32_t nb_commit_validation_cycles;
__host uint32_t nb_wasted_cycles;
__host uint32_t n_aborts;
__host uint32_t n_trans;

void
start_count(int tid)
{
    if (tid == 0)
    {
        n_trans = N_TRANSACTIONS * NR_TASKLETS;
        n_aborts = 0;

        initial_time = perfcounter_config(COUNT_CYCLES, false);
    }

    barrier_wait(&barr);
}

void
get_metrics(TYPE Tx *tx, int tid)
{
    barrier_wait(&barr);

    if (tid == 0)
    {
        nb_cycles = perfcounter_get() - initial_time;

        nb_process_cycles = 0;
        nb_commit_cycles = 0;
        nb_wasted_cycles = 0;
        nb_process_read_cycles = 0;
        nb_process_write_cycles = 0;
        nb_process_validation_cycles = 0;
        nb_commit_validation_cycles = 0;
    }

    for (int i = 0; i < NR_TASKLETS; ++i)
    {
        if (tid == i)
        {
            n_aborts += tx->aborts;

            nb_process_cycles +=
                ((double)tx->process_cycles / (N_TRANSACTIONS * NR_TASKLETS));
            nb_process_read_cycles +=
                ((double)tx->total_read_cycles / (N_TRANSACTIONS * NR_TASKLETS));
            nb_process_write_cycles +=
                ((double)tx->total_write_cycles / (N_TRANSACTIONS * NR_TASKLETS));
            nb_process_validation_cycles +=
                ((double)tx->total_validation_cycles / (N_TRANSACTIONS * NR_TASKLETS));
            nb_commit_cycles +=
                ((double)tx->commit_cycles / (N_TRANSACTIONS * NR_TASKLETS));
            nb_commit_validation_cycles += ((double)tx->total_commit_validation_cycles /
                                            (N_TRANSACTIONS * NR_TASKLETS));
            nb_wasted_cycles +=
                ((double)(tx->total_cycles - (tx->process_cycles + tx->commit_cycles)) /
                 (N_TRANSACTIONS * NR_TASKLETS));
        }

        barrier_wait(&barr);
    }
}

#endif /* _METRICS_H_ */
