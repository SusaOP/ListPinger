#include <vector>
#include <queue>
#include <thread>
#include <mutex>
#include <condition_variable>
#include <functional>
#include <atomic>
#include <future>

#ifndef THREADPOOL_H
#define THREADPOOL_H

namespace url_pinger {
class ThreadPool {
    public:
        ThreadPool(size_t);
        ~ThreadPool();
        template<class F, class... Args>
        auto enqueue(F&& f, Args&&... args)
            -> std::future<typename std::result_of<F(Args...)>::type>;
        void getTasksInQueue();
        bool getStop();

    private:
        std::vector<std::thread> workers;
        std::queue<std::function<void()>> tasks_;
        std::mutex queue_mutex_;
        std::condition_variable cv_;
        std::atomic<bool> stop_;
};
} // namespace url_pinger

#endif // THREADPOOL_H
