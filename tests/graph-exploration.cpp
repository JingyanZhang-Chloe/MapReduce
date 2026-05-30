//
// Created by $JingyanZhang on 26/05/2026.
//

#include <Master.hpp>
#include <RecursivelyEnumeratedSet.hpp>
#include <iostream>

#include <vector>
#include <utility>
#include <functional>
#include <algorithm>
#include <string>
#include <limits>

#include <cstdint>
#include <cstdlib>
#include <chrono>

/*
 * Here our object U should be the partial path. since we wanna do map on the partial path to get some property
 * such as length of the current path or some property of this path
 * Successor : take current path -> any possible path taking one more step from the current path
 * Map/Reduce function: depends on the goal (can be length for example)
 */

#include <Logger.h> // uses simple-cpp-logger
// control logging: compile with "cmake -DNO_LOGS=ON ../" or "cmake -DNO_LOGS=OFF ../"" (after only "cmake .." is enough)
#ifdef NO_LOGS
#define LogInfo(...) ((void)0)
#endif

#define MIN_WORKERS 1
#define MAX_WORKERS 10

void incorrect_usage() {
    LogInfo("Incorrect usage");
    std::cout << "Usage: ./CSE305_project <num_workers>" << std::endl;
    std::cout << "(at least " << MIN_WORKERS << " and at most " << MAX_WORKERS << " workers can be used)" << std::endl;
}


class PartialPath {
public:
    int current_node;
    std::vector<int> path;

    PartialPath(int current_node_, std::vector<int> path_)
        : current_node(current_node_), path(std::move(path_)) {}

    bool has_visited(int node) const {
        return std::find(path.begin(), path.end(), node) != path.end();
    }

    bool operator==(const PartialPath & other) const {
        return (this->current_node == other.current_node) && (this->path == other.path);
    }

    PartialPath extend_with(int next_node) const {
        PartialPath next_path = *this;
        next_path.current_node = next_node;
        next_path.path.push_back(next_node);
        return next_path;
    }

    std::string to_string() {
        std::string path;
        for (int steps : this->path) {
            path += std::to_string(steps);
            path += " -> ";
        }

        return path;
    }
};


template<>
struct std::hash<PartialPath>
{
    std::size_t operator()(const PartialPath& my_path) const noexcept
    {
        std::size_t hash_value = static_cast<size_t>(my_path.current_node);

        for (int node: my_path.path) {
            hash_value ^= static_cast<size_t>(node) + 0x9e3779b9 + (hash_value << 6) + (hash_value >> 2);
        }

        return hash_value;
    }
};


class Graph {
    std::vector<int> V;
    std::vector<std::pair<int, int>> E;

public:
    Graph(std::vector<int> V_, std::vector<std::pair<int, int>> E_)
        : V(std::move(V_)), E(std::move(E_)) {}

    std::vector<int> get_neighbors(int state) const {
        std::vector<int> neighbors;
        for (const std::pair<int, int>& path : this->E) {
            int u = path.first;
            int v = path.second;
            if (u == state) {
                neighbors.push_back(v);
            }
        }
        return neighbors;
    }

    size_t get_num_vertices() const {
        return V.size();
    }
};


std::string steal_style_name(int style) {
    switch (style) {
        case NO_STEAL: return "NO_STEAL";
        case NAIVE_STEAL: return "NAIVE_STEAL";
        case SMART_STEAL: return "SMART_STEAL";
        default: return "UNKNOWN";
    }
}


