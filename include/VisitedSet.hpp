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

    bool insert_vector(const std::vector<U>& elements) {
        // If insert fail, no element in the list will be inserted
        std::lock_guard<std::mutex> guard(mutex);
        std::unordered_set<U> tem;

        for (const auto& element : elements) {
            // check if it is in visited already
            if (visited.find(element) != visited.end()) {
                return false;
            }

            // check if elements has duplicates
            auto [it, inserted] = tem.insert(element);
            if (!inserted) {
                return false;
            }
        }

        std::unordered_set<U> inserted_set;

        try {
            for (const auto& element : elements) {
                auto [it, inserted] = visited.insert(element);
                if (!inserted) {
                    // This should not happen in theory but just in case
                    throw std::logic_error("insert_vector failed despite prior validation");
                }

                inserted_set.push_back(element);
            }
        } catch (...) {
            for (const auto& element : inserted_set) {
                visited.erase(element);
            }

            throw;
        }

        return true;
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
