/**
 * latch.h
 */
#ifndef CONCURRENCY_LATCH_H_
#define CONCURRENCY_LATCH_H_

#include <atomic>
#include <condition_variable>
#include <mutex>

namespace conc11 {

/**
 * Single-use count down latch.
 */
class Latch {
public:
    Latch(std::ptrdiff_t value):value(value) {
    }

    Latch(const Latch&) = delete;
    Latch& operator=(const Latch&) = delete;

    /**
     * Decrement the counter by 1 and wait for the counter to reach 0 if necessary.
     */
    void count_down_and_wait() {
        auto v = value.load(std::memory_order_acquire);
        if (v <= 0) {
            return;
        }
        count_down(1);
        wait();
    }

    /**
     * Decrement the counter by n. Calling count_down will not block the caller thread, unless
     * when the call is the one that caused the counter to reach 0, on which the caller thread
     * will need to acquire a mutex.
     */
    void count_down(std::ptrdiff_t n) {
        auto v = value.fetch_sub(n, std::memory_order_acq_rel);
        if (0 < v && v <= n) {
            std::unique_lock<std::mutex> lock(mtx);
            lock.unlock(); // Sync with the thread that already acquired lock but not yet waiting
            cv.notify_all();
        }
    }

    /**
     * Returns true if the counter has reached 0.
     * If the counter is minus it is treated as 0.
     */
    bool is_ready() const {
        auto v = value.load(std::memory_order_acquire);
        return v <= 0;
    }

    /**
     * Blocks the caller thread until the counter reaches 0, returns immediately if already
     * reached 0.
     */
    void wait() const {
        auto v = value.load(std::memory_order_acquire);
        if (v <= 0) {
            return;
        }
        std::unique_lock<std::mutex> lock(mtx);
        if (value.load(std::memory_order_acquire) <= 0) {
            return;
        }
        cv.wait(lock, [this](){return value.load(std::memory_order_acquire) <= 0;});
    }

private:
    std::atomic<std::ptrdiff_t> value;
    mutable std::mutex mtx;
    mutable std::condition_variable cv;
};

} // namespace conc11

#endif /* CONCURRENCY_LATCH_H_ */
