/**
 * executor.h
 */
#ifndef CONCURRENCY_EXECUTOR_H_
#define CONCURRENCY_EXECUTOR_H_

#include <chrono>
#include <functional>
#include <future>
#include <queue>
#include "../util/util.h"

namespace conc11 {

class ExecutorBase {
protected:
    class TaskBase {
    public:
        virtual ~TaskBase(){
        }

        virtual void operator()() = 0;
    protected:
        TaskBase() = default;

        TaskBase(const TaskBase&) = delete;
        TaskBase& operator=(const TaskBase&) = delete;
    };

    template<class RetType>
    class Task : public TaskBase {
    public:
        explicit Task(std::packaged_task<RetType()>&& task) noexcept :task(std::move(task)) {
        }

        virtual void operator()() override {
            task();
        }
    private:
        std::packaged_task<RetType()> task;
    };

    template<class Runnable>
    class UntrackableTask : public TaskBase {
    public:
        explicit UntrackableTask(Runnable&& r) noexcept:r(std::forward<Runnable>(r)) {
        }

        virtual void operator()() override {
            try {
                r();
            } catch (...) {
                // Swallows exceptions from RunnableTask as there are no means to retrive neither
                // results nor exceptions
            }
        }

    private:
        typename std::remove_reference<Runnable>::type r;
    };

    ExecutorBase() = default;
    ExecutorBase(const ExecutorBase&) = delete;
    ExecutorBase& operator=(const ExecutorBase&) = delete;
};

class ThreadPoolExecutor : public ExecutorBase{
public:
    ThreadPoolExecutor(size_t core_pool_size,
             size_t max_pool_size,
             std::chrono::nanoseconds::rep timeout_nanoseconds) :
            core_pool_size(core_pool_size), max_pool_size(max_pool_size),
                    timeout_nanoseconds(timeout_nanoseconds),
                    shut(false), active_count(0) {
        for (size_t i = 0; i < core_pool_size; ++i) {
            add_worker_locked(true);
        }
        size_t max_threads = core_pool_size < max_pool_size ? max_pool_size : core_pool_size;
        workers.reserve(max_threads);
        dead_workers.reserve(max_threads);
    }

    ~ThreadPoolExecutor() {
        if (!is_terminated()) {
            shutdown();
            await_termination();
        }
        // Destructor shall not be called from multiple threads so safe to use _locked methods.
        reap_dead_workers_locked();
    }

    /**
     * Submits a callable and its parameters to be executed at some time in the future.
     * The callable object and parameters will be stored using std::bind. The retsult and possible
     * exceptions may be retrived with the returned std::future object.
     */
    template<typename Callable, typename ... Args>
    auto submit(Callable&& c, Args&&... args)
    -> std::future<decltype(invoke(std::forward<Callable>(c), std::forward<Args>(args)...))> {
        using RetType = decltype(
                invoke(std::forward<Callable>(c), std::forward<Args>(args)...));

        if (shut.load()) {
            throw(std::system_error(std::make_error_code(std::errc::permission_denied)));
        }
        std::packaged_task<RetType()> task(
                std::bind(std::forward<Callable>(c), std::forward<Args>(args)...));
        std::future<RetType> f = task.get_future();
        auto uptr_task = conc11::make_unique<Task<RetType>>(std::move(task));
        {
            std::lock_guard<std::mutex> lock(main_lock);
            insert_task_locked(std::move(uptr_task));
        }
        return f;
    }

    /**
     * Submits a callable and its parameters to be executed at some time in the future. Unlike
     * submit(), result cannot be retrived as no std::future will be returned. If an exception
     * occured during execution, the execution will be interrupted and the exception will be
     * lost.
     * For executing tasks that requires no tracking var std::future this alternative method of
     * submitting tasks yields considerably higher performance on some platforms including Linux.
     */
    template<typename Callable, typename ... Args>
    void execute(Callable&& c, Args&&... args) {
        if (shut.load()) {
            throw(std::system_error(std::make_error_code(std::errc::permission_denied)));
        }
        auto bind_obj = std::bind(std::forward<Callable>(c), std::forward<Args>(args)...);
        auto uptr_task = conc11::make_unique<UntrackableTask<decltype(bind_obj)>>(
                std::move(bind_obj));
        {
            std::lock_guard<std::mutex> lock(main_lock);
            insert_task_locked(std::move(uptr_task));
        }
    }

