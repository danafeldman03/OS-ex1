#include <iostream>
#include <cassert>
#include "uthreads.h"
#include "uthreads.cpp"


using namespace std;

/*
 * =========================================
 * STAGE 2 TEST SUITE (CLEAN VERSION)
 * =========================================
 * Covers:
 * - uthread_init
 * - uthread_get_tid
 * - uthread_spawn (ID allocation)
 * - uthread_terminate (normal + edge cases)
 * - invalid termination
 * - stress tests
 * - main thread termination (tid = 0)
 *
 * IMPORTANT:
 * No context switching is tested in Stage 2.
 * Threads remain in READY state only.
 * =========================================
 */

void print_test(const string& name) {
    cout << "\n=============================\n";
    cout << "TEST: " << name << "\n";
    cout << "=============================\n";
}

// ------------------------------------
// Test 1: main thread
// ------------------------------------
void test_main_thread() {
    print_test("Main thread basic");

    uthread_init(10);

    int tid = uthread_get_tid();
    cout << "Current TID: " << tid << endl;

    assert(tid == 0);
    cout << "✔ main thread is 0\n";
}

// ------------------------------------
// Test 2: spawn basic
// ------------------------------------
void test_spawn_basic() {
    print_test("Spawn basic");

    uthread_init(10);

    int t1 = uthread_spawn([](){
        cout << "[t1] created\n";
    });

    int t2 = uthread_spawn([](){
        cout << "[t2] created\n";
    });

    cout << "t1=" << t1 << ", t2=" << t2 << endl;

    assert(t1 != t2);
    assert(t1 != 0 && t2 != 0);

    cout << "✔ unique IDs\n";
}

// ------------------------------------
// Test 3: terminate existing thread
// ------------------------------------
void test_terminate_thread() {
    print_test("Terminate thread");

    uthread_init(10);

    int t1 = uthread_spawn([](){
        cout << "[t1] running\n";
    });

    int res = uthread_terminate(t1);

    cout << "terminate(t1) = " << res << endl;

    assert(res == 0);

    cout << "✔ terminate success\n";
}

// ------------------------------------
// Test 4: terminate invalid thread
// ------------------------------------
void test_invalid_terminate() {
    print_test("Invalid terminate");

    uthread_init(10);

    int res = uthread_terminate(999);

    cout << "terminate(999) = " << res << endl;

    assert(res == -1);

    cout << "✔ correct error handling\n";
}

// ------------------------------------
// Test 5: stress spawn/terminate
// ------------------------------------
void test_stress() {
    print_test("Stress spawn/terminate");

    uthread_init(100);

    for (int i = 0; i < 30; i++) {
        int t = uthread_spawn([](){
            cout << "[thread] running\n";
        });

        int res = uthread_terminate(t);
        assert(res == 0);
    }

    cout << "✔ stress test passed\n";
}

// ------------------------------------
// Test 6: main thread termination (tid = 0)
// ------------------------------------
void test_main_thread_terminate() {
    print_test("Main thread termination");

    cout << "⚠ This test will terminate the program intentionally\n";

    uthread_init(10);

    uthread_spawn([](){
        cout << "[child] running before termination\n";
    });

    // חייב לסיים את כל התוכנית
    uthread_terminate(0);

    // אם מגיעים לפה → באג חמור
    cout << "❌ ERROR: program did NOT terminate\n";

    assert(false && "main thread termination failed");
}

// ------------------------------------
int main() {
    cout << "=== CLEAN STAGE 2 TEST SUITE ===\n";

    test_main_thread();
    test_spawn_basic();
    test_terminate_thread();
    test_invalid_terminate();
    test_stress();

   

    cout << "\n✔ ALL TESTS PASSED\n";

     // run separately if needed
    test_main_thread_terminate();
    cout << "\nfailed\n";

    return 0;
}