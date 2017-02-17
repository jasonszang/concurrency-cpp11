/**
 * test_latch.h
 */
#ifndef TEST_TEST_LATCH_H_
#define TEST_TEST_LATCH_H_

#include <atomic>
#include <cassert>
#include <thread>
#include <vector>
#include "../concurrency/latch.h"

namespace conc11 {

namespace test {

static std::atomic<int> ctr;

void thread_func_waiter(Latch* latch) {
    volatile int i = 0;
    for (; i < 10000; ++i)
        ;
    latch->wait();
    auto ctr_val = ctr.load(std::memory_order_relaxed);
    if (ctr_val != 10) {
        printf("ctr = %d\n", ctr_val);
        assert(false);
    }
//    printf("Thread awaken from latch\n");
}

void thread_func_counter(Latch* latch) {
    volatile int i = 0;
    for (; i < 10000; ++i)
        ;
    ctr.fetch_add(1, std::memory_order_relaxed);
    latch->count_down(1);
//    printf("Counted down by 1\n");
}

void test_latch() {
    Latch latch(10);
    std::vector<std::thread> waiters;
    std::vector<std::thread> counters;
    for (int i = 0; i < 10; ++i) {
        waiters.emplace_back(thread_func_waiter, &latch);
    }
    for (int i = 0; i < 10; ++i) {
        counters.emplace_back(thread_func_counter, &latch);
    }
    for (auto& th : waiters) {
        th.join();
    }
    for (auto& th : counters) {
        th.join();
    }
}

} // namespace test

} // namespace conc11



#endif /* TEST_TEST_LATCH_H_ */
