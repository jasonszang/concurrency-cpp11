/*
 * spin_lock.h
 *
 *  Created on: 2016-12-26
 *      Author: jasonszang
 */

#ifndef CONCURRENCY_SPINLOCK_H_
#define CONCURRENCY_SPINLOCK_H_

#include <atomic>
#include <cstdio>

namespace ttb {

static const uint_fast16_t SPIN_CYCLES_BEFORE_YIELD = 20000;

/**
 * @brief A simple spin lock that satisfies LockAble.
 */
class SpinLock {
public:

    void lock(){
        uint_fast16_t patience = SPIN_CYCLES_BEFORE_YIELD;
        while(l.test_and_set(std::memory_order_acquire)) {
            patience--;
            if (!patience) {
                patience = SPIN_CYCLES_BEFORE_YIELD;
                std::this_thread::yield();
            }
        }
    }

    void unlock() {
        l.clear(std::memory_order_release);
    }

    bool try_lock() {
        return !l.test_and_set(std::memory_order_acquire);
    }

private:
    std::atomic_flag l = ATOMIC_FLAG_INIT;
};

} // namespace ttb

#endif /* CONCURRENCY_SPINLOCK_H_ */
