#include "threadpool.h"



namespace url_pinger {
    ThreadPool::ThreadPool(size_t numThreads) : stop_(false) {
        for (size_t i = 0; i < numThreads; ++i) {
            workers.emplace_back(
                [this] {
                    for (;;) {
                        std::function<void()> task;
                        {
                            std::unique_lock<std::mutex> lock(queue_mutex_);
                            cv_.wait(lock, [this] { return stop_ || !tasks_.empty(); });
                            if (stop_ && tasks_.empty()) {
                                return;
                            }
                            task = std::move(tasks_.front());
                            tasks_.pop();
                        }
                        task();
                    }
                });
        }
    }

    
    // delete me
    void ThreadPool::getTasksInQueue() {
        std::cout << "Tasks in queue: " << tasks_.size() << std::endl;
        //std::cout << "\tTask at front: " << tasks_.front() << std::endl;
    }

    bool ThreadPool::getStop() {
        return stop_;
    }

    ThreadPool::~ThreadPool() {
        {
            std::unique_lock<std::mutex> lock(queue_mutex_);
            stop_ = true;
        }
        cv_.notify_all();
        for (std::thread& worker : workers) {
            worker.join();
        }
    }

    template<class F, class... Args>
    auto ThreadPool::enqueue(F&& f, Args&&... args) -> std::future<typename std::result_of<F(Args...)>::type> {
        using return_type = typename std::result_of<F(Args...)>::type;
        auto task = std::make_shared<std::packaged_task<return_type()>>(
            std::bind(std::forward<F>(f), std::forward<Args>(args)...)
        );

        std::future<return_type> res = task->get_future();
        {
            std::unique_lock<std::mutex> lock(queue_mutex_);
            if (stop_) {
                throw std::runtime_error("enqueue on stopped ThreadPool");
            }
            
            tasks_.emplace([task]() { (*task)(); });
        }
        cv_.notify_one();
        return res;
    }



}; // namespace url_pinger