    void shutdown() noexcept {
        std::lock_guard<std::mutex> lock(main_lock);
        shut.store(true);
        if (is_terminated_locked()) {
            wait_cv.notify_all();
        } else {
            cv.notify_all();
        }
    }

    void await_termination() {
        if (is_terminated()) {
            return;
        }
        std::unique_lock<std::mutex> lock(main_lock);
        if (is_terminated_locked()) { // double check in case last thread my exit between the
                                      // first check and now resulting in deadlock
            return;
        }
        wait_cv.wait(lock, [this](){return is_terminated_locked();});
    }

    template<class Rep, class Period>
    bool await_termination_for(const std::chrono::duration<Rep, Period>& timeout_duration) {
        auto timeout_time = std::chrono::steady_clock::now() + timeout_duration;
        return await_termination_until(timeout_time);
    }

    template<class Clock, class Duration>
    bool await_termination_until(const std::chrono::time_point<Clock, Duration>& timeout_time) {
        if (is_terminated()) {
            return true;
        }
        std::unique_lock<std::mutex> lock(main_lock);
        if (is_terminated_locked()) {
            return true;
        }
        return wait_cv.wait_until(lock, timeout_time, [this](){return is_terminated();});
    }

    /**
     * Return number of living threads in the thread pool.
     */
    size_t get_pool_size() noexcept {
        std::lock_guard<std::mutex> lock(main_lock);
        return workers.size();
    }

    /**
     * Return approximate active tasks being executed.
     */
    size_t get_active_count() noexcept {
        return active_count.load();
    }

    bool is_shutdown() {
        return shut.load();
    }

    bool is_terminated() {
        std::lock_guard<std::mutex> lock(main_lock);
        return is_terminated_locked();
    }

private:

    /**
     * Thread worker that manages a worker thread and will remove self from the workers array
     * when the thread exits.
     * Do not store pointer or reference to worker instances as they might be invalidated. Always
     * access from the queue.
     */
    class Worker {
    public:
        Worker(ThreadPoolExecutor& executor, bool core, size_t worker_index, int id) noexcept:
        exec(executor), core(core), worker_index(worker_index),
        id(id), worker_thread(std::ref(*this)) {
        }
        Worker(const Worker&) = delete;
        Worker& operator=(const Worker&) = delete;

        /*
         * Worker func. Should be called only by the corresponding thread of this worker.
         */
        void operator()() noexcept {
            run();
        }

        void join() {
            worker_thread.join();
        }

        int get_id() {
            return id;
        }

    private:

        /**
         * Fetch ont task from task queue.
         * Returns empty pointer when calling worker is non-core and timed out, or the executor
         * is shut down when waiting
         */
        std::unique_ptr<TaskBase> fetch_task_locked(std::unique_lock<std::mutex>& lock) {
            // Fetch if queue not empty
            if (!exec.task_queue.empty()) {
                std::unique_ptr<TaskBase> t(std::move(exec.task_queue.front()));
                exec.task_queue.pop();
                return t;
            }
            // Wait until queue not empty, executor shut down or timed out
            if (core) {
                exec.cv.wait(lock,
                        [this]() {return !exec.task_queue.empty() || exec.shut.load();});
            } else {
                auto timeout_time = std::chrono::steady_clock::now() + exec.timeout_nanoseconds;
                exec.cv.wait_until(lock, timeout_time,
                        [this]() {return !exec.task_queue.empty() || exec.shut.load();});
            }
            // Fetch if queue not empty, otherwise return nullptr
            if (!exec.task_queue.empty()) {
                std::unique_ptr<TaskBase> t(std::move(exec.task_queue.front()));
                exec.task_queue.pop();
                return t;
            } else {
                return std::unique_ptr<TaskBase>();
            }
        }

