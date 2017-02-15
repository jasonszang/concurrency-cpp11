/**
 * test_lru_cache.h
 */
#ifndef TEST_TEST_LRU_CACHE_H_
#define TEST_TEST_LRU_CACHE_H_

#include <thread>
#include <vector>
#include <unordered_map>
#include "../util/lru_cache.h"
#include "../util/util.h"

namespace conc11 {

namespace test {

struct Key {
    explicit Key(int k) noexcept:k(k) {
    }

    Key(const Key& rhs) noexcept:k(rhs.k) {
        printf("Key copy constructed\n");
    }

    Key(Key&& rhs) noexcept:k(rhs.k) {
        printf("Key move constructed\n");
    }

    bool operator==(const Key& rhs) const {
        return k == rhs.k;
    }

    int k;
};

}

}

namespace std {
template<>
struct hash<conc11::test::Key> {
    size_t operator()(const conc11::test::Key& x) const {
        return x.k;
    }
};
}

namespace conc11 {

namespace test {

static void test_lru_copy_pointee() {
    conc11::LRUCache<int, std::unique_ptr<int>> iml(1);
    auto m1 = conc11::make_unique<int>(10);
    auto m2 = conc11::make_unique<int>(20);
    iml.set(1, std::move(m1));
    iml.set(2, std::move(m2));
    int out = 0;
    iml.get_copy_pointee(1, &out);
    printf("get_copy_pointee with key 1: %d\n", out);
    iml.get_copy_pointee(2, &out);
    printf("get_copy_pointee with key 2: %d\n", out);
}

static void test_lru_copy_pointee_ptr() {
    conc11::LRUCache<int, int*> iml(1);
    auto m1 = new int(10);
    auto m2 = new int(20);
    iml.set(1, std::move(m1));
    iml.set(2, std::move(m2));
    int out = 0;
    iml.get_copy_pointee(1, &out);
    printf("get_copy_pointee with key 1: %d\n", out);
    iml.get_copy_pointee(2, &out);
    printf("get_copy_pointee with key 2: %d\n", out);
}

template<class LRU>
static void thread_func(LRU* lru) {
    for (int i = 0; i < 100000; ++i) {
        lru->set(i % 337, i % 613);
        int dummy = 0;
        lru->get_copy(i, &dummy);
    }
}

static void test_blocking_lru() {
    conc11::BlockingLRUCache<int, int, std::hash<int>, std::equal_to<int>, std::mutex> lru(100);
    std::vector<std::thread> threads;
    for (int i = 0; i < 10; ++i) {
        threads.emplace_back(&thread_func<decltype(lru)>, &lru);
    }
    for (auto& th : threads) {
        th.join();
    }
}

template<class LRU>
static void thread_func_large_obj(LRU* lru) {
    for (int i = 0; i < 100000; ++i) {
        std::vector<int> data;
        for (int j = 0; j < 4096; ++j) {
            data.emplace_back(j);
        }
        lru->set(i, std::move(data));
        std::vector<int> dummy;
        lru->get_copy(i, &dummy);
    }
}

static void test_blocking_lru_large_obj() {
    conc11::BlockingLRUCache<int, std::vector<int>> lru(10000);
    std::vector<std::thread> threads;
    for (int i = 0; i < 10; ++i) {
        threads.emplace_back(&thread_func_large_obj<decltype(lru)>, &lru);
    }
    for (auto& th : threads) {
        th.join();
    }
}

void run_test_lru_cache() {
    test_lru_copy_pointee();
    test_lru_copy_pointee_ptr();

    std::chrono::microseconds dur;
    conc11::timed_invoke(&dur, test_blocking_lru);
    printf("Micros elapsed: %lu\n", dur.count());
    conc11::timed_invoke(&dur, test_blocking_lru_large_obj);
    printf("Micros elapsed: %lu\n", dur.count());
}

} // namespace test

} // namespace conc11

#endif /* TEST_TEST_LRU_CACHE_H_ */
