/*
██╗  ██╗███████╗ █████╗ ██████╗ ██████╗ ██╗   ██╗██╗     ███████╗███████╗
██║  ██║██╔════╝██╔══██╗██╔══██╗██╔══██╗██║   ██║██║     ██╔════╝██╔════╝
███████║█████╗  ███████║██████╔╝██████╔╝██║   ██║██║     ███████╗█████╗  
██╔══██║██╔══╝  ██╔══██║██╔═══╝ ██╔═══╝ ██║   ██║██║     ╚════██║██╔══╝  
██║  ██║███████╗██║  ██║██║     ██║     ╚██████╔╝███████╗███████║███████╗
╚═╝  ╚═╝╚══════╝╚═╝  ╚═╝╚═╝     ╚═╝      ╚═════╝ ╚══════╝╚══════╝╚══════╝

███╗   ███╗███████╗███╗   ███╗ ██████╗ ██████╗ ██╗   ██╗                 
████╗ ████║██╔════╝████╗ ████║██╔═══██╗██╔══██╗╚██╗ ██╔╝                 
██╔████╔██║█████╗  ██╔████╔██║██║   ██║██████╔╝ ╚████╔╝                  
██║╚██╔╝██║██╔══╝  ██║╚██╔╝██║██║   ██║██╔══██╗  ╚██╔╝                   
██║ ╚═╝ ██║███████╗██║ ╚═╝ ██║╚██████╔╝██║  ██║   ██║                    
╚═╝     ╚═╝╚══════╝╚═╝     ╚═╝ ╚═════╝ ╚═╝  ╚═╝   ╚═╝                    

██████╗ ██████╗  ██████╗ ███████╗██╗██╗     ███████╗██████╗              
██╔══██╗██╔══██╗██╔═══██╗██╔════╝██║██║     ██╔════╝██╔══██╗             
██████╔╝██████╔╝██║   ██║█████╗  ██║██║     █████╗  ██████╔╝             
██╔═══╝ ██╔══██╗██║   ██║██╔══╝  ██║██║     ██╔══╝  ██╔══██╗             
██║     ██║  ██║╚██████╔╝██║     ██║███████╗███████╗██║  ██║             
╚═╝     ╚═╝  ╚═╝ ╚═════╝ ╚═╝     ╚═╝╚══════╝╚══════╝╚═╝  ╚═╝             
*/

#include <bkmalloc.h>

#include <config.hpp>
#include <timer.hpp>
#include <stack_io.hpp>
#include <stack_csv.hpp>
#include <interval_test.hpp>

#include "intervals/group_test.cpp"

#ifdef DUMMY_TEST
#include "intervals/dummy_test.cpp"
static DummyTest dt;
#endif

#ifdef COMPRESSION_TEST
#include "intervals/compression_test.cpp"
static CompressionTest ct;
#endif

#ifdef OBJECT_LIVENESS_TEST
#include "intervals/object_liveness_test.cpp"
static ObjectLivenessTest olt;
#endif

#ifdef PAGE_LIVENESS_TEST
#include "intervals/page_liveness_test.cpp"
static PageLivenessTest plt;
#endif

#ifdef PAGE_TRACKING_TEST
#include "intervals/page_tracking.cpp"
static PageTrackingTest ptt;
#endif

#ifdef GENERATIONAL_TEST
#include "intervals/generational.cpp"
static GenerationalTest gt;
#endif

#ifdef ACCESS_PATTERN_TEST
#include "intervals/access_patterns_test.cpp"
static AccessPatternTest apt;
#endif

#ifdef ACCESS_COMPRESSION_TEST
#include "intervals/access_compression_test.cpp"
static AccessCompressionTest act;
#endif

#ifdef HUGE_PAGE_ACCESS_COMPRESSION_TEST
#include "intervals/huge_page_access_compression_test.cpp"
static HugePageAccessCompressionTest huge_page_act;
#endif

static uint64_t malloc_count = 0;
static uint64_t free_count = 0;
static uint64_t mmap_count = 0;
static uint64_t munmap_count = 0;

const uint64_t STATS_INTERVAL_MS = 5000;
Stopwatch overhead_sw, update_sw, invalidate_sw;

static IntervalTestSuite *its = IntervalTestSuite::get_instance();

