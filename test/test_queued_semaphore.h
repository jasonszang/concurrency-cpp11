/**
 * test_queued_semaphore.h
 */

#ifndef TEST_TEST_QUEUED_SEMAPHORE_H_
#define TEST_TEST_QUEUED_SEMAPHORE_H_

#include <chrono>
#include <thread>
#include <vector>
#include "../concurrency/queued_semaphore.h"
#include "../concurrency/semaphore.h"

namespace ttb {

namespace test {

void thread_func_b(ttb::BasicQueuedSemaphore<ttb::SpinLock> *bqs, int id) {
    for (int i = 0; i < 100; ++i) {
//        printf("Acquiring\n");
        bqs->acquire();

//        printf("Thread id: %d, Permits = %d, Waiting nodes = %d\n", id, bqs->permits, bqs->num_waiting_nodes());
//        volatile int j;
//        for (j = 0; j < 1000000; ++j)
//            ;
        std::this_thread::sleep_for(std::chrono::milliseconds(3));

//        printf("Releasing\n");
        bqs->release();
    }
    printf("Blocking thread %d out\n", id);
}

void thread_func_nb(ttb::BasicQueuedSemaphore<ttb::SpinLock> *bqs, int id) {
    for (int i = 0; i < 100; ++i) {
        while (!bqs->try_acquire_for(10, 0))
            ;

//        volatile int j;
//        for (j = 0; j < 1000000; ++j)
//            ;
        std::this_thread::sleep_for(std::chrono::milliseconds(3));

        bqs->release();
    }
    printf("Non-blocking thread %d out\n", id);
}

void test_queued_semaphore() {
    static const int NUM_THREADS = 256;
    auto start = std::chrono::system_clock::now();
    ttb::BasicQueuedSemaphore<ttb::SpinLock> bqs(64);
    std::vector<std::thread> blocking_threads;
    for (int i = 0; i < NUM_THREADS; ++i) {
        blocking_threads.emplace_back(thread_func_b, &bqs, i);
    }
    std::vector<std::thread> non_blocking_threads;
    for (int i = NUM_THREADS; i < NUM_THREADS * 2; ++i) {
        non_blocking_threads.emplace_back(thread_func_nb, &bqs, i);
    }

    for (int i = 0; i < NUM_THREADS; ++i) {
        blocking_threads[i].join();
    }
    for (int i = 0; i < NUM_THREADS; ++i) {
        non_blocking_threads[i].join();
    }

    auto end = std::chrono::system_clock::now();
    std::chrono::duration<float> elapsed_seconds = end - start;
    printf("%.3f\n", elapsed_seconds.count());
}

} // namespace test

} // namespace ttb

#endif /* TEST_TEST_QUEUED_SEMAPHORE_H_ */
