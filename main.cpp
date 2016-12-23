/*
 * main.cpp
 *
 */

#include "test/test_semaphore.h"
#include "concurrency/semaphore.h"

int main() {
    ttb::Semaphore sem(13);
    bool succ = sem.try_acquire(10, 1000000);
    printf("%d\n", succ);
}
