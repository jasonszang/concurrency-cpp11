/*
 * test_semaphore.cpp
 *
 * Create 100 threads, each try to acquire a fixed number of semaphores, set these out and see
 * what goes on.
 */

#include <cstdio>
#include <thread>
#include <chrono>
#include <vector>
#include <memory>

#include "../concurrency/semaphore.h"

namespace ttb {

namespace test {

using std::vector;
using std::thread;
using std::unique_ptr;

static const int TICKS = 5;
static const int THREAD_NUMBER = 100;

class ThreadFunctor {
public:
    ThreadFunctor(int id, Semaphore* sem) :
            id(id), ctr(TICKS), sem(sem) {
    }
    void operator()() {
        while (ctr>0) {
            sem->acquire(id);
            printf("thread id:%d, tick counter %d, remaining sem %d\n",
                   id, ctr--, sem->available_permits());
            if (sem->available_permits()<0) {
                printf("ERROR! MINUS SEMAPHORES!!\n");
            }
            std::this_thread::sleep_for(std::chrono::milliseconds(50));
            sem->release(id);
        }
    }
private:
    int id;
    int ctr;
    Semaphore* sem;
};

void test_semaphore() {
    Semaphore* sem = new Semaphore(THREAD_NUMBER);
    vector<unique_ptr<thread> > threads;
    for(int i=0; i<THREAD_NUMBER; ++i) {
        ThreadFunctor func(i+1, sem);
        thread* curThread = new thread(func);
        threads.push_back(unique_ptr<thread>(curThread));
    }
    for (int i=0; i<THREAD_NUMBER; ++i) {
        threads[i]->join();
    }
}

} // namespace test

} // namespace ttb
