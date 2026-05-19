#ifndef WORKER_HPP
#define WORKER_HPP

#include <optional>
#include <vector>
#include <functional>
#include <condition_variable>
#include <mutex>

#include <Logger.h> // uses simple-cpp-logger
// control logging: compile with "cmake -DNO_LOGS=ON ../" or "cmake -DNO_LOGS=OFF ../"" (after only "cmake .." is enough)
#ifdef NO_LOGS
#define LogInfo(...) ((void)0)
#define LogTrace(...) ((void)0)
#endif

template<typename U, typename A>
class Master;

template<typename U, typename A>
class Worker {
private:
    size_t my_id;
    Master<U, A> *master;
    size_t num_workers;

    std::vector<U> tasks;
    std::function<std::vector<U>(const U&)> successors;

    std::function<A(const U&)> map_function;
    std::function<A(const A&, const A&)> reduce_function;
    A result;

    bool active;

    std::mutex tasks_lock;

    void map_reduce(const U& task) {
        U curr_task = task;

        // log
        LogInfo(
            "[Worker %i] Starting map_reduce on %s",
            my_id, std::to_string(curr_task).data()
        ); // FIXME assumes U can be converted to str

        while (true) {
            A mapped_val = map_function(curr_task);
            result = reduce_function(result, mapped_val);

            // compute successors
            std::vector<U> unexplored_successors = 
                master->get_visited()->extend(successors(curr_task));

            // log
            std::string s = "";
            for (U elt : unexplored_successors) s += std::to_string(elt) + " ";
            LogTrace(
                "[Worker %i] Mapped %i to %i, reduced to %i, got successors { %s}",
                my_id, curr_task, mapped_val, result, s.data()
            ); // FIXME assumes U is a number

            if (unexplored_successors.size() > 0) {
                curr_task = unexplored_successors.back();
                unexplored_successors.pop_back();
                if (unexplored_successors.size() > 0) {
                    // remaining successors will be map-reduced or stolen later
                    add_tasks(unexplored_successors);
                }
            } else break; // max depth reached
        }

        // log
        std::string s = "";
        for (U elt : tasks) s += std::to_string(elt) + " ";
        LogInfo(
            "[Worker %i] MapReduce computation over, remaining tasks are { %s}",
            my_id, s.data()
        );
    }

    std::optional<U> steal() {
        // size_t victim_id = (my_id + 1) % num_workers;
        // while (victim_id != my_id) {
        //     std::optional<U> maybe_task =
        //         master->get_workers()[victim_id]->take_task();
        //     if (maybe_task.has_value()) return maybe_task;

        //     victim_id = (victim_id + 1) % num_workers;
        // }

        return std::nullopt;
    }

    void add_tasks(std::vector<U> new_tasks) {
        std::lock_guard<std::mutex> tl(tasks_lock);
        for (U task : new_tasks) tasks.push_back(task);
    }

public:
    Worker(
        size_t my_id_,
        Master<U, A> *master_,
        std::vector<U> tasks_
    ):
        my_id(my_id_),
        master(master_),
        num_workers(master_->get_num_workers()),
        tasks(tasks_),
        successors(master_->get_successors()),
        map_function(master_->get_map_function()),
        reduce_function(master_->get_reduce_function()),
        result(master_->get_reduce_init()),
        active(false)
    {
        LogInfo("Created Worker with ID %i and %i initial tasks",
            my_id,
            tasks.size()
        );
    }

    // XXX for manual tests
    std::vector<U> get_tasks() {
        return tasks;
    }
    A get_result() {
        return result;
    }

    // TODO establish all getters worker needs from master

    void run() { // TODO make sure it is only called once
        LogInfo("[Worker %i] Starting...", my_id);

        active = true;
        // loop until shutdown or done
        while (active) {
            // try to take a node or steal
            std::optional<U> maybe_task = take_task();
            if (!maybe_task.has_value()) maybe_task = steal();

            if (maybe_task.has_value()) {
                // perform map_reduce
                U task = maybe_task.value();
                map_reduce(task);
            } else active = false; // no tasks left
        }
        LogInfo(
            "[Worker %i] Stopping with %i tasks remaining, and partial result %s",
            my_id, tasks.size(), std::to_string(result).data()
        );

        // send result to master and shut down
        // TODO allow master to manually stop the worker by setting active=false
        // master->receive_partial_result(result);

        // // TODO shut down properly

        // master->worker_finish();
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