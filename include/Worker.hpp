#ifndef WORKER_HPP
#define WORKER_HPP

#include <optional>
#include <type_traits>
#include <vector>
#include <deque>
#include <functional>
#include <condition_variable>
#include <mutex>
#include <atomic>

// #include <Logger.h> // uses simple-cpp-logger
// // control logging: compile with "cmake -DNO_LOGS=ON ../" or "cmake -DNO_LOGS=OFF ../"" (after only "cmake .." is enough)
// #ifdef NO_LOGS
// #define LogInfo(...) ((void)0)
// #define LogTrace(...) ((void)0)
// #endif

#define NO_STEAL 0
#define NAIVE_STEAL 1
#define SMART_STEAL 2

#define STEAL_THRESHOLD 2 // no stealing from workers with <=steal_threshold tasks

template<typename U, typename A>
class Master;

// template<typename T> // can be used both for tasks (U) and results (A)
// std::optional<std::string> to_log_string(const T& obj) {
//     if constexpr (requires { std::string(obj); }) {
//         return std::string(obj);
//     } else if constexpr (requires { std::to_string(obj); }) {
//         // opt 1
//         return std::to_string(obj);
//     } else if constexpr (requires { obj.to_string(); }) {
//         // opt 2
//         return obj.to_string();
//     } else {
//         // opt 3: other types
//         return std::nullopt;
//     }
// }

template<typename U, typename A>
class Worker {
private:
    size_t my_id;
    Master<U, A> *master;
    size_t num_workers;

    std::deque<U> tasks; // has O(1) pop_back, back, pop_front, front
    std::atomic<int>& num_tasks;
    std::mutex tasks_lock;
    std::function<std::vector<U>(const U&)> successors;

    std::function<A(const U&)> map_function;
    std::function<A(const A&, const A&)> reduce_function;
    A result;
    
    bool shutdown_request;

    std::function<std::optional<U>()> steal; // the steal function to use
    std::condition_variable victim_found;
    std::optional<size_t> provided_victim_id;
    std::optional<size_t> provided_num_steal;
    std::mutex theft_lock; // for provided_victim_id and provided_num_steal

    void map_reduce(const U& task) {
        U curr_task = task;

        // // log
        // std::optional<std::string> task_str = to_log_string(task);
        // if (task_str.has_value()) {
        //     LogInfo(
        //         "[Worker %i] Starting map_reduce on %s",
        //         my_id, task_str.value().c_str()
        //     );
        // } else LogInfo("[Worker %i] Starting map_reduce", my_id);

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

        // // log
        // std::string s = "";
        // bool converted = true;
        // for (U elt : tasks) {
        //     std::optional<std::string> task_str = to_log_string(elt);
        //     if (!task_str.has_value()) {
        //         converted = false;
        //         break;
        //     }
        //     s += task_str.value() + " ";
        // }
        // if (converted) {
        //     LogInfo(
        //         "[Worker %i] MapReduce computation over, remaining tasks are { %s}",
        //         my_id, s.data()
        //     );
        // } else {
        //     LogInfo(
        //         "[Worker %i] MapReduce computation over, %i tasks remaining",
        //         my_id, tasks.size()
        //     );
        // }
    }

    // no steal: once the worker runs out of tasks it shuts down
    std::optional<U> no_steal() {
        master->worker_finish();
        return std::nullopt;
    }

    // naive steal: worker goes through all other workers one by one until a task is acquired (or shutdown is initiated)
    std::optional<U> naive_steal() {
        master->worker_finish();

        size_t victim_id = (my_id + 1) % num_workers;
        while (!shutdown_request) {
            if (victim_id == my_id) {
                victim_id = (victim_id + 1) % num_workers;
                continue;
            }
            std::vector<U> acquired_tasks =
                master->get_worker(victim_id)->steal_tasks(std::nullopt);
            
            if (acquired_tasks.size() != 0) {
                // LogInfo(
                //     "[Worker %i] Stole %i tasks from Worker %i",
                //     my_id, acquired_tasks.size(), victim_id
                // );
                master->worker_restart();

                // take one task to process immediately and add remaining to tasks queue
                U task = acquired_tasks.back();
                acquired_tasks.pop_back();
                add_tasks(acquired_tasks);
                return task;
            }

            victim_id = (victim_id + 1) % num_workers;
        }
        return std::nullopt;
    }

