#ifndef WORKER_HPP
#define WORKER_HPP

#include <optional>
#include <vector>
#include <functional>
#include <condition_variable>
#include <mutex>

class Master;

template<typename U, typename A>
class Worker {
private:
    size_t my_id;
    Master *master;
    size_t num_workers;

    std::vector<std::vector<U>> tasks;
    std::function<std::vector<U>(const U&)> successors;

    std::function<A(const U&)> map_function;
    std::function<A(const A&, const A&)> reduce_function;
    A result;

    bool active;

    std::mutex tasks_lock;

    void map_reduce(const U& task) {
        U curr_task = U(task);
        while (true) {
            A mapped_val = map_function(curr_task);
            result = reduce_function(result, mapped_val);

            // compute successors
            std::vector<U> unexplored_successors =
                master->get_visited_set().extend(successors(curr_task));

            if (unexplored_successors.size() > 0) {
                curr_task = unexplored_successors.back();
                unexplored_successors.pop_back();
                if (unexplored_successors.size() > 0) {
                    // remaining successors will be map-reduced or stolen later
                    add_tasks(unexplored_successors);
                }
            } else break; // max depth reached
        }
    }

    std::optional<U> steal() {
        size_t victim_id = (my_id + 1) % num_workers;
        while (victim_id != my_id) {
            std::optional<U> maybe_task =
                master->get_workers()[victim_id]->take_task();
            if (maybe_task.has_value()) return maybe_task;

            victim_id = (victim_id + 1) % num_workers;
        }
        return std::nullopt;
    }

    void add_tasks(std::vector<U> new_tasks) {
        std::lock_guard<std::mutex> tl(tasks_lock);
        for (U task : new_tasks) tasks.push_back(task);
    }

public:
    Worker(
        size_t my_id_,
        Master *master_,
        std::vector<std::vector<U>> tasks_
    ):
        my_id(my_id_),
        master(master_),
        num_workers(master_->get_num_workers),
        tasks(tasks_),
        successors(master_->get_successors()),
        map_function(master_->get_map_function()),
        reduce_function(master_->get_reduce_function()),
        result(master_->get_reduce_init()),
        active(false)
    {}
    // TODO establish all getters worker needs from master

    void run() {
        active = true;
        // loop until shutdown or done
        while (active) {
            // try to take a node or steal
            std::optional<U> maybe_task = take_task();
            if (!maybe_task.has_value()) maybe_task = steal;

            if (maybe_task.has_value()) {
                // perform map_reduce
                U task = maybe_task.value();
                map_reduce(task);
            } else active = false; // no tasks left
        }
        // send result to master and shut down
        // TODO allow master to manually stop the worker by setting active=false
        master->receive_partial_result(result);

        // TODO shut down properly

        master->worker_finish();
    }

    std::optional<U>  take_task() {
        std::lock_guard<std::mutex> tl(tasks_lock);
        if (tasks.size() > 0) {
            U next_task = tasks.back();
            tasks.pop_back();
            return next_task;
        }
        return std::nullopt;
    }

    bool is_active() {
        // TODO thread safety (if master can change active for abort)
        return active;
    }
};

#endif //WORKER_HPP