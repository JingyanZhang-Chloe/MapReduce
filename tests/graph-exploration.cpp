//
// Created by $JingyanZhang on 26/05/2026.
//

#include <Master.hpp>
#include <iostream>

#include <vector>
#include <utility>
#include <functional>
#include <algorithm>
#include <string>

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


int main(int argc, char **argv) {
    if (argc < 2) {
        incorrect_usage();
        return 0;
    }

    Graph graph(
        {0, 1, 2, 3},
        {
            {0, 1},
            {0, 2},
            {1, 2},
            {1, 3},
            {2, 3}
        });

    size_t n = graph.get_num_vertices();

    int begin = 0;
    int end = 3;

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

    Master<PartialPath, int> master_Hamiltonian(
        num_workers,
        seeds,
        successors,
        map_function_Hamiltonian,
        reduce_function_Hamiltonian,
        reduce_init_Hamiltonian
    );

    int number_hamiltonian_paths = master_Hamiltonian.run2();
    std::cout << "Number of Hamiltonian paths: " << number_hamiltonian_paths << std::endl;

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

    Master<PartialPath, int> master_Longest(
        num_workers,
        seeds,
        successors,
        map_function_Longest,
        reduce_function_Longest,
        reduce_init_Longest
    );

    int longest_path_length = master_Longest.run2();
    std::cout << "Longest path length: " << longest_path_length << std::endl;
}