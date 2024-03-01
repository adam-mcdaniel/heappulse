#define BKMALLOC_HOOK
#include <bkmalloc.h>
#include <stack_io.hpp>
#include <timer.hpp>

#include <stack_csv.hpp>
#include <interval_test.hpp>

#ifdef COMPRESSION_TEST
#include "compression_test.cpp"
static CompressionTest ct;
#endif

#ifdef OBJECT_LIVENESS_TEST
#include "object_liveness_test.cpp"
static ObjectLivenessTest olt;
#endif

#ifdef PAGE_LIVENESS_TEST
#include "page_liveness_test.cpp"
static PageLivenessTest plt;
#endif

#ifdef PAGE_TRACKING_TEST
#include "page_tracking.cpp"
static PageTrackingTest ptt;
#endif

static IntervalTestSuite its;

static uint64_t malloc_count = 0;
static uint64_t free_count = 0;
static uint64_t mmap_count = 0;
static uint64_t munmap_count = 0;

const uint64_t STATS_INTERVAL_MS = 5000;

class Hooks {
public:
    Hooks() {
        stack_debugf("Hooks constructor\n");

        stack_debugf("Adding test...\n");
        hook_timer.start();
        #ifdef COMPRESSION_TEST
        its.add_test(&ct);
        #endif
        #ifdef OBJECT_LIVENESS_TEST
        its.add_test(&olt);
        #endif
        #ifdef PAGE_LIVENESS_TEST
        its.add_test(&plt);
        #endif
        #ifdef PAGE_TRACKING_TEST
        its.add_test(&ptt);
        #endif
        stack_debugf("Done\n");
    }

    void post_mmap(void *addr_in, size_t n_bytes, int prot, int flags, int fd, off_t offset, void *allocation_address) {
        if (its.is_done() || !its.can_update()) {
            stack_debugf("Test finished, not updating\n");
            return;
        }
        stack_debugf("Post mmap\n");
        if (!hook_lock.try_lock()) return;
        try {
            its.update(addr_in, n_bytes, (uintptr_t)BK_GET_RA());
            stack_debugf("Post mmap update\n");
        } catch (std::out_of_range& e) {
            stack_warnf("Post mmap out of range exception\n");
            stack_warnf(e.what());
            stack_warnf("\n");
        } catch (std::runtime_error& e) {
            stack_warnf("Post mmap runtime error exception\n");
            stack_warnf(e.what());
            stack_warnf("\n");
        } catch (...) {
            stack_warnf("Post mmap unknown exception\n");
        }

        hook_lock.unlock();
        stack_debugf("Post mmap done\n");
    }

    void post_alloc(bk_Heap *heap, u64 n_bytes, u64 alignment, int zero_mem, void *allocation_address) {
        if (its.is_done() || !its.can_update()) {
            stack_debugf("Test finished, not updating\n");
            return;
        }
        if (!hook_lock.try_lock()) return;
        stack_debugf("Post alloc\n");
        try {
            stack_debugf("About to update with arguments %p, %d, %d\n", allocation_address, n_bytes, (uintptr_t)BK_GET_RA());
            its.update(allocation_address, n_bytes, (uintptr_t)BK_GET_RA());
            stack_debugf("Post alloc update\n");
        } catch (std::out_of_range& e) {
            stack_warnf("Post alloc out of range exception\n");
            stack_warnf(e.what());
            stack_warnf("\n");
        } catch (std::runtime_error& e) {
            stack_warnf("Post alloc runtime error exception\n");
            stack_warnf(e.what());
            stack_warnf("\n");
        } catch (...) {
            stack_warnf("Post alloc unknown exception exception\n");
        }
        hook_lock.unlock();
        stack_debugf("Post alloc done\n");
    }

