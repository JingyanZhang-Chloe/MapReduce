#include <iostream>

#include <vector>

#include "Master.hpp"
#include "Worker.hpp"

#include <Logger.h> // uses simple-cpp-logger
// control logging: compile with "cmake -DNO_LOGS=ON ../" or "cmake -DNO_LOGS=OFF ../"" (after only "cmake .." is enough)
#ifdef NO_LOGS
#define LogInfo(...) ((void)0)
#endif

#define MIN_WORKERS 1
#define MAX_WORKERS 1

void incorrect_usage() {
    LogInfo("Incorrect usage");
    std::cout << "Usage: ./CSE305_project <num_workers>" << std::endl;
    std::cout << "(at least " << MIN_WORKERS << " and at most " << MAX_WORKERS << " workers can be used)" << std::endl;
}

void manual_worker_test(size_t num_workers);

int main(int argc, char **argv) {
    if (argc < 2) {
        incorrect_usage();
        return 0;
    }

    size_t num_workers;
    try {
        num_workers = std::stoi(argv[1]);
    } catch(...) {
        incorrect_usage();
        return 0;
    }

    if (num_workers < MIN_WORKERS || num_workers > MAX_WORKERS) {
        incorrect_usage();
        return 0;
    }

    manual_worker_test(num_workers);

    return 0;
}

void manual_worker_test(size_t num_workers) {
    // useful map and reduce functions (with integers)
    auto cardinal_map = [](const int& x){ return 1; };
    auto cardinal_reduce = [](const int& x, const int& y){ return x + y; };
    int cardinal_reduce_init = 0;
    auto even_count_map = [](const int& x){ return 1 - x % 2; };
    auto even_count_reduce = [](const int& x, const int& y){ return x + y; };
    int even_count_reduce_init = 0;
    auto max_map = [](const int& x){ return x; };
    auto max_reduce = [](const int& x, const int& y){
        if (x > y) return x;
        return y;
    };
    int max_reduce_init = 0;
    auto all_even_map = [](const int& x){
        if (x % 2 == 0) return true;
        return false;
    };
    auto all_even_reduce = [](const bool& x, const bool& y){ return x && y; };
    bool all_even_reduce_init = true;

    // useful seeds and successors
    // linear combinations of 2 and 3 up to 15
    std::vector<int> combs23_seeds = {0};
    auto combs23_successors = [](const int& x){
        std::vector<int> res = {};
        if (x + 3 <= 15) res.push_back(x + 3);
        if (x + 2 <= 15) res.push_back(x + 2);
        return res;
    };

    // create a dummy master
    // types: U = int; A = int
    std::vector<int> seeds = combs23_seeds;
    int reduce_init = 0;
    Master<int, int> *master = new Master<int, int>(
        num_workers,
        seeds,
        combs23_successors,
        cardinal_map,
        cardinal_reduce,
        cardinal_reduce_init
    );

    // manually create workers
    std::vector<Worker<int, int>*> workers(num_workers);
    for (size_t i = 0; i < num_workers; ++i) {
        std::vector<int> tasks = {};
        for (int j = i; j < seeds.size(); j += num_workers) {
            tasks.push_back(seeds[j]);
        }

        workers[i] = new Worker<int, int>(i, master, tasks);
    }

    if (workers[0]->get_tasks().size() > 0) {
        int task = workers[0]->get_tasks()[0];
        workers[0]->map_reduce(task);
    }
}