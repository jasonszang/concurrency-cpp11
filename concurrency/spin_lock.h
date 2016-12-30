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
#include <pthread.h>

namespace ttb {

static const uint_fast16_t SPIN_CYCLES_BEFORE_YIELD = 20000;
static const uint_fast16_t SPIN_CYCLES_BEFORE_YIELD_FAIR = 500;

/**
 * @brief A simple unfair spin lock that satisfies Lockable.
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
        return !l.test_and_set(std::memory_order_acq_rel);
    }

private:
    std::atomic_flag l = ATOMIC_FLAG_INIT;
};

/**
 * A fair spin lock using ticket lock algorithm that satisfies BasicLockable.
 * This type of spin locks quickly become very slow when number of thread competing for the lock
 * becomes large, for example tens. Use SpinLock if fairness is not a huge concern.
 */
class FairSpinLock {
public:
    FairSpinLock() :
            next(0), active(0) {
    }

    void lock() {
        unsigned int ticket = next.fetch_add(1, std::memory_order_acq_rel);
        uint_fast16_t patience = SPIN_CYCLES_BEFORE_YIELD_FAIR;
        while (active.load(std::memory_order_acquire) != ticket) {
            patience--;
            if (!patience) {
                patience = SPIN_CYCLES_BEFORE_YIELD_FAIR;
                std::this_thread::yield(); // can be insanely slow without yield
            }
        }
    }

    void unlock() {
        active.fetch_add(1, std::memory_order_release);
    }

private:
    std::atomic_uint next;
    std::atomic_uint active;
};

} // namespace ttb

#endif /* CONCURRENCY_SPINLOCK_H_ */
