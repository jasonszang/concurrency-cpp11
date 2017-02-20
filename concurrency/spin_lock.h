/*
 * spin_lock.h
 */

#ifndef CONCURRENCY_SPINLOCK_H_
#define CONCURRENCY_SPINLOCK_H_

#include <atomic>
#include <cstdio>
#include <pthread.h>

namespace conc11 {

static const uint_fast16_t SPIN_CYCLES_BEFORE_YIELD = 100;
static const uint_fast16_t SPIN_CYCLES_BEFORE_YIELD_FAIR = 100;

/**
 * A simple unfair spin lock that satisfies Lockable.
 */
class SpinLock {
public:

    SpinLock() noexcept = default;
    SpinLock(const SpinLock &rhs) = delete;
    SpinLock &operator=(const SpinLock &rhs) = delete;

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
 * This type of spin locks tends to become very slow under heavy contention. Consider using
 * SpinLock if fairness is not a huge concern.
 */
class FairSpinLock {
public:
    FairSpinLock() noexcept:
    next(0), active(0) {
    }

    FairSpinLock(const FairSpinLock &rhs) = delete;
    FairSpinLock &operator=(const FairSpinLock &rhs) = delete;

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

} // namespace conc11

#endif /* CONCURRENCY_SPINLOCK_H_ */
