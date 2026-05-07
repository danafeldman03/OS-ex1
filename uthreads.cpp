#include "uthreads.h"

#include <iostream>
#include <list>
#include <vector>
#include <setjmp.h>
#include <csignal>

//  מוסכמות 
//  if there is no thread of id tid so threads[tid] = nullptr 
// a thread is in ready_queue if & only if its state is READY

enum State {
    RUNNING,
    READY,
    BLOCKED,
};

struct Thread {
    int tid;
    State state;
    int quantums;
    thread_entry_point entry_point;
    char* stack = nullptr;
    sigjmp_buf env;

    std::list<int>::iterator ready_it;

    bool is_manually_blocked = false;

    int sleep_remaining = 0;


    Thread(int id, State st, thread_entry_point entry)
        : tid(id), state(st), quantums(0),
          entry_point(entry){}
};

class IDManager {
private:
    bool used[MAX_THREAD_NUM];
    int thread_cnt;

public:
    IDManager() {
        for (int i = 0; i < MAX_THREAD_NUM; i++) {
            used[i] = false;
        }
        thread_cnt = 0;
    }

    int allocate() {
        for (int i = 1; i < MAX_THREAD_NUM; i++) {
            if (thread_cnt == MAX_THREAD_NUM){
                return -1;
            }
            if (!used[i]) {
                used[i] = true;
                thread_cnt++;
                return i;
            }
        }
        return -1;
    }

    void release(int tid) {
        if (tid >= 0 && tid < MAX_THREAD_NUM) {
            used[tid] = false;
            thread_cnt--;
        }
    }
};

typedef unsigned long address_t;
#define JB_SP 6
#define JB_PC 7 

/* A translation is required when using an address of a variable.
   Use this as a black box in your code. */
address_t translate_address(address_t addr)
{
    address_t ret;
    asm volatile("xor    %%fs:0x30,%0\n"
        "rol    $0x11,%0\n"
                 : "=g" (ret)
                 : "0" (addr));
    return ret;
}


static int current_tid;
static int total_quantums;
static std::vector<Thread*> threads(MAX_THREAD_NUM, nullptr);
static std::list<int> ready_queue;
static IDManager id_manager;
static int thread_to_delete = -1;

/**
 * @brief initializes the thread library.
 *
 * Once this function returns, the main thread (tid == 0) will be set as RUNNING. There is no need to 
 * provide an entry_point or to create a stack for the main thread - it will be using the "regular" stack and PC.
 * You may assume that this function is called before any other thread library function, and that it is called
 * exactly once.
 * The input to the function is the length of a quantum in micro-seconds.
 * It is an error to call this function with non-positive quantum_usecs.
 *
 * @return On success, return 0. On failure, return -1.
*/
int uthread_init(int quantum_usecs) {
    if (quantum_usecs <= 0) {
        std::cerr << "thread library error: invalid quantum" << std::endl;
        return -1;
    }
    current_tid = 0;
    total_quantums = 1;
    Thread* main_thread = new Thread(current_tid, RUNNING, nullptr);
    main_thread->quantums = 1;
    threads[current_tid] = main_thread;
    return 0;
}


void thread_start() {
    Thread* t = threads[current_tid];
    t->entry_point();
    uthread_terminate(current_tid);
    // should never reach here
    while (true);
}


void setup_thread_context(Thread* thread){
    address_t sp = (address_t) thread->stack + STACK_SIZE - sizeof(address_t);
    address_t pc = (address_t) thread_start;
    sigsetjmp(thread->env, 1);
   (thread->env->__jmpbuf)[JB_SP] = translate_address(sp);
    (thread->env->__jmpbuf)[JB_PC] = translate_address(pc);
    sigemptyset(&thread->env->__saved_mask);
}

