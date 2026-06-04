#include <iostream>

#include <vector>

#include "Master.hpp"
#include "Worker.hpp"
#include "RecursivelyEnumeratedSet.hpp"

// #include <Logger.h> // uses simple-cpp-logger
// // control logging: compile with "cmake -DNO_LOGS=ON ../" or "cmake -DNO_LOGS=OFF ../"" (after only "cmake .." is enough)
// #ifdef NO_LOGS
// #define LogInfo(...) ((void)0)
// #endif

#define MIN_WORKERS 1
#define MAX_WORKERS 20

#define RED     "\033[31m"
#define GREEN   "\033[32m"
#define RESET   "\033[0m"

void incorrect_usage() {
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

    std::cout << "Set: linear combinations of 3 and 5 between 0 and 30" << std::endl;

    // sequential
    std::cout << "SEQUENTIAL TEST" << std::endl;
    // cardinality
    std::cout << "Compute cardinality..." << std::endl;
    RESetMapReduce<int, int> re_set1(combs35_seeds, combs35_successors);
    int re_set1_res = re_set1.map_reduce_avoid_duplicate(cardinal_map, cardinal_reduce, cardinal_reduce_init);
    if (cardinal_result == re_set1_res) {
        std::cout << GREEN "[Success]" RESET << " Result -- expected " << cardinal_result << " and got " << re_set1_res << std::endl;
    } else {
        std::cout << RED "[Fail]" RESET << " Result -- expected " << cardinal_result << " and got " << re_set1_res << std::endl;
    }
    // even
    std::cout << "Count even numbers..." << std::endl;
    RESetMapReduce<int, int> re_set2(combs35_seeds, combs35_successors);
    int re_set2_res = re_set2.map_reduce_avoid_duplicate(even_count_map, even_count_reduce, even_count_reduce_init);
    if (even_count_result == re_set2_res) {
        std::cout << GREEN "[Success]" RESET << " Result -- expected " << even_count_result << " and got " << re_set2_res << std::endl;
    } else {
        std::cout << RED "[Fail]" RESET << " Result -- expected " << even_count_result << " and got " << re_set2_res << std::endl;
    }
    // max
    std::cout << "Compute maximum..." << std::endl;
    RESetMapReduce<int, int> re_set3(combs35_seeds, combs35_successors);
    int re_set3_res = re_set3.map_reduce_avoid_duplicate(max_map, max_reduce, max_reduce_init);
    if (max_result == re_set3_res) {
        std::cout << GREEN "[Success]" RESET << " Result -- expected " << max_result << " and got " << re_set3_res << std::endl;
    } else {
        std::cout << RED "[Fail]" RESET << " Result -- expected " << max_result << " and got " << re_set3_res << std::endl;
    }
    // all even
    std::cout << "Check whether all numbers are even..." << std::endl;
    RESetMapReduce<int, bool> re_set4(combs35_seeds, combs35_successors);
    bool re_set4_res = re_set4.map_reduce_avoid_duplicate(all_even_map, all_even_reduce, all_even_reduce_init);
    if (all_even_result == re_set4_res) {
        std::cout << GREEN "[Success]" RESET << " Result -- expected " << all_even_result << " and got " << re_set4_res << std::endl;
    } else {
        std::cout << RED "[Fail]" RESET << " Result -- expected " << all_even_result << " and got " << re_set4_res << std::endl;
    }

    std::vector<int> steal_types = {NO_STEAL, NAIVE_STEAL, SMART_STEAL};
    std::vector<std::string> steal_types_str = {"NO WS TEST", "NAIVE WS TEST", "SMART WS TEST"};

    for (int i = 0; i < steal_types.size(); ++i) {
        std::cout << "--------------------" << std::endl;
        std::cout << steal_types_str[i] << std::endl;

        int steal_type = steal_types[i];

        // Test on cardinal_map
        std::cout << "Compute cardinality..." << std::endl;
        // Create a Master
        // types: U = int; A = int
        Master<int, int> master1(num_workers, combs35_seeds, combs35_successors, cardinal_map, cardinal_reduce, cardinal_reduce_init, steal_type);
        int master1_res = master1.run();
        if (cardinal_result == master1_res) {
            std::cout << GREEN "[Success]" RESET << " Result -- expected " << cardinal_result << " and got " << master1_res << std::endl;
        } else {
            std::cout << RED "[Fail]" RESET << " Result -- expected " << cardinal_result << " and got " << master1_res << std::endl;
        }

        // Test on even_count_map
        std::cout << "Count even numbers..." << std::endl;
        Master<int, int> master2(num_workers, combs35_seeds, combs35_successors, even_count_map, even_count_reduce, even_count_reduce_init, steal_type);
        int master2_res = master2.run();
        if (even_count_result == master2_res) {
            std::cout << GREEN "[Success]" RESET << " Result -- expected " << even_count_result << " and got " << master2_res << std::endl;
        } else {
            std::cout << RED "[Fail]" RESET << " Result -- expected " << even_count_result << " and got " << master2_res << std::endl;
        }

        // Test on max_map
        std::cout << "Compute maximum..." << std::endl;
        Master<int, int> master3(num_workers, combs35_seeds, combs35_successors, max_map, max_reduce, max_reduce_init, steal_type);
        int master3_res = master3.run();
        if (max_result == master3_res) {
            std::cout << GREEN "[Success]" RESET << " Result -- expected " << max_result << " and got " << master3_res << std::endl;
        } else {
            std::cout << RED "[Fail]" RESET << " Result -- expected " << max_result << " and got " << master3_res << std::endl;
        }

        // Test on all_even_map
        std::cout << "Check whether all numbers are even..." << std::endl;
        Master<int, int> master4(num_workers, combs35_seeds, combs35_successors, all_even_map, all_even_reduce, all_even_reduce_init, steal_type);
        int master4_res = master4.run();
        if (all_even_result == master4_res) {
            std::cout << GREEN "[Success]" RESET << " Result -- expected " << all_even_result << " and got " << master4_res << std::endl;
        } else {
            std::cout << RED "[Fail]" RESET << " Result -- expected " << all_even_result << " and got " << master4_res << std::endl;
        }
    }
}