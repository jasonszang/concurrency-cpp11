# concurrency-cpp11
**A concurrency toolbox for c++11, including a cached thread pool executor, a shared timed mutex, a fair semaphore and several other utilities.** 

This little library focuses on making C++11 multithreaded application development a little more comfortable, with several concurrency utilities that is designed to be easy to use. 

As an all header library, you can just drop the headers into your codebase and start using it 
using its simple API.

This project is released under BSD 2-Clause License.

## What you will find here

+ A thread pool executor with cached threads
+ An alternative, non-starving implementation of shared timed mutex for C++11 codebases
+ A fair, queued semaphore and a simple semaphore for higher throughput
+ A count-down latch
+ Several implementations of user-space spin locks
+ C++ wrappers for pthread spin lock and pthread shared mutex
+ A C++ wrapper for pthread_specific *for pre-c++11 code* to make pthread-specific-based thread
local storage much easier to use
+ A concurrent LRU Cache for caching objects in memory
+ Other miscellaneous utilites, including a complete implementation of C++14 std::make_unique, 
C++17 std::invoke, a rvalue wrapper for invoking function overloads taking rvalue reference
parameters through std::bind, and a lambda-expression-based scope guard