/**
 * @brief Creates a new thread, whose entry point is the function entry_point with the signature
 * void entry_point(void).
 *
 * The thread is added to the end of the READY threads list.
 * The uthread_spawn function should fail if it would cause the number of concurrent threads to exceed the
 * limit (MAX_THREAD_NUM).
 * Each thread should be allocated with a stack of size STACK_SIZE bytes.
 * It is an error to call this function with a null entry_point.
 *
 * @return On success, return the ID of the created thread. On failure, return -1.
*/
int uthread_spawn(thread_entry_point entry_point) {
    if (entry_point == nullptr){
        std::cerr << "thread library error: " << "entry point must be provided" << std::endl;
        return -1;   
    }
    int tid = id_manager.allocate();
    if (tid == -1) {
        std::cerr << "thread library error: max thread limit reached" << std::endl;
        return -1;
    }
    Thread* thread = new Thread(tid, READY, entry_point);
//     // allocate stack for thread
    char* stack = new (std::nothrow) char[STACK_SIZE];
    if (stack == nullptr) {
        std::cerr << "system error: memory allocation failed" << std::endl;
        id_manager.release(tid);  
        delete thread;
        exit(1);
    }
    thread->stack = stack;
    threads[tid] = thread;
    ready_queue.push_back(tid);
    thread->ready_it = std::prev(ready_queue.end());
    setup_thread_context(thread);
    return tid;
}

/**
 * @brief Deleates the thread with ID tid and deletes it from all relevant control structures.
 *
 *
 * @return The function returns 0 if the thread was successfully deleted and -1 otherwise. 
*/
int delete_thread(int tid){
    if (tid > MAX_THREAD_NUM){
        std::cerr << "thread " << tid << " dose not exist" << std::endl;
        return -1;
    }
    Thread* thread = threads[tid];
    if (thread == nullptr){
        std::cerr << "thread " << tid << " dose not exist" << std::endl;
        return -1;
    }
    if (!thread || thread->state == RUNNING) return -1;
    if (thread->state == READY){
        ready_queue.erase(thread->ready_it);
    }
    if (thread->stack != nullptr) {
        delete[] thread->stack;
        thread->stack = nullptr;
    }
    id_manager.release(tid);
    threads[tid] = nullptr;
    delete thread;
    return 0;
}

int context_switch(){
    for (Thread* t : threads) {
        if (t && t->sleep_remaining > 0) {
            t->sleep_remaining--;

            if (t->sleep_remaining == 0) {
                // becomes ready if not manually blocked
                if (!t->is_manually_blocked) {
                    t->state = READY;
                    ready_queue.push_back(t->tid);
                    t->ready_it = std::prev(ready_queue.end());
                }
            }
        }
    }
    int prev_tid = current_tid;
    Thread* prev = threads[prev_tid];
    if (sigsetjmp(prev->env, 1) == 0) {
        if (prev->state == RUNNING) {
            prev->state = READY;
            ready_queue.push_back(prev_tid);
            prev->ready_it = std::prev(ready_queue.end());
        }
        if (ready_queue.empty()) {
            return -1; // nothing to run
        }
        int next_tid = ready_queue.front();
        ready_queue.pop_front();
        Thread* next = threads[next_tid];
        next->state = RUNNING;
        current_tid = next_tid;
        next->quantums++;
        total_quantums++;
        siglongjmp(next->env, 1);
    }
    if (thread_to_delete != -1) {
        int tid = thread_to_delete;
        thread_to_delete = -1;
        delete_thread(tid);
    }
    return 0;
}

void terminate_process(){
    for (Thread* t : threads){
        if (t->stack != nullptr){
                delete[] t->stack;
            }
        delete t;
    }
    threads.clear();
    ready_queue.clear();
    exit(0);
}

/**
 * @brief Terminates the thread with ID tid and deletes it from all relevant control structures.
 *
 * All the resources allocated by the library for this thread should be released. If no thread with ID tid exists it
 * is considered an error. Terminating the main thread (tid == 0) will result in the termination of the entire
 * process using exit(0) (after releasing the assigned library memory).
 *
 * @return The function returns 0 if the thread was successfully terminated and -1 otherwise. If a thread terminates
 * itself or the main thread is terminated, the function does not return.
*/
int uthread_terminate(int tid){
    if (threads[tid] == nullptr){
        std::cerr << "thread " << tid << " dose not exist." << std::endl;
        return -1;
    }
// special case: main thread
    if (tid == 0) {
        terminate_process();
    }
    if(tid == current_tid){
        Thread* t = threads[tid];
        t->state = BLOCKED;
        thread_to_delete = current_tid;
        context_switch();
    }
    else{
        delete_thread(tid);
    }
    return 0;
}


