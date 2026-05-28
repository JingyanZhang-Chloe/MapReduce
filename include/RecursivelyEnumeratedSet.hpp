//
// Created by $JingyanZhang on 08/05/2026.
//

#ifndef RECURSIVELYENUMERATEDSET_HPP
#define RECURSIVELYENUMERATEDSET_HPP

#include <vector>
#include <functional>
#include <unordered_set>

template<typename U, typename A>
class RESetMapReduce {
private:
    std::vector<U> seeds;
    std::function<std::vector<U>(const U&)> successors;

public:
    //constructor to make the RE set
    RESetMapReduce(std::vector<U> seeds_, std::function<std::vector<U>(const U&)> successors_) {
        this->seeds = seeds_;
        this->successors = successors_;
    }

    //map reduce function
    // This function DOES NOT check duplicates
    // This do not need to store the full S
    A map_reduce(std::function<A(const U&)> map_function, std::function<A(const A&, const A&)> reduce_function, A reduce_init) {
        A result = reduce_init;
        std::vector<U> stack = seeds;

        while (!stack.empty()) {
            U current = stack.back();
            stack.pop_back();

            result = reduce_function(result, map_function(current));

            std::vector<U> children = successors(current);
            for (const U& child : children) {
                stack.push_back(child);
            }
        }

        return result;
    };

    // This function works on recursively enumerated graphs, where each elements can be reached multiple times by diff seeds
    // We should CHECK if it is possible to avoid storing all elements in S
    A map_reduce_avoid_duplicate(std::function<A(const U&)> map_function, std::function<A(const A&, const A&)> reduce_function, A reduce_init) {
        A result = reduce_init;
        std::vector<U> stack = seeds;
        std::vector<U> visited;

        for (const U& seed : seeds) {
            if (std::find(visited.begin(), visited.end(), seed) == visited.end()) {
                visited.push_back(seed);
            }
        }

        while (!stack.empty()) {
            U current = stack.back();
            stack.pop_back();

            result = reduce_function(result, map_function(current));

            std::vector<U> children = successors(current);
            for (const U& child : children) {
                if (std::find(visited.begin(), visited.end(), child) == visited.end()){
                    visited.push_back(child);
                    stack.push_back(child);
                }
            }
        }

        return result;
    }
};

#endif //RECURSIVELYENUMERATEDSET_HPP
