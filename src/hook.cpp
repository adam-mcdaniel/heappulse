#define BKMALLOC_HOOK
#include "bkmalloc.h"
#include <zlib.h>
#include <fstream>
#include <sstream>
#include <iostream>
// #include <stdio.h>
#include <stack_io.hpp>
#include <stdlib.h>
#include <chrono>
#include <vector>
#include <signal.h>
#include <unistd.h>
#include <csetjmp>
#include <thread>
#include <sys/mman.h>
#include <cassert>
#include <condition_variable>
#include <dlfcn.h>
#include <execinfo.h>
#include <timer.hpp>
#include "stack_map.cpp"
#include "stack_file.cpp"
#include <stack_csv.hpp>
#include <interval_test.hpp>


// std::mutex protect_mutex;
// static bool IS_PROTECTED = false;


// static u64 WORKING_THREAD_ID = 0;

// bool is_working_thread() {
//     return WORKING_THREAD_ID == (u64)pthread_self() && IS_PROTECTED;
// }

// void become_working_thread() {
//     WORKING_THREAD_ID = (u64)pthread_self();
//     IS_PROTECTED = true;
// }

// void no_longer_working_thread() {
//     WORKING_THREAD_ID = 0;
//     IS_PROTECTED = false;
// }

// // This is the handler for SIGSEGV. It's called when we try to access
// // a protected page. This will put the thread to sleep until we finish
// // compressing the page. This thread will then be woken up when we
// // unprotect the page.
// static void protection_handler(int sig, siginfo_t *si, void *unused)
// {
//     // std::cout << "Got SIGSEGV at address: 0x" << std::hex << si->si_addr << std::endl;
//     char buf[1024];
//     // sprintf(buf, "Got SIGSEGV at address: 0x%lx\n", (long) si->si_addr);
//     // write(STDOUT_FILENO, buf, strlen(buf));

//     // Is this thread the main?
//     if (is_working_thread()) {
//         // sprintf(buf, "[FAULT] Working thread, giving back access: 0x%lx\n", (long) si->si_addr);
//         // write(STDOUT_FILENO, buf, strlen(buf));

//         long page_size = sysconf(_SC_PAGESIZE);
//         void* aligned_address = (void*)((unsigned long)si->si_addr & ~(page_size - 1));
//         mprotect(aligned_address, getpagesize(), PROT_READ | PROT_WRITE | PROT_EXEC);
//     } else {
//         if (!IS_PROTECTED) {
//             sprintf(buf, "[FAULT] Dereferenced address 0x%lx when unprotected, halting program\n", (long) si->si_addr);
//             write(STDOUT_FILENO, buf, strlen(buf));
//             usleep(250000);
//         }
//         sprintf(buf, "[INFO] Caught access of temporarily protected memory: 0x%lx\n", (long) si->si_addr);
//         write(STDOUT_FILENO, buf, strlen(buf));
//         while (IS_PROTECTED) {}

//         sprintf(buf, "[INFO] Resuming after protection ended: 0x%lx\n", (long) si->si_addr);
//         write(STDOUT_FILENO, buf, strlen(buf));
//     }

//     // Put the thread to sleep until we finish compressing the page.
//     // This thread will then be woken up when we unprotect the page.

//     // check_compression_stats();
//     // while (IS_PROTECTED) {
//     //     write(STDOUT_FILENO, "Waiting...", 10);
//     //     sleep(1);
//     // }
//     // sleep(1);

//     // if (IS_PROTECTED) {
//     //     // std::longjmp(jump_buffer, 1);
//     //     // Align the address to the page boundary
//     //     long page_size = sysconf(_SC_PAGESIZE);
//     //     void* aligned_address = (void*)((unsigned long)si->si_addr & ~(page_size - 1));
//     //     mprotect(aligned_address, getpagesize(), PROT_READ | PROT_WRITE | PROT_EXEC);
//     //     sleep(1);
//     // } else {
//     //     sprintf(buf, "Not protected, exiting\n");
//     //     write(STDOUT_FILENO, buf, strlen(buf));
//     // }
// }

// // This sets the SEGV signal handler to be the protection handler.
// // The protection handler will capture the segfault caused by mprotect,
// // and keep the program blocked until the compression tests are done
// void setup_protection_handler()
// {
//     struct sigaction sa;
//     memset(&sa, 0, sizeof(struct sigaction));
//     sigemptyset(&sa.sa_mask);
//     sa.sa_sigaction = protection_handler;
//     sa.sa_flags = SA_SIGINFO | SA_ONSTACK;

