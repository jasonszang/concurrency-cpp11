/**
 * queued_semaphore.h
 */
#ifndef CONCURRENCY_QUEUED_SEMAPHORE_H_
#define CONCURRENCY_QUEUED_SEMAPHORE_H_

#include <condition_variable>
#include "spin_lock.h"

namespace ttb {

struct WaitNode {
    std::condition_variable_any cv;
    bool wakeable = false;
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
     * Cause the calling thread to enter the waiting queue. May cause memory allocation when cached
     * wait nodes are depleted.
     */
    WaitNode *enqueue() {
        if (!cache_head) {
            alloc_cache();
            cur_queue_capacity <<= 1;
        }
        WaitNode *cur = cache_head;
        cache_head = cache_head->next;
        cur->wakeable = false;
        cur->next = nullptr;
        if (!tail) {
            head = cur;
            tail = cur;
        } else {
            tail->next = cur;
            tail = cur;
        }
//        printf("enqueueing, nodes waiting now = %d\n", num_waiting_nodes());
        return cur;
    }

    /**
     * Dequeue WaitNode at head of the queue, returning it to the cache.
     */
    void dequeue() {
//        printf("dequeueing, nodes waiting now = %d\n", num_waiting_nodes());
        if (!head) {
//            printf("WARNING: head = nullptr in dequeue()");
            return; // waiting queue empty, just return;
        }
        WaitNode *cur = head;
        head = head->next;
        if (tail == cur) {
            tail = nullptr;
        }
        cur->next = cache_head;
        cache_head = cur;
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

    WaitNode *head = nullptr;
    WaitNode *tail = nullptr;
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
        std::unique_lock<ttb::SpinLock> lock(main_lock);
//        printf("Acquiring, permits now = %d\n", permits);
        if (permits >= 1 && queue.is_empty()) {
//            printf("Successfully fast-acquired, permits left = %d\n", permits);
            permits -= 1;
            return;
        }

        WaitNode *wait_node = queue.enqueue();
        wait_node->cv.wait(lock, [wait_node](){return wait_node->wakeable;});

        // When waked current thread is at the head of the queue and permits >= 1
        permits -= 1;
        queue.dequeue();
        if (permits >= 1) {
            queue.wake_head();
        }
//        printf("Successfully acquired, permits left = %d\n", permits);
        if (permits < 0) {
            printf("BOOM!");
            std::terminate();
        }
    }

    void release() {
        std::lock_guard<ttb::SpinLock> lock(main_lock);
        permits += 1;
        queue.wake_head(); // XXX
    }

private: // private
    int permits = 0;
    ttb::SpinLock main_lock;
    WaitQueue queue;
};

} // namespace ttb

#endif /* CONCURRENCY_QUEUED_SEMAPHORE_H_ */