template<typename U, typename A>
std::pair<std::vector<double>, std::vector<double>> run_test_case(
    const std::string& test_name,
    size_t num_workers,
    int num_iterations,
    const std::vector<U>& seeds,
    std::function<std::vector<U>(const U&)> successors,
    std::function<A(const U&)> map_function,
    std::function<A(const A&, const A&)> reduce_function,
    A reduce_init
) {
    std::cout << "\n------------ " << test_name << " ------------" << std::endl;
    std::cout << "\n------------ " << num_workers << " ------------" << std::endl;

    // we make this expected value, for every run in each method, we check if we get a diff result, if so raise
    A expected = reduce_init;
    size_t visited_set_size = 0;

    // should be
    // average_time_vector[0] = average squential
    // average_time_vector[1] = average for NO STEAL
    // average_time_vector[2] = average for NAIVE STEAL
    // average_time_vector[3] = average for SMART STEAL
    std::vector<double> average_time_vector;

    // should be
    // average_speedup_vector[0] = average squential
    // average_speedup_vector[1] = average for NO STEAL
    // average_speedup_vector[2] = average for NAIVE STEAL
    // average_speedup_vector[3] = average for SMART STEAL
    std::vector<double> average_speedup_vector;

    // ------------------------------------------------------------
    // Sequential
    long long total_sequential_time = 0;
    long long best_sequential_time = std::numeric_limits<long long>::max();

    for (int iter = 0; iter < num_iterations; iter++) {
        RESetMapReduce<U, A> sequential(seeds, successors);

        auto start = std::chrono::steady_clock::now();

        A result = sequential.map_reduce_avoid_duplicate(
            map_function,
            reduce_function,
            reduce_init
        );
        auto finish = std::chrono::steady_clock::now();

        long long elapsed = std::chrono::duration_cast<std::chrono::microseconds>(finish - start).count();

        size_t visited_set_size_sequential = sequential.get_visited_set_size();
        total_sequential_time += elapsed;
        best_sequential_time = std::min(best_sequential_time, elapsed);

        if (iter == 0) {
            // store the result first, store the size
            expected = result;
            visited_set_size = visited_set_size_sequential;
        } else if (result != expected) {
            throw std::logic_error("Sequential result changed between iterations");
        } else if (visited_set_size != visited_set_size_sequential) {
            throw std::logic_error("Sequential visited set size changed between iterations");
        }
    }

    double average_sequential_time = static_cast<double>(total_sequential_time) / num_iterations;
    average_time_vector.push_back(average_sequential_time);
    average_speedup_vector.push_back(1);

    std::cout << "Sequential average time: " << average_sequential_time << " μs" << std::endl;
    std::cout << "Sequential best time:    " << best_sequential_time << " μs" << std::endl;

    // ------------------------------------------------------------
    // Parallel for all stealing strategies

    std::vector<int> steal_styles = {
        NO_STEAL,
        NAIVE_STEAL,
        SMART_STEAL
    };

    for (int steal_style : steal_styles) {
        long long total_parallel_time = 0;
        long long best_parallel_time = std::numeric_limits<long long>::max();

        for (int iter = 0; iter < num_iterations; iter++) {
            Master<U, A> master(
                num_workers,
                seeds,
                successors,
                map_function,
                reduce_function,
                reduce_init,
                steal_style
            );

            auto start = std::chrono::steady_clock::now();

            A result = master.run();

            auto finish = std::chrono::steady_clock::now();

            long long elapsed = std::chrono::duration_cast<std::chrono::microseconds>(finish - start).count();

            size_t visited_set_size_parallel = master.get_visited_set_size();
            total_parallel_time += elapsed;
            best_parallel_time = std::min(best_parallel_time, elapsed);

            if (result != expected) {
                throw std::logic_error(
                    "Results mismatch in " + test_name +
                    " with " + steal_style_name(steal_style)
                );
            } else if (visited_set_size_parallel != visited_set_size) {
                throw std::logic_error(
                    "Visited set size mismatch in " + test_name +
                    " with " + steal_style_name(steal_style)
                );
            }
        }

        double average_parallel_time =
            static_cast<double>(total_parallel_time) / num_iterations;

        double average_speedup =
            average_sequential_time / average_parallel_time;

        double best_speedup =
            static_cast<double>(best_sequential_time) /
            static_cast<double>(best_parallel_time);

        average_time_vector.push_back(average_parallel_time);
        average_speedup_vector.push_back(average_speedup);

        std::cout << "\nMode: " << steal_style_name(steal_style) << std::endl;
        std::cout << "Average time: " << average_parallel_time << " μs" << std::endl;
        std::cout << "Best time:    " << best_parallel_time << " μs" << std::endl;
        std::cout << "Average speedup: " << average_speedup << "x" << std::endl;
        std::cout << "Best speedup:    " << best_speedup << "x" << std::endl;
    }

    // if we reach here means all results match
    // size also matches
    std::cout << "Visited set size: " << visited_set_size <<std::endl;
    std::cout << "TEST PASSSSSSSSSS" << std::endl;

    return {average_time_vector, average_speedup_vector};
}

