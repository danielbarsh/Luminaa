#pragma once

#include <atomic>
#include <condition_variable>
#include <functional>
#include <mutex>
#include <queue>
#include <thread>
#include <vector>

// Small persistent worker pool. Nothing fancy -- a task queue plus a
// parallelFor that chunks a range across the workers and blocks until every
// chunk reports back. Good enough to push the asteroid/bullet broad-phase
// off the main thread without paying thread-creation cost every frame.
class ThreadPool {
public:
    explicit ThreadPool(std::size_t threadCount = std::thread::hardware_concurrency()) {
        if (threadCount == 0)
            threadCount = 2;
        workers_.reserve(threadCount);
        for (std::size_t i = 0; i < threadCount; ++i)
            workers_.emplace_back([this] { workerLoop(); });
    }

    ~ThreadPool() {
        {
            std::lock_guard<std::mutex> lock(queueMutex_);
            stop_ = true;
        }
        queueCv_.notify_all();
        for (auto& t : workers_)
            t.join();
    }

    std::size_t threadCount() const { return workers_.size(); }

    // Splits [0, count) into one chunk per worker and runs fn(begin, end)
    // for each chunk concurrently. Blocks until all chunks finish.
    void parallelFor(std::size_t count, const std::function<void(std::size_t, std::size_t)>& fn) {
        if (count == 0)
            return;

        const std::size_t chunks = std::min(workers_.size(), count);
        const std::size_t chunkSize = (count + chunks - 1) / chunks;

        std::atomic<std::size_t> remaining{chunks};
        std::mutex doneMutex;
        std::condition_variable doneCv;

        for (std::size_t c = 0; c < chunks; ++c) {
            const std::size_t begin = c * chunkSize;
            const std::size_t end = std::min(count, begin + chunkSize);
            if (begin >= end) {
                if (--remaining == 0) doneCv.notify_one();
                continue;
            }

            enqueue([&fn, begin, end, &remaining, &doneMutex, &doneCv] {
                fn(begin, end);
                if (--remaining == 0) {
                    std::lock_guard<std::mutex> lock(doneMutex);
                    doneCv.notify_one();
                }
            });
        }

        std::unique_lock<std::mutex> lock(doneMutex);
        doneCv.wait(lock, [&remaining] { return remaining.load() == 0; });
    }

private:
    void enqueue(std::function<void()> task) {
        {
            std::lock_guard<std::mutex> lock(queueMutex_);
            tasks_.push(std::move(task));
        }
        queueCv_.notify_one();
    }

    void workerLoop() {
        while (true) {
            std::function<void()> task;
            {
                std::unique_lock<std::mutex> lock(queueMutex_);
                queueCv_.wait(lock, [this] { return stop_ || !tasks_.empty(); });
                if (stop_ && tasks_.empty())
                    return;
                task = std::move(tasks_.front());
                tasks_.pop();
            }
            task();
        }
    }

    std::vector<std::thread> workers_;
    std::queue<std::function<void()>> tasks_;
    std::mutex queueMutex_;
    std::condition_variable queueCv_;
    bool stop_ = false;
};