/**
 * @brief Blocks the thread with ID tid. The thread may be resumed later using uthread_resume.
 *
 * If no thread with ID tid exists it is considered as an error. In addition, it is an error to try blocking the
 * main thread (tid == 0). If a thread blocks itself, a scheduling decision should be made. Blocking a thread in
 * BLOCKED state has no effect and is *not* considered an error.
 *
 * @return On success, return 0. On failure, return -1.
*/
int uthread_block(int tid) {
    if (tid == 0){
        std::cerr << "cant block main thread." << std::endl;
        return -1;
    }
    if (tid > MAX_THREAD_NUM){
        std::cerr << "thread " << tid << " dose not exist" << std::endl;
        return -1;
    }
    Thread* thread = threads[tid];
    if (thread == nullptr){
        std::cerr << "thread " << tid << " dose not exist" << std::endl;
        return -1;
    }
    // what if a thread blocks itself  ==== blocks the ruuning ? 
    if (thread->state == READY){
        ready_queue.erase(thread->ready_it);
    }
    thread->is_manually_blocked = true;
    thread->state = BLOCKED;
    if (tid == current_tid){
        context_switch();
    }
    return 0;
}


/**
 * @brief Resumes a blocked thread with ID tid and moves it to the READY state.
 *
 * Resuming a thread in a RUNNING or READY state has no effect and is not considered as an error. If no thread with
 * ID tid exists it is considered an error.
 * When a thread transition to the READY state it is placed at the end of the READY queue.
 *
 * @return On success, return 0. On failure, return -1.
*/
int uthread_resume(int tid) {
    if (tid > MAX_THREAD_NUM){
        std::cerr << "thread " << tid << " dose not exist" << std::endl;
        return -1;
    }
    Thread* thread = threads[tid];
    if (thread == nullptr){
        std::cerr << "thread " << tid << " dose not exist" << std::endl;
        return -1;
    }
    thread->is_manually_blocked = false;
    if (thread->sleep_remaining == 0 && thread->state==BLOCKED){
        ready_queue.push_back(tid);
        thread->ready_it = std::prev(ready_queue.end());
        thread->state = READY;
    }
    return 0;
}


/**
 * @brief Blocks the RUNNING thread for num_quantums quantums.
 *
 * Immediately after the RUNNING thread transitions to the BLOCKED state a scheduling decision should be made.
 * After the sleeping time is over, the thread should go back to the end of the READY queue.
 * If the thread which was just RUNNING should also be added to the READY queue, or if multiple threads wake up 
 * at the same time, the order in which they're added to the end of the READY queue doesn't matter.
 * The number of quantums refers to the number of times a new quantum starts, regardless of the reason. Specifically,
 * the quantum of the thread which has made the call to uthread_sleep isn’t counted.
 * A call with num_quantums == 0 will immediately stop the thread and move it to the back of the execution queue.
 * 
 * It is considered an error if the main thread (tid == 0) calls this function with num_quantums != 0.
 *
 * @return On success, return 0. On failure, return -1.
*/
int uthread_sleep(int num_quantums) {
    // just for now
    /*
    if (num_quantums != 0){
        std::cerr << "thread library error: " << "num_quantums should be 0" << std::endl;
        return -1;
    }
    */
    if (current_tid==0 && num_quantums!=0){
        std::cerr << "thread library error: " << "cannot put main thread to sleep" << std::endl;
        return -1;
    }
    Thread* t = threads[current_tid];
    if(num_quantums!=0){
        t->sleep_remaining = num_quantums;
        t->state = BLOCKED;
    }
    context_switch();
    return 0;
}


/**
 * @brief Returns the thread ID of the calling thread.
 *
 * @return The ID of the calling thread.
*/
int uthread_get_tid() {
    return current_tid;
}


/**
 * @brief Returns the total number of quantums since the library was initialized, including the current quantum.
 *
 * Right after the call to uthread_init, the value should be 1.
 * Each time a new quantum starts, regardless of the reason, this number should be increased by 1.
 *
 * @return The total number of quantums.
*/
int uthread_get_total_quantums() {
    return total_quantums;
}


/**
 * @brief Returns the number of quantums the thread with ID tid was in RUNNING state.
 *
 * On the first time a thread runs, the function should return 1. Every additional quantum that the thread starts should
 * increase this value by 1 (so if the thread with ID tid is in RUNNING state when this function is called, include
 * also the current quantum). If no thread with ID tid exists it is considered an error.
 *
 * @return On success, return the number of quantums of the thread with ID tid. On failure, return -1.
*/
int uthread_get_quantums(int tid) {
    Thread*  thread = threads[tid];
    if (thread == nullptr) {
            std::cerr << "thread does not exist\n";
            return -1;
        }
    return thread->quantums;
}