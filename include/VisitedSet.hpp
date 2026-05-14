//
// Created by $JingyanZhang on 14/05/2026.
//

// Coase gain version to store all visited set using unordered set

#ifndef VISITEDSET_HPP
#define VISITEDSET_HPP

#include <unordered_set>

template<typename U>
class VisitedSet {
private:
    std::unordered_set<U> visited;
    std::mutex mutex;

public:
    bool insert(U element) {
        // try inserting the elements in the visited set, if the element already exits, return false
        // otherwise return true
        std::lock_guard<std::mutex> guard(mutex);

        auto [it, inserted] = visited.insert(element);
        return inserted;
    }

    bool contain(U element) {
        std::lock_guard<std::mutex> guard(mutex);
        return visited.find(element) != visited.end();
    }

    size_t size() {
        std::lock_guard<std::mutex> guard(mutex);
        return visited.size();
    }
};

#endif //VISITEDSET_HPP
