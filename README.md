# concurrency-cpp11
**A concurrency toolbox for c++11, including a cached thread pool executor, a shared timed mutex, a fair semaphore and several other utilities.** 

This little library focuses on making C++11 multithreaded application development a little more comfortable, with several concurrency utilities that is designed to be easy to use. 

As an all header library, you can just drop the headers into your codebase and start using it 
using its simple API. No external dependencies.

This project is released under BSD 2-Clause License.

### What you will find here

+ A thread pool executor with cached threads
+ An alternative, non-starving implementation of shared timed mutex for C++11 codebases
+ A fair, queued semaphore and a simple semaphore for higher throughput
+ A count-down latch
+ Several implementations of user-space spin locks
+ C++ wrappers for pthread spin lock and pthread shared mutex
+ A C++ wrapper for pthread_specific *for pre-c++11 code* to make pthread-specific-based thread
local storage much easier to use (It does not require C++11)
+ A concurrent LRU Cache for caching objects in memory
+ Other miscellaneous utilites, including a complete implementation of C++14 std::make_unique, 
C++17 std::invoke, a rvalue wrapper for invoking function overloads taking rvalue reference
parameters through std::bind, and a lambda-expression-based scope guard

### Prerequisites

C++11, and only C++11.

### Examples

#### Thread pool 

```c++
auto exec = conc11::make_cached_thread_pool();
std::future<RetType> f = exec->submit(func, param1, param2);
f.get();
exec->shutdown();
exec->await_termination();
```

#### Semaphore

```c++
using SemType = conc11::QueuedSemaphore<std::mutex>;
SemType* sem;
int ctr;

void thread_func() {
    SemaphoreGuard<SemType> sg(*sem, 1);
    ctr += 1;
}

int main() {
    sem = new SemType(1);
    std::vector<std::thread> threads;
    for (int i = 0; i < 10; ++i) {
        threads.emplace(thread_func);
    }
    for (auto& th : threads) {
        th.join();
    }
    assert(ctr==10);
}
```

#### Shared mutex

```c++
std::vector<int> shared_data;
using SharedMutexType = conc11::SharedTimedMutex;

void reader_func(SharedMutexType* smtx) {
    conc11::SharedLock<SharedMutexType> lck(*smtx);
    // do readonly work with shared_data
}

void writer_func(SharedMutexType* smtx) {
    std::unique_lock<SharedMutexType> lck(*smtx);
    // write to shared data
}

int main() {
    SharedMutexType smtx;
    std::thread reader_thread(reader_func, &smtx);
    std::thread writer_thread(writer_func, &smtx);
    reader_thread.join();
    writer_thread.join();
}
```

#### Pthread local ptr

```c++
static conc11::PThreadLocalPtr<std::vector<int>> tlstr; // tlstr dereference to a thread-local 
                                                // instance of std::vector<int> created on first
                                                // dereference operation on each thread 
```

#### LRU cache

```c++
conc11::LRUCache<int, std::unique_ptr<std::string>> cache(10);
// Use conc11::BlockingLRUCache if multithread accessing is needed

auto str1 = conc11::make_unique<std::string>("STR1"); // OK to use std::make_unique if c++14 is
auto str2 = conc11::make_unique<std::string>("STR2"); // available
cache.set(1, std::move(str1));
cache.set(2, std::move(str2));

std::string o1;
bool ret = cache.get_copy_pointee(1, &o1);
assert(ret);

std::unique_ptr<std::string>> o2;
ret = cache.get_move(2, &o2);
assert(ret);
```

