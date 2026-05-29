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
    int steal_style_cardinal = NO_STEAL;
    Master<int, int> master1(num_workers, combs35_seeds, combs35_successors, cardinal_map, cardinal_reduce, cardinal_reduce_init, steal_style_cardinal);
    int master1_res = master1.run();
    if (cardinal_result == master1_res) {
        LogInfo(GREEN "[Success] Result -- expected %i and got %i" RESET, cardinal_result, master1_res);
    } else {
        LogInfo(RED "[Fail] Result -- expected %i and got %i" RESET, cardinal_result, master1_res);
    }

    // Test on even_count_map
    int steal_style_even_count = NO_STEAL;
    Master<int, int> master2(num_workers, combs35_seeds, combs35_successors, even_count_map, even_count_reduce, even_count_reduce_init, steal_style_even_count);
    int master2_res = master2.run();
    if (even_count_result == master2_res) {
        LogInfo(GREEN "[Success] Result -- expected %i and got %i" RESET, even_count_result, master2_res);
    } else {
        LogInfo(RED "[Fail] Result -- expected %i and got %i" RESET, even_count_result, master2_res);
    }

    // Test on max_map
    int steal_style_max = NO_STEAL;
    Master<int, int> master3(num_workers, combs35_seeds, combs35_successors, max_map, max_reduce, max_reduce_init, steal_style_max);
    int master3_res = master3.run();
    if (max_result == master3_res) {
        LogInfo(GREEN "[Success] Result -- expected %i and got %i" RESET, max_result, master3_res);
    } else {
        LogInfo(RED "[Fail] Result -- expected %i and got %i" RESET, max_result, master3_res);
    }

    // Test on all_even_map
    int steal_style_all_even = NO_STEAL;
    Master<int, int> master4(num_workers, combs35_seeds, combs35_successors, all_even_map, all_even_reduce, all_even_reduce_init, steal_style_all_even);
    int master4_res = master4.run();
    if (all_even_result == master4_res) {
        LogInfo(GREEN "[Success] Result -- expected %i and got %i" RESET, all_even_result, master4_res);
    } else {
        LogInfo(RED "[Fail] Result -- expected %i and got %i" RESET, all_even_result, master4_res);
    }
}