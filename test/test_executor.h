/*
 * test_executor.h
 */

#ifndef TEST_TEST_EXECUTOR_H_
#define TEST_TEST_EXECUTOR_H_

#include <atomic>
#include <chrono>
#include "../concurrency/executor.h"

namespace conc11 {

namespace test {

static int dummy_func(int x, int y) {
    return 233 + x + y;
}

class Foo {
public:
    Foo(int x):x(x) {
    }
    virtual ~Foo() {
    }
    virtual int foo(int y) {
        return 233 + x + y;
    }
private:
    int x;
};

class FooFoo : public Foo{
public:
    FooFoo(int x):Foo(x){
    }
    virtual int foo(int y) override {
        int ret = 2100 + Foo::foo(y);
        printf ("%d\n", ret);
        return ret;
    }
};

void test_executor() {
    Foo f1(10000);
//    FooFoo* f2 = new FooFoo(1000000);
    auto exec = make_fixed_thread_pool(4);
    std::vector<std::future<int>> futures;
    for (int i = 0; i < 100000; ++i) {
        futures.emplace_back(exec->submit(dummy_func, 1, 2));
//        futures.emplace_back(exec.submit(&Foo::foo, &f1, 1));
//        futures.emplace_back(exec.submit(&Foo::foo, f2, 1));
//        futures.emplace_back(exec.submit(&Foo::foo, f2, 1));
//        exec.execute(dummy_func, 1, 2);
//        exec.execute(&Foo::foo, f2, 1);
    }
//    for (auto &future : futures) {
////        printf("%d\n", future.get());
//        future.get();
//    }
//    printf("active count %lu\n", exec.active_count.load());
    exec->shutdown();
    exec->await_termination();
}

class BlockingCounter {
public:
    void add(int x) {
        std::lock_guard<std::mutex> lock(mtx);
        ctr += x;
    }

    int get() {
        std::lock_guard<std::mutex> lock(mtx);
        return ctr;
    }

private:
    std::mutex mtx;
    int ctr = 0;
};

void test_executor_sync() {
    BlockingCounter bc;
    auto exec = make_cached_thread_pool();
    std::vector<std::future<void>> futures;
    for (int i = 0; i < 100000; ++i) {
        futures.emplace_back(exec->submit(&BlockingCounter::add, &bc, 1));
    }
    for (auto &future : futures) {
        future.get();
    }
    exec->shutdown();
    exec->await_termination();
    printf("Final value of blocking counter: %d\n", bc.get());
}

class AtomicCounter {
public:
    AtomicCounter() noexcept:ctr(0) {
    }

    void add(int x) {
        ctr.fetch_add(x);
    }

    int get() {
        return ctr.load();
    }
private:
    std::atomic<int> ctr;
};

void test_executor_atomic() {
    AtomicCounter ac;
    auto exec = make_cached_thread_pool();
    std::vector<std::future<void>> futures;
    for (int i = 0; i < 100000; ++i) {
        futures.emplace_back(exec->submit(&AtomicCounter::add, &ac, 1));
    }
    for (auto &future : futures) {
        future.get();
    }
    exec->shutdown();
    exec->await_termination();
    printf("Final value of atomic counter: %d\n", ac.get());
}

void test_thread_pool_executor() {
    std::chrono::microseconds dur;
    conc11::timed_invoke(&dur, test_executor);
    printf("Micros elapsed test_executor(): %lu\n", dur.count());

    conc11::timed_invoke(&dur, test_executor_sync);
    printf("Micros elapsed test_executor_sync(): %lu\n", dur.count());

    conc11::timed_invoke(&dur, test_executor_atomic);
    printf("Micros elapsed test_executor_atomic(): %lu\n", dur.count());
}

} // namespace test

} // namespace conc11

#endif /* TEST_TEST_EXECUTOR_H_ */