//     if (sigaction(SIGSEGV, &sa, NULL) == -1) {
//         perror("sigaction");
//         exit(EXIT_FAILURE);
//     } else {
//         // std::cout << "sigaction successful" << std::endl;
//     }
// }

#include "compression_test.cpp"


static CompressionTest ct;

class Hooks {
public:
    Hooks() {
        stack_logf("Hooks constructor\n");

        stack_logf("Adding test...\n");
        its.add_test(&ct);
        stack_logf("Done\n");
    }

    // void post_mmap(void*, size_t, int, int, int, off_t, void*); /* ARGS: addr in, length in, prot in, flags in, fd in, offset in, ret_addr in */
    void post_mmap(void *addr_in, size_t n_bytes, int prot, int flags, int fd, off_t offset, void *allocation_address) {
        stack_printf("Post mmap\n");
        if (!hook_lock.try_lock()) return;
        // stack_printf("Post mmap lock\n");
        // printf("Post MMAP! %p => %p\n", addr_in, allocation_address);
        // // post_alloc(NULL, n_bytes, PAGE_SIZE, 0, allocation_address);
        // if (IS_PROTECTED) {
        //     return;
        // }
        try {
            its.update(addr_in, n_bytes, (uintptr_t)BK_GET_RA());
            stack_printf("Post mmap update\n");
        } catch (std::out_of_range& e) {
            stack_printf("Post mmap out of range exception\n");
            stack_printf(e.what());
            stack_printf("\n");
        } catch (std::runtime_error& e) {
            stack_printf("Post mmap runtime error exception\n");
            stack_printf(e.what());
            stack_printf("\n");
        } catch (...) {
            stack_printf("Post mmap unknown exception\n");
        }
        // // #ifdef RANDOMIZE_ALLOCATION_DATA
        // // void *aligned_address = (void*)((u64)allocation_address - (u64)allocation_address % alignment);
        // // randomize_data(allocation_address, n_bytes);
        // // #endif

        // // bool protection = IS_PROTECTED;
        // // IS_PROTECTED = true;
        // // std::cout << "Alloc at " << std::hex << (u64)allocation_address << std::dec << std::endl;
        // // IS_PROTECTED = protection;
        // record_alloc(allocation_address, CompressionEntry(allocation_address, n_bytes));
        // compression_test();
        hook_lock.unlock();
        stack_printf("Post mmap done\n");
    }

    void post_alloc(bk_Heap *heap, u64 n_bytes, u64 alignment, int zero_mem, void *allocation_address) {
        stack_printf("Post alloc\n");
        if (!hook_lock.try_lock()) return;
        // stack_printf("Post alloc lock\n");
        // if (IS_PROTECTED) {
        //     return;
        // }

        // if (IS_PROTECTED) {
        //     return;
        // }

        // #ifdef RANDOMIZE_ALLOCATION_DATA
        // void *aligned_address = (void*)((u64)allocation_address - (u64)allocation_address % alignment);
        // randomize_data(aligned_address, n_bytes);
        // #endif

        // stack_logf("Post alloc pre update\n");
        try {
            stack_printf("About to update with arguments %p, %d, %d\n", allocation_address, n_bytes, (uintptr_t)BK_GET_RA());
            its.update(allocation_address, n_bytes, (uintptr_t)BK_GET_RA());
            stack_printf("Post alloc update\n");
        } catch (std::out_of_range& e) {
            stack_printf("Post alloc out of range exception\n");
            stack_printf(e.what());
            stack_printf("\n");
        } catch (std::runtime_error& e) {
            stack_printf("Post alloc runtime error exception\n");
            stack_printf(e.what());
            stack_printf("\n");
        } catch (...) {
            stack_printf("Post alloc unknown exception exception\n");
        }
        // // bool protection = IS_PROTECTED;
        // // IS_PROTECTED = true;
        // // std::cout << "Alloc at " << std::hex << (u64)allocation_address << std::dec << std::endl;
        // // IS_PROTECTED = protection;

        // record_alloc(allocation_address, CompressionEntry(allocation_address, n_bytes));
        // compression_test();

        // bk_Block *block;
        // u32       idx;
        // block = BK_ADDR_PARENT_BLOCK(addr);
        // idx   = block->meta.size_class_idx;
        // if (idx == BK_BIG_ALLOC_SIZE_CLASS_IDX) { idx = BK_NR_SIZE_CLASSES; }
        // hist[idx] += 1;
        // if (alloc_entry_idx < sizeof(alloc_arr) / sizeof(alloc_arr[0]))
        //     alloc_arr[alloc_entry_idx++] = { addr, n_bytes, 0 };
        hook_lock.unlock();
        stack_printf("Post alloc done\n");
    }