    // smart steal: worker sends a steal request to master who provides a worker to steal from
    std::optional<U> smart_steal() {
        while (!shutdown_request) {
            size_t victim_id = my_id;
            int num_steal = 1; // fallback value
            {
                std::unique_lock<std::mutex> tl(theft_lock);
                
                // reset provided_victim_id
                provided_victim_id = std::nullopt;

                // sleep until master gives us a victim
                master->request_steal(my_id);
                // LogTrace("[Worker %i] Sent steal request to master", my_id);
                victim_found.wait(tl, [this] {
                    return shutdown_request || provided_victim_id.has_value();
                });

                if (shutdown_request) break;

                victim_id = provided_victim_id.value();
                num_steal = provided_num_steal.value();
                // LogTrace(
                //     "[Worker %i] Woke up with provided id %i and steal amount %i",
                //     my_id, victim_id, num_steal
                // );
            }

            if (victim_id == my_id) {
                // LogTrace("[Worker %i] No one to steal from currently", my_id);
                continue;
            }
            
            // try to steal from the given victim
            std::vector<U> acquired_tasks =
                master->get_worker(victim_id)->steal_tasks(num_steal);
            
            if (acquired_tasks.size() != 0) {
                // LogInfo(
                //     "[Worker %i] Stole %i tasks from Worker %i",
                //     my_id, acquired_tasks.size(), victim_id
                // );
                master->worker_restart();

                // take one task to process immediately and add remaining to tasks
                U task = acquired_tasks.back();
                acquired_tasks.pop_back();
                add_tasks(acquired_tasks);
                return task;
            }
            
            // if the theft failed, then we send a request again
            // LogTrace("[Worker %i] Steal failed", my_id);
        }
        return std::nullopt;
    }

    std::optional<U> take_task() {
        std::lock_guard<std::mutex> tl(tasks_lock);
        if (tasks.size() > 0) {
            U next_task = tasks.back();
            tasks.pop_back();
            //num_tasks.fetch_sub(1);
            num_tasks.store(tasks.size());
            return next_task;
        }
        return std::nullopt;
    }

    void add_tasks(std::vector<U> new_tasks) {
        std::lock_guard<std::mutex> tl(tasks_lock);
        for (U& task : new_tasks) tasks.push_back(task);
        num_tasks.store(tasks.size());
    }

public:
    Worker(
        size_t my_id_,
        Master<U, A> *master_,
        std::vector<U> tasks_,
        std::atomic<int>& num_tasks_,
        int steal_type
    ):
        my_id(my_id_),
        master(master_),
        num_workers(master_->get_num_workers()),
        successors(master_->get_successors()),
        map_function(master_->get_map_function()),
        reduce_function(master_->get_reduce_function()),
        result(master_->get_reduce_init()),
        shutdown_request(false),
        num_tasks(num_tasks_),
        provided_victim_id(my_id_)
    {
        // initiate the tasks queue
        tasks = std::deque<U>(tasks_.begin(), tasks_.end());

        // choose the correct steal function
        switch (steal_type) {
            case NO_STEAL:
                // no steal
                steal = [this]() { return no_steal(); };
                break;
            case NAIVE_STEAL:
                // naive steal
                steal = [this]() { return naive_steal(); };
                break;
            case SMART_STEAL:
                // smart steal
                steal = [this]() { return smart_steal(); };
                break;
            
            default:
                throw std::runtime_error("Improper steal type provided");
                break;
        }

        // LogInfo("Created Worker with ID %i and %i initial tasks",
        //     my_id,
        //     tasks.size()
        // );
    }

    void run() {
        // LogInfo("[Worker %i] Starting...", my_id);

        // loop until shutdown or done
        while (!shutdown_request) {
            // try to take a node or steal
            std::optional<U> maybe_task = take_task();
            if (!maybe_task.has_value()) maybe_task = steal();

            if (maybe_task.has_value()) {
                // perform map_reduce
                U task = maybe_task.value();
                map_reduce(task);
            } else break; // for "no steal" -- stop once all tasks have been processed
        }

        // std::optional<std::string> res_str = to_log_string(result);
        // if (res_str.has_value()) {
        //     LogInfo(
        //         "[Worker %i] Stopping with %i tasks remaining, and partial result %s",
        //         my_id, tasks.size(), res_str.value().data()
        //     );
        // } else {
        //     LogInfo(
        //         "[Worker %i] Stopping with %i tasks remaining",
        //         my_id, tasks.size()
        //     );
        // }

        // send result to master and shut down
        master->receive_partial_result(std::ref(result));
    }

    std::vector<U> steal_tasks(std::optional<int> maybe_num_steal) {
        std::lock_guard<std::mutex> tl(tasks_lock);
        std::vector<U> stolen; // returned empty if stealing is unsuccessful

        if (tasks.size() > STEAL_THRESHOLD) {
            // steal as many tasks as said by master OR try stealing 1/num_workers of all tasks
            int num_steal = maybe_num_steal.value_or(std::max<int>(1, (int)tasks.size() / num_workers));

            for (int i = 0; i < num_steal; ++i) {
                stolen.push_back(tasks.front());
                tasks.pop_front();
            }
            num_tasks.store(tasks.size());
        }

        return stolen;
    }

    void request_shutdown() {
        // LogInfo("[Worker %i] Told to shut down by master", my_id);
        shutdown_request = true;
        victim_found.notify_one(); // instead of hanging waiting for a victim should shut down
    }

    void set_victim(std::pair<size_t, int> victim_pair) {
        std::unique_lock<std::mutex> tl(theft_lock);
        provided_victim_id = victim_pair.first;
        provided_num_steal = victim_pair.second;
        victim_found.notify_one();
    }
};

#endif //WORKER_HPP