class Hooks {
public:
    Hooks() {
        stack_debugf("Hooks constructor\n");

        stack_debugf("Adding test...\n");
        hook_timer.start();

        #ifdef DUMMY_TEST
        its->add_test(&dt);
        #endif
        #ifdef COMPRESSION_TEST
        its->add_test(&ct);
        #endif
        #ifdef OBJECT_LIVENESS_TEST
        its->add_test(&olt);
        #endif
        #ifdef PAGE_LIVENESS_TEST
        its->add_test(&plt);
        #endif
        #ifdef PAGE_TRACKING_TEST
        its->add_test(&ptt);
        #endif
        #ifdef GENERATIONAL_TEST
        its->add_test(&gt);
        #endif
        #ifdef ACCESS_PATTERN_TEST
        its->add_test(&apt);
        #endif
        #ifdef ACCESS_COMPRESSION_TEST
        its->add_test(&act);
        #endif
        #ifdef HUGE_PAGE_ACCESS_COMPRESSION_TEST
        its->add_test(&huge_page_act);
        #endif
        
        stack_debugf("Done\n");

        setup_protection_handler();
    }

    void block_new(bk_Heap *heap, union bk_Block *block) {
        stack_debugf("Block new\n");
        its->new_huge_page((uint8_t*)block, block->meta.size);
        stack_debugf("Block new done\n");
    }

    void block_release(bk_Heap *heap, union bk_Block *block) {
        stack_debugf("Block release\n");
        its->free_huge_page((uint8_t*)block, block->meta.size);
        stack_debugf("Block release done\n");
    }

    void post_mmap(void *addr_in, size_t n_bytes, int prot, int flags, int fd, off_t offset, void *allocation_address) {
        if (its->is_done() || !its->can_update()) {
            stack_debugf("Test finished, not updating\n");
            return;
        }
        stack_debugf("Post mmap\n");
        update_sw.start();
        its->update(addr_in, n_bytes, (uintptr_t)BK_GET_RA());
        update_sw.stop();
        stack_debugf("Post mmap update\n");
        /*
        if (!hook_lock.try_lock()) return;
        try {
            its->update(addr_in, n_bytes, (uintptr_t)BK_GET_RA());
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
        */

        hook_lock.unlock();
        stack_debugf("Post mmap done\n");
    }

    void post_alloc(bk_Heap *heap, u64 n_bytes, u64 alignment, int zero_mem, void *allocation_address) {
        if (its->is_done() || !its->can_update()) {
            stack_debugf("Test finished, not updating\n");
            return;
        }
        // if (!hook_lock.try_lock()) return;
        stack_debugf("Post alloc\n");
        stack_debugf("About to update with arguments %p, %d, %d\n", allocation_address, n_bytes, (uintptr_t)BK_GET_RA());
        update_sw.start();
        its->update(allocation_address, n_bytes, (uintptr_t)BK_GET_RA());
        update_sw.stop();
        stack_debugf("Post alloc update\n");
        try {
            stack_debugf("About to update with arguments %p, %d, %d\n", allocation_address, n_bytes, (uintptr_t)BK_GET_RA());
            its->update(allocation_address, n_bytes, (uintptr_t)BK_GET_RA());
            stack_debugf("Post alloc update\n");
        } catch (std::out_of_range& e) {
            stack_errorf("Post alloc out of range exception\n");
            stack_errorf(e.what());
            stack_errorf("\n");
            exit(1);
        } catch (std::runtime_error& e) {
            stack_errorf("Post alloc runtime error exception\n");
            stack_errorf(e.what());
            stack_errorf("\n");
            exit(1);
        } catch (...) {
            stack_errorf("Post alloc unknown exception exception\n");
            exit(1);
        }
        // hook_lock.unlock();
        stack_debugf("Post alloc done\n");
    }

    void pre_free(bk_Heap *heap, void *addr) {
        if (its->is_done()) {
            stack_debugf("Test finished, not updating\n");
            return;
        }
        stack_debugf("Pre free\n");
        if (its->contains(addr)) {
            stack_debugf("About to invalidate %p\n", addr);
            hook_lock.lock();
            invalidate_sw.start();
            its->invalidate(addr);
            invalidate_sw.stop();
            hook_lock.unlock();
        } else {
            stack_debugf("Pre free does not contain %p\n", addr);
        }
        /*
        try {
            if (its->contains(addr)) {
                stack_debugf("About to invalidate %p\n", addr);
                hook_lock.lock();
                its->invalidate(addr);
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
        */
        stack_debugf("Pre free done\n");
    }