    void pre_free(bk_Heap *heap, void *addr) {
        stack_printf("Pre free\n");
        hook_lock.lock();
        try {
            if (its.contains(addr)) {
                // stack_logf("Pre free contains\n");
                // if (IS_PROTECTED) {
                //     return;
                // }
                // stack_logf("Pre free lock\n");
                its.invalidate(addr);
                // stack_printf("Pre free invalidate\n");
                // compression_test();
                // stack_printf("Pre free unlock\n");
            } else {
                stack_printf("Pre free does not contain\n");
            }
        } catch (std::out_of_range& e) {
            stack_printf("Pre free out of range exception\n");
            stack_printf(e.what());
            stack_printf("\n");
        } catch (std::runtime_error& e) {
            stack_printf("Pre free runtime error exception\n");
            stack_printf(e.what());
            stack_printf("\n");
        } catch (...) {
            stack_printf("Pre free unknown exception\n");
        }
        stack_printf("Pre free done\n");
        // std::cout << "FrTeeing " << addr << std::endl;
        hook_lock.unlock();
        
        // record_free(addr);
        // compression_test();

        // record_free(addr);
        // if (IS_PROTECTED) {
        //     return;
        // }
        // compression_test();

        // bk_Block *block;
        // u32       idx;
        // block = BK_ADDR_PARENT_BLOCK(addr);
        // idx   = block->meta.size_class_idx;
        // if (idx == BK_BIG_ALLOC_SIZE_CLASS_IDX) { idx = BK_NR_SIZE_CLASSES; }
        // hist[idx] -= 1;
    }


    ~Hooks() {
        stack_logf("Hooks destructor\n");
    }

private:
    static std::mutex hook_lock;

    IntervalTestSuite its;
};


static Hooks hooks;
// static IntervalTestSuite hooks;

static std::mutex bk_lock;

extern "C"
void bk_post_alloc_hook(bk_Heap *heap, u64 n_bytes, u64 alignment, int zero_mem, void *addr) {
    // return;
    // bk_printf("Entering hook\n");
    if (!bk_lock.try_lock()) {
        // stack_printf("Failed to lock\n");
        return;
    }
    stack_printf("Entering hook\n");

    // stack_printf("Allocated %d (0x%x) bytes at %p\n", n_bytes, n_bytes, addr);
    hooks.post_alloc(heap, n_bytes, alignment, zero_mem, addr);
    // // hooks.update(addr, n_bytes, (uintptr_t)BK_GET_RA());
    stack_printf("Leaving hook\n");
    bk_lock.unlock();
}

extern "C"
void bk_pre_free_hook(bk_Heap *heap, void *addr) {
    // return;
    stack_printf("Entering hook\n");
    // if (!bk_lock.try_lock()) return;
    // stack_printf("Entering hook\n");
    // // IS_PROTECTED = false;
    bk_lock.lock();

    // stack_printf("Freeing %\n", addr);
    // hooks.invalidate(addr);
    hooks.pre_free(heap, addr);
    stack_printf("Leaving hook\n");
    bk_lock.unlock();
}

extern "C"
void bk_post_mmap_hook(void *addr, size_t n_bytes, int prot, int flags, int fd, off_t offset, void *ret_addr) {
    // return;

    // stack_printf("MMAP'd % bytes at %\n", n_bytes, addr);
    // // hooks.update(addr, n_bytes, (uintptr_t)BK_GET_RA());
    if (!bk_lock.try_lock()) {
        // stack_printf("Failed to lock\n");
        return;
    }
    stack_printf("Entering hook\n");

    hooks.post_mmap(addr, n_bytes, prot, flags, fd, offset, ret_addr);
    stack_printf("Leaving hook\n");
    bk_lock.unlock();
}