    void pre_free(bk_Heap *heap, void *addr) {
        if (its.is_done()) {
            stack_debugf("Test finished, not updating\n");
            return;
        }
        stack_debugf("Pre free\n");
        try {
            if (its.contains(addr)) {
                stack_debugf("About to invalidate %p\n", addr);
                hook_lock.lock();
                its.invalidate(addr);
                hook_lock.unlock();
            } else {
                stack_debugf("Pre free does not contain %p\n", addr);
            }
        } catch (std::out_of_range& e) {
            stack_warnf("Pre free out of range exception\n");
            stack_warnf(e.what());
            stack_warnf("\n");
        } catch (std::runtime_error& e) {
            stack_warnf("Pre free runtime error exception\n");
            stack_warnf(e.what());
            stack_warnf("\n");
        } catch (...) {
            stack_warnf("Pre free unknown exception\n");
        }
        stack_debugf("Pre free done\n");
    }

    bool can_update() const {
        return its.can_update();
    }

    bool contains(void *addr) const {
        return its.contains(addr);
    }

    bool is_done() const {
        return its.is_done();
    }

    void print_stats() {
        stack_infof("Elapsed time: % ms\n", hook_timer.elapsed_milliseconds());
        stack_infof("Malloc count: %\n", malloc_count);
        stack_infof("Free count: %\n", free_count);
        stack_infof("Mmap count: %\n", mmap_count);
        stack_infof("Munmap count: %\n", munmap_count);
        stack_infof("Total allocations: %\n", malloc_count + mmap_count);
        stack_infof("Total frees: %\n", free_count + munmap_count);
    }

    void report_stats() {
        if (stats_timer.has_elapsed(STATS_INTERVAL_MS)) {
            print_stats();
            stats_timer.reset();
        }
    }

    ~Hooks() {
        its.finish();
        print_stats();
        stack_logf("Hooks destructor\n");
    }

private:
    std::mutex hook_lock;
    Timer stats_timer;
    Timer hook_timer;
};


static Hooks hooks;

std::mutex bk_lock;


extern "C"
void bk_post_alloc_hook(bk_Heap *heap, u64 n_bytes, u64 alignment, int zero_mem, void *addr) {
    if (hooks.is_done() || !hooks.can_update()) return;
    if (!bk_lock.try_lock()) {
        stack_debugf("Failed to lock\n");
        return;
    }
    malloc_count++;
    hooks.report_stats();
    stack_debugf("Entering hook\n");
    hooks.post_alloc(heap, n_bytes, alignment, zero_mem, addr);
    stack_debugf("Leaving hook\n");
    bk_lock.unlock();
}

extern "C"
void bk_pre_free_hook(bk_Heap *heap, void *addr) {
    if (hooks.is_done()) return;
    if (hooks.contains(addr)) {
        stack_debugf("About to block on lock\n");
        bk_lock.lock();
        free_count++;
        hooks.report_stats();
        stack_debugf("Entering hook\n");
        hooks.pre_free(heap, addr);
        stack_debugf("Leaving hook\n");
        bk_lock.unlock();
    }
}

extern "C"
void bk_post_mmap_hook(void *addr, size_t n_bytes, int prot, int flags, int fd, off_t offset, void *ret_addr) {;
    if (hooks.is_done() || !hooks.can_update()) return;
    if (!bk_lock.try_lock()) {
        stack_debugf("Failed to lock\n");
        return;
    }
    mmap_count++;
    hooks.report_stats()
    stack_debugf("Entering hook\n");
    hooks.post_mmap(ret_addr, n_bytes, prot, flags, fd, offset, ret_addr);
    stack_debugf("Leaving hook\n");
    bk_lock.unlock();
}

extern "C"
void bk_post_munmap_hook(void *addr, size_t n_bytes) {
    if (hooks.is_done()) return;
    if (hooks.contains(addr)) {
        stack_debugf("About to block on lock\n");
        bk_lock.lock();
        munmap_count++;
        hooks.report_stats();
        stack_debugf("Entering hook\n");
        hooks.pre_free(NULL, addr);
        stack_debugf("Leaving hook\n");
        bk_lock.unlock();
    }
}
