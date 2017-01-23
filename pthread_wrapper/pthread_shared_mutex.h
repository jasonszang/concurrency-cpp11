/**
 * pthread_shared_mutex.h
 */
#ifndef PTHREAD_WRAPPER_PTHREAD_SHARED_MUTEX_H_
#define PTHREAD_WRAPPER_PTHREAD_SHARED_MUTEX_H_

#include <cassert>
#include <chrono>
#include <errno.h>
#include <pthread.h>
#include <system_error>

namespace ttb {

class PThreadSharedMutex {
public:
    PThreadSharedMutex() {
        int ret = pthread_rwlock_init(&rwlock_handle, NULL);
        if (ret == ENOMEM) {
            throw(std::bad_alloc());
        } else if (ret == EAGAIN) {
            throw(std::system_error(
                    std::make_error_code(std::errc::resource_unavailable_try_again)));
        } else if (ret == EPERM) {
            throw(std::system_error(
                    std::make_error_code(std::errc::operation_not_permitted)));
        }
        // Errors not handled: EBUSY, EINVAL
        assert(ret == 0);
    }

    PThreadSharedMutex(const PThreadSharedMutex&) = delete;
    PThreadSharedMutex& operator=(const PThreadSharedMutex&) = delete;

    ~PThreadSharedMutex() {
        int ret = pthread_rwlock_destroy(&rwlock_handle);
        (void) ret;
        // Errors not handled: EBUSY, EINVAL
        assert(ret == 0);
    }

    void lock() {
        int ret = pthread_rwlock_wrlock(&rwlock_handle);
        if (ret == EDEADLK) {
            throw(std::system_error(
                    std::make_error_code(std::errc::resource_deadlock_would_occur)));
        }
        // Errors not handled: EINVAL
        assert(ret == 0);
    }

    void unlock() {
        int ret = pthread_rwlock_unlock(&rwlock_handle);
        (void) ret;
        // Errors not handled: EPERM, EBUSY, EINVAL
        assert(ret == 0);
    }

    bool try_lock() {
        int ret = pthread_rwlock_trywrlock(&rwlock_handle);
        if (ret == EBUSY) {
            return false;
        }
        // Errors not handled: EINVAL
        assert(ret == 0);
        return true;
    }

    void lock_shared() {
        int ret;
        while (true) {
            ret = pthread_rwlock_rdlock(&rwlock_handle);
            if (ret != EAGAIN) {
                break;
            }
        }
        if (ret == EDEADLK) {
            throw(std::system_error(
                    std::make_error_code(std::errc::resource_deadlock_would_occur)));
        }
        // Errors not handled: EINVAL
        assert(ret == 0);
    }

    bool try_lock_shared() {
        int ret = pthread_rwlock_tryrdlock(&rwlock_handle);
        if (ret == EBUSY || ret == EAGAIN) {
            return false;
        }
        // Errors not handled: EINVAL
        assert(ret == 0);
        return true;
    }

    void unlock_shared() {
        unlock();
    }

private:
    pthread_rwlock_t rwlock_handle;
};

} // namespace ttb

#endif /* PTHREAD_WRAPPER_PTHREAD_SHARED_MUTEX_H_ */
