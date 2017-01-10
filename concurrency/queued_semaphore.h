/**
 * queued_semaphore.h
 */
#ifndef CONCURRENCY_QUEUED_SEMAPHORE_H_
#define CONCURRENCY_QUEUED_SEMAPHORE_H_

#include <chrono>
#include <condition_variable>
#include "spin_lock.h"
#include "../pthread_wrapper/pthread_spinlock.h"

namespace ttb {

struct WaitNode {
    std::condition_variable_any cv;
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
//        printf("enqueued, nodes waiting now = %d\n", num_waiting_nodes());
        return cur;
    }

    /**
     * Dequeue WaitNode at head of the queue, returning it to the cache.
     */
    void dequeue() {
//        printf("dequeueing, nodes waiting now = %d\n", num_waiting_nodes());
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

class BasicQueuedSemaphore {
public:
    BasicQueuedSemaphore() :
            BasicQueuedSemaphore(0) {
    }

    BasicQueuedSemaphore(int initial_permits) :
            permits(initial_permits) {
    }

    void acquire() {
        try_acquire0(false, 0, 0);
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

    bool try_acquire(unsigned long millis, unsigned int micros) {
        return try_acquire0(true, millis, micros);
    }

private: // private

    bool try_acquire0(bool timed, unsigned long millis, unsigned int micros) {
        std::unique_lock<ttb::SpinLock> lock(main_lock);
//        printf("Acquiring, permits now = %d\n", permits);
        if (permits >= 1 && queue.is_empty()) {
//            printf("Successfully fast-acquired, permits left = %d\n", permits);
            permits -= 1;
            return true;
        }

        if (!timed) {
            while (true) {
                WaitNode *wait_node = queue.enqueue();
                wait_node->cv.wait(lock, [wait_node]() {return wait_node->wakeable;});
                queue.dequeue();
                if (permits >= 1) {
                    break;
                }
            }
        } else {
            std::chrono::steady_clock::time_point until = std::chrono::steady_clock::now() +
                    std::chrono::milliseconds(millis) + std::chrono::microseconds(micros);
            while (true) {
                WaitNode *wait_node = queue.enqueue();
                bool timeout = !(wait_node->cv.wait_until(lock, until,
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
//        printf("Successfully acquired, permits left = %d\n", permits);
        if (permits < 0) {
            printf("BOOM!");
            std::terminate(); // BOOM when something went very wrong. Will be removed later.
        }

        return true;
    }

    int permits = 0;
    ttb::SpinLock main_lock;
    WaitQueue queue;
};

} // namespace ttb

#endif /* CONCURRENCY_QUEUED_SEMAPHORE_H_ */
