//
// Created by $JingyanZhang on 14/05/2026.
//

#ifndef MASTER_HPP
#define MASTER_HPP

#include <utility>
#include <vector>
#include <functional>
#include <condition_variable>
#include <thread>
#include <string>
#include <stdexcept>
#include <algorithm>
#include <cstddef>
#include <mutex>
#include <memory>
#include <iostream>

#include "VisitedSet.hpp"
#include "Worker.hpp"

#include <Logger.h> // uses simple-cpp-logger
// control logging: compile with "cmake -DNO_LOGS=ON ../" or "cmake -DNO_LOGS=OFF ../"" (after only "cmake .." is enough)
#ifdef NO_LOGS
#define LogInfo(...) ((void)0)
#define LogTrace(...) ((void)0)
#endif


template<typename U, typename A>
class Master {

private:
    const size_t num_workers;

    // TODO: Check if this should be passed separately or by RecursivelyEnumeratedSet,
    const std::vector<U> seeds;
    const std::function<std::vector<U>(const U&)> successors;

    const std::function<A(const U&)> map_function;
    const std::function<A(const A&, const A&)> reduce_function;
    const A reduce_init;

    VisitedSet<U> visited;
    size_t num_active_workers;
    std::condition_variable wake_up_my_master;
    std::vector<std::unique_ptr<Worker<U, A>>> workers;
    std::vector<std::thread> worker_threads{};
    std::vector<std::atomic<int>> list_num_tasks;
    std::vector<size_t> request_steal_queue;
    // In theory we dont need this, but i just feel a bit unsafe so i add it
    // We could just check request_steal_queue.size()
    int num_waiting_workers;
    int steal_type;

    std::mutex mutex;
    bool shutdown_request;
    bool has_error;
    std::string error_message;

    std::vector<A> partial_results;

    void start_workers() {
        if (num_workers == 0) {
            throw std::invalid_argument("Master requires at least one worker");
        }

        std::vector<std::vector<U>> initial_task(num_workers);

        for (size_t i = 0; i < seeds.size(); ++i) {
            size_t worker_id = i % num_workers;
            initial_task[worker_id].push_back(seeds[i]);
            this->list_num_tasks[worker_id].fetch_add(1);
        }

        for (size_t i = 0; i < num_workers; ++i) {
            // This part check with Worker actual implementation
            // TODO: check with Worker actual implementation
            size_t worker_id = i % num_workers;
            workers.push_back(
                std::make_unique<Worker<U,A>>(
                    worker_id,
                    this,
                    initial_task[worker_id],
                    std::ref(this->list_num_tasks[worker_id]),
                    this->steal_type
                    )
                );
        }

        {
            std::lock_guard<std::mutex> guard(mutex);
            num_active_workers = num_workers;
        }

        for (size_t i = 0; i < num_workers; ++i) {
            worker_threads.emplace_back(
                &Worker<U,A>::run,
                workers[i].get()
                );
        }
    }

    void join_workers() {
        // TODO : Manually shut down by setting the workers field to false
        for (size_t i = 0; i < this->num_workers; ++i) {
            this->workers[i]->request_shutdown();
            if (this->worker_threads[i].joinable()) {
                this->worker_threads[i].join();
            }
        }
    }

    std::string get_error_message() {
        std::lock_guard<std::mutex> guard(mutex);
        return error_message;
    }

    A final_reduce() {
        std::lock_guard<std::mutex> guard(mutex);
        A result = reduce_init;

        for (const A& partial_reuslt : partial_results) {
            result = reduce_function(result, partial_reuslt);
        }

        return result;
    }

    std::pair<size_t, int> choose_victim(size_t theif) {
        size_t victim = theif;
        int best = 0;

        for (size_t i = 0; i < this->list_num_tasks.size(); ++i) {
            if (i == theif) {
                continue;
            }

            int current = this->list_num_tasks[i].load();

            if (current >= best) {
                victim = i;
                best = current;
            }
        }

        // here the queue is not locked, so probably we could use the num of total workers
        int steal_amount = best / num_workers;
        return std::pair(victim, steal_amount);
    }


public:
    Master(
        size_t num_workers_,
        std::vector<U> seeds_,
        std::function<std::vector<U>(const U&)> successors_,
        std::function<A(const U&)> map_function_,
        std::function<A(const A&, const A&)> reduce_function_,
        A reduce_init_,
        int steal_type_
    )
        : num_workers(num_workers_),
          seeds(seeds_),
          successors(successors_),
          map_function(map_function_),
          reduce_function(reduce_function_),
          reduce_init(reduce_init_),
          visited(VisitedSet(seeds_)),
          num_active_workers(0),
          list_num_tasks(static_cast<std::vector<int>::size_type>(num_workers_)),
          num_waiting_workers(0),
          steal_type(steal_type_),
          shutdown_request(false),
          has_error(false)
    {
        workers.reserve(num_workers);
        worker_threads.reserve(num_workers);

        for (auto& x : list_num_tasks) {
            x.store(0);
        }
    }

    void abort() {
        std::lock_guard<std::mutex> guard(mutex);
        has_error = true;
        shutdown_request = true;
        wake_up_my_master.notify_all();
    }

