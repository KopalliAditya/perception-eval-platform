// thread_pool.hpp — a small, reusable fixed-size thread pool.
//
// Tasks are submitted with enqueue() and a std::future is returned for the
// result. Worker threads pull tasks from a shared queue guarded by a mutex and
// condition variable. The destructor drains in-flight tasks and joins cleanly
// (RAII) — no manual start/stop required.
//
// Determinism note: the pool makes no ordering guarantees about task
// execution. Callers that need deterministic output must write each task's
// result into a pre-sized, index-addressed slot (as the evaluator does) rather
// than appending to a shared container.
//
// Author: Aditya Kopalli, 2026. MIT licensed.

#ifndef PEV_THREAD_POOL_HPP
#define PEV_THREAD_POOL_HPP

#include <algorithm>
#include <condition_variable>
#include <functional>
#include <future>
#include <memory>
#include <mutex>
#include <queue>
#include <stdexcept>
#include <thread>
#include <utility>
#include <vector>

namespace pev {

class ThreadPool {
public:
    // Creates `thread_count` workers (clamped to at least 1).
    explicit ThreadPool(std::size_t thread_count) {
        if (thread_count == 0) {
            thread_count = 1;
        }
        workers_.reserve(thread_count);
        for (std::size_t i = 0; i < thread_count; ++i) {
            workers_.emplace_back([this] { worker_loop(); });
        }
    }

    ThreadPool(const ThreadPool&) = delete;
    ThreadPool& operator=(const ThreadPool&) = delete;

    ~ThreadPool() {
        {
            std::lock_guard<std::mutex> lock(mutex_);
            stopping_ = true;
        }
        cv_.notify_all();
        for (std::thread& w : workers_) {
            if (w.joinable()) {
                w.join();
            }
        }
    }

    std::size_t size() const { return workers_.size(); }

    // Submits a callable; returns a future for its result.
    template <typename F, typename... Args>
    auto enqueue(F&& f, Args&&... args)
        -> std::future<typename std::invoke_result_t<F, Args...>> {
        using Return = typename std::invoke_result_t<F, Args...>;

        auto task = std::make_shared<std::packaged_task<Return()>>(
            std::bind(std::forward<F>(f), std::forward<Args>(args)...));
        std::future<Return> result = task->get_future();
        {
            std::lock_guard<std::mutex> lock(mutex_);
            if (stopping_) {
                throw std::runtime_error("enqueue on a stopped ThreadPool");
            }
            tasks_.emplace([task] { (*task)(); });
        }
        cv_.notify_one();
        return result;
    }

private:
    void worker_loop() {
        for (;;) {
            std::function<void()> task;
            {
                std::unique_lock<std::mutex> lock(mutex_);
                cv_.wait(lock,
                         [this] { return stopping_ || !tasks_.empty(); });
                if (stopping_ && tasks_.empty()) {
                    return;
                }
                task = std::move(tasks_.front());
                tasks_.pop();
            }
            task();
        }
    }

    std::vector<std::thread> workers_;
    std::queue<std::function<void()>> tasks_;
    std::mutex mutex_;
    std::condition_variable cv_;
    bool stopping_ = false;
};

// Convenience: hardware_concurrency() clamped to at least 1.
inline std::size_t default_thread_count() {
    const unsigned hc = std::thread::hardware_concurrency();
    return hc == 0 ? 1u : hc;
}

}  // namespace pev

#endif  // PEV_THREAD_POOL_HPP