    bool can_update() const {
        return its->can_update();
    }

    bool contains(void *addr) const {
        return its->contains(addr);
    }

    bool is_done() const {
        return its->is_done();
    }

    void print_stats() {
        stack_infof("Elapsed time: % ms\n", hook_timer.elapsed_milliseconds());
        stack_infof("Total overhead: % ms\n", overhead_sw.elapsed_milliseconds());
        stack_infof("  Update overhead:     % ms\n", update_sw.elapsed_milliseconds());
        stack_infof("  Invalidate overhead: % ms\n", invalidate_sw.elapsed_milliseconds());
        stack_infof("Malloc count: %\n", malloc_count);
        stack_infof("Free count: %\n", free_count);
        stack_infof("Mmap count: %\n", mmap_count);
        stack_infof("Munmap count: %\n", munmap_count);
        stack_infof("Total allocations: %\n", malloc_count + mmap_count);
        stack_infof("Total frees: %\n", free_count + munmap_count);
        Compressor<>::summary();
    }

    void report_stats() {
        if (stats_timer.has_elapsed(STATS_INTERVAL_MS)) {
            print_stats();
            stats_timer.reset();
        }
    }

    ~Hooks() {
        its->finish();
        stack_infof("*** TESTS FINISHED! ***\n");
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
void bk_block_new_hook(struct bk_Heap *heap, union bk_Block *block) {
    // stack_debugf("Block new hook\n");
    // stack_infof("New block:\n");
    // stack_infof("   Size: %d\n", block->meta.size);
    // stack_infof("   Location: %p\n", (void*)block);
    // stack_infof("   Bump Base Location: %p\n", block->meta.bump_base);
    // size_t i = 0;
    // for (char *addr=(char*)block; addr<(char*)block->meta.end; addr+=8) {
    //     i += 8;
    // }
    // stack_infof("   Measured size: %d\n", i);
    hooks.block_new(heap, block);
}

extern "C"
void bk_block_release_hook(struct bk_Heap *heap, union bk_Block *block) {
    // stack_debugf("Block release hook\n");
    // stack_infof("Released block:\n");
    // stack_infof("   Size: %d\n", block->meta.size);
    // stack_infof("   Location: %p\n", (void*)block);
    hooks.block_release(heap, block);
}


extern "C"
void bk_post_alloc_hook(bk_Heap *heap, u64 n_bytes, u64 alignment, int zero_mem, void *addr) {
    setup_protection_handler();
    overhead_sw.start();
    if (hooks.is_done() || !hooks.can_update()) {
        overhead_sw.stop();
        return;
    }
    if (!bk_lock.try_lock()) {
        stack_debugf("Failed to lock\n");
        overhead_sw.stop();
        return;
    }
    malloc_count++;
    hooks.report_stats();
    stack_debugf("Entering hook\n");
    hooks.post_alloc(heap, n_bytes, alignment, zero_mem, addr);
    stack_debugf("Leaving hook\n");
    bk_lock.unlock();
    overhead_sw.stop();
}

extern "C"
void bk_pre_free_hook(bk_Heap *heap, void *addr) {
    setup_protection_handler();
    overhead_sw.start();
    if (hooks.is_done()) {
        overhead_sw.stop();
        return;
    }
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
    overhead_sw.stop();
}

extern "C"
void bk_post_mmap_hook(void *addr, size_t n_bytes, int prot, int flags, int fd, off_t offset, void *ret_addr) {;
    setup_protection_handler();
    overhead_sw.start();
    if (hooks.is_done() || !hooks.can_update()) {
        overhead_sw.stop();
        return;
    }
    if (!bk_lock.try_lock()) {
        stack_debugf("Failed to lock\n");
        overhead_sw.stop();
        return;
    }
    mmap_count++;
    hooks.report_stats();
    stack_debugf("Entering hook\n");
    hooks.post_mmap(ret_addr, n_bytes, prot, flags, fd, offset, ret_addr);
    stack_debugf("Leaving hook\n");
    bk_lock.unlock();
    overhead_sw.stop();
}

extern "C"
void bk_post_munmap_hook(void *addr, size_t n_bytes) {
    setup_protection_handler();
    overhead_sw.start();
    if (hooks.is_done()) {
        overhead_sw.stop();
        return;
    }
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
    overhead_sw.stop();
}
