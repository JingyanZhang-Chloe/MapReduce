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
#define MAX_WORKERS 10

#define RED     "\033[31m"
#define GREEN   "\033[32m"
#define RESET   "\033[0m"

void incorrect_usage() {
    LogInfo("Incorrect usage");
    std::cout << "Usage: ./CSE305_project <num_workers>" << std::endl;
    std::cout << "(at least " << MIN_WORKERS << " and at most " << MAX_WORKERS << " workers can be used)" << std::endl;
}

void validation_test(size_t num_workers);

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

    validation_test(num_workers);

    return 0;
}

void validation_test(size_t num_workers) {
    // useful map and reduce functions (with integers)
    auto cardinal_map = [](const int& x){ return 1; };
    auto cardinal_reduce = [](const int& x, const int& y){ return x + y; };
    int cardinal_reduce_init = 0;
    int cardinal_result = 27;

    auto even_count_map = [](const int& x){ return 1 - x % 2; };
    auto even_count_reduce = [](const int& x, const int& y){ return x + y; };
    int even_count_reduce_init = 0;
    int even_count_result = 14;

    auto max_map = [](const int& x){ return x; };
    auto max_reduce = [](const int& x, const int& y){
        if (x > y) return x;
        return y;
    };
    int max_reduce_init = 0;
    int max_result = 30;

    auto all_even_map = [](const int& x){
        if (x % 2 == 0) return true;
        return false;
    };
    auto all_even_reduce = [](const bool& x, const bool& y){ return x && y; };
    bool all_even_reduce_init = true;
    bool all_even_result = false;

    // useful seeds and successors
    // linear combinations of 3 and 5 up to 30
    std::vector<int> combs35_seeds = {0, 30};
    auto combs35_successors = [](const int& x){
        std::vector<int> res = {};
        if (x + 5 <= 30) res.push_back(x + 5);
        if (x + 3 <= 30) res.push_back(x + 3);
        return res;
    };

    // Test on cardinal_map
    // Create a Master
    // types: U = int; A = int
    Master<int, int> master1(num_workers, combs35_seeds, combs35_successors, cardinal_map, cardinal_reduce, cardinal_reduce_init);
    int master1_res = master1.run();
    if (cardinal_result == master1_res) {
        LogInfo(GREEN "[Success] Result -- expected %i and got %i" RESET, cardinal_result, master1_res);
    } else {
        LogInfo(RED "[Fail] Result -- expected %i and got %i" RESET, cardinal_result, master1_res);
    }

    // Test on even_count_map
    Master<int, int> master2(num_workers, combs35_seeds, combs35_successors, even_count_map, even_count_reduce, even_count_reduce_init);
    int master2_res = master2.run();
    if (even_count_result == master2_res) {
        LogInfo(GREEN "[Success] Result -- expected %i and got %i" RESET, even_count_result, master2_res);
    } else {
        LogInfo(RED "[Fail] Result -- expected %i and got %i" RESET, even_count_result, master2_res);
    }

    // Test on max_map
    Master<int, int> master3(num_workers, combs35_seeds, combs35_successors, max_map, max_reduce, max_reduce_init);
    int master3_res = master3.run();
    if (max_result == master3_res) {
        LogInfo(GREEN "[Success] Result -- expected %i and got %i" RESET, max_result, master3_res);
    } else {
        LogInfo(RED "[Fail] Result -- expected %i and got %i" RESET, max_result, master3_res);
    }

    // Test on all_even_map
    Master<int, int> master4(num_workers, combs35_seeds, combs35_successors, all_even_map, all_even_reduce, all_even_reduce_init);
    int master4_res = master4.run();
    if (all_even_result == master4_res) {
        LogInfo(GREEN "[Success] Result -- expected %i and got %i" RESET, all_even_result, master4_res);
    } else {
        LogInfo(RED "[Fail] Result -- expected %i and got %i" RESET, all_even_result, master4_res);
    }




    // create a dummy master
    // types: U = int; A = int
    /*
    std::vector<int> seeds = combs35_seeds;
    int reduce_init = 0;
    Master<int, int> *master = new Master<int, int>(
        num_workers,
        seeds,
        combs35_successors,
        cardinal_map,
        cardinal_reduce,
        cardinal_reduce_init
    );


    // manually create workers
    std::vector<std::unique_ptr<Worker<int, int>>> workers(num_workers);
    for (size_t i = 0; i < num_workers; ++i) {
        std::vector<int> tasks = {};
        for (int j = i; j < seeds.size(); j += num_workers) {
            tasks.push_back(seeds[j]);
        }

        workers[i] = std::make_unique<Worker<int, int>>(i, master, tasks);
    }

    // // 1 worker
    // workers[0]->run();
    // LogInfo("Result -- expected %i and got %i", cardinal_result, workers[0]->get_result());

    // 2 workers
    std::thread A(&Worker<int, int>::run, workers[0].get());
    std::thread B(&Worker<int, int>::run, workers[1].get());
    A.join();
    B.join();
    int combined_res = master->get_reduce_function()(
        workers[0]->get_result(),
        workers[1]->get_result()
    );
    LogInfo("Result -- expected %i and got %i", cardinal_result, combined_res);
    */
}