#include "uthreads.h"
#include "uthreads.cpp"
#include <iostream>

/*
 * =========================================
 * STAGE 5 STRICT TESTS
 * =========================================
 * No infinite loops
 * No timing assumptions
 * Only direct API validation
 * =========================================
 */

/*
 * -----------------------------------------
 * TEST 1 HELPERS
 * -----------------------------------------
 */

static int order_counter = 0;

void yield_a() {
    std::cout << "[A] entered\n";

    order_counter++;

    uthread_sleep(0);

    order_counter++;

    std::cout << "[A] resumed\n";

    uthread_terminate(uthread_get_tid());
}

void yield_b() {
    std::cout << "[B] entered\n";

    order_counter++;

    uthread_sleep(0);

    order_counter++;

    std::cout << "[B] resumed\n";

    uthread_terminate(uthread_get_tid());
}

/*
 * -----------------------------------------
 * TEST 1: sleep(0) acts like yield
 * -----------------------------------------
 */

void test_sleep_zero() {
    std::cout << "\n=== TEST 1: sleep(0) yield ===\n";

    uthread_init(100000);

    uthread_spawn(yield_a);
    uthread_spawn(yield_b);

    uthread_sleep(0);
    uthread_sleep(0);
    uthread_sleep(0);
    uthread_sleep(0);

    if (order_counter != 4) {
        std::cout << "ERROR: sleep(0) broken\n";
        exit(1);
    }

    std::cout << "[main] TEST 1 PASSED\n";
}

/*
 * -----------------------------------------
 * TEST 2 HELPERS
 * -----------------------------------------
 */

static int wake_flag = 0;

void sleeper_basic() {
    std::cout << "[sleeper] before sleep\n";

    uthread_sleep(2);

    wake_flag = 1;

    std::cout << "[sleeper] woke\n";

    uthread_terminate(uthread_get_tid());
}

void runner_basic() {
    uthread_sleep(0);
    uthread_sleep(0);
    uthread_sleep(0);

    uthread_terminate(uthread_get_tid());
}

/*
 * -----------------------------------------
 * TEST 2: normal sleep wakeup
 * -----------------------------------------
 */

void test_basic_sleep() {
    std::cout << "\n=== TEST 2: basic sleep ===\n";

    uthread_init(100000);

    uthread_spawn(sleeper_basic);
    uthread_spawn(runner_basic);

    uthread_sleep(0);
    uthread_sleep(0);
    uthread_sleep(0);
    uthread_sleep(0);
    uthread_sleep(0);

    if (wake_flag != 1) {
        std::cout << "ERROR: sleeper never woke\n";
        exit(1);
    }

    std::cout << "[main] TEST 2 PASSED\n";
}

/*
 * -----------------------------------------
 * TEST 3 HELPERS
 * -----------------------------------------
 */

static int multi_wake_counter = 0;

void sleeper_one() {
    uthread_sleep(3);

    multi_wake_counter++;

    std::cout << "[s1] woke\n";

    uthread_terminate(uthread_get_tid());
}

void sleeper_two() {
    uthread_sleep(3);

    multi_wake_counter++;

    std::cout << "[s2] woke\n";

    uthread_terminate(uthread_get_tid());
}

/*
 * -----------------------------------------
 * TEST 3: multiple sleepers wake
 * -----------------------------------------
 */

void test_multiple_wakeup() {
    std::cout << "\n=== TEST 3: multiple wakeup ===\n";

    uthread_init(100000);

    uthread_spawn(sleeper_one);
    uthread_spawn(sleeper_two);

    for (int i = 0; i < 10; i++) {
        uthread_sleep(0);
    }

    if (multi_wake_counter != 2) {
        std::cout << "ERROR: not all sleepers woke\n";
        exit(1);
    }

    std::cout << "[main] TEST 3 PASSED\n";
}

/*
 * -----------------------------------------
 * TEST 4 HELPERS
 * -----------------------------------------
 */

static int blocked_tid = -1;
static int blocked_woke = 0;

void blocked_sleeper() {
    uthread_sleep(3);

    blocked_woke = 1;

    std::cout << "[blocked sleeper] woke\n";

    uthread_terminate(uthread_get_tid());
}

void blocker() {
    uthread_sleep(1);

    uthread_block(blocked_tid);

    /*
     * sleeper sleep should finish now
     * but thread must still remain blocked
     */

    uthread_sleep(5);

    if (blocked_woke != 0) {
        std::cout << "ERROR: blocked sleeper woke too early\n";
        exit(1);
    }

    uthread_resume(blocked_tid);

    uthread_sleep(0);

    uthread_terminate(uthread_get_tid());
}

/*
 * -----------------------------------------
 * TEST 4: blocked + sleeping
 * -----------------------------------------
 */

void test_blocked_sleep() {
    std::cout << "\n=== TEST 4: blocked + sleeping ===\n";

    uthread_init(100000);

    blocked_tid = uthread_spawn(blocked_sleeper);

    uthread_spawn(blocker);

    for (int i = 0; i < 20; i++) {
        uthread_sleep(0);
    }

    if (blocked_woke != 1) {
        std::cout << "ERROR: blocked sleeper never resumed\n";
        exit(1);
    }

    std::cout << "[main] TEST 4 PASSED\n";
}

/*
 * -----------------------------------------
 * TEST 5 HELPERS
 * -----------------------------------------
 */

static int doomed_tid = -1;
static int doomed_ran = 0;

void doomed_sleeper() {
    uthread_sleep(100);

    doomed_ran = 1;

    std::cout << "ERROR: terminated sleeper ran\n";

    uthread_terminate(uthread_get_tid());
}

void killer() {
    uthread_sleep(2);

    uthread_terminate(doomed_tid);

    uthread_terminate(uthread_get_tid());
}

/*
 * -----------------------------------------
 * TEST 5: terminate sleeping thread
 * -----------------------------------------
 */

void test_terminate_sleeping() {
    std::cout << "\n=== TEST 5: terminate sleeping ===\n";

    uthread_init(100000);

    doomed_tid = uthread_spawn(doomed_sleeper);

    uthread_spawn(killer);

    for (int i = 0; i < 15; i++) {
        uthread_sleep(0);
    }

    if (doomed_ran != 0) {
        std::cout << "ERROR: terminated sleeper executed\n";
        exit(1);
    }

    std::cout << "[main] TEST 5 PASSED\n";
}

/*
 * -----------------------------------------
 * MAIN
 * -----------------------------------------
 */

int main() {
    std::cout << "==============================\n";
    std::cout << " STAGE 5 STRICT TESTS\n";
    std::cout << "==============================\n";

    test_sleep_zero();

    /*
     * Run one test at a time unless your
     * library supports repeated init().
     */

     test_basic_sleep();
     test_multiple_wakeup();
     test_blocked_sleep();
     test_terminate_sleeping();

    std::cout << "\nALL TESTS PASSED 🎉\n";

    return 0;
}