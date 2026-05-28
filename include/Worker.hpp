#ifndef WORKER_HPP
#define WORKER_HPP

#include <optional>
#include <type_traits>
#include <vector>
#include <functional>
#include <condition_variable>
#include <mutex>
#include <atomic>

#include <Logger.h> // uses simple-cpp-logger
// control logging: compile with "cmake -DNO_LOGS=ON ../" or "cmake -DNO_LOGS=OFF ../"" (after only "cmake .." is enough)
#ifdef NO_LOGS
#define LogInfo(...) ((void)0)
#define LogTrace(...) ((void)0)
#endif

template<typename U, typename A>
class Master;

template<typename U>
std::string task_to_log_string(const U& task) {
    if constexpr (requires { std::string(task); }) {
        return std::string(task);
    } else if constexpr (requires { std::to_string(task); }) {
        // opt 1
        return std::to_string(task);
    } else if constexpr (requires { task.to_string(); }) {
        // opt 2
        return task.to_string();
    } else {
        // opt 3: other types
        return "";
    }
}

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

    std::atomic<int>& num_tasks; // for communication with master
    std::condition_variable victim_found;
    
    std::optional<size_t> provided_victim_id;
    std::mutex theft_lock; // for provided_victim_id

    std::mutex tasks_lock;

    void map_reduce(const U& task) {
        U curr_task = task;

        std::string task_str = task_to_log_string(task);

        if (!task_str.empty()) {
            LogInfo(
                "[Worker %i] Starting map_reduce on %s",
                my_id, task_str.c_str()
                );
        } else {
            LogInfo("[Worker %i] Starting map_reduce", my_id);
        }

        // log
        // opt 1: on numbers
        // LogInfo(
        //    "[Worker %i] Starting map_reduce on %s",
        //    my_id, std::to_string(curr_task).data()
        // );
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
        for (U elt : tasks) s += task_to_log_string(elt) + " ";
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

    std::optional<U> steal2() {
        while (!shutdown_request) {
            size_t victim_id = my_id;
            {
                std::unique_lock<std::mutex> tl(theft_lock);
                
                // reset provided_victim_id
                provided_victim_id = std::nullopt;

                // sleep until master gives us a victim
                master->request_steal(my_id);
                LogTrace("[Worker %i] Sent steal request to master", my_id);
                victim_found.wait(tl, [this] {
                    return shutdown_request || provided_victim_id.has_value();
                });

                if (shutdown_request) break;

                victim_id = provided_victim_id.value();
                LogTrace("[Worker %i] Woke up with provided id %i", my_id, provided_victim_id);
            }

            if (victim_id == my_id) {
                LogTrace("[Worker %i] No one to steal from currently", my_id);
                usleep(500); // to not overwhelm with requests???
                continue;
            }
            
            // try to steal from the given victim
            std::optional<U> maybe_task =
                master->get_worker(victim_id)->take_task();
            if (maybe_task.has_value()) {
                LogInfo("[Worker %i] Stole task from Worker %i", my_id, victim_id);
                return maybe_task;
            }
            // if the theft failed, then we send a request again
            LogTrace("[Worker %i] Steal failed", my_id);
        }
        return std::nullopt;
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
        num_tasks.fetch_add(new_tasks.size());
    }

public:
    Worker(
        size_t my_id_,
        Master<U, A> *master_,
        std::vector<U> tasks_,
        std::atomic<int>& num_tasks_
    ):
        my_id(my_id_),
        master(master_),
        num_workers(master_->get_num_workers()),
        tasks(tasks_),
        successors(master_->get_successors()),
        map_function(master_->get_map_function()),
        reduce_function(master_->get_reduce_function()),
        result(master_->get_reduce_init()),
        shutdown_request(false),
        num_tasks(num_tasks_),
        provided_victim_id(my_id_)
    {
        LogInfo("Created Worker with ID %i and %i initial tasks",
            my_id,
            tasks.size()
        );
    }

    // TODO establish all getters worker needs from master

    void run() { // TODO make sure it is only called once
        LogInfo("[Worker %i] Starting...", my_id);

        // loop until shutdown or done
        while (!shutdown_request) {
            // try to take a node or steal
            std::optional<U> maybe_task = take_task();
            if (!maybe_task.has_value()) maybe_task = steal2(); // XXX the second version

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
        master->receive_partial_result(std::ref(result));
    }

    std::optional<U>  take_task() {
        std::lock_guard<std::mutex> tl(tasks_lock);
        if (tasks.size() > 0) {
            U next_task = tasks.back();
            tasks.pop_back();
            num_tasks.fetch_sub(1);
            return next_task;
        }
        return std::nullopt;
    }

    void request_shutdown() {
        LogInfo("[Worker %i] Told to shut down by master", my_id);
        shutdown_request = true;
        victim_found.notify_one(); // instead of hanging waiting for a victim should shut down
    }

    void set_victim(size_t victim_id) {
        std::unique_lock<std::mutex> tl(theft_lock);
        provided_victim_id = victim_id;
        victim_found.notify_one();
    }
};

#endif //WORKER_HPP
