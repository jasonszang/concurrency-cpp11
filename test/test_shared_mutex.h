/*
 * file test_shared_mutex.h
 */

#ifndef TEST_TEST_SHARED_MUTEX_H_
#define TEST_TEST_SHARED_MUTEX_H_

#include <chrono>
#include <thread>
#include <map>
#include "../concurrency/shared_mutex.h"
#include "../pthread_wrapper/pthread_shared_mutex.h"

namespace conc11 {

namespace test {

using SharedMutexType = conc11::ReaderPreferringSharedTimedMutex;

void reader_func(int id, SharedMutexType *sm, int* shared_data) {
    for (int i = 0; i < 100; ++i) {
        conc11::SharedLock<SharedMutexType> lock(*sm);
//        conc11::SharedLock<conc11::SharedTimedMutex> lock(*sm, std::defer_lock);
//        while (!lock.try_lock_for(std::chrono::microseconds(1000))) {
//            printf("Reader %d\ttimeout\n", id);
//        }
        printf("Reader %d\t, %d\n", id, *shared_data);
        std::this_thread::sleep_for(std::chrono::microseconds(100));
        lock.unlock();
        std::this_thread::sleep_for(std::chrono::microseconds(500));
    }
}

void writer_func(int id, SharedMutexType *sm, int* shared_data) {
    for (int i = 0; i < 100; ++i) {
        std::unique_lock<SharedMutexType> lock(*sm);
//        std::unique_lock<conc11::SharedTimedMutex> lock(*sm, std::defer_lock);
//        while (!lock.try_lock_for(std::chrono::microseconds(1000))) {
//            printf("Writer %d\ttimeout\n", id);
//        }
        *shared_data += 1;
        printf("Writer %d\t, %d\n", id, *shared_data);
        std::this_thread::sleep_for(std::chrono::microseconds(100));
        lock.unlock();
        std::this_thread::sleep_for(std::chrono::microseconds(500));
    }
}

void reader_func_cont(int id, SharedMutexType* sm, std::map<int, std::string>* shared_map) {
    for (int i = 0; i < 100; ++i) {
        conc11::SharedLock<SharedMutexType> lock(*sm, std::defer_lock);

//        lock.lock();

//        while (!lock.try_lock()) {
//            std::this_thread::yield();
//        }

        while (!lock.try_lock_for(std::chrono::microseconds(1000))) {
        }

        if (shared_map->size() != 0) {
            printf("Reader %d, current size: %lu, current largest number: %s\n",
                    id, shared_map->size(), (--shared_map->end())->second.c_str());
        } else {
            printf("Reader %d, Empty map\n", id);
        }
        lock.unlock();
        std::this_thread::sleep_for(std::chrono::microseconds(1000));
    }
}

void writer_func_cont(int id, SharedMutexType* sm, std::map<int, std::string>* shared_map) {
    for (int i = 0; i < 100; ++i) {
        std::unique_lock<SharedMutexType> lock(*sm, std::defer_lock);

//        lock.lock();

//        while (!lock.try_lock()) {
//            std::this_thread::yield();
//        }

        while (!lock.try_lock_for(std::chrono::microseconds(1000))) {
        }

        int key = id * 100 + i;
        auto iter = shared_map->find(key);
        if (iter == shared_map->end()) {
            shared_map->emplace(key, std::to_string(key));
            printf("Writer %d, inserted %d\n", id, key);
        }
        std::this_thread::sleep_for(std::chrono::microseconds(1000));
    }
}

template <class SharedDataType, class ReaderFunc, class WriterFunc>
void do_test_shared_mutex(SharedDataType* shared_data, ReaderFunc rf, WriterFunc wf,
                          int num_readers, int num_writers) {
    std::vector<std::thread> readers;
    readers.reserve(num_readers);
    std::vector<std::thread> writers;
    writers.reserve(num_writers);

    SharedMutexType sm;

    for (int i = 0; i < num_readers; ++i) {
        readers.emplace_back(rf, i, &sm, shared_data);
    }
    for (int i = 0; i < num_writers; ++i) {
        writers.emplace_back(wf, i, &sm, shared_data);
    }

    for (auto& thread : readers) {
        thread.join();
    }
    for (auto& thread : writers) {
        thread.join();
    }
}

// Mainly for testing throughput
void test_shared_mutex() {
    int shared_data = 0;
    do_test_shared_mutex(&shared_data, reader_func, writer_func, 256, 8);
}

// Mainly for testing correctness
void test_shared_mutex_cont() {
    std::map<int, std::string> shared_map;
    do_test_shared_mutex(&shared_map, reader_func_cont, writer_func_cont, 1, 64);
    printf("Final size: %lu\n", shared_map.size());
}

} // namespace test

} // namespace conc11

#endif /* TEST_TEST_SHARED_MUTEX_H_ */
