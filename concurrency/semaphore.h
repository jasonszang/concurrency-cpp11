/*
 * semaphore.h
 *
 *  Created on: 2016-7-5
 *      Author: jasonszang
 */

#ifndef CONCURRENCY_SEMAPHORE_H_
#define CONCURRENCY_SEMAPHORE_H_

#include <mutex>
#include <chrono>
#include <condition_variable>
#include "spin_lock.h"

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
        std::unique_lock<ttb::SpinLock> ul(mtx);
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
            std::lock_guard<ttb::SpinLock> lock(mtx);
            count += permits;
        }
        cv.notify_one();
    }

    bool try_acquire() {
        return try_acquire(1);
    }

    bool try_acquire(unsigned int permits) {
        return try_acquire(permits, 0);
    }

    bool try_acquire(unsigned int permits, uint32_t timeout_us) {
        std::unique_lock<ttb::SpinLock> ul(mtx);
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
        std::lock_guard<ttb::SpinLock> lock(mtx);
        return count;
    }

    Semaphore(const Semaphore &rhs) = delete;
    void operator =(const Semaphore &rhs) = delete;

private:
    ttb::SpinLock mtx;
    std::condition_variable_any cv;
    int32_t count;
};

} // namespace ttb

#endif /* CONCURRENCY_SEMAPHORE_H_ */
