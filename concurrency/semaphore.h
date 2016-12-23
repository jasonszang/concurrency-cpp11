/*
 * semaphore.h
 *
 *  Created on: 2016-7-5
 *      Author: jasonszang
 */

#ifndef SEMAPHORE_H_
#define SEMAPHORE_H_

#include <mutex>
#include <chrono>
#include <condition_variable>

namespace ttb {

class Semaphore {
public:

    Semaphore(int initial_permits) :
            count(initial_permits) {
    }

    void acquire() {
        acquire(1);
    }

    void acquire(unsigned int permits) {
        std::unique_lock<std::mutex> ul(mtx);
        while (count < (int) permits) {
            cv.wait(ul);
        }
        count -= permits;
    }

    void release() {
        release(1);
    }

    void release(unsigned int permits) {
        {
            std::lock_guard<std::mutex> lock(mtx);
            count += permits;
        }
        cv.notify_all(); // Attention: thundering herd

        // Thundering herd can be avoided by keeping track all waiting acquires and notify one by one
        // until the amount of permits gets depleted. However this result in awaking 1/2 of waiting
        // threads on average, and the cost in maintaining required data structures might make this
        // optimization ineffective.
        // Another strategy is let threads wait on different condition_variables to provide a mechanic
        // for waking specific threads, then implement an algorithm to select threads to wake.
    }

    bool try_acquire() {
        return try_acquire(1);
    }

    bool try_acquire(unsigned int permits) {
        return try_acquire(permits, 0);
    }

    bool try_acquire(unsigned int permits, uint32_t timeout_us) {
        std::unique_lock<std::mutex> ul(mtx);
        auto until = std::chrono::steady_clock::now() + std::chrono::microseconds(timeout_us);
        while (count < (int) permits){
            if (cv.wait_until(ul, until) == std::cv_status::timeout){
                return false;
            }
        }
        count -= permits;
        return true;
    }

    int available_permits() {
        std::lock_guard<std::mutex> lock(mtx);
        return count;
    }

    Semaphore(const Semaphore &rhs) = delete;
    void operator =(const Semaphore &rhs) = delete;

private:
    std::mutex mtx;
    std::condition_variable cv;
    int32_t count;
};

} // namespace ttb

#endif /* SEMAPHORE_H_ */