    // ---------------------------------------------------
    // Here are some other functions that Workers can call to get, instead of passing them as input for Worker

    size_t get_visited_set_size() {
        std::lock_guard<std::mutex> guard(mutex);
        return visited.size();
    }

    void request_steal(size_t worker_id) {
        std::lock_guard<std::mutex> guard(mutex);

        request_steal_queue.push_back(worker_id);
        ++num_waiting_workers;
        wake_up_my_master.notify_all();

        // LogInfo("[Master] Worker with ID %zu added to the request steal queue",
        //     worker_id
        // );
    }

    void receive_partial_result(const A& partial_result) {
        // This function is supposed to call by Worker after Worker has done her computation
        // i.e. Worker call master->receive_partial_result(partial_result);

        std::lock_guard<std::mutex> guard(mutex);
        partial_results.push_back(partial_result);
    }

    // Used only in version 1
    void worker_finish() {
        // This function is supposed to call by Worker after Worker has done her computation and called receive_partial_result
        // i.e. Worker call master->worker_finish();

        std::lock_guard<std::mutex> guard(mutex);

        if (num_active_workers > 0) {
            --num_active_workers;
        }

        if (num_active_workers == 0) {
            shutdown_request = true;
            wake_up_my_master.notify_all();
        }
    }

    // Used only in version 1
    void worker_restart() {
        std::lock_guard<std::mutex> guard(mutex);
        ++num_active_workers;
    }

    bool should_shutdown() {
        std::lock_guard<std::mutex> guard(mutex);
        return shutdown_request;
    }

    VisitedSet<U>& get_visited_set() {
        // Should we lock it?
        return visited;
    }

    size_t get_num_workers() {
        return this->num_workers;
    }

    std::function<A(const U&)> get_map_function() {
        return this->map_function;
    }

    std::function<A(const A&, const A&)> get_reduce_function() {
        return this->reduce_function;
    }

    std::function<std::vector<U>(const U&)> get_successors() {
        return this->successors;
    }

    A get_reduce_init() {
        return this->reduce_init;
    }

    Worker<U, A>* get_worker(size_t id) {
        return this->workers[id].get();
    }

    void report_error(std::string worker_error_message) {
        std::lock_guard<std::mutex> guard(mutex);
        has_error = true;
        shutdown_request = true;
        error_message = std::move(worker_error_message);
        wake_up_my_master.notify_all();
    }
    // ---------------------------------------------------

    A run_naive() {
        start_workers();
        bool error_happened = false;

        // TODO: Check if shutdown_request needs to be automic
        while (!shutdown_request) {
            std::unique_lock<std::mutex> lock(mutex);
            wake_up_my_master.wait(lock, [this] {
                return has_error || (num_active_workers == 0);
            });

            if (has_error) {
                error_happened = true;
                shutdown_request = true;
            }

            if (num_active_workers == 0) {
                shutdown_request = true;
                break;
            }
        }

        join_workers();

        if (error_happened) {
            throw std::runtime_error(error_message);
        }

        A final_result = final_reduce();

        return final_result;
    }

    A run_smart() {
        start_workers();
        bool error_happened = false;

        while (true) {
            bool request_happened = false;
            size_t theif;

            {
                std::unique_lock<std::mutex> lock(mutex);
                wake_up_my_master.wait(lock, [this] {
                    return shutdown_request || has_error || (!request_steal_queue.empty());
                });

                if (shutdown_request) {
                    break;
                }

                if (has_error) {
                    error_happened = true;
                    shutdown_request = true;
                    break;
                }

                if (request_steal_queue.size() == num_workers) {
                    // for debuug issue
                    if (num_waiting_workers != num_workers) {
                        throw std::runtime_error("Something wrong with waiting queue implementation");
                    }

                    shutdown_request = true;
                    break;
                }

                if (!request_steal_queue.empty()) {
                    request_happened = true;
                }

                if (request_happened) {
                    theif = request_steal_queue.back();
                    request_steal_queue.pop_back();
                    --num_waiting_workers;
                }
            }

            // We release lock here so that more workers can join the waiting list
            // And for each loop, we only handle one case
            // Since everything related to queue, state variable change should be under the lock,
            // but we cannot hold the lock for long time since new workers should be able to join

            if (request_happened) {
                std::pair<size_t, int> victim_pair = choose_victim(theif);
                workers[theif]->set_victim(victim_pair); // this victim can be yourself! If currently no task can be stolen
                // LogInfo("[Master] Giving Worker with ID %zu an victm %zu to steal %i amount of work",
                //     theif,
                //     victim_pair.first,
                //     victim_pair.second
                // );
            }
        }

        join_workers();

        if (error_happened) {
            throw std::runtime_error(error_message);
        }

        A final_result = final_reduce();

        return final_result;
    }

    A run() {
        switch (steal_type) {
            case NO_STEAL:
                return run_naive();
            break;
            case NAIVE_STEAL:
                return run_naive();
            break;
            case SMART_STEAL:
                return run_smart();
            break;

            default:
                throw std::runtime_error("Improper steal type provided");
            break;
        }
    }
};

#endif //MASTER_HPP
