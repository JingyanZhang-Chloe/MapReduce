#ifndef WORKER_HPP
#define WORKER_HPP

#include <optional>
#include <type_traits>
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

    //bool active;
    bool shutdown_request;

    //const int& num_tasks; // TODO add and remove when manipulating tasks

    std::mutex tasks_lock;

    void map_reduce(const U& task) {
        U curr_task = task;

        // log
        // opt 1: on numbers
        LogInfo(
            "[Worker %i] Starting map_reduce on %s",
            my_id, std::to_string(curr_task).data()
        );
        // opt 2: on custom with to_string method
        // LogInfo(
        //     "[Worker %i] Starting map_reduce on %s",
        //     my_id, curr_task.to_string().data()
        // );
        // opt 3: on other types
        // LogInfo("[Worker %i] Starting map_reduce", my_id);

        while (!shutdown_request) {
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

        // log
        // opt 1: on numbers
        std::string s = "";
        for (U elt : tasks) s += std::to_string(elt) + " ";
        LogInfo(
            "[Worker %i] MapReduce computation over, remaining tasks are { %s}",
            my_id, s.data()
        );
        // opt 2: on custom types with to_string method
        // std::string s = "";
        // for (U elt : tasks) s += elt.to_string() + " ";
        // LogInfo(
        //     "[Worker %i] MapReduce computation over, remaining tasks are { %s}",
        //     my_id, s.data()
        // );
        // opt 3: on other types
        // LogInfo(
        //     "[Worker %i] MapReduce computation over, 0 tasks remaining",
        //     my_id
        // );
    }

    std::optional<U> steal() {
        master->worker_finish();

        // TODO what if we try all and fail but in the meantime someone computes a lot of new successors?
        // XXX keep trying until shutdown (busy), sleep until some signal?
        size_t victim_id = (my_id + 1) % num_workers;
        while (!shutdown_request) {
            if (victim_id == my_id) {
                victim_id = (victim_id + 1) % num_workers;
                continue;
            }
            std::optional<U> maybe_task =
                master->get_worker(victim_id)->take_task();
            if (maybe_task.has_value()) {
                LogInfo("[Worker %i] STOLE task from Worker %i", my_id, victim_id);
                master->worker_restart();
                return maybe_task;
            }

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
        Master<U, A> *master_,
        std::vector<U> tasks_
        //const int& num_tasks_
    ):
        my_id(my_id_),
        master(master_),
        num_workers(master_->get_num_workers()),
        tasks(tasks_),
        successors(master_->get_successors()),
        map_function(master_->get_map_function()),
        reduce_function(master_->get_reduce_function()),
        result(master_->get_reduce_init()),
        shutdown_request(false)
        //num_tasks(num_tasks_)
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

        //active = true;
        // loop until shutdown or done
        while (!shutdown_request) {
            // try to take a node or steal
            std::optional<U> maybe_task = take_task();
            if (!maybe_task.has_value()) maybe_task = steal();

            if (maybe_task.has_value()) {
                // perform map_reduce
                U task = maybe_task.value();
                map_reduce(task);
            } //else active = false; // no tasks left
        }
        LogInfo(
            "[Worker %i] Stopping with %i tasks remaining, and partial result %s",
            my_id, tasks.size(), std::to_string(result).data()
        ); // FIXME assumes that A can be converted to string

        // send result to master and shut down
        // TODO allow master to manually stop the worker by setting active=false
        master->receive_partial_result(std::ref(result));

        // // TODO shut down properly

        //master->worker_finish();
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

    void request_shutdown() {
        LogInfo("[Worker %i] Told to shut down by master", my_id);
        shutdown_request = true;
    }
};

#endif //WORKER_HPP
