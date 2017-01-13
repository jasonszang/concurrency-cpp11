/**
 * queued_semaphore.h
 */
#ifndef CONCURRENCY_QUEUED_SEMAPHORE_H_
#define CONCURRENCY_QUEUED_SEMAPHORE_H_

#include <chrono>
#include <condition_variable>
#include <type_traits>
#include "spin_lock.h"
#include "../pthread_wrapper/pthread_spinlock.h"

namespace ttb {

template<class LockType>
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
template<class LockType>
class WaitQueue {
public:

    WaitQueue() {
        alloc_cache();
    }

    ~WaitQueue() {
        while (head) {
            WaitNode<LockType> *next = head->next;
            delete head;
            head = next;
        }
        while (cache_head) {
            WaitNode<LockType> *next = cache_head->next;
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
    WaitNode<LockType> *enqueue() {
        if (!cache_head) {
            alloc_cache();
            cur_queue_capacity <<= 1;
        }
        WaitNode<LockType> *cur = cache_head;
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
    void remove(WaitNode<LockType> *node) {
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
            WaitNode<LockType> *p = head;
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
            WaitNode<LockType> *cur = new WaitNode<LockType>();
            cur->next = cache_head;
            cache_head = cur;
        }
    }

    // Doubly linked list as the waiting queue
    WaitNode<LockType> *head = nullptr;
    WaitNode<LockType> *tail = nullptr;

    // **Forward** linked list of cached nodes
    WaitNode<LockType> *cache_head = nullptr;

    size_t cur_queue_capacity = 256;
};

template <class LockType>
class BasicQueuedSemaphore {
public:
    BasicQueuedSemaphore() :
            BasicQueuedSemaphore(0) {
    }

    BasicQueuedSemaphore(int initial_permits) :
            permits(initial_permits) {
    }

    void acquire() {
        try_acquire0(false,
                (std::chrono::time_point<std::chrono::steady_clock,
                        std::chrono::microseconds>*) nullptr);
    }

    void release() {
        std::lock_guard<ttb::SpinLock> lock(main_lock);
        permits += 1;
        queue.wake_head(); // XXX
    }

    /**
     * Untimed try_acquire. Note that untimed version of try_acquire is not fair.
     */
    bool try_acquire() {
        std::lock_guard<ttb::SpinLock> lock(main_lock);
        if (permits >= 1) {
            permits -= 1;
            return true;
        } else {
            return false;
        }
    }

    bool try_acquire_for(unsigned long millis, unsigned int micros) {
        std::chrono::steady_clock::time_point timeout_time = std::chrono::steady_clock::now() +
                std::chrono::milliseconds(millis) + std::chrono::microseconds(micros);
        return try_acquire0(true, &timeout_time);
    }

    template<class Rep, class Period>
    bool try_acquire_for(const std::chrono::duration<Rep, Period>& timeout_duration) {
        std::chrono::steady_clock::time_point timeout_time = std::chrono::steady_clock::now() +
                timeout_duration;
        return try_acquire0(true, &timeout_time);
    }

    template<class Clock, class Duration>
    bool try_acquire_until(const std::chrono::time_point<Clock, Duration> &timeout_time) {
        return try_acquire0(true, &timeout_time);
    }

private:
    // private

    template<class Clock, class Duration>
    bool try_acquire0(bool timed, const std::chrono::time_point<Clock, Duration> *timeout_time) {
        std::unique_lock<ttb::SpinLock> lock(main_lock);
        if (permits >= 1 && queue.is_empty()) {
            permits -= 1;
            return true;
        }

        if (!timed) {
            while (true) {
                WaitNode<LockType> *wait_node = queue.enqueue();
                wait_node->cv.wait(lock, [wait_node]() {return wait_node->wakeable;});
                queue.dequeue();
                if (permits >= 1) {
                    break;
                }
            }
        } else {
            while (true) {
                WaitNode<LockType> *wait_node = queue.enqueue();
                bool timeout = !(wait_node->cv.wait_until(lock, *timeout_time,
                        [wait_node]() {return wait_node->wakeable;}));
                if (timeout) {
                    queue.remove(wait_node);
                    return false;
                }
                queue.dequeue();
                if (permits >= 1) {
                    break;
                }
            }
        }

        // When control reaches here current thread is at the head of the queue and permits >= 1
        permits -= 1;
        if (permits >= 1) {
            queue.wake_head(); // propogate waking signal if there are permits left now
        }
        if (permits < 0) {
            printf("BOOM!");
            std::terminate(); // BOOM when something went very wrong. Will be removed later.
        }

        return true;
    }

    int permits = 0;
    LockType main_lock;
    WaitQueue<LockType> queue;
};

} // namespace ttb

#endif /* CONCURRENCY_QUEUED_SEMAPHORE_H_ */
