/*
 * pthread_spin_lock.h
 *
 *  Created on: 2016-12-30
 *      Author: jasonszang
 */

#ifndef PTHREAD_WRAPPER_PTHREAD_SPINLOCK_H_
#define PTHREAD_WRAPPER_PTHREAD_SPINLOCK_H_

#include <cassert>
#include <errno.h>
#include <pthread.h>
#include <system_error>

namespace conc11 {

/**
 * @brief A thin-layer wrapper for pthread_spin, satisfies Lockable.
 */
class PThreadSpinLockWrapper {
public:
    PThreadSpinLockWrapper() {
        int ret = pthread_spin_init(&l, PTHREAD_PROCESS_PRIVATE);
        if (ret == ENOMEM) {
            throw(std::bad_alloc());
        } else if (ret == EAGAIN) {
            throw(std::system_error(
                    std::make_error_code(std::errc::resource_unavailable_try_again)));
        }
        // Errors not handled: EBUSY, EINVAL
        assert(ret == 0);
    }

    ~PThreadSpinLockWrapper() {
        int ret = pthread_spin_destroy(&l);
        (void) ret;
        // Errors not handled: EBUSY, EINVAL
        assert(ret == 0);
    }

    PThreadSpinLockWrapper(const PThreadSpinLockWrapper &rhs) = delete;
    PThreadSpinLockWrapper &operator=(const PThreadSpinLockWrapper &rhs) = delete;

    void lock() {
        int ret = pthread_spin_lock(&l); // undefined if caller has the lock
        if (ret == EDEADLK) {
            throw(std::system_error(
                    std::make_error_code(std::errc::resource_deadlock_would_occur)));
        }
        // Errors not handled: EINVAL
        assert(ret == 0);
    }

    void unlock() {
        int ret = pthread_spin_unlock(&l);
        (void) ret;
        // Errors not handled: EINVAL, EPERM
        assert(ret == 0);
    }

    bool try_lock() {
        int ret = pthread_spin_trylock(&l);
        if (ret == EDEADLK || ret == EBUSY) {
            return false;
        }
        // Errors not handled: EINVAL
        assert(ret == 0);
        return true;
    }

private:
    pthread_spinlock_t l;
};

}
 // namespace conc11

#endif /* PTHREAD_WRAPPER_PTHREAD_SPINLOCK_H_ */
