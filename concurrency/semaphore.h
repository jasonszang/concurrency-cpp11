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

template<class LockType>
class Semaphore {
public:

    Semaphore(int initial_permits) :
            count(initial_permits) {
    }

    Semaphore(const Semaphore&) = delete;
    Semaphore& operator=(const Semaphore&) = delete;

    void acquire() {
        acquire(1);
    }

    void acquire(unsigned int request) {
        std::unique_lock<LockType> ul(mtx);
        while (count < (int) request) {
            cv.wait(ul);
        }
        count -= request;
    }

    void release() {
        release(1);
    }

    void release(unsigned int request) {
        {
            std::lock_guard<LockType> lock(mtx);
            count += request;
        }
        cv.notify_all();
    }

    bool try_acquire() {
        return try_acquire(1);
    }

    bool try_acquire(unsigned int request) {
        std::lock_guard<LockType> lock(mtx);
        if (count >= (int) request) {
            count -= request;
            return true;
        } else {
            return false;
        }
    }

    bool try_acquire_for(unsigned long millis, unsigned int micros) {
        return try_acquire_for(1, millis, micros);
    }

    bool try_acquire_for(unsigned int request, unsigned long millis, unsigned int micros) {
        std::chrono::steady_clock::time_point timeout_time = std::chrono::steady_clock::now() +
                std::chrono::milliseconds(millis) + std::chrono::microseconds(micros);
        return try_acquire0(request, timeout_time);
    }

    template<class Rep, class Period>
    bool try_acquire_for(const std::chrono::duration<Rep, Period>& timeout_duration) {
        return try_acquire_for(1, timeout_duration);
    }

    template<class Rep, class Period>
    bool try_acquire_for(unsigned int request,
                         const std::chrono::duration<Rep, Period>& timeout_duration) {
        std::chrono::steady_clock::time_point timeout_time = std::chrono::steady_clock::now() +
                timeout_duration;
        return try_acquire0(request, timeout_time);
    }

    template<class Clock, class Duration>
    bool try_acquire_until(const std::chrono::time_point<Clock, Duration> &timeout_time) {
        return try_acquire_until(1, timeout_time);
    }

    template<class Clock, class Duration>
    bool try_acquire_until(unsigned int request,
                           const std::chrono::time_point<Clock, Duration> &timeout_time) {
        return try_acquire0(true, timeout_time);
    }

    int available_permits() {
        std::lock_guard<LockType> lock(mtx);
        return count;
    }

private:
    template<class Clock, class Duration>
    bool try_acquire0(unsigned int permits,
                      const std::chrono::time_point<Clock, Duration> &timeout_time) {
        std::unique_lock<LockType> ul(mtx);
        while (count < (int) permits) {
            if (cv.wait_until(ul, timeout_time) == std::cv_status::timeout) {
                return false;
            }
        }
        count -= permits;
        return true;
    }

    LockType mtx;
    typename std::conditional<std::is_same<LockType, std::mutex>::value,
          std::condition_variable,
          std::condition_variable_any>::type cv;
    int32_t count;
};

} // namespace ttb

#endif /* CONCURRENCY_SEMAPHORE_H_ */
