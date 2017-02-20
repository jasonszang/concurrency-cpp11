/**
 * shared_mutex.h
 */
#ifndef CONCURRENCY_SHARED_MUTEX_H_
#define CONCURRENCY_SHARED_MUTEX_H_

/**
 * Alternative implementations of shared mutex for C++11. Use C++14 std::shared_timed_mutex and
 * C++17 std::shared_mutex if available.
 */

namespace ttb {

/**
 * An implementation of shared mutex that satisfies C++14 SharedMutex concept and does not starve
 * readers or writers.
 */
class SharedMutex {
public:
    SharedMutex() = default;

    SharedMutex(const SharedMutex&) = delete;
    SharedMutex& operator=(const SharedMutex&) = delete;

    // Execlusive lock
    void lock() {
        std::unique_lock<std::mutex> lock(mtx);
        while (state & WRITER_ENTERED_MASK) {
            rgate.wait(lock);
        }
        state |= WRITER_ENTERED_MASK;
        while (state & NUM_READER_MASK) {
            wgate.wait(lock);
        }
    }

    void unlock() {
        {
            std::lock_guard<std::mutex> lock(mtx);
            state &= ~WRITER_ENTERED_MASK; // should always set to 0
        }
        rgate.notify_all();
    }

    bool try_lock() {
        std::unique_lock<std::mutex> lock(mtx, std::try_to_lock);
        if (lock.owns_lock() && state == 0) {
            return true;
        } else {
            return false;
        }
    }

    void lock_shared() {
        std::unique_lock<std::mutex> lock(mtx);
        while ((state & WRITER_ENTERED_MASK) || ((state & NUM_READER_MASK) == NUM_READER_MASK)) {
            rgate.wait(lock);
        }
        state += 1;
    }

    void unlock_shared() {
        std::unique_lock<std::mutex> lock(mtx);
        state -= 1;
        uint_fast32_t num_readers_left = state & NUM_READER_MASK;
        if ((state & WRITER_ENTERED_MASK) && (num_readers_left == 0)) {
            lock.unlock();
            wgate.notify_one();
        } else if (num_readers_left == NUM_READER_MASK - 1) {
            lock.unlock();
            rgate.notify_one();
        }
    }

    bool try_lock_shared() {
        std::unique_lock<std::mutex> lock(mtx, std::try_to_lock);
        if (lock.owns_lock() && !(state & WRITER_ENTERED_MASK)
                && ((state & NUM_READER_MASK) != NUM_READER_MASK)) {
            state += 1;
            return true;
        } else {
            return false;
        }
    }

private:
    static const uint_fast32_t WRITER_ENTERED_MASK = 1U << 31;
    static const uint_fast32_t NUM_READER_MASK = WRITER_ENTERED_MASK - 1;

    // Main lock
    std::mutex mtx;

    // Readers pass this gate has shared ownership. New readers and writers wait at this gate
    // if one writer has passed rgate.
    std::condition_variable rgate;

    // Writer pass this gate has exclusive ownership. Only the one writer passed rgate waits
    // at this gate for all remaining readers to leave.
    std::condition_variable wgate;

    // Combined state: The highest bit indicated weather a writer has entered (i.e. passed rgate),
    // lower 31 bits is number of active readers.
    uint_fast32_t state = 0;
};

} // namespace ttb

#endif /* CONCURRENCY_SHARED_MUTEX_H_ */
