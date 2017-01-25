/**
 * test_queued_semaphore.h
 */

#ifndef TEST_TEST_QUEUED_SEMAPHORE_H_
#define TEST_TEST_QUEUED_SEMAPHORE_H_

#include <chrono>
#include <thread>
#include <vector>

#include "../concurrency/semaphore.h"

namespace conc11 {

namespace test {

template <class Semaphore>
void thread_func_b(Semaphore *sem, int id) {
    for (int i = 0; i < 100; ++i) {
//        printf("Acquiring\n");
        SemaphoreGuard<conc11::QueuedSemaphore<std::mutex>> sg(*sem, i % 20);
//        printf("Thread id: %d, Permits = %d, Waiting nodes = %d\n", id, bqs->permits, bqs->num_waiting_nodes());
        std::this_thread::sleep_for(std::chrono::milliseconds(3));
//        printf("Releasing\n");
    }
//    printf("Blocking thread %d out\n", id);
}

template <class Semaphore>
void thread_func_nb(Semaphore *sem, int id) {
    for (int i = 0; i < 100; ++i) {
        while (!sem->try_acquire_for(10, 0))
            ;
        std::this_thread::sleep_for(std::chrono::milliseconds(3));

        sem->release();
    }
//    printf("Non-blocking thread %d out\n", id);
}

void test_queued_semaphore() {
    using SemaphoreType = conc11::QueuedSemaphore<std::mutex>;
    static const int NUM_THREADS = 512;
    SemaphoreType sem(64);
    std::vector<std::thread> blocking_threads;
    for (int i = 0; i < NUM_THREADS; ++i) {
        blocking_threads.emplace_back(thread_func_b<SemaphoreType>, &sem, i);
    }
//    std::vector<std::thread> non_blocking_threads;
//    for (int i = NUM_THREADS; i < NUM_THREADS * 2; ++i) {
//        non_blocking_threads.emplace_back(thread_func_nb, &bqs, i);
//    }

    for (int i = 0; i < NUM_THREADS; ++i) {
        blocking_threads[i].join();
    }
//    for (int i = 0; i < NUM_THREADS; ++i) {
//        non_blocking_threads[i].join();
//    }
}

} // namespace test

} // namespace conc11

#endif /* TEST_TEST_QUEUED_SEMAPHORE_H_ */