template<typename U, typename A>
void plot_test_case(
    const std::string& test_name,
    int num_iterations,
    const std::vector<U>& seeds,
    std::function<std::vector<U>(const U&)> successors,
    std::function<A(const U&)> map_function,
    std::function<A(const A&, const A&)> reduce_function,
    A reduce_init,
    const std::vector<size_t>& num_workers_range
    ) {
    // we need to have two plots, one for time, one for speedup

    std::vector<double> sequential_time_vector;
    std::vector<double> NO_STEAL_time_vector;
    std::vector<double> NAIVE_STEAL_time_vector;
    std::vector<double> SMART_STEAL_time_vector;

    std::vector<double> NO_STEAL_speedup_vector;
    std::vector<double> NAIVE_STEAL_speedup_vector;
    std::vector<double> SMART_STEAL_speedup_vector;

    for (size_t num_worker : num_workers_range) {
        auto the_pair = run_test_case(test_name,
            num_worker,
            num_iterations,
            seeds,
            successors,
            map_function,
            reduce_function,
            reduce_init);

        std::vector<double> average_time_vector = the_pair.first;
        std::vector<double> average_speedup_vector = the_pair.second;

        sequential_time_vector.push_back(average_time_vector[0]);
        NO_STEAL_time_vector.push_back(average_time_vector[1]);
        NAIVE_STEAL_time_vector.push_back(average_time_vector[2]);
        SMART_STEAL_time_vector.push_back(average_time_vector[3]);

        NO_STEAL_speedup_vector.push_back(average_speedup_vector[1]);
        NAIVE_STEAL_speedup_vector.push_back(average_speedup_vector[2]);
        SMART_STEAL_speedup_vector.push_back(average_speedup_vector[3]);
    }

    std::ofstream csv_file("benchmark_results.csv");
    if (!csv_file.is_open()) {
        std::cerr << "Error opening file for writing CSV!" << std::endl;
        return;
    }

    // CSV Header
    csv_file << "Workers,Seq_Time,NoSteal_Time,NaiveSteal_Time,SmartSteal_Time,"
             << "NoSteal_Speedup,NaiveSteal_Speedup,SmartSteal_Speedup\n";

    // Write Data rows
    for (size_t i = 0; i < num_workers_range.size(); ++i) {
        csv_file << num_workers_range[i] << ","
                 << sequential_time_vector[i] << ","
                 << NO_STEAL_time_vector[i] << ","
                 << NAIVE_STEAL_time_vector[i] << ","
                 << SMART_STEAL_time_vector[i] << ","
                 << NO_STEAL_speedup_vector[i] << ","
                 << NAIVE_STEAL_speedup_vector[i] << ","
                 << SMART_STEAL_speedup_vector[i] << "\n";
    }

    csv_file.close();
}


