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

/**
 * @brief A simple unfair spin lock that satisfies LockAble.
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

/**
 * @brief A thin-layer wrapper for pthread_spin
 */
class PthreadSpinLockWrapper {
public:
    PthreadSpinLockWrapper():l(new pthread_spinlock_t()) {
        pthread_spin_init(l, PTHREAD_PROCESS_PRIVATE);
    }

    ~PthreadSpinLockWrapper() {
        pthread_spin_destroy(l);
        delete l;
    }

    void lock() {
        pthread_spin_lock(l); // undefined if caller has the lock
    }

    void unlock() {
        pthread_spin_unlock(l);
    }

    bool try_lock() {
        return pthread_spin_trylock(l) == 0;
    }

private:
    pthread_spinlock_t * const l;
};

} // namespace ttb

#endif /* CONCURRENCY_SPINLOCK_H_ */
