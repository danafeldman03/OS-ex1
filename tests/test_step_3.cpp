#include "uthreads.h"
#include "uthreads.cpp"
#include <iostream>
#include <cassert>

/*
 * Test: Thread A terminates Thread B
 *
 * This test verifies:
 * 1. A thread can terminate another thread successfully
 * 2. The terminated thread does not continue execution
 * 3. terminate() returns correctly
 * 4. Self-termination behaves correctly
 */

int victim_tid = -1;
int killer_tid = -1;

void victim() {
    std::cout << "[Victim] started, tid = " << uthread_get_tid() << "\n";

    // Yield to allow killer thread to run
    uthread_sleep(0);

    // If we reach here, termination failed
    std::cout << "[Victim] ERROR: still running after being terminated!\n";
    assert(false);
}

void killer() {
    std::cout << "[Killer] started, tid = " << uthread_get_tid() << "\n";

    std::cout << "[Killer] terminating victim (tid = " << victim_tid << ")\n";

    int res = uthread_terminate(victim_tid);
    std::cout << "[Killer] terminate(victim) returned: " << res << "\n";

    // Give victim a chance to run again if termination failed
    uthread_sleep(0);

    std::cout << "[Killer] self terminating\n";
    uthread_terminate(uthread_get_tid());

    // Should never reach here
    assert(false);
}

int main() {
    std::cout << "[Main] initializing thread library\n";

    int res = uthread_init(100000);
    assert(res == 0);

    std::cout << "[Main] spawning threads\n";

    victim_tid = uthread_spawn(victim);
    assert(victim_tid != -1);

    killer_tid = uthread_spawn(killer);
    assert(killer_tid != -1);

    std::cout << "[Main] victim tid = " << victim_tid
              << ", killer tid = " << killer_tid << "\n";

    // Start scheduling
    uthread_sleep(0);

    std::cout << "[Main] DONE (if tests pass, victim must not continue running)\n";

    return 0;
}