//
// Created by $JingyanZhang on 14/05/2026.
//

#ifndef VISITEDSET_HPP
#define VISITEDSET_HPP

#include <unordered_set>
#include <vector>
#include <array>
#include <mutex>
#include <iostream>

// XXX fine-grained with unordered_set
template<typename U>
class VisitedSet {
private:
    static constexpr size_t NUM_GROUPS = 32; // more than workers

    // XXX change to class with dedicated methods??
    struct NodeGroup {
        std::mutex mutex;
        std::unordered_set<U> elements;
    };

    std::array<NodeGroup, NUM_GROUPS> groups; // fixed size so array
    
    // get group index from value
    size_t groud_index(const U& node) const {
        size_t hash = std::hash<U>{}(node);
        return hash % NUM_GROUPS;
    }

public:
    VisitedSet(std::vector<U>& seeds) {
        for (const U& node : seeds) {
            size_t i = groud_index(node);
            groups[i].elements.insert(node);
        }
    }

    // adds only elements not already in visited, returns all such elements
    std::vector<U> extend(std::vector<U> elements) {
        // group elements
        std::array<std::vector<U>, NUM_GROUPS> elements_grouped;
        for (const U& elem : elements) elements_grouped[groud_index(elem)].push_back(elem);

        std::vector<U> unexplored{};

        // go by group
        // XXX use while loop untill all sections added then lock_once to avoid waiting on each lock in order??
        // XXX or randomize group analysis order??
        for (size_t i = 0; i < NUM_GROUPS; ++i) {
            if (elements_grouped[i].empty()) continue; // no nodes for this group

            {
                std::lock_guard<std::mutex> lk(groups[i].mutex);

                for (const U& elem : elements_grouped[i]) {
                    auto [it, inserted] = groups[i].elements.insert(elem);
                    if (inserted) {
                        // this is a new node
                        unexplored.push_back(elem);
                    }
                }
            }
        }
        
        return unexplored;
    }
};

// XXX fine-grained with vector
// template<typename U>
// class VisitedSet {
// private:
//     static constexpr size_t NUM_GROUPS = 32; // more than workers

//     // XXX change to class with dedicated methods??
//     struct NodeGroup {
//         std::mutex mutex;
//         std::vector<U> elements;
//     };

//     std::array<NodeGroup, NUM_GROUPS> groups; // fixed size so array
    
//     // get group index from value
//     size_t groud_index(const U& node) const {
//         size_t hash = std::hash<U>{}(node);
//         return hash % NUM_GROUPS;
//     }

// public:
//     VisitedSet(std::vector<U>& seeds) {
//         for (const U& node : seeds) {
//             size_t i = groud_index(node);
//             groups[i].elements.push_back(node);
//         }
//     }

//     // adds only elements not already in visited, returns all such elements
//     std::vector<U> extend(std::vector<U> elements) {
//         // group elements
//         std::array<std::vector<U>, NUM_GROUPS> elements_grouped;
//         for (const U& elem : elements) elements_grouped[groud_index(elem)].push_back(elem);

//         std::vector<U> unexplored{};

//         // go by group
//         for (size_t i = 0; i < NUM_GROUPS; ++i) {
//             if (elements_grouped[i].empty()) continue; // no nodes for this group

//             {
//                 std::lock_guard<std::mutex> lk(groups[i].mutex);

//                 for (const U& elem : elements_grouped[i]) {
//                     // FIXME find is O(n)
//                     if (std::find(groups[i].elements.begin(), groups[i].elements.end(), elem) == groups[i].elements.end()) {
//                         // this is a new node
//                         unexplored.push_back(elem);
//                         groups[i].elements.push_back(elem);
//                     }
//                 }
//             }
//         }
        
//         return unexplored;
//     }
// };

// XXX coarse-grained with vector
// template<typename U>
// class VisitedSet {
// private:
//     std::vector<U> visited;
//     std::mutex mutex;

// public:
//     VisitedSet(std::vector<U> seeds) {
//         std::vector<U> initial_set(seeds.begin(), seeds.end());
//         visited = initial_set;
//     }

//     size_t size() {
//         std::lock_guard<std::mutex> guard(mutex);
//         return visited.size();
//     }

//     // XXX another way of adding a vector - fewer times locking??
//     // adds only elements not already in visited, returns all such elements
//     std::vector<U> extend(std::vector<U> elements) {
//         std::lock_guard<std::mutex> guard(mutex);

//         std::vector<U> unexplored{};

//         for (const auto& element : elements) {
//             if (std::find(visited.begin(), visited.end(), element) == visited.end()) {
//                 // this element is new to visited (possibly not new to unexplored)
//                 visited.push_back(element);
//                 if (std::find(
//                         unexplored.begin(), unexplored.end(), element
//                     ) == unexplored.end()) {
//                     unexplored.push_back(element);
//                 }
//             }
//         }

//         return unexplored;
//     }
// };

#endif //VISITEDSET_HPP
