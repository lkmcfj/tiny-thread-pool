#pragma once

#include <cstdint>
#include <thread>
#include <mutex>
#include <condition_variable>
#include <memory>
#include <queue>
#include <vector>

using thread_num_t = std::uint32_t;

class ThreadPool;
void thread_main(thread_num_t, ThreadPool*, std::mutex*);

class Task {
private:
    std::mutex finish;
public:
    Task() {
        finish.lock();
    }

    virtual void run(thread_num_t thread_id) = 0;
    virtual bool stop_on_finished() {
        return false;
    }
    
    friend void thread_main(thread_num_t, ThreadPool*, std::mutex*);
    friend class ThreadPool;
};

template<class F>
class SimpleCallableTask : public Task {
private:
    F func;
public:
    SimpleCallableTask(F f): func(f) {}

    void run(thread_num_t thread_id) override {
        func(thread_id);
    }
};

class StopRequest : public Task {
public:
    void run (thread_num_t thread_id) override {
    }
    
    bool stop_on_finished() override {
        return true;
    }
};

class Scheduler {
public:
    virtual size_t size() = 0;
    virtual void add(std::shared_ptr<Task> task) = 0;
    virtual std::shared_ptr<Task> get() = 0;
};

class QueueScheduler : public Scheduler {
    std::queue<std::shared_ptr<Task>> task_queue;
public:
    size_t size() override {
        return task_queue.size();
    }
    void add(std::shared_ptr<Task> task) override {
        task_queue.push(std::move(task));
    }
    std::shared_ptr<Task> get() override {
        auto ret = std::move(task_queue.front());
        task_queue.pop();
        return ret;
    }
};

class ThreadPool {
private:
    std::mutex scheduler_mutex;
    std::condition_variable scheduler_signal;
    std::unique_ptr<Scheduler> scheduler;
    std::vector<std::unique_ptr<std::mutex>> thread_stop_signal;
    
    void create_one_worker_thread() {
        thread_num_t cur_id = static_cast<thread_num_t>(thread_stop_signal.size());
        auto cur_stop_signal = std::make_unique<std::mutex>();
        std::mutex *cur_stop_signal_raw_ptr = cur_stop_signal.get(); // thread_main should use this instead of pool->thread_stop_signal[thread_id] to avoid data race
        cur_stop_signal_raw_ptr->lock();
        thread_stop_signal.push_back(std::move(cur_stop_signal));
        std::thread cur_worker{thread_main, cur_id, this, cur_stop_signal_raw_ptr};
        cur_worker.detach();
    }
public:
    ThreadPool(std::unique_ptr<Scheduler> init_scheduler): scheduler(std::move(init_scheduler)) {}
    ThreadPool(const ThreadPool &other) = delete;
    ThreadPool& operator=(const ThreadPool &other) = delete;
    ~ThreadPool() {
        std::vector<std::shared_ptr<Task>> stop_all;
        for (size_t i = 0; i < thread_stop_signal.size(); ++i) {
            stop_all.push_back(std::make_shared<StopRequest>());
        }
        issue_tasks(std::move(stop_all), false);
        for (auto &i : thread_stop_signal) {
            i->lock();
        }
    }

    void create_worker_threads(thread_num_t thread_count) {
        while (thread_count--) {
            create_one_worker_thread();
        }
    }
    void issue_task(std::shared_ptr<Task> task, bool wait_for_finish) {
        {
            std::lock_guard<std::mutex> guard{scheduler_mutex};
            scheduler->add(task);
        }
        scheduler_signal.notify_one();
        if (wait_for_finish) {
            task->finish.lock();
        }
    }
    template<class F>
    void issue_task_callable(F func, bool wait_for_finish) {
        std::shared_ptr<Task> task = std::make_shared<SimpleCallableTask>(func);
        issue_task(std::move(task), wait_for_finish);
    }
    void issue_tasks(std::vector<std::shared_ptr<Task>> tasks, bool wait_for_finish) {
        {
            std::lock_guard<std::mutex> guard{scheduler_mutex};
            for (auto &task : tasks) {
                scheduler->add(task);
            }
        }
        scheduler_signal.notify_all();
        if (wait_for_finish) {
            for (auto &task : tasks) {
                task->finish.lock();
            }
        }
    }
    
    friend void thread_main(thread_num_t, ThreadPool*, std::mutex*);
};

inline void thread_main(thread_num_t thread_id, ThreadPool *pool, std::mutex *stop_signal) {
    Scheduler *scheduler = pool->scheduler.get();
    while (true) {
        std::shared_ptr<Task> cur;
        {
            std::unique_lock<std::mutex> guard{pool->scheduler_mutex};
            pool->scheduler_signal.wait(guard, [scheduler]() { return scheduler->size() > 0; });
            cur = scheduler->get();
        }
        cur->run(thread_id);
        cur->finish.unlock();
        if (cur->stop_on_finished()) {
            break;
        }
    }
    stop_signal->unlock();
}