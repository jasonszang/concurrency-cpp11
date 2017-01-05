/*
 * pthread_spin_lock.h
 *
 *  Created on: 2016-12-30
 *      Author: jasonszang
 */

#ifndef PTHREAD_WRAPPER_PTHREAD_SPINLOCK_H_
#define PTHREAD_WRAPPER_PTHREAD_SPINLOCK_H_

#include <pthread.h>

namespace ttb {

/**
 * @brief A thin-layer wrapper for pthread_spin, satisfies Lockable.
 */
class PThreadSpinLockWrapper {
public:
    PThreadSpinLockWrapper() noexcept: l(new pthread_spinlock_t()) {
        pthread_spin_init(l, PTHREAD_PROCESS_PRIVATE);
    }

    ~PThreadSpinLockWrapper() {
        pthread_spin_destroy(l);
        delete l;
    }

    PThreadSpinLockWrapper(const PThreadSpinLockWrapper &rhs) = delete;
    PThreadSpinLockWrapper &operator=(const PThreadSpinLockWrapper &rhs) = delete;

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

}
 // namespace ttb

#endif /* PTHREAD_WRAPPER_PTHREAD_SPINLOCK_H_ */
