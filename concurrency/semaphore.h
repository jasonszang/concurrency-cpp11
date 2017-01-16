/*
 * semaphore.h
 *
 *  Created on: 2016-7-5
 *      Author: jasonszang
 */

#ifndef CONCURRENCY_SEMAPHORE_H_
#define CONCURRENCY_SEMAPHORE_H_

#include <chrono>
#include <condition_variable>
#include <map>
#include <mutex>
#include <type_traits>

namespace ttb {

template <class LockType>
class BasicQueuedSemaphore {
public:
    BasicQueuedSemaphore() :
            BasicQueuedSemaphore(0) {
    }

    explicit BasicQueuedSemaphore(int initial_permits) :
            permits(initial_permits) {
    }

    BasicQueuedSemaphore(const BasicQueuedSemaphore&) = delete;
    BasicQueuedSemaphore& operator=(const BasicQueuedSemaphore&) = delete;

    void acquire() {
        acquire(1);
    }

    void acquire(unsigned int request) {
        try_acquire0(false, request,
                (std::chrono::time_point<std::chrono::steady_clock,
                        std::chrono::microseconds>*) nullptr);
    }

    void release() {
        release(1);
    }

    void release(unsigned int request) {
        std::lock_guard<LockType> lock(main_lock);
        permits += request;
        if (permits >= request_record_min()) {
            queue.wake_head();
        }
    }

    /**
     * Untimed try_acquire. Note that untimed version of try_acquire is not fair.
     */
    bool try_acquire() {
        return try_acquire(1);
    }