        /**
         * Remove self by swapping state with last element in the worker array and decrease size.
         * Assumes the thread associated with this Worker instance owns the main lock.
         */
        void remove_self_locked() {
            std::swap(exec.workers[worker_index], exec.workers.back());
            exec.workers[worker_index]->worker_index = worker_index;
            exec.dead_workers.emplace_back(std::move(exec.workers.back()));
            exec.workers.pop_back();
        }

        void run() {
            // There is a small period where worker_thread seen in the new thread is invalid.
            while (true) { // Worker main loop
                std::unique_lock<std::mutex> lock(exec.main_lock);
                if (exec.shut.load() && exec.task_queue.empty()) {
                    break;
                }
                std::unique_ptr<TaskBase> task = fetch_task_locked(lock);
                if (!task) {
                    break;
                }
                ++exec.active_count;
                lock.unlock();
                (*task)();
//                task.reset();
                lock.lock();
                exec.reap_dead_workers_locked();
                --exec.active_count;
            }
            {
                // handle thread exit
                std::lock_guard<std::mutex> lock(exec.main_lock);
                remove_self_locked();
                if (exec.is_terminated_locked()) {
                    // Last worker exit after shutdown, finalize executor and notify all waiters
                    exec.wait_cv.notify_all();
                }
            }
        }

        // The executor this worker belongs to
        ThreadPoolExecutor& exec;
        // Is core thread or not. Core threads do not exit after a timeout period.
        bool core = false;
        // Index of this worker instance in the worker list.
        size_t worker_index = 0;
        // Instance of std::thread corresponding to this
        int id;
        std::thread worker_thread;
    };

    void insert_task_locked(std::unique_ptr<TaskBase>&& task) {
        task_queue.emplace(std::move(task));
        size_t idle_workers = workers.size() - active_count.load();
        if (idle_workers == 0 && workers.size() < max_pool_size) {
            add_worker_locked(false);
        }
        cv.notify_one();
    }

    void add_worker_locked(bool core) {
        static int id = 1;
        workers.emplace_back(new Worker(*this, core, workers.size(), id++));
    }

    bool is_terminated_locked() {
        return shut.load() && task_queue.empty() && workers.empty();
    }

    void reap_dead_workers_locked() {
        if (dead_workers.empty()) {
            return;
        }
        for (const auto &worker : dead_workers) {
            worker->join();
        }
        dead_workers.clear();
    }

    const size_t core_pool_size;
    const size_t max_pool_size;
    const std::chrono::nanoseconds timeout_nanoseconds;

    std::queue<std::unique_ptr<TaskBase>> task_queue;

    std::vector<std::unique_ptr<Worker>> workers;
    std::vector<std::unique_ptr<Worker>> dead_workers;

    std::mutex main_lock;
    std::condition_variable cv;
    std::condition_variable wait_cv;
    std::atomic<bool> shut;
    std::atomic<size_t> active_count;
};

std::unique_ptr<ThreadPoolExecutor> make_single_thread_executor() {
    return conc11::make_unique<ThreadPoolExecutor>(1, 1, 0L);
}

std::unique_ptr<ThreadPoolExecutor> make_fixed_thread_pool(size_t num_threads) {
    return conc11::make_unique<ThreadPoolExecutor>(num_threads, num_threads, 0L);
}

std::unique_ptr<ThreadPoolExecutor> make_cached_thread_pool() {
    static const size_t MAX_THREADS = 1024;
    return conc11::make_unique<ThreadPoolExecutor>(1, MAX_THREADS, 10 * 1000000000LL);
}

} // namespace conc11

#endif /* CONCURRENCY_EXECUTOR_H_ */
