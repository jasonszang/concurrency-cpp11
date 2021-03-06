/**
 * A simple wrapper of pthread_specific thread local storage to a smart-pointer-like object.
 */

#include <cstdio>
#include <string>
#include <vector>
#if __cplusplus >= 201103L
#include <thread>
#include <chrono>
#endif

#include "../pthread_wrapper/pthread_local_ptr.h"

namespace conc11 {

namespace test {

#if __cplusplus >= 201103L

class PThreadLocalPtrCannotCreate {
public:
    PThreadLocalPtrCannotCreate(std::string &&rref) :
            d(std::move(rref)) {
    }
    PThreadLocalPtrCannotCreate(const std::string &rhs) = delete;
private:
    std::string d;
};

// PThreadLocalPtr used as static member
class StaticHolder {
public:
    void print_address(int id) {
        printf("Thread id: %d, address of class static: %p\n", id, sp.get());
    }

private:
    static PThreadLocalPtr<int> sp;
};
PThreadLocalPtr<int> StaticHolder::sp;

// Another layer of function calls
void thread_func_internal(int id) {
    std::string a="A";
    static PThreadLocalPtr<std::pair<std::string, std::string>> pi(std::move(a), std::string("B"));
    printf("Thread id: %d, address of pi: %p\n", id, pi.get());
    printf("pi = (%s, %s), a=%s\n", pi->first.c_str(), pi->second.c_str(), a.c_str());
           // only a, b in the first thread will be moved since static only get
           // initialized once

    StaticHolder sh;
    sh.print_address(id);

    // static PThreadLocalPtr<PThreadLocalPtrCannotCreate> wontcompile1;
    // static PThreadLocalPtr<PThreadLocalPtrCannotCreate> wontcompile2(std::string("foo"));
}

void thread_func(int id) {
    std::string ini("INI");
    static PThreadLocalPtr<std::string> p(ini);
    printf("Thread id: %d, address of p: %p\n", id, p.get());
    *p = "Thread id: " + std::to_string(id);
    printf("%s\n", p->c_str());
    std::this_thread::sleep_for(std::chrono::milliseconds(50));
    thread_func_internal(id);
    std::this_thread::sleep_for(std::chrono::milliseconds(50));
    thread_func_internal(id); // Multiple entry to another function
    std::this_thread::sleep_for(std::chrono::milliseconds(50));
}

void test_pthread_specific() {
    std::vector<std::thread> threads;
    for (int i = 0; i < 5; ++i) {
        threads.emplace_back(thread_func, i);
    }
    for (auto &th : threads) {
        th.join();
    }
}

#else

void* thread_func_pthread(void *id) {
    static PThreadLocalPtr<long> p;
    *p = (long)id;
    printf("%ld\n", *p);
    printf("%p\n", p.get());
    for (volatile int i = 100000000; i != 0; --i);
    return NULL;
}

void test_pthread_specific() {
    static const int NUM_THREADS = 10;
    pthread_t t[NUM_THREADS];
    void *ret;
    for (long i=0; i<NUM_THREADS; ++i) {
        pthread_create(&t[i], NULL, thread_func_pthread, (void*)i);
    }
    for (int i=0; i<10; ++i) {
        pthread_join(t[i], &ret);
    }
}

#endif

}
 // namespace test

}// namespace conc11