int main(int argc, char **argv) {
    if (argc < 2) {
        incorrect_usage();
        return 0;
    }

    Graph graph(
    {0, 1, 2, 3, 4, 5, 6, 7, 8, 9,
     10, 11, 12, 13, 14, 15, 16, 17, 18, 19,
     20, 21, 22, 23, 24, 25, 26, 27, 28, 29},
    {
        {0, 1}, {0, 2}, {1, 3}, {2, 3}, {3, 4},
        {4, 5}, {5, 6}, {6, 7}, {7, 8}, {8, 9},
        {9, 10}, {10, 11}, {11, 12}, {12, 13}, {13, 14},
        {14, 15}, {15, 16}, {16, 17}, {17, 18}, {18, 19},
        {19, 20}, {20, 21}, {21, 22}, {22, 23}, {23, 24},
        {24, 25}, {25, 26}, {26, 27}, {27, 28}, {28, 29},

        {0, 5}, {1, 6}, {2, 7}, {3, 8}, {4, 9},
        {5, 10}, {6, 11}, {7, 12}, {8, 13}, {9, 14},
        {10, 15}, {11, 16}, {12, 17}, {13, 18}, {14, 19},
        {15, 20}, {16, 21}, {17, 22}, {18, 23}, {19, 24},
        {20, 25}, {21, 26}, {22, 27}, {23, 28}, {24, 29},

        {0, 10}, {2, 12}, {4, 14}, {6, 16}, {8, 18},
        {10, 20}, {12, 22}, {14, 24}, {16, 26}, {18, 28},

        {3, 15}, {5, 17}, {7, 19}, {9, 21}, {11, 23},
        {13, 25}, {15, 27}, {17, 29}
    });

    size_t n = graph.get_num_vertices();

    int begin = 0;
    int end = 12;

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

    int num_iterations = 5;

    std::vector<PartialPath> seeds = {PartialPath(begin, {begin})};

    std::function<std::vector<PartialPath>(const PartialPath&)> successors = [graph, end](const PartialPath& current_path) {
        std::vector<PartialPath> next_paths;

        if (current_path.current_node == end) {
            return next_paths;
        }

        for (int next_node : graph.get_neighbors(current_path.current_node)) {
            if (!current_path.has_visited(next_node)) {
                // we could extend this next node so that we have one next_path
                PartialPath next_path = current_path.extend_with(next_node);
                next_paths.push_back(next_path);
            }
        }

        return next_paths;
    };


    std::vector<size_t> num_workers_range;
    for (size_t i = 1; i <= num_workers; ++i) {
        num_workers_range.push_back(i);
    }

    // ---------------------------------------------------------------------------------------------------
    // Application 1: Count Hamiltonian paths
    // Here A is int, we want to return the num of paths that are Hamiltonian
    std::function<int(const PartialPath&)> map_function_Hamiltonian = [n, end](const PartialPath& current_path) {
        if (current_path.path.size() == n && current_path.current_node == end) {
            // since we make sure theres no revisiting, this implies the path is H
            return 1;
        } else {
            return 0;
        }
    };

    std::function<int(const int&, const int&)> reduce_function_Hamiltonian = [](const int& a, const int& b) -> int {
        return a + b;
    };

    int reduce_init_Hamiltonian = 0;

    plot_test_case(
        "Application 1: Count Hamiltonian paths",
        num_iterations,
        seeds,
        successors,
        map_function_Hamiltonian,
        reduce_function_Hamiltonian,
        reduce_init_Hamiltonian,
        num_workers_range
        );

    // run_test_case(
    //     "Application 1: Count Hamiltonian paths",
    //     num_workers,
    //     num_iterations,
    //     seeds,
    //     successors,
    //     map_function_Hamiltonian,
    //     reduce_function_Hamiltonian,
    //     reduce_init_Hamiltonian );




    // ---------------------------------------------------------------------------------------------------
    // Application 2: find the longest simple path between two nodes
    std::function<int(const PartialPath&)> map_function_Longest = [end](const PartialPath& current_path) -> int {
        if (current_path.current_node == end) {
            return static_cast<int>(current_path.path.size()) - 1; // we count the number of edges
        }

        return -1; // this path otherwise is not validdd
    };

    std::function<int(const int&, const int&)> reduce_function_Longest = [](const int& a, const int& b) -> int {
        return std::max(a, b);
    };

    int reduce_init_Longest = -1;

    // run_test_case(
    //     "Application 2: find the longest simple path between two nodes",
    //     num_workers,
    //     num_iterations,
    //     seeds,
    //     successors,
    //     map_function_Longest,
    //     reduce_function_Longest,
    //     reduce_init_Longest );
}