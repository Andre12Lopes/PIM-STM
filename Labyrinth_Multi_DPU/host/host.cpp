#include <chrono>
#include <dpu>
#include <iostream>
#include <random>
#include <unistd.h>

using namespace dpu;

#ifndef N_DPUS
#define N_DPUS 10
#endif

#define NUM_PATHS 100
#define RANGE_X 16
#define RANGE_Y 16
#define RANGE_Z 3

void 
create_bach(DpuSet &system, std::vector<int> &bach);

int
main(int argc, char **argv)
{
    (void) argc;
    (void) argv;

    double total_copy_time = 0;
    double total_time = 0;

    try
    {
        auto system = DpuSet::allocate(N_DPUS);

        system.load("./Labyrinth_Multi_DPU/bin/dpu");

        std::vector<int> bach(6 * NUM_PATHS);
        
    	create_bach(system, bach);

        auto start = std::chrono::steady_clock::now();
    
        system.copy("bach", bach);

        auto end_copy = std::chrono::steady_clock::now();

        system.exec();

        auto end = std::chrono::steady_clock::now();

        total_copy_time += std::chrono::duration_cast<std::chrono::microseconds>(end_copy - start).count();
        total_time += std::chrono::duration_cast<std::chrono::microseconds>(end - start).count();

        std::cout << NR_TASKLETS << "\t"
                  << NUM_PATHS * 2 << "\t"
                  << N_DPUS << "\t"
                  << total_copy_time << "\t"
                  << total_time << std::endl;
    }
    catch (const DpuError &e)
    {
        std::cerr << e.what() << std::endl;
    }
    
    return 0;
}

void 
create_bach(DpuSet &system, std::vector<int> &bach)
{
    (void) system;
    std::random_device dev;
    std::mt19937 rng(dev());
    std::uniform_int_distribution<std::mt19937::result_type> rand_x_y(0, RANGE_X - 1);
    std::uniform_int_distribution<std::mt19937::result_type> rand_z(0, RANGE_Z - 1);
    int index;

    for (int j = 0; j < NUM_PATHS; ++j)
    {
        index = j * 6;

        bach[index] = rand_x_y(rng);
        bach[index + 1] = rand_x_y(rng);
        bach[index + 2] = rand_z(rng);

        do
        {
            bach[index + 3] = rand_x_y(rng);
            bach[index + 4] = rand_x_y(rng);
            bach[index + 5] = rand_z(rng);
        } 
        while (bach[index] == bach[index + 3] &&
               bach[index + 1] == bach[index + 4] &&
               bach[index + 2] == bach[index + 5]);
    }
}
