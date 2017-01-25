/*
 * test_spin_lock.h
 *
 *  Created on: 2016-12-30
 *      Author: jasonszang
 */

#ifndef TEST_TEST_SPIN_LOCK_H_
#define TEST_TEST_SPIN_LOCK_H_

#include <cstdio>
#include <mutex>
#include <thread>
#include <vector>

#include "../concurrency/spin_lock.h"
#include "../pthread_wrapper/pthread_spinlock.h"
#include "../concurrency/semaphore.h"

namespace conc11 {

namespace test {

static int g = 100000;
conc11::SpinLock sl;
conc11::PThreadSpinLockWrapper psw;
conc11::FairSpinLock fsl;

conc11::SimpleSemaphore<std::mutex> sem(1);
conc11::SemaphoreTimedLockableAdapter<decltype(sem)> sem_lock(sem, 1);
std::mutex m;

inline void thread_function(int num) {
    for (int i = 0; i < num; ++i) {
//        std::lock_guard<conc11::SpinLock> l(sl);
        std::lock_guard<conc11::PThreadSpinLockWrapper> l(psw);
//        std::lock_guard<conc11::FairSpinLock> l(fsl);
//        std::lock_guard<decltype(sem_lock)> l(sem_lock);
//        std::lock_guard<std::mutex> l(m);
        g -= 1;
        volatile int j = 500;
        for (; j != 0; --j)
            ; // do some work
//        printf("%d\n", g);
    }
}

inline void test_spin_lock() {
    std::vector<std::thread> threads;
    for (int i = 0; i < 10; ++i) {
        threads.emplace_back(&thread_function, 10000);
    }
    for (auto &thread : threads) {
        thread.join();
    }
    printf("This should be zero: %d\n", g);
}

} // namespace test

} // namespace conc11

#endif /* TEST_TEST_SPIN_LOCK_H_ */