    bool try_acquire(unsigned int request) {
        std::lock_guard<LockType> lock(main_lock);
        if (permits >= request) {
            permits -= request;
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
        return try_acquire0(true, request, &timeout_time);
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
        return try_acquire0(true, request, &timeout_time);
    }

    template<class Clock, class Duration>
    bool try_acquire_until(const std::chrono::time_point<Clock, Duration> &timeout_time) {
        return try_acquire_until(1, &timeout_time);
    }

    template<class Clock, class Duration>
    bool try_acquire_until(unsigned int request,
                           const std::chrono::time_point<Clock, Duration> &timeout_time) {
        return try_acquire0(true, &timeout_time);
    }

    int available_permits() {
        std::lock_guard<LockType> lock(main_lock);
        return permits;
    }

private:
    struct WaitNode {
        typename std::conditional<std::is_same<LockType, std::mutex>::value,
                std::condition_variable,
                std::condition_variable_any>::type cv;
        bool wakeable = false;
        WaitNode *prev = nullptr;
        WaitNode *next = nullptr;
    };

    /**
     * Implementation of the thread waiting queue, a cached linked list of waiting nodes. This class
     * is not thread safe and should be protected by the outer semaphore.
     */
    class WaitQueue {
    public:

        WaitQueue() {
            alloc_cache();
        }

        ~WaitQueue() {
            while (head) {
                WaitNode *next = head->next;
                delete head;
                head = next;
            }
            while (cache_head) {
                WaitNode *next = cache_head->next;
                delete cache_head;
                cache_head = next;
            }
        }

        WaitQueue(const WaitQueue &) = delete;
        WaitQueue &operator=(const WaitQueue &) = delete;

        /**
         * Enqueue a waiting node at the tail and return a pointer to it.
         * May cause memory allocation when cached wait nodes are depleted.
         */
        WaitNode *enqueue() {
            if (!cache_head) {
                alloc_cache();
                cur_queue_capacity <<= 1;
            }
            WaitNode *cur = cache_head;
            cache_head = cache_head->next;

            cur->wakeable = false;
            // cur->prev = nullptr; // already guaranteed
            cur->next = nullptr;
            if (!tail) {
                head = cur;
                tail = cur;
            } else {
                tail->next = cur;
                cur->prev = tail;
                tail = cur;
            }
            return cur;
        }

        /**
         * Dequeue WaitNode at head of the queue, returning it to the cache.
         */
        void dequeue() {
            remove(head);
        }

        /**
         * Remove a WaitNode from the queue and return it to the cache. Undefined behaviour if node is
         * not already in the queue. Do nothing if node is nullptr.
         */
        void remove(WaitNode *node) {
            if (!node) {
                return;
            }
            if (node == head && head == tail) {
                head = nullptr;
                tail = nullptr;
            } // handle special condition in which the last node is removed from the queue
            if (node == head) {
                head = node->next;
            }
            if (node == tail) {
                tail = node->prev;
            }
            if (node->prev) {
                node->prev->next = node->next;
            }
            if (node->next) {
                node->next->prev = node->prev;
            }

            node->prev = nullptr;
            node->next = cache_head;
            cache_head = node;
        }

        /**
         * Wake the thread waiting at head of the queue.
         */
        void wake_head() {
            if (!head) {
                return;
            }
            head->wakeable = true;
            head->cv.notify_all();
        }

        bool is_empty() {
            return (!head);
        }

        int num_waiting_nodes() {
            if (!head) {
                return 0;
            } else {
                int ctr = 0;
                WaitNode *p = head;
                while (p) {
                    ctr += 1;
                    p = p->next;
                }
                return ctr;
            }
        }

    private:

        void alloc_cache() {
            for (size_t i = 0; i < cur_queue_capacity; ++i) {
                WaitNode *cur = new WaitNode();
                cur->next = cache_head;
                cache_head = cur;
            }
        }

        // Doubly linked list as the waiting queue
        WaitNode *head = nullptr;
        WaitNode *tail = nullptr;

        // **Forward** linked list of cached nodes
        WaitNode *cache_head = nullptr;

        size_t cur_queue_capacity = 256;
    };

    template<class Clock, class Duration>
    bool try_acquire0(bool timed, unsigned int request,
                      const std::chrono::time_point<Clock, Duration> *timeout_time) {
        std::unique_lock<LockType> lock(main_lock);
//        printf("permits before acquire: %d\n", permits);
        if (permits >= (int) request && queue.is_empty()) {
            permits -= request;
            return true;
        }

        request_record_insert(request);
        if (!timed) {
            while (true) {
                WaitNode *wait_node = queue.enqueue();
                wait_node->cv.wait(lock, [wait_node]() {return wait_node->wakeable;});
                queue.dequeue();
                if (permits >= (int) request) {
                    break;
                }
            }
        } else {
            while (true) {
                WaitNode *wait_node = queue.enqueue();
                bool timeout = !(wait_node->cv.wait_until(lock, *timeout_time,
                        [wait_node]() {return wait_node->wakeable;}));
                if (timeout) {
                    queue.remove(wait_node);
                    return false;
                }
                queue.dequeue();
                if (permits >= (int) request) {
                    break;
                }
            }
        }
        request_record_remove(request);

        // When control reaches here current thread is at the head of the queue and
        // permits are enough
        permits -= request;
        if (permits >= request_record_min()) {
            queue.wake_head(); // propogate waking signal if there are permits left now
        }
        if (permits < 0) {
            printf("BOOM!");
            std::terminate(); // BOOM when something went very wrong. Will be removed later.
        }

        return true;
    }

    void request_record_insert(unsigned int request) {
        auto iter = request_record.find(request);
        if (iter != request_record.end()) {
            iter->second += 1;
        } else {
            request_record.emplace(request, 1);
        }
    }

    void request_record_remove(unsigned int request) {
        auto iter = request_record.find(request);
        if (iter != request_record.end()) {
            if (iter->second == 1) {
                request_record.erase(iter);
            } else {
                iter->second -= 1;
            }
        }
    }

    int request_record_min() {
        if (request_record.begin() != request_record.end()) {
            return request_record.begin()->first;
        } else {
            return 0;
        }
    }

    int permits = 0;
    LockType main_lock;
    WaitQueue queue;
    std::map<unsigned int, std::size_t> request_record;
};

template<class LockType>
class Semaphore {
public:

    explicit Semaphore(int initial_permits) :
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
