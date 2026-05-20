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
#include <mutex>
#include <memory>
#include <iostream>

#include "VisitedSet.hpp"
#include "Worker.hpp"

template<typename U, typename A>
class Master {

private:
    size_t num_workers;

    // TODO: Check if this should be passed separately or by RecursivelyEnumeratedSet,
    std::vector<U> seeds;
    std::function<std::vector<U>(const U&)> successors;

    std::function<A(const U&)> map_function;
    std::function<A(const A&, const A&)> reduce_function;
    A reduce_init;

    VisitedSet<U> visited;
    size_t num_active_workers;
    std::condition_variable wake_up_my_master;
    std::vector<std::unique_ptr<Worker<U, A>>> workers;
    std::vector<std::thread> worker_threads{};

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
        }

        for (size_t i = 0; i < num_workers; ++i) {
            // This part check with Worker actual implementation
            // TODO: check with Worker actual implementation
            size_t worker_id = i % num_workers;
            workers.push_back(
                std::make_unique<Worker<U,A>>(
                    worker_id,
                    this,
                    initial_task[worker_id]
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
        for (std::thread& worker_thread : worker_threads) {
            if (worker_thread.joinable()) {
                worker_thread.join();
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


public:
    Master(
        size_t num_workers_,
        std::vector<U> seeds_,
        std::function<std::vector<U>(const U&)> successors_,
        std::function<A(const U&)> map_function_,
        std::function<A(const A&, const A&)> reduce_function_,
        A reduce_init_
    )
        : num_workers(num_workers_),
          seeds(seeds_),
          successors(successors_),
          map_function(map_function_),
          reduce_function(reduce_function_),
          reduce_init(reduce_init_),
          visited(VisitedSet(seeds_)),
          num_active_workers(0),
          shutdown_request(false),
          has_error(false)
    {
        workers.reserve(num_workers);
        worker_threads.reserve(num_workers);
    }

    void abort() {
        std::lock_guard<std::mutex> guard(mutex);
        has_error = true;
        shutdown_request = true;
        wake_up_my_master.notify_all();
    }

    // ---------------------------------------------------
    // Here are some other functions that Workers can call to get, instead of passing them as input for Worker

    void receive_partial_result(const A& partial_result) {
        // This function is supposed to call by Worker after Worker has done her computation
        // i.e. Worker call master->receive_partial_result(partial_result);

        std::lock_guard<std::mutex> guard(mutex);
        partial_results.push_back(partial_result);
    }

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

    bool should_shutdown() {
        std::lock_guard<std::mutex> guard(mutex);
        return shutdown_request;
    }

    VisitedSet<U>& get_visited_set() {
        // Should we lock it?
        return std::ref(visited);
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

    void report_error(std::string worker_error_message) {
        std::lock_guard<std::mutex> guard(mutex);
        has_error = true;
        shutdown_request = true;
        error_message = std::move(worker_error_message);
        wake_up_my_master.notify_all();
    }
    // ---------------------------------------------------

    A run() {
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
};

#endif //MASTER_HPP
