#include <dpu>
#include <iostream>

using namespace dpu;

int
main()
{
    std::vector<std::vector<uint64_t>> nbCycles(1);
    std::vector<std::vector<uint32_t>> clocksPerSec(1);
    std::vector<std::vector<uint32_t>> nAborts(1);
    std::vector<std::vector<uint32_t>> nTransactions(1);

    std::vector<std::vector<uint32_t>> nbProcessCycles(1);
    std::vector<std::vector<uint32_t>> nbProcessReadCycles(1);
    std::vector<std::vector<uint32_t>> nbXyz(1);
    std::vector<std::vector<uint32_t>> nbProcessWriteCycles(1);
    std::vector<std::vector<uint32_t>> nbProcessValidationCycles(1);

    std::vector<std::vector<uint32_t>> nbCommitCycles(1);
    std::vector<std::vector<uint32_t>> nbCommitValidationCycles(1);

    std::vector<std::vector<uint32_t>> nbWastedCycles(1);

    try
    {
        auto dpu = DpuSet::allocate(1);

        dpu.load("./Bank/bin/dpu");
        dpu.exec();

        nbCycles.front().resize(1);
        dpu.copy(nbCycles, "nb_cycles");

        clocksPerSec.front().resize(1);
        dpu.copy(clocksPerSec, "CLOCKS_PER_SEC");

        nAborts.front().resize(1);
        dpu.copy(nAborts, "n_aborts");

        nTransactions.front().resize(1);
        dpu.copy(nTransactions, "n_trans");

        nbProcessCycles.front().resize(1);
        dpu.copy(nbProcessCycles, "nb_process_cycles");

        nbProcessReadCycles.front().resize(1);
	dpu.copy(nbProcessReadCycles, "nb_process_read_cycles");
	
	nbXyz.front().resize(1);
        dpu.copy(nbXyz, "nb_xyz");
        
	nbProcessWriteCycles.front().resize(1);
        dpu.copy(nbProcessWriteCycles, "nb_process_write_cycles");

        nbProcessValidationCycles.front().resize(1);
        dpu.copy(nbProcessValidationCycles, "nb_process_validation_cycles");

        nbCommitCycles.front().resize(1);
        dpu.copy(nbCommitCycles, "nb_commit_cycles");

        nbCommitValidationCycles.front().resize(1);
        dpu.copy(nbCommitValidationCycles, "nb_commit_validation_cycles");

        nbWastedCycles.front().resize(1);
        dpu.copy(nbWastedCycles, "nb_wasted_cycles");

        double time = (double)nbCycles.front().front() / clocksPerSec.front().front();

        double process_time =
            (double)nbProcessCycles.front().front() / clocksPerSec.front().front();
        double process_read_time =
            (double)nbProcessReadCycles.front().front() / clocksPerSec.front().front();
        double process_write_time =
            (double)nbProcessWriteCycles.front().front() / clocksPerSec.front().front();
        double process_validation_time =
            (double)nbProcessValidationCycles.front().front() /
            clocksPerSec.front().front();

        double commit_time =
            (double)nbCommitCycles.front().front() / clocksPerSec.front().front();
        double commit_validation_time = (double)nbCommitValidationCycles.front().front() /
                                        clocksPerSec.front().front();

        double wasted_time =
            (double)nbWastedCycles.front().front() / clocksPerSec.front().front();

        long aborts = nAborts.front().front();

        std::cout << NR_TASKLETS << "\t" << nTransactions.front().front() << "\t" << time
                  << "\t"
                  << ((double)aborts * 100) / (aborts + nTransactions.front().front())
                  << "\t" << process_read_time - process_validation_time << "\t"
                  << process_write_time << "\t" << process_validation_time << "\t"
                  << process_time
                  << "\t" << commit_validation_time << "\t"
                  << commit_time - commit_validation_time << "\t" << wasted_time
                  << std::endl;
    
	std::cout << (double)nbXyz.front().front() / clocksPerSec.front().front() << std::endl;
    }
    catch (const DpuError &e)
    {
        std::cerr << e.what() << std::endl;
    }

    return 0;
}
