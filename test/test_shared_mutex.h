/*
 * file test_shared_mutex.h
 */

#ifndef TEST_TEST_SHARED_MUTEX_H_
#define TEST_TEST_SHARED_MUTEX_H_

#include "../concurrency/shared_mutex.h"
#include <chrono>
#include <thread>

namespace ttb {

namespace test {

void reader_func(int id, std::mutex *sm, int* shared_data) {
    for (int i = 0; i < 100; ++i) {
        sm->lock();
//        printf("Reader %d\t, %d\n", id, *shared_data);
        std::this_thread::sleep_for(std::chrono::microseconds(100));
        sm->unlock();
        std::this_thread::sleep_for(std::chrono::microseconds(500));
    }
}

void writer_func(int id, std::mutex *sm, int* shared_data) {
    for (int i = 0; i < 100; ++i) {
        sm->lock();
        *shared_data += 1;
//        printf("Writer %d\t, %d\n", id, *shared_data);
        std::this_thread::sleep_for(std::chrono::microseconds(1000));
        sm->unlock();
        std::this_thread::sleep_for(std::chrono::microseconds(500));
    }
}

void test_shared_mutex() {
    const static int NUM_READERS = 256;
    const static int NUM_WRITERS = 8;
    std::vector<std::thread> readers;
    readers.reserve(NUM_READERS);
    std::vector<std::thread> writers;
    writers.reserve(NUM_WRITERS);

    std::mutex sm;
    int shared_data = 0;

    for (int i = 0; i < NUM_READERS; ++i) {
        readers.emplace_back(reader_func, i, &sm, &shared_data);
    }
    for (int i = 0; i < NUM_WRITERS; ++i) {
        writers.emplace_back(writer_func, i, &sm, &shared_data);
    }

    for (auto& thread : readers) {
        thread.join();
    }
    for (auto& thread : writers) {
        thread.join();
    }
}

} // namespace test

} // namespace ttb

#endif /* TEST_TEST_SHARED_MUTEX_H_ */
