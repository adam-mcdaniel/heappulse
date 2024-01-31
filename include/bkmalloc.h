/*
 * bkmalloc.h
 * Brandon Kammerdiener
 * June, 2022
 *
 * === What this file provides:
 *
 * A performant, tunable, general-purpose malloc implementation.
 *
 * A facility for hooking into and controlling allocation events.
 *
 * Allocation routines that create/use named "heaps" to colocate and organize
 * related data.
 *
 * === How to build the library:
 *
 * Use the provided build.sh, look at it for inspiration, or, at a minimum:
 *   C only:
 *     gcc -shared -fPIC -o libbkmalloc.so -x c bkmalloc.h -DBKMALLOC_IMPL -ldl
 *   With C++ support:
 *     g++ -shared -fPIC -fno-rtti -o libbkmalloc.so -x c++ bkmalloc.h -DBKMALLOC_IMPL -ldl
 *
 * === How to use this file:
 *
 * As standard malloc:
 *     Link program with -lbkmalloc
 *  OR
 *     LD_PRELOAD=path/to/libbkmalloc.so
 *
 * Embedded in your application:
 *
 *     #define BKMALLOC_IMPL
 *     #include <bkmalloc.h>
 *
 * To write a hook:
 *
 *   Example myhook.c:
 *     // Compile: gcc -o myhook.so myhook.c -shared -fPIC -ldl -lbkmalloc
 *
 *     #define BKMALLOC_HOOK
 *     #include <bkmalloc.h>
 *
 *     void bk_pre_alloc_hook(struct bk_Heap **heapp, u64 *n_bytesp, u64 *alignmentp, int *zero_memp) {
 *         // Do things before allocation.
 *     }
 *
 *   Example myhook.cpp:
 *     // Compile: g++ -o myhook.so myhook.c -shared -fPIC -ldl -lbkmalloc
 *
 *     #define BKMALLOC_HOOK
 *     #include <bkmalloc.h>
 *
 *     extern "C"
 *     void bk_block_new_hook(struct bk_Heap *heap, union bk_Block *block) {
 *         // Do things after a new block has been created.
 *     }
 *
 *   Search "@@hooks" in this file to see the functions that you can hook into bkmalloc.
 *   They should all be prefixed with "bk_" and suffixed with "_hook".
 *
 *   The hook utility also includes some bits on slot and chunk allocations intended to be
 *   used by hooks for any purpose.
 *
 *   For slot allocations, there is a single bit available. You can use the following macros
 *   to get/set/clear it:
 *
 *     BK_GET_SLOT_HOOK_BIT(slots_pointer, region_index, slot_index)
 *     BK_SET_SLOT_HOOK_BIT(slots_pointer, region_index, slot_index)
 *     BK_CLEAR_SLOT_HOOK_BIT(slots_pointer, region_index, slot_index)
 *
 *   Chunk allocations have a total of 11 bits available for hooks, which are stored in the
 *   chunk's flag field:
 *
 *     BK_HOOK_FLAG_0
 *     BK_HOOK_FLAG_1
 *     BK_HOOK_FLAG_2
 *     BK_HOOK_FLAG_3
 *     BK_HOOK_FLAG_4
 *     BK_HOOK_FLAG_5
 *     BK_HOOK_FLAG_6
 *     BK_HOOK_FLAG_7
 *     BK_HOOK_FLAG_8
 *     BK_HOOK_FLAG_9
 *     BK_HOOK_FLAG_10
 *
 *   Unfortunately, allocations from the bump space do not have any bits available for hooks.
 */


#ifndef __BKMALLOC_H__
#define __BKMALLOC_H__

#include <stddef.h> /* size_t */


struct bk_Heap;

#define BK_THREAD_LOCAL __thread

#ifdef __cplusplus
#define BK_THROW throw()
extern "C" {
#else
#define BK_THROW
#endif /* __cplusplus */


#ifdef BK_RETURN_ADDR

extern BK_THREAD_LOCAL void *_bk_return_addr;

#define BK_STORE_RA()                                                  \
(_bk_return_addr =                                                     \
    (__builtin_frame_address(0) == 0                                   \
        ? NULL                                                         \
        : __builtin_extract_return_addr(__builtin_return_address(0))))

#define BK_GET_RA() (_bk_return_addr)

#else

#define BK_STORE_RA()

#endif /* BK_RETURN_ADDR */


struct bk_Heap *bk_heap(const char *name);
void            bk_destroy_heap(const char *name);

#ifdef BKMALLOC_HOOK
void bk_unhook(void);
#endif

void * bk_malloc(struct bk_Heap *heap, size_t n_bytes);
void * bk_calloc(struct bk_Heap *heap, size_t count, size_t n_bytes);
void * bk_realloc(struct bk_Heap *heap, void *addr, size_t n_bytes);
void * bk_reallocf(struct bk_Heap *heap, void *addr, size_t n_bytes);
void * bk_valloc(struct bk_Heap *heap, size_t n_bytes);
void   bk_free(void *addr);
int    bk_posix_memalign(struct bk_Heap *heap, void **memptr, size_t alignment, size_t n_bytes);
void * bk_aligned_alloc(struct bk_Heap *heap, size_t alignment, size_t size);
size_t bk_malloc_size(void *addr);

void * _bk_new(size_t n_bytes) BK_THROW;

void * malloc(size_t n_bytes) BK_THROW;
void * calloc(size_t count, size_t n_bytes) BK_THROW;
void * realloc(void *addr, size_t n_bytes) BK_THROW;
void * reallocf(void *addr, size_t n_bytes) BK_THROW;
void * valloc(size_t n_bytes) BK_THROW;
void * pvalloc(size_t n_bytes) BK_THROW;
void   free(void *addr) BK_THROW;
int    posix_memalign(void **memptr, size_t alignment, size_t size) BK_THROW;
void * aligned_alloc(size_t alignment, size_t size) BK_THROW;
void * memalign(size_t alignment, size_t size) BK_THROW;
size_t malloc_size(void *addr) BK_THROW;
size_t malloc_usable_size(void *addr) BK_THROW;

#ifdef __cplusplus
}
#endif /* __cplusplus */

#ifdef __cplusplus

#include <new>
#include <mutex>

void * operator new(std::size_t size);
void * operator new[](std::size_t size);
void * operator new(std::size_t size, const std::nothrow_t &) noexcept;
void * operator new[](std::size_t size, const std::nothrow_t &) noexcept;
void   operator delete(void *ptr) noexcept;
void   operator delete[](void *ptr) noexcept;
void   operator delete(void *ptr, const std::nothrow_t &) noexcept;
void   operator delete[](void *ptr, const std::nothrow_t &) noexcept;

#if __cpp_sized_deallocation >= 201309
/* C++14's sized-delete operators. */
void operator delete(void *ptr, std::size_t size) noexcept;
void operator delete[](void *ptr, std::size_t size) noexcept;
#endif

#if __cpp_aligned_new >= 201606
/* C++17's over-aligned operators. */
void * operator new(std::size_t size, std::align_val_t);
void * operator new(std::size_t size, std::align_val_t, const std::nothrow_t &) noexcept;
void * operator new[](std::size_t size, std::align_val_t);
void * operator new[](std::size_t size, std::align_val_t, const std::nothrow_t &) noexcept;
void   operator delete(void* ptr, std::align_val_t) noexcept;
void   operator delete(void* ptr, std::align_val_t, const std::nothrow_t &) noexcept;
void   operator delete(void* ptr, std::size_t size, std::align_val_t al) noexcept;
void   operator delete[](void* ptr, std::align_val_t) noexcept;
void   operator delete[](void* ptr, std::align_val_t, const std::nothrow_t &) noexcept;
void   operator delete[](void* ptr, std::size_t size, std::align_val_t al) noexcept;
#endif

#ifndef BKMALLOC_HOOK

static inline void *
bk_handle_OOM(std::size_t size, bool nothrow) {
    void *ptr;

    ptr = nullptr;

    while (ptr == nullptr) {
        std::new_handler handler;
        /* GCC-4.8 and clang 4.0 do not have std::get_new_handler. */
        {
            static std::mutex mtx;
            std::lock_guard<std::mutex> lock(mtx);

            handler = std::set_new_handler(nullptr);
            std::set_new_handler(handler);
        }
        if (handler == nullptr) {
            break;
        }

        try {
            handler();
        } catch (const std::bad_alloc &) {
            break;
        }

        ptr = _bk_new(size);
    }

    if (ptr == nullptr && !nothrow) { std::__throw_bad_alloc(); }

    return ptr;
}

template <bool is_no_except>
static inline void *
bk_new_impl(std::size_t size) noexcept(is_no_except) {
    void *ptr;

    ptr = _bk_new(size);
    if (__builtin_expect(ptr != nullptr, 1)) { return ptr; }

    return bk_handle_OOM(size, is_no_except);
}

void * operator new(std::size_t size)                                    { BK_STORE_RA(); return bk_new_impl<false>(size); }
void * operator new[](std::size_t size)                                  { BK_STORE_RA(); return bk_new_impl<false>(size); }
void * operator new(std::size_t size, const std::nothrow_t &) noexcept   { BK_STORE_RA(); return bk_new_impl<true>(size);  }
void * operator new[](std::size_t size, const std::nothrow_t &) noexcept { BK_STORE_RA(); return bk_new_impl<true>(size);  }

#if __cpp_aligned_new >= 201606

template <bool is_no_except>
static inline void *
bk_aligned_new_impl(std::size_t size, std::align_val_t alignment) noexcept(is_no_except) {
    void *ptr;

    ptr = aligned_alloc(static_cast<std::size_t>(alignment), size);

    if (__builtin_expect(ptr != nullptr, 1)) {
        return ptr;
    }

    return bk_handle_OOM(size, is_no_except);
}

void * operator new(std::size_t size, std::align_val_t alignment)                                    { BK_STORE_RA(); return bk_aligned_new_impl<false>(size, alignment); }
void * operator new[](std::size_t size, std::align_val_t alignment)                                  { BK_STORE_RA(); return bk_aligned_new_impl<false>(size, alignment); }
void * operator new(std::size_t size, std::align_val_t alignment, const std::nothrow_t &) noexcept   { BK_STORE_RA(); return bk_aligned_new_impl<true>(size, alignment);  }
void * operator new[](std::size_t size, std::align_val_t alignment, const std::nothrow_t &) noexcept { BK_STORE_RA(); return bk_aligned_new_impl<true>(size, alignment);  }

#endif  /* __cpp_aligned_new */

void operator delete(void *ptr) noexcept                           { free(ptr); }
void operator delete[](void *ptr) noexcept                         { free(ptr); }
void operator delete(void *ptr, const std::nothrow_t &) noexcept   { free(ptr); }
void operator delete[](void *ptr, const std::nothrow_t &) noexcept { free(ptr); }

#if __cpp_sized_deallocation >= 201309

static inline void
bk_sized_delete_impl(void* ptr, std::size_t size) noexcept {
    (void)size;

    if (__builtin_expect(ptr == nullptr, 0)) {
        return;
    }
    free(ptr);
}

void operator delete(void *ptr, std::size_t size) noexcept   { bk_sized_delete_impl(ptr, size); }
void operator delete[](void *ptr, std::size_t size) noexcept { bk_sized_delete_impl(ptr, size); }

#endif  /* __cpp_sized_deallocation */

#if __cpp_aligned_new >= 201606

static inline void
bk_aligned_sized_delete_impl(void* ptr, std::size_t size, std::align_val_t alignment) noexcept {
    if (__builtin_expect(ptr == nullptr, 0)) {
        return;
    }
    free(ptr);
}

void operator delete(void* ptr, std::align_val_t) noexcept                               { free(ptr);                                          }
void operator delete[](void* ptr, std::align_val_t) noexcept                             { free(ptr);                                          }
void operator delete(void* ptr, std::align_val_t, const std::nothrow_t&) noexcept        { free(ptr);                                          }
void operator delete[](void* ptr, std::align_val_t, const std::nothrow_t&) noexcept      { free(ptr);                                          }
void operator delete(void* ptr, std::size_t size, std::align_val_t alignment) noexcept   { bk_aligned_sized_delete_impl(ptr, size, alignment); }
void operator delete[](void* ptr, std::size_t size, std::align_val_t alignment) noexcept { bk_aligned_sized_delete_impl(ptr, size, alignment); }

#endif /* BKMALLOC_HOOK */

#endif  /* __cpp_aligned_new */
#endif /* __cplusplus */


/******************************* @impl *******************************/
#if defined(BKMALLOC_IMPL) || defined(BKMALLOC_HOOK)

#include <stdint.h>
#include <stdarg.h>
#include <stddef.h>
#include <stdio.h>
#include <string.h> /* memcpy(), memset() */
#include <threads.h>
#include <time.h>
#include <stdlib.h>
/* Stuff from stdlib.h, but I don't want to include stdlib.h */
// char *getenv(const char *name);
// void  exit(int status);

/* #define BK_MIN_ALIGN                            (16ULL) */
#define BK_MIN_ALIGN                            (32ULL)
#define BK_BLOCK_SIZE                           (MB(1))
#define BK_BLOCK_ALIGN                          (MB(1))
#define BK_BASE_SIZE_CLASS                      (BK_MIN_ALIGN)
#define BK_NR_SIZE_CLASSES                      (LOG2_64BIT(BK_BLOCK_SIZE / BK_BASE_SIZE_CLASS) - 1)
#define BK_MAX_BLOCK_ALLOC_SIZE                 (BK_BASE_SIZE_CLASS * (1 << (BK_NR_SIZE_CLASSES - 1)))
#define BK_BIG_ALLOC_SIZE_CLASS                 (0xFFFFFFFF)
#define BK_BIG_ALLOC_SIZE_CLASS_IDX             (0xFFFFFFFF)
#define BK_REQUEST_NEEDS_BIG_ALLOC(_sz, _a, _z) (unlikely((_sz) > BK_MAX_BLOCK_ALLOC_SIZE) || ((_z) && (_sz) >= KB(16)))

#ifndef BKMALLOC_HOOK
static inline void bk_init(void);
#endif /* BKMALLOC_HOOK */


/******************************* @@util *******************************/

#define likely(x)   (__builtin_expect(!!(x), 1))
#define unlikely(x) (__builtin_expect(!!(x), 0))

#define MIN(a, b) ((a) <= (b) ? (a) : (b))
#define MAX(a, b) ((a) >= (b) ? (a) : (b))

#define UINT(w) uint##w##_t
#define SINT(w) int##w##_t

#define u8  UINT(8 )
#define u16 UINT(16)
#define u32 UINT(32)
#define u64 UINT(64)

#define s8  SINT(8 )
#define s16 SINT(16)
#define s32 SINT(32)
#define s64 SINT(64)

#ifdef BK_DEBUG
#define BK_ALWAYS_INLINE
#else
#define BK_ALWAYS_INLINE __attribute__((always_inline))
#endif /* BK_DEBUG */

#define BK_HOT __attribute__((hot))

#define BK_GLOBAL_ALIGN(_a) __attribute__((aligned((_a))))
#define BK_FIELD_ALIGN(_a)  __attribute__((aligned((_a))))
#define BK_PACKED           __attribute__((packed))

#define XOR_SWAP_64(a, b) do {   \
    a = ((u64)(a)) ^ ((u64)(b)); \
    b = ((u64)(b)) ^ ((u64)(a)); \
    a = ((u64)(a)) ^ ((u64)(b)); \
} while (0);

#define XOR_SWAP_PTR(a, b) do {           \
    a = (void*)(((u64)(a)) ^ ((u64)(b))); \
    b = (void*)(((u64)(b)) ^ ((u64)(a))); \
    a = (void*)(((u64)(a)) ^ ((u64)(b))); \
} while (0);

#define ALIGN_UP(x, align)      ((__typeof(x))((((u64)(x)) + ((u64)align)) & ~(((u64)align) - 1ULL)))
#define ALIGN_DOWN(x, align)    ((__typeof(x))(((u64)(x)) & ~(((u64)align) - 1ULL)))
#define IS_ALIGNED(x, align)    (!(((u64)(x)) & (((u64)align) - 1ULL)))
#define IS_POWER_OF_TWO(x)      ((x) != 0 && IS_ALIGNED((x), (x)))

#define LOG2_8BIT(v)  (8 - 90/(((v)/4+14)|1) - 2/((v)/2+1))
#define LOG2_16BIT(v) (8*((v)>255) + LOG2_8BIT((v) >>8*((v)>255)))
#define LOG2_32BIT(v)                                        \
    (16*((v)>65535L) + LOG2_16BIT((v)*1L >>16*((v)>65535L)))
#define LOG2_64BIT(v)                                        \
    (32*((v)/2L>>31 > 0)                                     \
     + LOG2_32BIT((v)*1L >>16*((v)/2L>>31 > 0)               \
                         >>16*((v)/2L>>31 > 0)))

#define KB(x) ((x) * 1024ULL)
#define MB(x) ((x) * 1024ULL * KB(1ULL))
#define GB(x) ((x) * 1024ULL * MB(1ULL))
#define TB(x) ((x) * 1024ULL * GB(1ULL))

#define CAS(_ptr, _old, _new) (__sync_bool_compare_and_swap((_ptr), (_old), (_new)))
#define FAA(_ptr, _val)       (__sync_fetch_and_add((_ptr), (_val)))
#define CLZ(_val)             (__builtin_clzll((_val) | 1ULL))
#define CTZ(_val)             (__builtin_ctzll((_val) | 1ULL))

#define CACHE_LINE     (64ULL)
#define PAGE_SIZE      (4096ULL)
#define LOG2_PAGE_SIZE (LOG2_64BIT(PAGE_SIZE))


static inline int bk_is_space(int c) {
    unsigned char d = c - 9;
    return (0x80001FU >> (d & 31)) & (1U >> (d >> 5));
}

static void bk_fdprintf(int fd, const char *fmt, ...);
static void bk_sprintf(char *buff, const char *fmt, ...);

#define bk_printf(...) (bk_fdprintf(1, __VA_ARGS__))
#define bk_logf(...)                                 \
do {                                                 \
    if (bk_config.log_fd != 0) {                     \
        bk_fdprintf(bk_config.log_fd, __VA_ARGS__);  \
    }                                                \
} while (0)


#ifdef BK_DO_ASSERTIONS
static void bk_assert_fail(const char *msg, const char *fname, int line, const char *cond_str) {
    volatile int *trap;

    bk_printf("Assertion failed -- %s\n"
              "at  %s :: line %d\n"
              "    Condition: '%s'\n"
              , msg, fname, line, cond_str);

    asm volatile ("" ::: "memory");

    trap = 0;
    (void)*trap;
}

#define BK_ASSERT(cond, msg)                        \
do { if (unlikely(!(cond))) {                       \
    bk_assert_fail(msg, __FILE__, __LINE__, #cond); \
} } while (0)
#else
#define BK_ASSERT(cond, mst) ;
#endif

BK_ALWAYS_INLINE
static inline void bk_memzero(void *addr, u64 size) {
    __builtin_memset(__builtin_assume_aligned(addr, BK_MIN_ALIGN), 0, size);
}


#ifndef BKMALLOC_HOOK

static void *bk_imalloc(u64 n_bytes);
static void  bk_ifree(void *addr);
static char *bk_istrdup(const char *s);

#endif /* BKMALLOC_HOOK */

typedef const char *bk_str;

BK_ALWAYS_INLINE
static inline int bk_str_equ(bk_str a, bk_str b) {
    return strcmp(a, b) == 0;
}

BK_ALWAYS_INLINE
static inline u64 bk_str_hash(bk_str s) {
    unsigned long hash = 5381;
    int c;

    while ((c = *s++)) {
        hash = ((hash << 5) + hash) + c; /* hash * 33 + c */
    }

    return hash;
}


/******************************* @@platform *******************************/
#if defined(unix) || defined(__unix__) || defined(__unix) || defined(__linux__) || defined(__APPLE__)
    #define BK_UNIX
    #include <unistd.h>
    #include <errno.h>
    #include <sys/mman.h>
    #include <fcntl.h>
    #include <dlfcn.h>

    #if defined(__APPLE__)
        #define BK_APPLE
    #endif

    #if defined(__linux__)
        #define BK_LINUX
        #include <sys/syscall.h>
    #endif
#elif defined(_WIN64)
    #define BK_WIN
#endif

#if defined(BK_LINUX)
static inline u32 bk_get_nr_cpus_linux(void) {
    u32 nr_cpus;

    nr_cpus = sysconf(_SC_NPROCESSORS_ONLN);

    return nr_cpus;
}
#endif

BK_ALWAYS_INLINE
static inline u32 bk_get_nr_cpus(void) {
#if defined(BK_LINUX)
    return bk_get_nr_cpus_linux();
#else
    #error "platform missing implementation"
#endif
}

#if defined(BK_LINUX)
BK_ALWAYS_INLINE
static inline u32 bk_getcpu_linux(void) {
    unsigned cpu;

    syscall(SYS_getcpu, &cpu, NULL, NULL);

    return cpu;
}
#endif

BK_ALWAYS_INLINE
static inline u32 bk_getcpu(void) {
#if defined(BK_LINUX)
    return bk_getcpu_linux();
#else
    #error "platform missing implementation"
#endif
}

#if defined(BK_UNIX)
static inline void *bk_open_library_unix(const char *path) {
    return dlopen(path, RTLD_NOW);
}

static inline void *bk_library_symbol_unix(void *lib_handle, const char *name) {
    return dlsym(lib_handle, name);
}
#endif

#if defined(BK_UNIX)

static BK_THREAD_LOCAL int _bk_internal_mmap;
static BK_THREAD_LOCAL int _bk_internal_munmap;

static inline void * bk_get_pages_unix(u64 n_pages) {
    u64   desired_size;
    void *pages;

    desired_size = n_pages * PAGE_SIZE;

    _bk_internal_mmap = 1;

    errno = 0;
    pages = mmap(NULL, desired_size,
                 PROT_READ   | PROT_WRITE,
                 MAP_PRIVATE | MAP_ANONYMOUS,
                 -1, (off_t)0);

    _bk_internal_mmap = 0;

    if (unlikely(pages == MAP_FAILED || pages == NULL)) {
        BK_ASSERT(0,
                  "mmap() failed");
        return NULL;
    }

    BK_ASSERT(errno == 0,
              "errno non-zero after mmap()");

    return pages;
}

static inline void bk_release_pages_unix(void *addr, u64 n_pages) {
    int err;

    (void)err;

    _bk_internal_munmap = 1;
    err = munmap(addr, n_pages * PAGE_SIZE);
    _bk_internal_munmap = 0;

    BK_ASSERT(err == 0,
              "munmap() failed");
}

static inline void bk_decommit_pages_unix(void *addr, u64 n_pages) {
    int err;

    (void)err;

    err = madvise(addr, n_pages * PAGE_SIZE, MADV_DONTNEED);

    BK_ASSERT(err == 0,
              "munmap() failed");
}

#elif defined(BK_WIN)
#error "Windows is not supported yet."

static inline void * bk_open_library_win(const char *path)                     { return NULL; }
static inline void * bk_library_symbol_win(void *lib_handle, const char *name) { return NULL; }
static inline void * bk_get_pages_win(u64 n_pages)                             { return NULL; }
static inline void   bk_release_pages_win(void *addr, u64 n_pages)             {              }
static inline void   bk_decommit_pages_win(void *addr, u64 n_pages)            {              }

#else
#error "Unknown platform."
#endif


static inline void * bk_open_library(const char *path) {
#if defined(BK_UNIX)
    return bk_open_library_unix(path);
#else
    #error "platform missing implementation"
#endif
}

static inline void * bk_library_symbol(void *lib_handle, const char *name) {
#if defined(BK_UNIX)
    return bk_library_symbol_unix(lib_handle, name);
#else
    #error "platform missing implementation"
#endif
}

static inline void * bk_get_pages(u64 n_pages) {
    void *pages;

    BK_ASSERT(n_pages > 0,
              "n_pages is zero");

#if defined(BK_UNIX)
    pages = bk_get_pages_unix(n_pages);
#else
    #error "platform missing implementation"
#endif

    return pages;
}

static inline void bk_release_pages(void *addr, u64 n_pages) {
    BK_ASSERT(n_pages > 0,
              "n_pages is zero");

#if defined(BK_UNIX)
    bk_release_pages_unix(addr, n_pages);
#elif defined(BK_WIN)
    #error "need Windows release_pages"
#endif
}

static inline void bk_decommit_pages(void *addr, u64 n_pages) {
    BK_ASSERT(n_pages > 0,
              "n_pages is zero");

#if defined(BK_UNIX)
    bk_decommit_pages_unix(addr, n_pages);
#elif defined(BK_WIN)
    #error "need Windows release_pages"
#endif
}

static inline void * bk_get_aligned_pages(u64 n_pages, u64 alignment) {
    u64   desired_size;
    u64   first_map_size;
    void *mem_start;
    void *aligned_start;
    void *aligned_end;
    void *mem_end;

    alignment    = MAX(alignment, PAGE_SIZE);
    desired_size = n_pages * PAGE_SIZE;

    /* Ask for memory twice the desired size so that we can get aligned memory. */
    first_map_size = desired_size;
    if (alignment > PAGE_SIZE) {
        first_map_size += ALIGN_UP(alignment, PAGE_SIZE);
    }

    mem_start = bk_get_pages(first_map_size >> LOG2_PAGE_SIZE);

    aligned_start = ALIGN_UP(mem_start, alignment);
    aligned_end   = (void*)((u8*)aligned_start + desired_size);
    mem_end       = (void*)((u8*)mem_start + first_map_size);

    /* Trim off the edges we don't need. */
    if (mem_start != aligned_start) {
        bk_release_pages(mem_start, ((u8*)aligned_start - (u8*)mem_start) >> LOG2_PAGE_SIZE);
    }

    if (mem_end != aligned_end) {
        bk_release_pages(aligned_end, ((u8*)mem_end - (u8*)aligned_end) >> LOG2_PAGE_SIZE);
    }

    return aligned_start;
}

/******************************* @@locks *******************************/

typedef struct {
    s32 val;
} bk_Spinlock;

#define BK_SPIN_UNLOCKED (0)
#define BK_SPIN_LOCKED   (1)
#define BK_STATIC_SPIN_LOCK_INIT ((bk_Spinlock){ BK_SPIN_UNLOCKED })

BK_ALWAYS_INLINE static inline void bk_spin_init(bk_Spinlock *spin) {
    spin->val = BK_SPIN_UNLOCKED;
}

BK_ALWAYS_INLINE static inline void bk_spin_lock(bk_Spinlock *spin) {
    while (!CAS(&spin->val, 0, 1));
}

BK_ALWAYS_INLINE static inline void bk_spin_unlock(bk_Spinlock *spin) {
    BK_ASSERT(spin->val == BK_SPIN_LOCKED, "lock was not locked");
    (void)CAS(&spin->val, 1, 0);
}



/******************************* @@printf *******************************/

#define BK_PR_RESET      "\e[0m"
#define BK_PR_FG_BLACK   "\e[30m"
#define BK_PR_FG_BLUE    "\e[34m"
#define BK_PR_FG_GREEN   "\e[32m"
#define BK_PR_FG_CYAN    "\e[36m"
#define BK_PR_FG_RED     "\e[31m"
#define BK_PR_FG_YELLOW  "\e[33m"
#define BK_PR_FG_MAGENTA "\e[35m"
#define BK_PR_BG_BLACK   "\e[40m"
#define BK_PR_BG_BLUE    "\e[44m"
#define BK_PR_BG_GREEN   "\e[42m"
#define BK_PR_BG_CYAN    "\e[46m"
#define BK_PR_BG_RED     "\e[41m"
#define BK_PR_BG_YELLOW  "\e[43m"
#define BK_PR_BG_MAGENTA "\e[45m"

#define BK_PUTC(fd, _c)    \
do {                       \
    char c;                \
    int  x;                \
    c = (_c);              \
    x = write((fd), &c, 1);\
    (void)x;               \
} while (0)

static void bk_puts(int fd, const char *s) {
    while (*s) { BK_PUTC(fd, *s); s += 1; }
}

static char * bk_atos(char *p, char c) {
    p[0] = c;
    p[1] = 0;
    return p;
}

static char * bk_dtos(char *p, s32 d) {
    int neg;

    neg = d < 0;

    if (neg) { d = -d; }

    p += 3 * sizeof(s32);
    *--p = 0;

    do {
        *--p = '0' + d % 10;
        d /= 10;
    } while (d);

    if (neg) { *--p = '-'; }

    return p;
}

static char * bk_Dtos(char *p, s64 d) {
    int neg;

    neg = d < 0;

    if (neg) { d = -d; }

    p += 3 * sizeof(s64);
    *--p = 0;

    do {
        *--p = '0' + d % 10;
        d /= 10;
    } while (d);

    if (neg) { *--p = '-'; }

    return p;
}

static char * bk_utos(char *p, u32 d) {
    p += 3 * sizeof(u32);
    *--p = 0;

    do {
        *--p = '0' + d % 10;
        d /= 10;
    } while (d);

    return p;
}

static char * bk_Utos(char *p, u64 d) {
    p += 3 * sizeof(u64);
    *--p = 0;

    do {
        *--p = '0' + d % 10;
        d /= 10;
    } while (d);

    return p;
}

static char * bk_xtos(char *p, u32 x) {
    const char *digits = "0123456789ABCDEF";

    if (x == 0) { *p = '0'; *(p + 1) = 0; return p; }

    p += 3 * sizeof(u32);
    *--p = 0;

    while (x) {
        *--p = digits[x & 0xf];
        x >>= 4;
    }

    return p;
}

static char * bk_Xtos(char *p, u64 x) {
    const char *digits = "0123456789ABCDEF";

    if (x == 0) { *p = '0'; *(p + 1) = 0; return p; }

    p += 3 * sizeof(u64);
    *--p = 0;

    while (x) {
        *--p = digits[x & 0xf];
        x >>= 4;
    }

    return p;
}

static const char * bk_eat_pad(const char *s, int *pad, va_list *argsp) {
    int neg;

    if (!*s) { return s; }

    *pad = 0;

    neg = *s == '-';

    if (neg || *s == '0') { s += 1; }

    if (*s == '*') {
        *pad = va_arg(*argsp, s32);
        s += 1;
    } else {
        while (*s >= '0' && *s <= '9') {
            *pad = 10 * *pad + (*s - '0');
            s += 1;
        }
    }

    if (neg) { *pad = -*pad; }

    return s;
}

static bk_Spinlock bk_printf_lock;

void bk_vprintf(char *out_buff, int fd, const char *fmt, va_list _args) {
    va_list     args;
    int         last_was_perc;
    char        c;
    int         pad;
    int         padz;
    char        buff[64];
    const char *p;
    int         p_len;
    int         len;

    bk_spin_lock(&bk_printf_lock);

    va_copy(args, _args);

    last_was_perc = 0;

    if (out_buff != NULL) { out_buff[0] = 0; }

    while ((c = *fmt)) {
        if (c == '%' && !last_was_perc) {
            last_was_perc = 1;
            goto next;
        }

        if (last_was_perc) {
            pad = padz = 0;

            if (c == '-' || c == '0' || (c >= '1' && c <= '9')) {
                padz = c == '0';
                fmt = bk_eat_pad(fmt, &pad, &args);
                c = *fmt;
            }

            switch (c) {
                case '%': p = "%";                               break;
                case '_': p = BK_PR_RESET;                       break;
                case 'k': p = BK_PR_FG_BLACK;                    break;
                case 'b': p = BK_PR_FG_BLUE;                     break;
                case 'g': p = BK_PR_FG_GREEN;                    break;
                case 'c': p = BK_PR_FG_CYAN;                     break;
                case 'r': p = BK_PR_FG_RED;                      break;
                case 'y': p = BK_PR_FG_YELLOW;                   break;
                case 'm': p = BK_PR_FG_MAGENTA;                  break;
                case 'K': p = BK_PR_BG_BLACK;                    break;
                case 'B': p = BK_PR_BG_BLUE;                     break;
                case 'G': p = BK_PR_BG_GREEN;                    break;
                case 'C': p = BK_PR_BG_CYAN;                     break;
                case 'R': p = BK_PR_BG_RED;                      break;
                case 'Y': p = BK_PR_BG_YELLOW;                   break;
                case 'M': p = BK_PR_BG_MAGENTA;                  break;
                case 'a': p = bk_atos(buff, va_arg(args, s32));  break;
                case 'd': p = bk_dtos(buff, va_arg(args, s32));  break;
                case 'D': p = bk_Dtos(buff, va_arg(args, s64));  break;
                case 'u': p = bk_utos(buff, va_arg(args, u32));  break;
                case 'U': p = bk_Utos(buff, va_arg(args, u64));  break;
                case 'x': p = bk_xtos(buff, va_arg(args, u32));  break;
                case 'X': p = bk_Xtos(buff, va_arg(args, u64));  break;
                case 's': p = va_arg(args, char*);               break;

                default: goto noprint;
            }

            for (p_len = 0; p[p_len]; p_len += 1);

            for (; pad - p_len > 0; pad -= 1) { BK_PUTC(fd, padz ? '0' : ' '); }

            if (out_buff != NULL) {
                strcat(out_buff, p);
            } else {
                bk_puts(fd, p);
            }

            for (; pad + p_len < 0; pad += 1) { BK_PUTC(fd, padz ? '0' : ' '); }

noprint:;
            last_was_perc = 0;
        } else {
            if (out_buff != NULL) {
                len               = strlen(out_buff);
                out_buff[len]     = *fmt;
                out_buff[len + 1] = 0;
            } else {
                BK_PUTC(fd, *fmt);
            }
        }

next:;
        fmt += 1;
    }

    va_end(args);

    bk_spin_unlock(&bk_printf_lock);
}

static void bk_fdprintf(int fd, const char *fmt, ...) {
    va_list args;

    va_start(args, fmt);
    bk_vprintf(NULL, fd, fmt, args);
    va_end(args);
}

static void bk_sprintf(char *buff, const char *fmt, ...) {
    va_list args;

    va_start(args, fmt);
    bk_vprintf(buff, -1, fmt, args);
    va_end(args);
}


/******************************* @@hash_table *******************************/

#define hash_table_make(K_T, V_T, HASH) (CAT2(hash_table(K_T, V_T), _make)((HASH), NULL))
#define hash_table_make_e(K_T, V_T, HASH, EQU) (CAT2(hash_table(K_T, V_T), _make)((HASH), (EQU)))
#define hash_table_len(t) (t->len)
#define hash_table_free(t) (t->_free((t)))
#define hash_table_get_key(t, k) (t->_get_key((t), (k)))
#define hash_table_get_val(t, k) (t->_get_val((t), (k)))
#define hash_table_insert(t, k, v) (t->_insert((t), (k), (v)))
#define hash_table_delete(t, k) (t->_delete((t), (k)))
#define hash_table_traverse(t, key, val_ptr)                     \
    for (/* vars */                                              \
         uint64_t __i    = 0,                                    \
                  __size = ht_prime_sizes[t->_size_idx];         \
         /* conditions */                                        \
         __i < __size;                                           \
         /* increment */                                         \
         __i += 1)                                               \
        for (/* vars */                                          \
             __typeof__(*t->_data) *__slot_ptr = t->_data + __i, \
                                    __slot     = *__slot_ptr;    \
                                                                 \
             /* conditions */                                    \
             __slot != NULL                 &&                   \
             (key     = __slot->_key   , 1) &&                   \
             (val_ptr = &(__slot->_val), 1);                     \
                                                                 \
             /* increment */                                     \
             __slot_ptr = &(__slot->_next),                      \
             __slot = *__slot_ptr)                               \
            /* LOOP BODY HERE */                                 \


#define STR(x) _STR(x)
#define _STR(x) #x

#define CAT2(x, y) _CAT2(x, y)
#define _CAT2(x, y) x##y

#define CAT3(x, y, z) _CAT3(x, y, z)
#define _CAT3(x, y, z) x##y##z

#define CAT4(a, b, c, d) _CAT4(a, b, c, d)
#define _CAT4(a, b, c, d) a##b##c##d

#define _hash_table_slot(K_T, V_T) CAT4(_hash_table_slot_, K_T, _, V_T)
#define hash_table_slot(K_T, V_T) CAT4(hash_table_slot_, K_T, _, V_T)
#define _hash_table(K_T, V_T) CAT4(_hash_table_, K_T, _, V_T)
#define hash_table(K_T, V_T) CAT4(hash_table_, K_T, _, V_T)
#define hash_table_pretty_name(K_T, V_T) ("hash_table(" CAT3(K_T, ", ", V_T) ")")

#define _HASH_TABLE_EQU(t_ptr, l, r) \
    ((t_ptr)->_equ ? (t_ptr)->_equ((l), (r)) : (memcmp(&(l), &(r), sizeof((l))) == 0))

#define DEFAULT_START_SIZE_IDX (3)

static uint64_t ht_prime_sizes[] = {
    5ULL,  11ULL,  23ULL,  47ULL,  97ULL,  199ULL,  409ULL,  823ULL,  1741ULL,  3469ULL,
    6949ULL,  14033ULL,  28411ULL,  57557ULL,  116731ULL,  236897ULL,  480881ULL,  976369ULL,
    1982627ULL,  4026031ULL,  8175383ULL,  16601593ULL,  33712729ULL,  68460391ULL,
    139022417ULL,  282312799ULL,  573292817ULL,  1164186217ULL,  2364114217ULL,
    4294967291ULL,  8589934583ULL,  17179869143ULL,  34359738337ULL,  68719476731ULL,
    137438953447ULL,  274877906899ULL,  549755813881ULL,  1099511627689ULL,  2199023255531ULL,
    4398046511093ULL,  8796093022151ULL,  17592186044399ULL,  35184372088777ULL,
    70368744177643ULL,  140737488355213ULL,  281474976710597ULL,  562949953421231ULL,
    1125899906842597ULL,  2251799813685119ULL,  4503599627370449ULL,  9007199254740881ULL,
    18014398509481951ULL,  36028797018963913ULL,  72057594037927931ULL,
    144115188075855859ULL,  288230376151711717ULL,  576460752303423433ULL,
    1152921504606846883ULL,  2305843009213693951ULL,  4611686018427387847ULL,
    9223372036854775783ULL,  18446744073709551557ULL
};

#define use_hash_table(K_T, V_T)                                                             \
    struct _hash_table(K_T, V_T);                                                            \
                                                                                             \
    typedef struct _hash_table_slot(K_T, V_T) {                                              \
        K_T _key;                                                                            \
        V_T _val;                                                                            \
        uint64_t _hash;                                                                      \
        struct _hash_table_slot(K_T, V_T) *_next;                                            \
    }                                                                                        \
    *hash_table_slot(K_T, V_T);                                                              \
                                                                                             \
    typedef void (*CAT2(hash_table(K_T, V_T), _free_t))                                      \
        (struct _hash_table(K_T, V_T) *);                                                    \
    typedef K_T* (*CAT2(hash_table(K_T, V_T), _get_key_t))                                   \
        (struct _hash_table(K_T, V_T) *, K_T);                                               \
    typedef V_T* (*CAT2(hash_table(K_T, V_T), _get_val_t))                                   \
        (struct _hash_table(K_T, V_T) *, K_T);                                               \
    typedef void (*CAT2(hash_table(K_T, V_T), _insert_t))                                    \
        (struct _hash_table(K_T, V_T) *, K_T, V_T);                                          \
    typedef int (*CAT2(hash_table(K_T, V_T), _delete_t))                                     \
        (struct _hash_table(K_T, V_T) *, K_T);                                               \
    typedef uint64_t (*CAT2(hash_table(K_T, V_T), _hash_t))(K_T);                            \
    typedef int (*CAT2(hash_table(K_T, V_T), _equ_t))(K_T, K_T);                             \
                                                                                             \
    typedef struct _hash_table(K_T, V_T) {                                                   \
        hash_table_slot(K_T, V_T) *_data;                                                    \
        uint64_t len, _size_idx, _load_thresh;                                               \
                                                                                             \
        CAT2(hash_table(K_T, V_T), _free_t)    const _free;                                  \
        CAT2(hash_table(K_T, V_T), _get_key_t) const _get_key;                               \
        CAT2(hash_table(K_T, V_T), _get_val_t) const _get_val;                               \
        CAT2(hash_table(K_T, V_T), _insert_t)  const _insert;                                \
        CAT2(hash_table(K_T, V_T), _delete_t)  const _delete;                                \
        CAT2(hash_table(K_T, V_T), _hash_t)    const _hash;                                  \
        CAT2(hash_table(K_T, V_T), _equ_t)     const _equ;                                   \
    }                                                                                        \
    *hash_table(K_T, V_T);                                                                   \
                                                                                             \
    /* hash_table slot */                                                                    \
    static inline hash_table_slot(K_T, V_T)                                                  \
        CAT2(hash_table_slot(K_T, V_T), _make)(K_T key, V_T val, uint64_t hash) {            \
        hash_table_slot(K_T, V_T) slot =                                                     \
            (hash_table_slot(K_T, V_T))bk_imalloc(sizeof(*slot));                            \
                                                                                             \
        slot->_key  = key;                                                                   \
        slot->_val  = val;                                                                   \
        slot->_hash = hash;                                                                  \
        slot->_next = NULL;                                                                  \
                                                                                             \
        return slot;                                                                         \
    }                                                                                        \
                                                                                             \
    /* hash_table */                                                                         \
    static inline void CAT2(hash_table(K_T, V_T), _rehash_insert)                            \
        (hash_table(K_T, V_T) t, hash_table_slot(K_T, V_T) insert_slot) {                    \
                                                                                             \
        uint64_t h, data_size, idx;                                                          \
        hash_table_slot(K_T, V_T) slot, *slot_ptr;                                           \
                                                                                             \
        h         = insert_slot->_hash;                                                      \
        data_size = ht_prime_sizes[t->_size_idx];                                            \
        idx       = h % data_size;                                                           \
        slot_ptr  = t->_data + idx;                                                          \
                                                                                             \
        while ((slot = *slot_ptr))    { slot_ptr = &(slot->_next); }                         \
                                                                                             \
        *slot_ptr = insert_slot;                                                             \
    }                                                                                        \
                                                                                             \
    static inline void                                                                       \
        CAT2(hash_table(K_T, V_T), _update_load_thresh)(hash_table(K_T, V_T) t) {            \
                                                                                             \
        uint64_t cur_size;                                                                   \
                                                                                             \
        cur_size        = ht_prime_sizes[t->_size_idx];                                      \
        t->_load_thresh = ((double)((cur_size << 1ULL))                                      \
                            / ((double)(cur_size * 3)))                                      \
                            * cur_size;                                                      \
    }                                                                                        \
                                                                                             \
    static inline void CAT2(hash_table(K_T, V_T), _rehash)(hash_table(K_T, V_T) t) {         \
        uint64_t                   old_size,                                                 \
                                   new_data_size;                                            \
        hash_table_slot(K_T, V_T) *old_data,                                                 \
                                   slot,                                                     \
                                  *slot_ptr,                                                 \
                                   next;                                                     \
                                                                                             \
        old_size      = ht_prime_sizes[t->_size_idx];                                        \
        old_data      = t->_data;                                                            \
        t->_size_idx += 1;                                                                   \
        new_data_size = sizeof(hash_table_slot(K_T, V_T)) * ht_prime_sizes[t->_size_idx];    \
        t->_data      = (hash_table_slot(K_T, V_T)*)bk_imalloc(new_data_size);               \
        memset(t->_data, 0, new_data_size);                                                  \
                                                                                             \
        for (u64 i = 0; i < old_size; i += 1) {                                              \
            slot_ptr = old_data + i;                                                         \
            next = *slot_ptr;                                                                \
            while ((slot = next)) {                                                          \
                next        = slot->_next;                                                   \
                slot->_next = NULL;                                                          \
                CAT2(hash_table(K_T, V_T), _rehash_insert)(t, slot);                         \
            }                                                                                \
        }                                                                                    \
                                                                                             \
        bk_ifree(old_data);                                                                  \
                                                                                             \
        CAT2(hash_table(K_T, V_T), _update_load_thresh)(t);                                  \
    }                                                                                        \
                                                                                             \
    static inline void                                                                       \
        CAT2(hash_table(K_T, V_T), _insert)(hash_table(K_T, V_T) t, K_T key, V_T val) {      \
        uint64_t h, data_size, idx;                                                          \
        hash_table_slot(K_T, V_T) slot, *slot_ptr;                                           \
                                                                                             \
        h         = t->_hash(key);                                                           \
        data_size = ht_prime_sizes[t->_size_idx];                                            \
        idx       = h % data_size;                                                           \
        slot_ptr  = t->_data + idx;                                                          \
                                                                                             \
        while ((slot = *slot_ptr)) {                                                         \
            if (_HASH_TABLE_EQU(t, slot->_key, key)) {                                       \
                slot->_val = val;                                                            \
                return;                                                                      \
            }                                                                                \
            slot_ptr = &(slot->_next);                                                       \
        }                                                                                    \
                                                                                             \
        *slot_ptr = CAT2(hash_table_slot(K_T, V_T), _make)(key, val, h);                     \
        t->len   += 1;                                                                       \
                                                                                             \
        if (t->len == t->_load_thresh) {                                                     \
            CAT2(hash_table(K_T, V_T), _rehash)(t);                                          \
        }                                                                                    \
    }                                                                                        \
                                                                                             \
    static inline int CAT2(hash_table(K_T, V_T), _delete)                                    \
        (hash_table(K_T, V_T) t, K_T key) {                                                  \
                                                                                             \
        uint64_t h, data_size, idx;                                                          \
        hash_table_slot(K_T, V_T) slot, prev, *slot_ptr;                                     \
                                                                                             \
        h = t->_hash(key);                                                                   \
        data_size = ht_prime_sizes[t->_size_idx];                                            \
        idx = h % data_size;                                                                 \
        slot_ptr = t->_data + idx;                                                           \
        prev = NULL;                                                                         \
                                                                                             \
        while ((slot = *slot_ptr)) {                                                         \
            if (_HASH_TABLE_EQU(t, slot->_key, key)) {                                       \
                break;                                                                       \
            }                                                                                \
            prev     = slot;                                                                 \
            slot_ptr = &(slot->_next);                                                       \
        }                                                                                    \
                                                                                             \
        if ((slot = *slot_ptr)) {                                                            \
            if (prev) {                                                                      \
                prev->_next = slot->_next;                                                   \
            } else {                                                                         \
                *slot_ptr = slot->_next;                                                     \
            }                                                                                \
            bk_ifree(slot);                                                                  \
            t->len -= 1;                                                                     \
            return 1;                                                                        \
        }                                                                                    \
        return 0;                                                                            \
    }                                                                                        \
                                                                                             \
    static inline K_T*                                                                       \
        CAT2(hash_table(K_T, V_T), _get_key)(hash_table(K_T, V_T) t, K_T key) {              \
                                                                                             \
        uint64_t h, data_size, idx;                                                          \
        hash_table_slot(K_T, V_T) slot, *slot_ptr;                                           \
                                                                                             \
        h         = t->_hash(key);                                                           \
        data_size = ht_prime_sizes[t->_size_idx];                                            \
        idx       = h % data_size;                                                           \
        slot_ptr  = t->_data + idx;                                                          \
                                                                                             \
        while ((slot = *slot_ptr)) {                                                         \
            if (_HASH_TABLE_EQU(t, slot->_key, key)) {                                       \
                return &slot->_key;                                                          \
            }                                                                                \
            slot_ptr = &(slot->_next);                                                       \
        }                                                                                    \
                                                                                             \
        return NULL;                                                                         \
    }                                                                                        \
                                                                                             \
    static inline V_T*                                                                       \
        CAT2(hash_table(K_T, V_T), _get_val)(hash_table(K_T, V_T) t, K_T key) {              \
                                                                                             \
        uint64_t h, data_size, idx;                                                          \
        hash_table_slot(K_T, V_T) slot, *slot_ptr;                                           \
                                                                                             \
        h         = t->_hash(key);                                                           \
        data_size = ht_prime_sizes[t->_size_idx];                                            \
        idx       = h % data_size;                                                           \
        slot_ptr  = t->_data + idx;                                                          \
                                                                                             \
        while ((slot = *slot_ptr)) {                                                         \
            if (_HASH_TABLE_EQU(t, slot->_key, key)) {                                       \
                return &slot->_val;                                                          \
            }                                                                                \
            slot_ptr = &(slot->_next);                                                       \
        }                                                                                    \
                                                                                             \
        return NULL;                                                                         \
    }                                                                                        \
                                                                                             \
    static inline void CAT2(hash_table(K_T, V_T), _free)(hash_table(K_T, V_T) t) {           \
        for (u64 i = 0; i < ht_prime_sizes[t->_size_idx]; i += 1) {                          \
            hash_table_slot(K_T, V_T) next, slot = t->_data[i];                              \
            while (slot != NULL) {                                                           \
                next = slot->_next;                                                          \
                bk_ifree(slot);                                                              \
                slot = next;                                                                 \
            }                                                                                \
        }                                                                                    \
        bk_ifree(t->_data);                                                                  \
        bk_ifree(t);                                                                         \
    }                                                                                        \
                                                                                             \
    static inline hash_table(K_T, V_T)                                                       \
    CAT2(hash_table(K_T, V_T), _make)(CAT2(hash_table(K_T, V_T), _hash_t) hash, void *equ) { \
        hash_table(K_T, V_T) t = (hash_table(K_T, V_T))bk_imalloc(sizeof(*t));               \
                                                                                             \
        uint64_t data_size                                                                   \
            = ht_prime_sizes[DEFAULT_START_SIZE_IDX] * sizeof(hash_table_slot(K_T, V_T));    \
        hash_table_slot(K_T, V_T) *the_data                                                  \
            = (hash_table_slot(K_T, V_T)*)bk_imalloc(data_size);                             \
                                                                                             \
        memset(the_data, 0, data_size);                                                      \
                                                                                             \
        struct _hash_table(K_T, V_T)                                                         \
            init = {._data     = the_data,                                                   \
                    .len       = 0,                                                          \
                    ._size_idx = DEFAULT_START_SIZE_IDX,                                     \
                    ._free     = CAT2(hash_table(K_T, V_T), _free),                          \
                    ._get_key  = CAT2(hash_table(K_T, V_T), _get_key),                       \
                    ._get_val  = CAT2(hash_table(K_T, V_T), _get_val),                       \
                    ._insert   = CAT2(hash_table(K_T, V_T), _insert),                        \
                    ._delete   = CAT2(hash_table(K_T, V_T), _delete),                        \
                    ._hash     = (CAT2(hash_table(K_T, V_T), _hash_t))hash,                  \
                    ._equ      = (CAT2(hash_table(K_T, V_T), _equ_t))equ};                   \
                                                                                             \
        memcpy((void*)t, (void*)&init, sizeof(*t));                                          \
                                                                                             \
        CAT2(hash_table(K_T, V_T), _update_load_thresh)(t);                                  \
                                                                                             \
        return t;                                                                            \
    }                                                                                        \



/******************************* @@config *******************************/

#ifndef BKMALLOC_HOOK

#define BK_SCFG_ERR_NONE          (0)
#define BK_SCFG_ERR_BAD_FILE      (1)
#define BK_SCFG_ERR_BAD_SYNTAX    (2)
#define BK_SCFG_ERR_BAD_KEY       (3)
#define BK_SCFG_ERR_BAD_VAL       (4)
#define BK_SCFG_ERR_VALIDATE      (5)

#define BK_SCFG_KIND_BOOL         (1)
#define BK_SCFG_KIND_INT          (2)
#define BK_SCFG_KIND_FLOAT        (3)
#define BK_SCFG_KIND_STRING       (4)

#define BK_SCFG_ENTRY_REQUIRED    (0x1)
#define BK_SCFG_ENTRY_HAS_DEFAULT (0x2)
#define BK_SCFG_ENTRY_SET         (0x4)

struct bk_scfg;

typedef int (*bk_scfg_validate_bool_fn_t)  (struct bk_scfg*, int);
typedef int (*bk_scfg_validate_int_fn_t)   (struct bk_scfg*, int);
typedef int (*bk_scfg_validate_float_fn_t) (struct bk_scfg*, float);
typedef int (*bk_scfg_validate_string_fn_t)(struct bk_scfg*, const char*);

typedef struct {
    u32 kind;
    u32 flags;
    union {
        u32         *baddr;
        s32         *iaddr;
        float       *faddr;
        const char **saddr;
        void        *_addr;
    };
    union {
        u32         bdef;
        s32         idef;
        float       fdef;
        const char *sdef;
    };
    void *validate;
} bk_scfg_entry_t;

static void bk_scfg_entry_free(bk_scfg_entry_t *entry) {
    if (entry->kind == BK_SCFG_KIND_STRING) {
        if (entry->sdef != NULL) {
            bk_ifree((void*)entry->sdef);
        }
    }
}



use_hash_table(bk_str, bk_scfg_entry_t);

typedef hash_table(bk_str, bk_scfg_entry_t) bk_hash_table_t;


struct bk_scfg {
    bk_hash_table_t  table;
    const char      *err_msg;
    u32              err_kind;
    const char      *valid_err_msg;
};

#define BK_SCFG_SET_ERR(cfg, kind, fmt, ...)                 \
do {                                                         \
    char __bk_scfg_err_buff[512];                            \
                                                             \
    snprintf(__bk_scfg_err_buff, 512, (fmt), ##__VA_ARGS__); \
    if ((cfg)->err_msg) {                                    \
        free((void*)(cfg)->err_msg);                         \
    }                                                        \
    (cfg)->err_msg  = bk_istrdup(__bk_scfg_err_buff);        \
    (cfg)->err_kind = (kind);                                \
} while (0)

static struct bk_scfg* bk_scfg_make(void) {
    struct bk_scfg *cfg;

    cfg = (struct bk_scfg*)bk_imalloc(sizeof(*cfg));
    memset((void*)cfg, 0, sizeof(*cfg));

    cfg->table = hash_table_make_e(bk_str, bk_scfg_entry_t, bk_str_hash, (void*)bk_str_equ);

    return cfg;
}

static void bk_scfg_free(struct bk_scfg *cfg) {
    const char      *key;
    bk_scfg_entry_t *val;

    hash_table_traverse(cfg->table, key, val) {
        bk_scfg_entry_free(val);
        bk_ifree((void*)key);
    }

    hash_table_free(cfg->table);
    bk_ifree(cfg);
}

static void bk_scfg_add_entry(struct bk_scfg *cfg, const char *key, u32 kind, void *addr) {
    bk_scfg_entry_t  new_entry,
                    *entry;

    entry = hash_table_get_val(cfg->table, key);

    if (!entry) {
        memset((void*)&new_entry, 0, sizeof(new_entry));
        new_entry.kind  = kind;
        new_entry._addr = addr;
        hash_table_insert(cfg->table, strdup(key), new_entry);
    } else {
        bk_scfg_entry_free(entry);
        memset((void*)entry, 0, sizeof(*entry));
        entry->kind  = kind;
        entry->_addr = addr;
    }
}

static void bk_scfg_add_bool(struct bk_scfg *cfg, const char *key, u32 *addr) {
    bk_scfg_add_entry(cfg, key, BK_SCFG_KIND_BOOL, addr);
}

static void bk_scfg_add_int(struct bk_scfg *cfg, const char *key, s32 *addr) {
    bk_scfg_add_entry(cfg, key, BK_SCFG_KIND_INT, addr);
}

static void bk_scfg_add_float(struct bk_scfg *cfg, const char *key, float *addr) {
    bk_scfg_add_entry(cfg, key, BK_SCFG_KIND_FLOAT, addr);
}

static void bk_scfg_add_string(struct bk_scfg *cfg, const char *key, const char **addr) {
    bk_scfg_add_entry(cfg, key, BK_SCFG_KIND_STRING, addr);
}


#define GET_ENTRY(_cfg, _entry_ptr, _key, _kind)              \
    ((_entry_ptr) = hash_table_get_val((_cfg)->table, (_key)),\
    ((_entry_ptr) ? (_entry_ptr)->kind == (_kind) : 0))

static void bk_scfg_require(struct bk_scfg *cfg, const char *key) {
    bk_scfg_entry_t *entry;

    entry = hash_table_get_val(cfg->table, key);

    if (entry) {
        entry->flags |= BK_SCFG_ENTRY_REQUIRED;
    }
}


static void bk_scfg_default_bool(struct bk_scfg *cfg, const char *key, u32 def) {
    bk_scfg_entry_t *entry;

    if (!GET_ENTRY(cfg, entry, key, BK_SCFG_KIND_BOOL)) { return; }

    entry->bdef   = !!def;
    entry->flags |= BK_SCFG_ENTRY_HAS_DEFAULT;
}

static void bk_scfg_default_int(struct bk_scfg *cfg, const char *key, s32 def) {
    bk_scfg_entry_t *entry;

    if (!GET_ENTRY(cfg, entry, key, BK_SCFG_KIND_INT)) { return; }

    entry->idef   = def;
    entry->flags |= BK_SCFG_ENTRY_HAS_DEFAULT;
}

static void bk_scfg_default_float(struct bk_scfg *cfg, const char *key, float def) {
    bk_scfg_entry_t *entry;

    if (!GET_ENTRY(cfg, entry, key, BK_SCFG_KIND_FLOAT)) { return; }

    entry->fdef   = def;
    entry->flags |= BK_SCFG_ENTRY_HAS_DEFAULT;
}

static void bk_scfg_default_string(struct bk_scfg *cfg, const char *key, const char *def) {
    bk_scfg_entry_t *entry;

    if (!GET_ENTRY(cfg, entry, key, BK_SCFG_KIND_STRING)) { return; }

    entry->sdef   = bk_istrdup(def);
    entry->flags |= BK_SCFG_ENTRY_HAS_DEFAULT;
}

static void bk_scfg_validate_bool(struct bk_scfg *cfg, const char *key, bk_scfg_validate_bool_fn_t validate) {
    bk_scfg_entry_t *entry;

    if (!GET_ENTRY(cfg, entry, key, BK_SCFG_KIND_BOOL)) { return; }

    entry->validate = (void*)validate;
}

static void bk_scfg_validate_int(struct bk_scfg *cfg, const char *key, bk_scfg_validate_int_fn_t validate) {
    bk_scfg_entry_t *entry;

    if (!GET_ENTRY(cfg, entry, key, BK_SCFG_KIND_INT)) { return; }

    entry->validate = (void*)validate;
}

static void bk_scfg_validate_float(struct bk_scfg *cfg, const char *key, bk_scfg_validate_float_fn_t validate) {
    bk_scfg_entry_t *entry;

    if (!GET_ENTRY(cfg, entry, key, BK_SCFG_KIND_FLOAT)) { return; }

    entry->validate = (void*)validate;
}

static void bk_scfg_validate_string(struct bk_scfg *cfg, const char *key, bk_scfg_validate_string_fn_t validate) {
    bk_scfg_entry_t *entry;

    if (!GET_ENTRY(cfg, entry, key, BK_SCFG_KIND_STRING)) { return; }

    entry->validate = (void*)validate;
}

#undef GET_ENTRY


static void bk_scfg_validate_set_err(struct bk_scfg *cfg, const char *msg) {
    if (cfg->valid_err_msg) {
        bk_ifree((void*)cfg->valid_err_msg);
    }
    cfg->valid_err_msg = bk_istrdup(msg);
}

static int bk_sh_split(const char *s, char **words) {
    int      n;
    char    *copy,
            *sub,
            *sub_p;
    char     c, prev;
    int      len,
             start,
             end,
             q,
             sub_len,
             i;

#define SH_PUSH(word) \
do {                  \
    words[n]  = word; \
    n        += 1;    \
} while (0)

    n     = 0;
    copy  = bk_istrdup(s);
    len   = strlen(copy);
    start = 0;
    end   = 0;
    prev  = 0;

    while (start < len && bk_is_space(copy[start])) { start += 1; }

    while (start < len) {
        c   = copy[start];
        q   = 0;
        end = start;

        if (c == '#' && prev != '\\') {
            break;
        } else if (c == '"') {
            start += 1;
            prev   = copy[end];
            while (end + 1 < len
            &&    (copy[end + 1] != '"' || prev == '\\')) {
                end += 1;
                prev = copy[end];
            }
            q = 1;
        } else if (c == '\'') {
            start += 1;
            prev   = copy[end];
            while (end + 1 < len
            &&    (copy[end + 1] != '\'' || prev == '\\')) {
                end += 1;
                prev = copy[end];
            }
            q = 1;
        } else {
            while (end + 1 < len
            &&     !bk_is_space(copy[end + 1])) {
                end += 1;
            }
        }

        sub_len = end - start + 1;
        if (q && sub_len == 0 && start == len) {
            sub    = (char*)bk_imalloc(2);
            sub[0] = copy[end];
            sub[1] = 0;
        } else {
            sub   = (char*)bk_imalloc(sub_len + 1);
            sub_p = sub;
            for (i = 0; i < sub_len; i += 1) {
                c = copy[start + i];
                if (c == '\\'
                &&  i < sub_len - 1
                &&  (copy[start + i + 1] == '"'
                || copy[start + i + 1] == '\''
                || copy[start + i + 1] == '#')) {
                    continue;
                }
                *sub_p = c;
                sub_p += 1;
            }
            *sub_p = 0;
        }

        SH_PUSH(sub);

        end  += q;
        start = end + 1;

        while (start < len && bk_is_space(copy[start])) { start += 1; }
    }

    bk_ifree(copy);

    return n;

#undef SH_PUSH
}

static void bk_scfg_validate_entry(struct bk_scfg *cfg, bk_scfg_entry_t *entry, const char *key, const char *path, int line_nr) {
    int valid;

    bk_scfg_validate_set_err(cfg, "failed validation");

    if (entry->validate == NULL) {
        return;
    }

    valid = 0;

    switch (entry->kind) {
        case BK_SCFG_KIND_BOOL:
            valid = ((bk_scfg_validate_bool_fn_t)entry->validate)(cfg, *entry->baddr);
            break;
        case BK_SCFG_KIND_INT:
            valid = ((bk_scfg_validate_int_fn_t)entry->validate)(cfg, *entry->iaddr);
            break;
        case BK_SCFG_KIND_FLOAT:
            valid = ((bk_scfg_validate_float_fn_t)entry->validate)(cfg, *entry->faddr);
            break;
        case BK_SCFG_KIND_STRING:
            valid = ((bk_scfg_validate_string_fn_t)entry->validate)(cfg, *entry->saddr);
            break;
    }

    if (!valid) {
        BK_SCFG_SET_ERR(cfg, BK_SCFG_ERR_VALIDATE, "%s line %d: option '%s': %s", path, line_nr, key, cfg->valid_err_msg);
    }
}

static void bk_scfg_parse_bool(struct bk_scfg *cfg, bk_scfg_entry_t *entry, char *key, const char *path, int line_nr, char *val) {
    if (strlen(val) == 0) {
        *entry->baddr = 1;
    } else
    if (strcmp(val, "0")     == 0
    ||  strcmp(val, "f")     == 0
    ||  strcmp(val, "F")     == 0
    ||  strcmp(val, "false") == 0
    ||  strcmp(val, "False") == 0
    ||  strcmp(val, "FALSE") == 0
    ||  strcmp(val, "off")   == 0
    ||  strcmp(val, "Off")   == 0
    ||  strcmp(val, "OFF")   == 0
    ||  strcmp(val, "n")     == 0
    ||  strcmp(val, "N")     == 0
    ||  strcmp(val, "no")    == 0
    ||  strcmp(val, "No")    == 0
    ||  strcmp(val, "NO")    == 0) {
        *entry->baddr = 0;
    } else
    if (strcmp(val, "1")    == 0
    ||  strcmp(val, "t")    == 0
    ||  strcmp(val, "T")    == 0
    ||  strcmp(val, "true") == 0
    ||  strcmp(val, "True") == 0
    ||  strcmp(val, "TRUE") == 0
    ||  strcmp(val, "on")   == 0
    ||  strcmp(val, "On")   == 0
    ||  strcmp(val, "ON")   == 0
    ||  strcmp(val, "y")    == 0
    ||  strcmp(val, "Y")    == 0
    ||  strcmp(val, "yes")  == 0
    ||  strcmp(val, "Yes")  == 0
    ||  strcmp(val, "YES")  == 0) {
        *entry->baddr = 1;
    } else {
        BK_SCFG_SET_ERR(cfg, BK_SCFG_ERR_BAD_VAL, "%s line %d: option '%s': could not parse bool from '%s'", path, line_nr, key, val);
    }
}

static void bk_scfg_parse_int(struct bk_scfg *cfg, bk_scfg_entry_t *entry, char *key, const char *path, int line_nr, char *val) {
    if (!sscanf(val, "%d", entry->iaddr)) {
        BK_SCFG_SET_ERR(cfg, BK_SCFG_ERR_BAD_VAL, "%s line %d: option '%s': could not parse int from '%s'", path, line_nr, key, val);
    }
}

static void bk_scfg_parse_float(struct bk_scfg *cfg, bk_scfg_entry_t *entry, char *key, const char *path, int line_nr, char *val) {
    if (!sscanf(val, "%f", entry->faddr)) {
        BK_SCFG_SET_ERR(cfg, BK_SCFG_ERR_BAD_VAL, "%s line %d: option '%s': could not parse float from '%s'", path, line_nr, key, val);
    }
}

static void bk_scfg_parse_line(struct bk_scfg *cfg, const char *path, char *line, int line_nr) {
    char            *words[64];
    int              n, i;
    char            *key;
    bk_scfg_entry_t *entry;
    char            *val;

    n = bk_sh_split(line, words);

    if (n == 0) { goto out; }
    if (n >= 3) {
        BK_SCFG_SET_ERR(cfg, BK_SCFG_ERR_BAD_SYNTAX,
                     "%s line %d: unexpected extra token%s ('%s'...) after '%s'",
                     path, line_nr,
                     n == 3 ? "" : "s",
                     words[2],
                     words[1]);
        goto out;
    }

    key   = words[0];
    entry = hash_table_get_val(cfg->table, key);

    if (!entry) {
        BK_SCFG_SET_ERR(cfg, BK_SCFG_ERR_BAD_KEY, "%s line %d: '%s' is not a configurable option", path, line_nr, key);
        goto out;
    }

    if (n == 1) {
        if (entry->kind != BK_SCFG_KIND_BOOL) {
            BK_SCFG_SET_ERR(cfg, BK_SCFG_ERR_BAD_VAL, "%s line %d: missing value for option '%s'", path, line_nr, key);
            goto out;
        }

        *entry->baddr = 1;
    } else {
        val = words[1];

        switch (entry->kind) {
            case BK_SCFG_KIND_BOOL:
                bk_scfg_parse_bool(cfg, entry, key, path, line_nr, val);
                break;
            case BK_SCFG_KIND_INT:
                bk_scfg_parse_int(cfg, entry, key, path, line_nr, val);
                break;
            case BK_SCFG_KIND_FLOAT:
                bk_scfg_parse_float(cfg, entry, key, path, line_nr, val);
                break;
            case BK_SCFG_KIND_STRING:
                *entry->saddr = strdup(val);
                break;
        }
    }

    if (cfg->err_kind == BK_SCFG_ERR_NONE) {
        entry->flags |= BK_SCFG_ENTRY_SET;
        bk_scfg_validate_entry(cfg, entry, key, path, line_nr);
    }

out:
    for (i = 0; i < n; i += 1) {
        bk_ifree(words[i]);
    }
}

static void bk_scfg_apply_default(struct bk_scfg *cfg, bk_scfg_entry_t *entry) {
    switch (entry->kind) {
        case BK_SCFG_KIND_BOOL:   *entry->baddr = entry->bdef; break;
        case BK_SCFG_KIND_INT:    *entry->iaddr = entry->idef; break;
        case BK_SCFG_KIND_FLOAT:  *entry->faddr = entry->fdef; break;
        case BK_SCFG_KIND_STRING:
            *entry->saddr = entry->sdef;
            entry->sdef   = NULL;
            break;
    }
    entry->flags |= BK_SCFG_ENTRY_SET;
}

static int bk_scfg_parse(struct bk_scfg *cfg, const char *path, int require_file) {
    FILE         *f;
    int           line_nr;
    char          line[512];
    const char   *key;
    bk_scfg_entry_t *entry;

    f = fopen(path, "r");

    if (f != NULL) {
        line_nr = 0;
        while (fgets(line, sizeof(line), f)) {
            line_nr += 1;

            bk_scfg_parse_line(cfg, path, line, line_nr);
            if (cfg->err_kind != BK_SCFG_ERR_NONE) { break; }
        }

        fclose(f);
    } else if (require_file) {
        BK_SCFG_SET_ERR(cfg, BK_SCFG_ERR_BAD_FILE, "unable to fopen path '%s'", path);
        return cfg->err_kind;
    }

    if (cfg->err_kind == BK_SCFG_ERR_NONE) {
        hash_table_traverse(cfg->table, key, entry) {
            if (!(entry->flags & BK_SCFG_ENTRY_SET)) {
                if (entry->flags & BK_SCFG_ENTRY_REQUIRED) {
                    BK_SCFG_SET_ERR(cfg, BK_SCFG_ERR_BAD_FILE, "required option '%s' not set", key);
                    return cfg->err_kind;
                } else if (entry->flags & BK_SCFG_ENTRY_HAS_DEFAULT) {
                    bk_scfg_apply_default(cfg, entry);
                    bk_scfg_validate_entry(cfg, entry, key, "<default value>", 0);
                }
            }
        }
    }

    return cfg->err_kind;
}

static int bk_scfg_parse_env(struct bk_scfg *cfg, int argc, const char **argv) {
    int              i;
    const char      *option;
    char             option_name[512];
    int              option_len;
    const char      *val;
    char             fake_line[1024];
    const char      *key;
    bk_scfg_entry_t *entry;

    for (i = 0; i < argc; i += 1) {
        option = argv[i];
        if (strlen(option) < 2) {
            BK_SCFG_SET_ERR(cfg,
                         BK_SCFG_ERR_BAD_SYNTAX,
                         "invalid env syntax '%s': expected '--'",
                         option);
            goto out;
        }
        option += 2;

        option_len = 0;
        while (option[option_len] && option[option_len] != '=') {
            option_name[option_len]  = option[option_len];
            option_len              += 1;
        }
        option_name[option_len]  = 0;

        if (option_len == 0) {
            BK_SCFG_SET_ERR(cfg,
                         BK_SCFG_ERR_BAD_SYNTAX,
                         "invalid env syntax '%s': expected option name after '--'",
                         option - 2);
            goto out;
        }


        if (option[option_len] == '=') {
            val = option + option_len + 1;
            if (strlen(val) == 0) {
                BK_SCFG_SET_ERR(cfg,
                            BK_SCFG_ERR_BAD_SYNTAX,
                            "invalid env syntax '%s': expected value after '='",
                            option - 2);
                goto out;
            }
        } else {
            val = option + option_len;
        }

        snprintf(fake_line, sizeof(fake_line), "%s %s", option_name, val);

        bk_scfg_parse_line(cfg, "<env>", fake_line, 1);
        if (cfg->err_kind != BK_SCFG_ERR_NONE) { goto out; }
    }

    if (cfg->err_kind == BK_SCFG_ERR_NONE) {
        hash_table_traverse(cfg->table, key, entry) {
            if (!(entry->flags & BK_SCFG_ENTRY_SET)) {
                if (entry->flags & BK_SCFG_ENTRY_REQUIRED) {
                    BK_SCFG_SET_ERR(cfg, BK_SCFG_ERR_BAD_FILE, "<env>: required option '%s' not set", key);
                    return cfg->err_kind;
                } else if (entry->flags & BK_SCFG_ENTRY_HAS_DEFAULT) {
                    bk_scfg_apply_default(cfg, entry);
                }
            }
        }
    }

out:;
    return cfg->err_kind;
}

static const char *bk_scfg_err_msg(struct bk_scfg *cfg) {
    if (cfg->err_msg) {
        return cfg->err_msg;
    }
    return "";
}

#endif /* BKMALLOC_HOOK */


#define LIST_GC_POLICIES(X) \
    X(never)                \
    X(free_threshold)       \
    X(failure)

#define E(pol) BK_GC_POL_##pol,
enum { LIST_GC_POLICIES(E) BK_NUM_GC_POLICIES };
#undef E


typedef struct {
    struct bk_scfg *_scfg;

/* LOGGING */
    const char *log_to;
    int         log_fd;
    u32         log_config;
    u32         log_allocs;
    u32         log_frees;
    u32         log_big;
    u32         log_blocks;
    u32         log_heaps;
    u32         log_gc;
    u32         log_hooks;
    u32         log_all;
/* METHODS */
    u32         disable_bump;
    u32         disable_slots;
    u32         disable_chunks;
/* HEAPS */
    u32         per_thread_heaps;
    u32         main_thread_use_global;
/* BLOCKS */
    u32         disable_block_reuse;
    u32         trim_reuse_blocks;
    s32         max_reuse_list_size_MB;
/* GARBAGE COLLECTION */
    const char *_gc_policy;
    u32         gc_policy;
    s32         gc_free_threshold;
    s32         gc_decommit_reuse_after_passes;
    s32         gc_release_reuse_after_passes;
/* HOOKS */
    const char *hooks_file;
    u32         disable_hooks;
} bk_Config;

#ifndef BKMALLOC_HOOK

static bk_Config bk_config;

enum {
    BK_OPT_BOOL,
    BK_OPT_INT,
    BK_OPT_STRING,
};

enum {
    BK_OPT_LOGGING,
    BK_OPT_HEAPS,
    BK_OPT_BLOCKS,
    BK_OPT_METHODS,
    BK_OPT_GARBAGE_COLLECTION,
    BK_OPT_HOOKS,
    BK_NR_OPT_CATEGORIES,
};

static const char *bk_opt_category_strings[] = {
    "LOGGING",
    "HEAPS",
    "BLOCKS",
    "METHODS",
    "GARBAGE COLLECTION",
    "HOOKS",
};

typedef struct {
    char *name;
    u32   type;
    u32   category;
    char *desc;
    void *ptr;
} bk_opt;

#define BK_MAX_OPTS (32)

static bk_opt bk_options[BK_MAX_OPTS];
static u32    bk_n_options;

static void bk_option(const char *name, u32 type, u32 category, const char *desc, void *ptr) {
    bk_opt *new_opt;

    new_opt       = &bk_options[bk_n_options];
    bk_n_options += 1;

    new_opt->name     = bk_istrdup(name);
    new_opt->type     = type;
    new_opt->category = category;
    new_opt->desc     = bk_istrdup(desc);
    new_opt->ptr      = ptr;

    switch (type) {
        case BK_OPT_BOOL:
            bk_scfg_add_bool(bk_config._scfg, name, (u32*)ptr);           break;
        case BK_OPT_INT:
            bk_scfg_add_int(bk_config._scfg, name, (s32*)ptr);            break;
        case BK_OPT_STRING:
            bk_scfg_add_string(bk_config._scfg, name, (const char**)ptr); break;
    }
}

static void bk_opt_require(const char *name)                                         { bk_scfg_require(bk_config._scfg, name);             }
static void bk_opt_default_bool(const char *name, int val)                           { bk_scfg_default_bool(bk_config._scfg, name, val);   }
static void bk_opt_default_int(const char *name, int val)                            { bk_scfg_default_int(bk_config._scfg, name, val);    }
static void bk_opt_default_string(const char *name, const char *val)                 { bk_scfg_default_string(bk_config._scfg, name, val); }
static void bk_opt_validate_bool(const char *name, bk_scfg_validate_bool_fn_t v)     { bk_scfg_validate_bool(bk_config._scfg, name, v);    }
static void bk_opt_validate_int(const char *name, bk_scfg_validate_int_fn_t v)       { bk_scfg_validate_int(bk_config._scfg, name, v);     }
static void bk_opt_validate_string(const char *name, bk_scfg_validate_string_fn_t v) { bk_scfg_validate_string(bk_config._scfg, name, v);  }


void bk_log_config(void) {
    size_t  max_name_len;
    bk_opt *o;
    u32     i;
    u32     j;

    max_name_len = 0;
    for (i = 0; i < bk_n_options; i += 1) {
        o = &bk_options[i];
        if (strlen(o->name) > max_name_len) {
            max_name_len = strlen(o->name);
        }
    }

    bk_logf("================================ BKMALLOC CONFIG ================================\n");
    for (i = 0; i < BK_NR_OPT_CATEGORIES; i += 1) {
        bk_logf("%s:\n", bk_opt_category_strings[i]);
        for (j = 0; j < bk_n_options; j += 1) {
            o = &bk_options[j];
            if (o->category == i) {
                switch (o->type) {
                    case BK_OPT_BOOL:
                        bk_logf("  %-*s  %s\n", (int)max_name_len, o->name, *(int*)o->ptr ? "YES" : "NO");
                        break;
                    case BK_OPT_INT:
                        bk_logf("  %-*s  %d\n", (int)max_name_len, o->name, *(int*)o->ptr);
                        break;
                    case BK_OPT_STRING:
                        if (*(char**)o->ptr == NULL) {
                            bk_logf("  %-*s  NULL\n", (int)max_name_len, o->name);
                        } else {
                            bk_logf("  %-*s  '%s'\n", (int)max_name_len, o->name, *(char**)o->ptr);
                        }
                        break;
                }
            }
        }
    }
    bk_logf("=================================================================================\n\n");
}

static int bk_validate_gc_policy_opt(struct bk_scfg *cfg, const char *val) {
    char        err_buff[1024];
    const char *lazy_comma;
    char        val_buff[256];
    int         i;

    err_buff[0] = 0;
    strcpy(err_buff, "invalid gc-policy -- options are: ");

    lazy_comma = "";

#define X(_pol)                                                     \
strcpy(val_buff, #_pol);                                            \
for (i = 0; i < (int)strlen(val_buff); i += 1) {                    \
    if (val_buff[i] == '_') { val_buff[i] = '-'; }                  \
}                                                                   \
if (strcmp(val, val_buff) == 0) {                                   \
    bk_config.gc_policy = BK_GC_POL_##_pol;                         \
    return 1;                                                       \
}                                                                   \
sprintf(err_buff + strlen(err_buff), "%s%s", lazy_comma, val_buff); \
lazy_comma = ", ";

    LIST_GC_POLICIES(X)
#undef X

    bk_scfg_validate_set_err(cfg, err_buff);

    return 0;

}

#undef LIST_GC_POLICIES

static int bk_init_config(void) {
    int         status;
    const char *config_file;
    const char *env_opts;
    char       *words[2 * BK_MAX_OPTS];
    int         n_words;

    status          = 0;
    bk_config._scfg = bk_scfg_make();

/******************************* @@@options *******************************/

/* LOGGING */
    bk_option("log-to",                          BK_OPT_STRING, BK_OPT_LOGGING,
                                                 "Path to log file.",
                                                 &bk_config.log_to);
                                                 bk_opt_default_string("log-to", "/dev/stdout");
    bk_option("log-config",                      BK_OPT_BOOL, BK_OPT_LOGGING,
                                                 "Print the configuration to the log file.",
                                                 &bk_config.log_config);
                                                 bk_opt_default_bool("log-config", 0);
    bk_option("log-allocs",                      BK_OPT_BOOL, BK_OPT_LOGGING,
                                                 "Log allocations.",
                                                 &bk_config.log_allocs);
                                                 bk_opt_default_bool("log-allocs", 0);
    bk_option("log-frees",                       BK_OPT_BOOL, BK_OPT_LOGGING,
                                                 "Log frees.",
                                                 &bk_config.log_frees);
                                                 bk_opt_default_bool("log-frees", 0);
    bk_option("log-big",                         BK_OPT_BOOL, BK_OPT_LOGGING,
                                                 "Log allocations and frees of big objects (need their own block).",
                                                 &bk_config.log_big);
                                                 bk_opt_default_bool("log-big", 0);
    bk_option("log-blocks",                      BK_OPT_BOOL, BK_OPT_LOGGING,
                                                 "Log block allocation and release events.",
                                                 &bk_config.log_blocks);
                                                 bk_opt_default_bool("log-blocks", 0);
    bk_option("log-heaps",                       BK_OPT_BOOL, BK_OPT_LOGGING,
                                                 "Log heap creation events.",
                                                 &bk_config.log_heaps);
                                                 bk_opt_default_bool("log-heaps", 0);
    bk_option("log-gc",                          BK_OPT_BOOL, BK_OPT_LOGGING,
                                                 "Log GC events.",
                                                 &bk_config.log_gc);
                                                 bk_opt_default_bool("log-gc", 0);
    bk_option("log-hooks",                       BK_OPT_BOOL, BK_OPT_LOGGING,
                                                 "Log when hooks are loaded.",
                                                 &bk_config.log_hooks);
                                                 bk_opt_default_bool("log-hooks", 0);
    bk_option("log-all",                         BK_OPT_BOOL, BK_OPT_LOGGING,
                                                 "Log everything.",
                                                 &bk_config.log_all);
                                                 bk_opt_default_bool("log-all", 0);
/* METHODS */
    bk_option("disable-bump",                    BK_OPT_BOOL, BK_OPT_METHODS,
                                                 "Disable allocation from a block's bump space.",
                                                 &bk_config.disable_bump);
                                                 bk_opt_default_bool("disable-bump", 0);
    bk_option("disable-slots",                   BK_OPT_BOOL, BK_OPT_METHODS,
                                                 "Disable allocation from a block's slots.",
                                                 &bk_config.disable_slots);
                                                 bk_opt_default_bool("disable-slots", 0);
    bk_option("disable-chunks",                  BK_OPT_BOOL, BK_OPT_METHODS,
                                                 "Disable allocation from a block's chunk list.",
                                                 &bk_config.disable_chunks);
                                                 bk_opt_default_bool("disable-chunks", 0);
/* HEAPS */
    bk_option("per-thread-heaps",                BK_OPT_BOOL, BK_OPT_HEAPS,
                                                 "Use per-(hardware)thread heaps to limit lock contention.",
                                                 &bk_config.per_thread_heaps);
                                                 bk_opt_default_bool("per-thread-heaps", 1);
    bk_option("main-thread-use-global",          BK_OPT_BOOL, BK_OPT_HEAPS,
                                                 "When set, the main thread uses the global heap instead of creating a new thread-specific one."
                                                 " Other threads create new heaps as normal.",
                                                 &bk_config.main_thread_use_global);
                                                 bk_opt_default_bool("main-thread-use-global", 1);
/* BLOCKS */
    bk_option("disable-block-reuse",             BK_OPT_BOOL, BK_OPT_BLOCKS,
                                                 "Do not keep unused blocks on a list for later reuse.",
                                                 &bk_config.disable_block_reuse);
                                                 bk_opt_default_bool("disable-block-reuse", 0);
    bk_option("max-reuse-list-size-MB",          BK_OPT_INT, BK_OPT_BLOCKS,
                                                 "The total allowed size (across all heaps) that can be occupied by blocks on a reuse list.",
                                                 &bk_config.max_reuse_list_size_MB);
                                                 bk_opt_default_int("max-reuse-list-size-MB", 4000);
    bk_option("trim-reuse-blocks",               BK_OPT_BOOL, BK_OPT_BLOCKS,
                                                 "If a block is selected for reuse, but is larger than the request, decommit unneeded pages.",
                                                 &bk_config.trim_reuse_blocks);
                                                 bk_opt_default_bool("trim-reuse-blocks", 1);
/* GARBAGE COLLECTION */
    bk_option("gc-policy",                       BK_OPT_STRING, BK_OPT_GARBAGE_COLLECTION,
                                                 "When and how to perform garbage collection on blocks.",
                                                 &bk_config._gc_policy);
                                                 bk_opt_default_string("gc-policy", "failure");
                                                 bk_opt_validate_string("gc-policy", bk_validate_gc_policy_opt);
    bk_option("gc-free-threshold",               BK_OPT_INT, BK_OPT_GARBAGE_COLLECTION,
                                                 "Number of frees to a heap before GC is triggered for that heap.",
                                                 &bk_config.gc_free_threshold);
                                                 bk_opt_default_int("gc-free-threshold", 256);
    bk_option("gc-decommit-reuse-after-passes",  BK_OPT_INT, BK_OPT_GARBAGE_COLLECTION,
                                                 "Decommit pages of a block on a reuse list if it has gone unselected this number of times.",
                                                 &bk_config.gc_decommit_reuse_after_passes);
                                                 bk_opt_default_int("gc-decommit-reuse-after-passes", 1000);
    bk_option("gc-release-reuse-after-passes",   BK_OPT_INT, BK_OPT_GARBAGE_COLLECTION,
                                                 "Release a block on a reuse list if it has gone unselected this number of times.",
                                                 &bk_config.gc_release_reuse_after_passes);
                                                 bk_opt_default_int("gc-release-reuse-after-passes", 1200);
/* HOOKS */
    bk_option("hooks-file",                      BK_OPT_STRING, BK_OPT_HOOKS,
                                                 "Path to a binary file containing hooks.",
                                                 &bk_config.hooks_file);
    bk_option("disable-hooks",                   BK_OPT_BOOL, BK_OPT_HOOKS,
                                                 "Do not load hooks.",
                                                 &bk_config.disable_hooks);
                                                 bk_opt_default_bool("disable-hooks", 0);



    config_file = getenv("BKMALLOC_CONFIG");
    if (config_file == NULL) { config_file = "bkmalloc.config"; }
    status = bk_scfg_parse(bk_config._scfg, config_file, 0);

    if (status == BK_SCFG_ERR_NONE) {
        env_opts = getenv("BKMALLOC_OPTS");
        if (env_opts != NULL) {
            n_words       = bk_sh_split(env_opts, words);
            status        = bk_scfg_parse_env(bk_config._scfg,
                                              n_words,
                                              (const char**)(words));
        }
    }

    if (status != BK_SCFG_ERR_NONE) {
        bk_printf("[bkmalloc]: %s\n", bk_scfg_err_msg(bk_config._scfg));
        goto out;
    }

    bk_config.log_fd = open(bk_config.log_to, O_WRONLY | O_CREAT | O_TRUNC, S_IRUSR | S_IWUSR);
    if (bk_config.log_fd == -1) {
        bk_printf("[bkmalloc]: unable to open log file (%s) -- errno = %d\n", bk_config.log_to, errno);
        bk_config.log_fd = 0;
        errno             = 0;
        status            = -1;
    }

    if (bk_config.log_all) {
        bk_config.log_allocs = 1;
        bk_config.log_frees  = 1;
        bk_config.log_blocks = 1;
        bk_config.log_heaps  = 1;
        bk_config.log_gc     = 1;
        bk_config.log_hooks  = 1;
    }

    if (status == 0 && bk_config.log_config) { bk_log_config(); }

out:;
    return status;
}


#endif /* BKMALLOC_HOOK */

/******************************* @@hooks *******************************/

#ifndef BKMALLOC_HOOK

union bk_Block;

typedef struct {
    void *handle;
    void (*block_new)(struct bk_Heap*, union bk_Block*);           /* ARGS: heap in, block in                                                    */
    void (*block_release)(struct bk_Heap*, union bk_Block*);       /* ARGS: heap in, block in                                                    */
    void (*pre_alloc)(struct bk_Heap**, u64*, u64*, int*);         /* ARGS: heap inout, n_bytes inout, alignment inout, zero_mem inout           */
    void (*post_alloc)(struct bk_Heap*, u64, u64, int, void*);     /* ARGS: heap in, n_bytes in, alignment in, zero_mem in, address in           */
    void (*pre_free)(struct bk_Heap*, void*);                      /* ARGS: heap in, address in                                                  */
#ifdef BK_MMAP_OVERRIDE
    void (*post_mmap)(void*, size_t, int, int, int, off_t, void*); /* ARGS: addr in, length in, prot in, flags in, fd in, offset in, ret_addr in */
    void (*post_munmap)(void*, size_t);                            /* ARGS: addr in, length in                                                   */
#endif
    int  unhooked;
} bk_Hooks;

static bk_Hooks bk_hooks;

#define BK_HOOK(_name, ...)          \
do {                                 \
    if (bk_hooks._name != NULL) {    \
        bk_hooks._name(__VA_ARGS__); \
    }                                \
} while (0)

static void bk_init_hooks(void) {
    if (bk_config.disable_hooks) { return; }

    if (bk_config.log_hooks) {
        if (bk_config.hooks_file == NULL) {
            bk_logf("hooks-load built-in\n");
        } else {
            bk_logf("hooks-load %s\n", bk_config.hooks_file);
        }
    }

    bk_hooks.handle = bk_open_library(bk_config.hooks_file ? bk_config.hooks_file : "libbkmalloc.so");

    if (bk_hooks.unhooked) {
        bk_memzero(&bk_hooks, sizeof(bk_hooks));
        return;
    }

    if (bk_hooks.handle == NULL) {
        if (bk_config.log_hooks) {
            bk_logf("error loading %shooks: %s\n", bk_config.hooks_file ? "" : "built-in ", dlerror());
        }
        return;
    }

#define INSTALL_HOOK(_name)                                                                              \
do {                                                                                                     \
    bk_hooks._name = (__typeof(bk_hooks._name))bk_library_symbol(bk_hooks.handle, "bk_" #_name "_hook"); \
    if (bk_config.log_hooks) {                                                                           \
        if (bk_hooks._name != NULL) {                                                                    \
            bk_logf("hooks-install %-13s LOADED\n", #_name);                                             \
        } else {                                                                                         \
            bk_logf("hooks-install %-13s NOT FOUND %s\n", #_name, dlerror());                            \
        }                                                                                                \
    }                                                                                                    \
} while (0)

    INSTALL_HOOK(block_new);
    INSTALL_HOOK(block_release);
    INSTALL_HOOK(pre_alloc);
    INSTALL_HOOK(post_alloc);
    INSTALL_HOOK(pre_free);
#ifdef BK_MMAP_OVERRIDE
    INSTALL_HOOK(post_mmap);
    INSTALL_HOOK(post_munmap);
#endif

#undef INSTALL_HOOK
}

#endif /* BKMALLOC_HOOK */

/******************************* @@blocks *******************************/


#define BK_ADDR_PARENT_BLOCK(addr) \
    ((bk_Block*)(void*)(ALIGN_DOWN(((u64)addr), BK_BLOCK_ALIGN)))

#define BK_BLOCK_GET_HEAP_PTR(b) \
    ((struct bk_Heap*)((b)->meta.heap))


#define BK_CHUNK_IS_FREE   (1ULL << 0ULL)
#define BK_CHUNK_IS_BIG    (1ULL << 1ULL)
#define BK_CHUNK_BEEN_USED (1ULL << 2ULL)
#define BK_HOOK_FLAG_0     (1ULL << 3ULL)
#define BK_HOOK_FLAG_1     (1ULL << 4ULL)
#define BK_HOOK_FLAG_2     (1ULL << 5ULL)
#define BK_HOOK_FLAG_3     (1ULL << 6ULL)
#define BK_HOOK_FLAG_4     (1ULL << 7ULL)
#define BK_HOOK_FLAG_5     (1ULL << 8ULL)
#define BK_HOOK_FLAG_6     (1ULL << 9ULL)
#define BK_HOOK_FLAG_7     (1ULL << 10ULL)
#define BK_HOOK_FLAG_8     (1ULL << 11ULL)
#define BK_HOOK_FLAG_9     (1ULL << 12ULL)
#define BK_HOOK_FLAG_10    (1ULL << 13ULL)

#define BK_CHUNK_MAGIC (0x22)

typedef union {
    struct {
        u64 magic        : 8;
        u64 offset_next  : 20;
        u64 size         : 25;
        u64 flags        : 11;
    };
    struct {
        u64 big_magic    : 8;
        u64 big_size     : 45;
        u64 big_flags    : 11;
    };
} bk_Chunk_Header;

typedef union {
    bk_Chunk_Header header;
    u64             __u64;
} bk_Chunk;

#define BK_CHUNK_NEXT(addr)                                                  \
    ((bk_Chunk*)((unlikely(((bk_Chunk*)(addr))->header.offset_next == 0)) \
        ? NULL                                                            \
        :   ((u8*)(addr))                                                 \
          + (((bk_Chunk*)(addr))->header.offset_next)))

#define BK_CHUNK_NEXT_UNCHECKED(addr)                                        \
    ((bk_Chunk*)(((u8*)(addr))                                            \
        + (((bk_Chunk*)(addr))->header.offset_next)))

#define BK_CHUNK_HAS_PREV(addr) (((bk_Chunk*)(addr))->header.offset_prev != 0)
#define BK_CHUNK_HAS_NEXT(addr) (((bk_Chunk*)(addr))->header.offset_next != 0)

#define BK_CHUNK_USER_MEM(addr) ((void*)(((u8*)(addr)) + sizeof(bk_Chunk)))

#define BK_CHUNK_FROM_USER_MEM(addr) ((bk_Chunk*)(((u8*)(addr)) - sizeof(bk_Chunk)))

#define BK_CHUNK_DISTANCE(a, b) (((u8*)(a)) - (((u8*)(b))))

#define BK_CHUNK_PARENT_BLOCK(addr) \
    BK_ADDR_PARENT_BLOCK(addr)


typedef struct {
    u64 regions_bitfield;
    u64 slots_bitfields[128];
    u32 n_allocations;
    u32 max_allocations;
} bk_Slots;

#define BK_REGION_FULL      (1ULL)
#define BK_SLOT_TAKEN       (1ULL)
#define BK_ALL_REGIONS_FULL (0xFFFFFFFFFFFFFFFF)
#define BK_ALL_SLOTS_TAKEN  (0xFFFFFFFFFFFFFFFF)

#define BK_GET_SLOT_BITFIELD(_slots, _r)      ((_slots).slots_bitfields[(_r) << 1ULL])
#define BK_GET_SLOT_HOOK_BITFIELD(_slots, _r) ((_slots).slots_bitfields[((_r) << 1ULL) + 1])

#define BK_GET_REGION_BIT(_slots, _r) \
    (!!((_slots).regions_bitfield & (1ULL << (63ULL - (_r)))))
#define BK_SET_REGION_BIT_FULL(_slots, _r) \
    ((_slots).regions_bitfield |= (BK_REGION_FULL << (63ULL - (_r))))
#define BK_SET_REGION_BIT_NOT_FULL(_slots, _r) \
    ((_slots).regions_bitfield &= ~(BK_REGION_FULL << (63ULL - (_r))))

#define BK_GET_SLOT_BIT(_slots, _r, _s) \
    (!!(BK_GET_SLOT_BITFIELD((_slots), (_r)) & (1ULL << (63ULL - (_s)))))
#define BK_SET_SLOT_BIT_TAKEN(_slots, _r, _s) \
    (BK_GET_SLOT_BITFIELD((_slots), (_r)) |= (BK_SLOT_TAKEN << (63ULL - (_s))))
#define BK_SET_SLOT_BIT_FREE(_slots, _r, _s) \
    (BK_GET_SLOT_BITFIELD((_slots), (_r)) &= ~(BK_SLOT_TAKEN << (63ULL - (_s))))

#define BK_GET_SLOT_HOOK_BIT(_slots, _r, _s) \
    (!!(BK_GET_SLOT_HOOK_BITFIELD((_slots), (_r)) & (1ULL << (63ULL - (_s)))))
#define BK_SET_SLOT_HOOK_BIT(_slots, _r, _s) \
    (BK_GET_SLOT_HOOK_BITFIELD((_slots), (_r)) |= (1ULL << (63ULL - (_s))))
#define BK_CLEAR_SLOT_HOOK_BIT(_slots, _r, _s) \
    (BK_GET_SLOT_HOOK_BITFIELD((_slots), (_r)) &= ~(1ULL << (63ULL - (_s))))


union bk_Block;

typedef struct BK_PACKED {
    struct bk_Heap *heap;
    void           *end;
    u64             size;
    union bk_Block *next;
    u32             size_class;
    u32             log2_size_class;
    u32             size_class_idx;
    u32             all_chunks_used;
    void           *bump_base;
    void           *bump;
    u32             n_bump;
    u32             n_bump_free;
    u32             zero;
    u32             from_calloc;
    u32             reuse_passes;
    u8              _pad1[4];
} bk_Block_Metadata;

#define BK_CHUNK_SPACE ((2 * BK_MAX_BLOCK_ALLOC_SIZE) - sizeof(bk_Block_Metadata) - sizeof(bk_Slots))
#define BK_SLOT_SPACE  (   BK_BLOCK_SIZE             \
                         - sizeof(bk_Block_Metadata) \
                         - BK_CHUNK_SPACE            \
                         - sizeof(bk_Slots))

typedef union BK_PACKED bk_Block {
    struct BK_PACKED {
        bk_Block_Metadata meta;
        u8                _chunk_space[BK_CHUNK_SPACE];
        bk_Slots          slots;
        u8                _slot_space[BK_SLOT_SPACE];
    } BK_PACKED;
    u8 _bytes[BK_BLOCK_SIZE];
} BK_PACKED bk_Block;

#define BK_BLOCK_FIRST_CHUNK(_block) \
    ((bk_Chunk*)((_block)->_bytes + offsetof(bk_Block, _chunk_space)))



/******************************* @@heaps *******************************/

/*
 * If a heap has neither BK_HEAP_THREAD or BK_HEAP_USER,
 * it is assumed to be the global heap. This allows us to
 * zero initialize the global heap.
 */
enum {
    BK_HEAP_THREAD,
    BK_HEAP_USER,
};

typedef struct bk_Heap {
    u32          flags;
    u32          hid;
    bk_Spinlock  block_list_locks[BK_NR_SIZE_CLASSES];
    bk_Block    *block_lists[BK_NR_SIZE_CLASSES];
    u32          free_count;
    u32          reuse_list_len;
    u64          reuse_list_size;
    bk_Block    *reuse_list;
    bk_Spinlock  reuse_list_lock;
    char        *user_key;
} bk_Heap;


#ifndef BKMALLOC_HOOK

static u32 bk_hid_counter = 1;

static bk_Heap _bk_global_heap;
#if 0 /* Equivalent to this initialization: */

static bk_Heap _bk_global_heap = {
    .flags                 = 0,
    .hid                   = 0,
    .block_list_locks      = { BK_STATIC_SPIN_LOCK_INIT },
    .block_lists           = { NULL },
    .big_list_lock         = BK_STATIC_SPIN_LOCK_INIT,
    .big_list              = NULL,
    .free_count            = 0,
};

#endif

BK_GLOBAL_ALIGN(CACHE_LINE)
static bk_Heap *bk_global_heap = &_bk_global_heap;

#endif /* BKMALLOC_HOOK */


BK_ALWAYS_INLINE
static inline u32 bk_get_size_class_idx(u64 size) {
    u64 shift;
    u32 size_class_idx;

    size = MAX(BK_MIN_ALIGN, size);

    shift          = 64ULL - 1 - CLZ(size);
    size_class_idx = ((size == (1ULL << shift)) ? shift : shift + 1ULL) - LOG2_64BIT(BK_BASE_SIZE_CLASS);

    BK_ASSERT(size_class_idx < BK_NR_SIZE_CLASSES, "invalid size class");

    return size_class_idx;
}

#define LOG2_SIZE_CLASS_FROM_IDX(_idx) ((_idx) + LOG2_64BIT(BK_BASE_SIZE_CLASS))

BK_ALWAYS_INLINE
static inline u64 bk_size_class_idx_to_size(u32 idx) {
    return (1ULL << (LOG2_SIZE_CLASS_FROM_IDX(idx)));
}

BK_ALWAYS_INLINE
static inline int bk_addr_to_region_and_slot(void *addr, u32 *rp, u32 *sp) {
    bk_Block *block;
    u32       idx;

    block = BK_ADDR_PARENT_BLOCK(addr);

    if ((u8*)addr >= block->_slot_space && (u8*)addr < (u8*)block->meta.bump_base) {
        idx = ((u8*)addr - block->_slot_space) >> block->meta.log2_size_class;
        *rp = idx >> 6ULL;
        *sp = idx  & 63ULL;

        return 1;
    }

    return 0;
}

#ifndef BKMALLOC_HOOK

typedef bk_Heap *bk_heap_ptr;
use_hash_table(bk_str, bk_heap_ptr);

static hash_table(bk_str, bk_heap_ptr) bk_user_heaps;
static bk_Spinlock                     bk_user_heaps_lock;

static void bk_init_user_heaps(void) {
    bk_user_heaps = hash_table_make_e(bk_str, bk_heap_ptr, bk_str_hash, (void*)bk_str_equ);
}

BK_ALWAYS_INLINE
static inline void bk_reset_block(bk_Block *block, bk_Heap *heap, u32 size_class_idx, u64 size, u32 is_zero_mem) {
    bk_Chunk *first_chunk;

    bk_memzero((void*)&block->meta, PAGE_SIZE);

    block->meta.heap = heap;
    block->meta.end  = ((u8*)block) + size;
    block->meta.size = size;
    block->meta.bump = NULL;
    block->meta.zero = is_zero_mem;

    block->meta.size_class_idx = size_class_idx;

    if (unlikely(size_class_idx == BK_BIG_ALLOC_SIZE_CLASS_IDX)) {
        block->meta.size_class      = BK_BIG_ALLOC_SIZE_CLASS;
        block->meta.log2_size_class = 0;
    } else {
        block->meta.size_class      = bk_size_class_idx_to_size(size_class_idx);
        block->meta.log2_size_class = LOG2_SIZE_CLASS_FROM_IDX(size_class_idx);

        first_chunk = BK_BLOCK_FIRST_CHUNK(block);
        first_chunk->header.magic       = BK_CHUNK_MAGIC;
        first_chunk->header.offset_next = 0;
        first_chunk->header.size        = BK_CHUNK_SPACE - sizeof(bk_Chunk);
        first_chunk->header.flags       = BK_CHUNK_IS_FREE;
        if (!block->meta.zero) {
            first_chunk->header.flags |= BK_CHUNK_BEEN_USED;
        }

        bk_memzero((void*)&block->slots, sizeof(block->slots));
        block->slots.max_allocations = MIN(BK_SLOT_SPACE >> block->meta.log2_size_class, 4096ULL);

        block->meta.bump_base = block->_slot_space + block->meta.size_class * block->slots.max_allocations;
        block->meta.bump      = block->meta.bump_base;
    }
}

BK_ALWAYS_INLINE
static inline bk_Block * bk_make_block(bk_Heap *heap, u32 size_class_idx, u64 size) {
    bk_Block *block;

    BK_ASSERT(size >= BK_BLOCK_SIZE,
              "size too small for a block");

    if (!IS_ALIGNED(size, PAGE_SIZE)) { size = ALIGN_UP(size, PAGE_SIZE); }

    block = (bk_Block*)bk_get_aligned_pages(size >> LOG2_PAGE_SIZE, BK_BLOCK_ALIGN);

    bk_reset_block(block, heap, size_class_idx, size, 1);

    if (bk_config.log_blocks) {
        if (block->meta.size_class == BK_BIG_ALLOC_SIZE_CLASS) {
            bk_logf("block-alloc 0x%X - 0x%X size_class BIG\n", block, block->meta.end);
        } else {
            bk_logf("block-alloc 0x%X - 0x%X size_class %U\n", block, block->meta.end, block->meta.size_class);
        }
    }

    BK_HOOK(block_new, heap, block);

    return block;
}

BK_ALWAYS_INLINE
static inline void bk_release_block(bk_Block *block) {
    BK_HOOK(block_release, block->meta.heap, block);

    if (bk_config.log_blocks) {
        if (block->meta.size_class == BK_BIG_ALLOC_SIZE_CLASS) {
            bk_logf("block-release 0x%X - 0x%X size_class BIG\n", block, block->meta.end);
        } else {
            bk_logf("block-release 0x%X - 0x%X size_class %U\n", block, block->meta.end, block->meta.size_class);
        }
    }

    bk_release_pages(block, ((u8*)block->meta.end - (u8*)block) >> LOG2_PAGE_SIZE);
}

static u64         bk_reuse_list_bytes;
static bk_Spinlock bk_reuse_list_bytes_lock = BK_STATIC_SPIN_LOCK_INIT;

#ifdef BK_DO_ASSERTIONS
static int bk_verify_reuse_list(bk_Heap *heap) {
    u32       count;
    bk_Block *block;

    BK_ASSERT(heap->reuse_list_lock.val == BK_SPIN_LOCKED,
              "reuse_list_lock not held");

    count = 0;
    block = heap->reuse_list;
    while (block != NULL) {
        count += 1;
        block = block->meta.next;
    }

    if (count != heap->reuse_list_len) {
        BK_ASSERT(0,
                  "reuse_list length does not match reuse_list_len");
        return 0;
    }

    BK_ASSERT(bk_reuse_list_bytes <= MB(bk_config.max_reuse_list_size_MB),
              "block reuse lists are too large");

    return 1;
}
#endif

static inline void bk_init_heap(bk_Heap *heap, u32 kind, const char *user_key) {
    u32 i;

    bk_memzero((void*)heap, sizeof(*heap));

    heap->flags |= 1 << kind;

    for (i = 0; i < BK_NR_SIZE_CLASSES; i += 1) {
        bk_spin_init(&heap->block_list_locks[i]);
    }

    heap->hid = FAA(&bk_hid_counter, 1);

    if (user_key != NULL) {
        heap->user_key = bk_istrdup(user_key);
    }

    if (bk_config.log_heaps) {
        if (heap->user_key != NULL) {
            bk_logf("heap %u kind %s (%s)\n", heap->hid, kind == BK_HEAP_THREAD ? "THREAD" : "USER", heap->user_key);
        } else {
            bk_logf("heap %u kind %s\n", heap->hid, kind == BK_HEAP_THREAD ? "THREAD" : "USER");
        }
    }
}

static inline void bk_free_heap(bk_Heap *heap) {
    u32       i;
    bk_Block *block;
    bk_Block *next;

    for (i = 0; i < BK_NR_SIZE_CLASSES; i += 1) {
        bk_spin_lock(&heap->block_list_locks[i]);

        block = heap->block_lists[i];
        while (block != NULL) {
            next = block->meta.next;
            bk_release_block(block);
            block = next;
        }

        heap->block_lists[i] = NULL;

        bk_spin_unlock(&heap->block_list_locks[i]);
    }

    bk_spin_lock(&heap->reuse_list_lock);

    block = heap->reuse_list;
    while (block != NULL) {
        next = block->meta.next;
        bk_release_block(block);
        block = next;
    }
    heap->reuse_list = NULL;

    if (heap->user_key != NULL) {
        bk_ifree(heap->user_key);
    }

    bk_spin_unlock(&heap->reuse_list_lock);
}

BK_ALWAYS_INLINE
static inline bk_Block * bk_get_block(bk_Heap *heap, u32 size_class_idx, u64 size, int zero_mem) {
    bk_Block *block;
    bk_Block *prev;
    bk_Block *best_fit;
    bk_Block *best_fit_prev;
    bk_Block *next;
    u64       reuse_size;

    bk_spin_lock(&heap->reuse_list_lock);

    block         = heap->reuse_list;
    prev          = NULL;
    best_fit      = NULL;
    best_fit_prev = NULL;

    while (block != NULL) {
        if (block->meta.size >= size) {
            if (best_fit == NULL
            ||  block->meta.size < best_fit->meta.size) {
                best_fit      = block;
                best_fit_prev = prev;

                if (block->meta.size == size) { break; }
            }
        }

        prev  = block;
        block = block->meta.next;
    }

    if (best_fit != NULL) {
        if (best_fit_prev == NULL) {
            heap->reuse_list = best_fit->meta.next;
        } else {
            best_fit_prev->meta.next = best_fit->meta.next;
        }
        heap->reuse_list_len  -= 1;
        heap->reuse_list_size -= best_fit->meta.size;

        bk_spin_lock(&bk_reuse_list_bytes_lock);

        BK_ASSERT(bk_reuse_list_bytes >= best_fit->meta.size, "bad bk_reuse_list_bytes");
        bk_reuse_list_bytes -= best_fit->meta.size;

        bk_spin_unlock(&bk_reuse_list_bytes_lock);

        BK_ASSERT(bk_verify_reuse_list(heap), "bad reuse list");

        block = heap->reuse_list;
        prev  = NULL;

        while (block != NULL) {
            next = block->meta.next;

            block->meta.reuse_passes += 1;

            if (bk_config.gc_policy) {
                if (block->meta.reuse_passes == (u32)bk_config.gc_release_reuse_after_passes) {
                    if (prev == NULL) {
                        heap->reuse_list = next;
                    } else {
                        prev->meta.next = next;
                    }
                    heap->reuse_list_len  -= 1;
                    heap->reuse_list_size -= block->meta.size;

                    reuse_size = block->meta.size;

                    bk_release_block(block);

                    bk_spin_lock(&bk_reuse_list_bytes_lock);

                    BK_ASSERT(bk_reuse_list_bytes >= best_fit->meta.size, "bad bk_reuse_list_bytes");
                    bk_reuse_list_bytes -= reuse_size;

                    bk_spin_unlock(&bk_reuse_list_bytes_lock);

                    block = prev;
                } else if (block->meta.reuse_passes == (u32)bk_config.gc_decommit_reuse_after_passes && !block->meta.zero) {
                    if (bk_config.log_gc) {
                        if (block->meta.size_class == BK_BIG_ALLOC_SIZE_CLASS) {
                            bk_logf("gc-decommit 0x%X - 0x%X size_class BIG\n", block, block->meta.end);
                        } else {
                            bk_logf("gc-decommit 0x%X - 0x%X size_class %U\n", block, block->meta.end, block->meta.size_class);
                        }
                    }

                    bk_decommit_pages((void*)((u8*)block + PAGE_SIZE), (block->meta.size - PAGE_SIZE) / PAGE_SIZE);
                    block->meta.zero = 1;
                }
            }

            prev  = block;
            block = next;
        }


        bk_spin_unlock(&heap->reuse_list_lock);

        bk_reset_block(best_fit, heap, size_class_idx, best_fit->meta.size, best_fit->meta.zero);

        if (zero_mem && !best_fit->meta.zero) {
            bk_decommit_pages((void*)((u8*)best_fit + PAGE_SIZE), (best_fit->meta.size - PAGE_SIZE) / PAGE_SIZE);
        } else if (bk_config.trim_reuse_blocks && best_fit->meta.size != size) {
            bk_decommit_pages((void*)((u8*)best_fit + size), (best_fit->meta.size - size) / PAGE_SIZE);
        }

        if (bk_config.log_blocks) {
            if (best_fit->meta.size_class == BK_BIG_ALLOC_SIZE_CLASS) {
                bk_logf("block-reuse 0x%X - 0x%X size_class BIG\n", best_fit, best_fit->meta.end);
            } else {
                bk_logf("block-reuse 0x%X - 0x%X size_class %U\n", best_fit, best_fit->meta.end, best_fit->meta.size_class);
            }
        }

        block = best_fit;
        goto out;
    }

    bk_spin_unlock(&heap->reuse_list_lock);

    block = bk_make_block(heap, size_class_idx, size);

out:;

    block->meta.from_calloc = zero_mem;

    return block;
}

static inline bk_Block * bk_add_block(bk_Heap *heap, u32 size_class_idx) {
    bk_Block *block;

    BK_ASSERT(heap->block_list_locks[size_class_idx].val == BK_SPIN_LOCKED,
              "lock not held");

    block = bk_get_block(heap, size_class_idx, BK_BLOCK_SIZE, 0);

    block->meta.next                  = heap->block_lists[size_class_idx];
    heap->block_lists[size_class_idx] = block;

    return block;
}

static inline void bk_remove_block(bk_Heap *heap, bk_Block *block) {
    u32       idx;
    bk_Block *b;
    bk_Block *prev;
    int       on_reuse;

    idx = block->meta.size_class_idx;

    if (idx != BK_BIG_ALLOC_SIZE_CLASS_IDX) {
        BK_ASSERT(heap->block_list_locks[idx].val == BK_SPIN_LOCKED,
                "lock not held");

        if (heap->block_lists[idx] == block) {
            heap->block_lists[idx] = block->meta.next;
        } else {
            b = heap->block_lists[idx];
            while (b->meta.next != NULL) {
                if (b->meta.next == block) {
                    b->meta.next = block->meta.next;
                    break;
                }
                b = b->meta.next;
            }
        }
    }

    if (bk_config.disable_block_reuse) {

        BK_ASSERT(heap->reuse_list == NULL,
                  "config says we aren't reusing blocks, but reuse_list != NULL");

        bk_release_block(block);
    } else {
        if (block->meta.from_calloc) {
            bk_decommit_pages((void*)((u8*)block + PAGE_SIZE), (block->meta.size - PAGE_SIZE) / PAGE_SIZE);
            block->meta.zero = 1;
        } else {
            block->meta.zero = 0;
        }


        on_reuse = 0;

        bk_spin_lock(&heap->reuse_list_lock);
        bk_spin_lock(&bk_reuse_list_bytes_lock);

        if (bk_reuse_list_bytes - heap->reuse_list_size + block->meta.size > MB(bk_config.max_reuse_list_size_MB)) {
            /* Too big to reuse. */
            goto out_unlock;
        }

        while (bk_reuse_list_bytes + block->meta.size > MB(bk_config.max_reuse_list_size_MB)) {
            BK_ASSERT(heap->reuse_list != NULL,
                      "heap says there bytes on its reuse list, but the list is empty");
            BK_ASSERT(heap->reuse_list_size <= MB(bk_config.max_reuse_list_size_MB),
                      "probable underflow of heap->reuse_list_size");

            b    = heap->reuse_list;
            prev = NULL;
            while (b->meta.next != NULL) {
                prev = b;
                b    = b->meta.next;
            }

            if (prev == NULL) {
                heap->reuse_list = NULL;
            } else {
                prev->meta.next = NULL;
            }

            heap->reuse_list_len  -= 1;
            heap->reuse_list_size -= b->meta.size;
            bk_reuse_list_bytes   -= b->meta.size;

            bk_release_block(b);
        }

        block->meta.next = heap->reuse_list;
        heap->reuse_list = block;

        heap->reuse_list_len  += 1;
        heap->reuse_list_size += block->meta.size;
        bk_reuse_list_bytes   += block->meta.size;

        on_reuse = 1;

        BK_ASSERT(bk_verify_reuse_list(heap), "bad reuse list");

out_unlock:;
        bk_spin_unlock(&bk_reuse_list_bytes_lock);
        bk_spin_unlock(&heap->reuse_list_lock);

        if (!on_reuse) { bk_release_block(block); }

        BK_ASSERT(bk_reuse_list_bytes <= MB(bk_config.max_reuse_list_size_MB),
                  "reuse lists too large");
    }
}

static inline void bk_gc_block(bk_Heap *heap, bk_Block *block) {
    int       chunks_used;
    bk_Chunk *chunk;
    bk_Chunk *next;
    bk_Chunk *next_next;

    BK_ASSERT(heap->block_list_locks[block->meta.size_class_idx].val == BK_SPIN_LOCKED,
              "lock not held");

    if (bk_config.log_gc) {
        if (block->meta.size_class == BK_BIG_ALLOC_SIZE_CLASS) {
            bk_logf("gc-block 0x%X - 0x%X size_class BIG\n", block, block->meta.end);
        } else {
            bk_logf("gc-block 0x%X - 0x%X size_class %U\n", block, block->meta.end, block->meta.size_class);
        }
    }

    chunks_used = 0;
    chunk       = BK_BLOCK_FIRST_CHUNK(block);

    BK_ASSERT(chunk->header.magic == BK_CHUNK_MAGIC,
              "chunk bad magic");

    while (BK_CHUNK_HAS_NEXT(chunk)) {
        next = BK_CHUNK_NEXT(chunk);

        BK_ASSERT(next->header.magic == BK_CHUNK_MAGIC,
                  "next bad magic");

        if (chunk->header.flags & next->header.flags & BK_CHUNK_IS_FREE) {
            next_next = BK_CHUNK_NEXT(next);

            BK_ASSERT(next->header.magic == BK_CHUNK_MAGIC,
                      "next_next bad magic");

            chunk->header.size = BK_CHUNK_DISTANCE(next, chunk) + next->header.size;

            if (next_next == NULL) {
                chunk->header.offset_next = 0;
            } else {
                chunk->header.offset_next = BK_CHUNK_DISTANCE(next_next, chunk);
            }

            BK_ASSERT((u8*)chunk + sizeof(bk_Chunk) + chunk->header.size <= block->_chunk_space + BK_CHUNK_SPACE,
                      "coalesce made chunk too large");
        } else {
            chunks_used = 1;
            chunk       = next;
        }

        chunk = next;
    }

    if (!(chunk->header.flags & BK_CHUNK_IS_FREE)) {
        chunks_used = 1;
    }

    /* Can we release this block? */
    if (!chunks_used
    &&  block->slots.n_allocations == 0
    &&  block->meta.n_bump == block->meta.n_bump_free) {

        bk_remove_block(heap, block);
        return;
    }
}

static inline void bk_gc_heap(bk_Heap *heap) {
    u32       i;
    bk_Block *block;
    bk_Block *next;

    if (bk_config.log_gc) {
        bk_logf("gc-heap %u\n", heap->hid);
    }

    for (i = 0; i < BK_NR_SIZE_CLASSES; i += 1) {
        bk_spin_lock(&heap->block_list_locks[i]);

        block = heap->block_lists[i];
        while (block != NULL) {
            /* bk_gc_block() might destroy block (and remove it from the list,
               so get next first. */
            next = block->meta.next;
            bk_gc_block(heap, block);
            block = next;
        }

        bk_spin_unlock(&heap->block_list_locks[i]);
    }

    heap->free_count = 0;
}

static inline void * bk_big_alloc(bk_Heap *heap, u64 n_bytes, u64 alignment, int zero_mem, bk_Block **blockp) {
    u64       request_size;
    u8       *mem_start;
    void     *aligned;
    bk_Block *block;
    bk_Chunk *chunk;


    BK_ASSERT(IS_POWER_OF_TWO(alignment),
              "alignment is not a power of two");
    BK_ASSERT(alignment <= BK_BLOCK_ALIGN / 2,
              "alignment is too large");

    n_bytes = ALIGN_UP(n_bytes, PAGE_SIZE) + PAGE_SIZE;

    /* Ask for memory twice the desired size so that we can get aligned memory. */
    request_size = PAGE_SIZE + n_bytes + alignment;

    *blockp   = block = bk_get_block(heap, BK_BIG_ALLOC_SIZE_CLASS_IDX, MAX(request_size, BK_BLOCK_SIZE), zero_mem);
    mem_start = block->_chunk_space;
    aligned   = ALIGN_UP(mem_start, alignment);

    BK_ASSERT(BK_ADDR_PARENT_BLOCK(aligned) == block,
              "big allocation too far from block start");

    BK_ASSERT((u8*)aligned + n_bytes <= (u8*)block->meta.end,
              "big alloc doesn't fit in its block");

    chunk = (bk_Chunk*)((u8*)aligned - sizeof(bk_Chunk));
    chunk->header.big_magic = BK_CHUNK_MAGIC;
    chunk->header.big_flags = BK_CHUNK_IS_BIG;
    chunk->header.big_size  = (u8*)block->meta.end - (u8*)aligned;

    BK_ASSERT((u8*)chunk >= block->_chunk_space,
              "chunk too low in block");

    if (bk_config.log_big) {
        bk_logf("big-alloc 0x%X size %U align %U\n", aligned, n_bytes, alignment);
    }

    return aligned;
}

static inline void bk_big_free(bk_Heap *heap, bk_Block *block, void *addr) {
    BK_ASSERT(BK_CHUNK_FROM_USER_MEM(addr)->header.big_magic == BK_CHUNK_MAGIC,
              "big free bad magic");

    bk_remove_block(heap, block);

    if (bk_config.log_big) {
        bk_logf("big-free 0x%X\n", addr);
    }
}

BK_ALWAYS_INLINE
static inline void * bk_alloc_chunk(bk_Heap *heap, bk_Block *block, u64 n_bytes, u64 alignment) {
    bk_Chunk *chunk;
    void     *addr;
    void     *end;
    void     *aligned;
    void     *used_end;
    bk_Chunk *next;
    bk_Chunk *split;

    BK_ASSERT(IS_ALIGNED(n_bytes, BK_MIN_ALIGN),
              "n_bytes is not aligned to BK_MIN_ALIGN");

    BK_ASSERT(n_bytes <= block->meta.size_class,
              "n_bytes too large");

    chunk = BK_BLOCK_FIRST_CHUNK(block);

    while (chunk != NULL) {
        if (chunk->header.flags & BK_CHUNK_IS_FREE) {
            addr    = BK_CHUNK_USER_MEM(chunk);
            end     = (void*)((u8*)addr + chunk->header.size);
            aligned = addr;

            if (!IS_ALIGNED(aligned, alignment)) {
                aligned = (u8*)aligned + sizeof(bk_Chunk) + block->meta.size_class;
                if (!IS_ALIGNED(aligned, alignment)) {
                    aligned = ALIGN_UP(aligned, alignment);
                }
            }

            used_end = (u8*)aligned + n_bytes;

            if ((u8*)used_end <= (u8*)end) {
                next = BK_CHUNK_NEXT(chunk);

                if (aligned != addr) {
                    BK_ASSERT((u64)((u8*)aligned - (u8*)addr) >= sizeof(bk_Chunk) + block->meta.size_class,
                              "aligned doesn't leave enough space to split");

                    split = BK_CHUNK_FROM_USER_MEM(aligned);

                    split->header.magic       = BK_CHUNK_MAGIC;
                    split->header.flags       = chunk->header.flags;
                    split->header.size        = (u8*)end - (u8*)aligned;
                    split->header.offset_next = next == NULL
                                                    ? 0
                                                    : BK_CHUNK_DISTANCE(next, split);
                    chunk->header.offset_next = BK_CHUNK_DISTANCE(split, chunk);
                    chunk->header.size        = (u8*)split - (u8*)addr;

                    BK_ASSERT(BK_CHUNK_NEXT(chunk) == split,
                              "bad split link left");
                    BK_ASSERT((u8*)chunk + sizeof(bk_Chunk) + chunk->header.size == (u8*)split,
                              "bad split left size");
                    BK_ASSERT(next == NULL || BK_CHUNK_NEXT(split) == next,
                              "bad split right");
                    BK_ASSERT(next == NULL || (u8*)split + sizeof(bk_Chunk) + split->header.size == (u8*)next,
                              "bad split right size");
                    BK_ASSERT(next != NULL || (u8*)split + sizeof(bk_Chunk) + split->header.size == block->_chunk_space + BK_CHUNK_SPACE,
                              "right split has no next, but doesn't span rest of chunk space");
                    BK_ASSERT(chunk->header.size >= block->meta.size_class,
                              "left split too small");

                    chunk = split;
                }

                chunk->header.flags &= ~BK_CHUNK_IS_FREE;

                BK_ASSERT((u8*)BK_CHUNK_USER_MEM(chunk) + chunk->header.size <= block->_chunk_space + BK_CHUNK_SPACE,
                          "chunk goes beyond chunk space");
                BK_ASSERT((u8*)aligned + n_bytes <= block->_chunk_space + BK_CHUNK_SPACE,
                          "chunk allocation goes beyond chunk space");

                return aligned;
            }
        }

        chunk = BK_CHUNK_NEXT(chunk);
    }

    block->meta.all_chunks_used = 1;

    return NULL;
}

BK_ALWAYS_INLINE
static inline void bk_free_chunk(bk_Heap *heap, bk_Block *block, bk_Chunk *chunk) {
    BK_ASSERT(!(chunk->header.flags & BK_CHUNK_IS_FREE),
              "double free");
    BK_ASSERT(chunk->header.magic == BK_CHUNK_MAGIC,
              "free'd address not from bkmalloc chunk");
    chunk->header.flags |= BK_CHUNK_IS_FREE | BK_CHUNK_BEEN_USED;

    block->meta.all_chunks_used = 0;
}

BK_ALWAYS_INLINE
static inline void * bk_alloc_slot(bk_Heap *heap, bk_Block *block) {
    u64   r;
    u64   s;
    void *addr;

    if (block->slots.n_allocations == block->slots.max_allocations) {
        return NULL;
    }

    BK_ASSERT(block->slots.n_allocations < block->slots.max_allocations,
              "slots.n_allocations mismatch");

    block->slots.n_allocations += 1;

    BK_ASSERT(block->slots.regions_bitfield != BK_ALL_REGIONS_FULL,
              "slots remain, but all regions marked full");

    r = CLZ(~block->slots.regions_bitfield);
    s = CLZ(~BK_GET_SLOT_BITFIELD(block->slots, r));

    BK_ASSERT(BK_GET_SLOT_BIT(block->slots, r, s) != BK_SLOT_TAKEN,
              "slot already taken");

    BK_SET_SLOT_BIT_TAKEN(block->slots, r, s);

    if (BK_GET_SLOT_BITFIELD(block->slots, r) == BK_ALL_SLOTS_TAKEN) {
        BK_SET_REGION_BIT_FULL(block->slots, r);
    }

    addr = block->_slot_space + (((r << 6ULL) + s) << block->meta.log2_size_class);

    BK_ASSERT((u8*)addr >= block->_slot_space,
              "slot is too low in block");
    BK_ASSERT((u8*)addr + block->meta.size_class <= (u8*)block + BK_BLOCK_SIZE,
              "slot goes beyond block");

    return addr;
}

BK_ALWAYS_INLINE
static inline void bk_free_slot(bk_Heap *heap, bk_Block *block, void *addr) {
    u32 idx;
    u32 region;
    u32 slot;

    idx    = ((u8*)addr - block->_slot_space) >> block->meta.log2_size_class;
    region = idx >> 6ULL;
    slot   = idx  & 63ULL;

    BK_ASSERT(BK_GET_SLOT_BIT(block->slots, region, slot) == BK_SLOT_TAKEN,
              "slot was not taken");
    BK_ASSERT(block->slots.n_allocations != 0,
              "slot taken, but n_allocations is 0");

    BK_SET_SLOT_BIT_FREE(block->slots, region, slot);
    BK_SET_REGION_BIT_NOT_FULL(block->slots, region);
    block->slots.n_allocations -= 1;
}

BK_ALWAYS_INLINE
static inline void * bk_alloc_from_block(bk_Heap *heap, bk_Block *block, u64 n_bytes, u64 alignment) {
    void     *mem;
    bk_Chunk *chunk;

    BK_ASSERT(heap->block_list_locks[block->meta.size_class_idx].val == BK_SPIN_LOCKED,
              "block list lock is not held");

    /* From slots? */
    if (!bk_config.disable_slots) {
        if (likely(alignment <= block->meta.size_class)) {
            if ((mem = bk_alloc_slot(heap, block))) { goto out; }
        }
    }

    /* Use a chunk? */
    if (!bk_config.disable_chunks) {
        if (!block->meta.all_chunks_used
        &&  (mem = bk_alloc_chunk(heap, block, n_bytes, alignment))) { goto out; }
    }

    /* From bump space? */
    if (!bk_config.disable_bump) {
        mem = ALIGN_UP(block->meta.bump, MAX(BK_MIN_ALIGN, alignment));
        if ((u8*)mem + n_bytes <= (u8*)block + BK_BLOCK_SIZE) {
            chunk = BK_CHUNK_FROM_USER_MEM(mem);
            chunk->header.size  = n_bytes;

            block->meta.bump    = (u8*)mem + n_bytes;
            block->meta.n_bump += 1;

            goto out;
        }
        mem = NULL;
    }

out:;
    return mem;
}

BK_ALWAYS_INLINE
static inline int bk_mem_is_zero(bk_Block *block, void *mem) {
    if ((u8*)mem >= (u8*)block->meta.bump_base) {
        return 1;
    } else if ((u8*)mem >= block->_slot_space) {
        /* No guarantees for slots. */
    } else if ((u8*)mem >= block->_chunk_space) {
        if (!(BK_CHUNK_FROM_USER_MEM(mem)->header.flags & BK_CHUNK_BEEN_USED)) {
            return 1;
        }
    }

    return 0;
}

BK_ALWAYS_INLINE BK_HOT
static inline void * bk_heap_alloc(bk_Heap *heap, u64 n_bytes, u64 alignment, int zero_mem) {
    void     *mem;
    u32       size_class_idx;
    int       did_gc;
    bk_Block *block;
    bk_Block *next;

    BK_ASSERT(IS_POWER_OF_TWO(alignment), "alignment not a power of two");

    mem = NULL;

    if (BK_REQUEST_NEEDS_BIG_ALLOC(n_bytes, alignment, zero_mem)) {

        BK_HOOK(pre_alloc, &heap, &n_bytes, &alignment, &zero_mem);

        mem = bk_big_alloc(heap, n_bytes, alignment, zero_mem, &block);
        /* Zeroing the memory will be handled for us for big allocs */
        zero_mem = 0;
        goto out;

    } else if (!IS_ALIGNED(n_bytes, BK_MIN_ALIGN)) {
        n_bytes = ALIGN_UP(n_bytes, BK_MIN_ALIGN);
    } else if (unlikely(n_bytes == 0)) {
        n_bytes = BK_MIN_ALIGN;
    }

    BK_HOOK(pre_alloc, &heap, &n_bytes, &alignment, &zero_mem);

    size_class_idx = bk_get_size_class_idx(n_bytes);

    did_gc = 0;

    bk_spin_lock(&heap->block_list_locks[size_class_idx]);

    block = heap->block_lists[size_class_idx];

again:;
    while (block != NULL) {
        mem = bk_alloc_from_block(heap, block, n_bytes, alignment);
        if (mem != NULL) { goto out_unlock; }
        block = block->meta.next;
    }

    if (bk_config.gc_policy == BK_GC_POL_failure && !did_gc) {
        block = heap->block_lists[size_class_idx];
        while (block != NULL) {
            /* bk_gc_block() might destroy block (and remove it from the list,
               so get next first. */
            next = block->meta.next;
            bk_gc_block(heap, block);
            block = next;
        }

        did_gc = 1;
        goto again;
    }

    block = bk_add_block(heap, size_class_idx);
    mem   = bk_alloc_from_block(heap, block, n_bytes, alignment);

out_unlock:;
    bk_spin_unlock(&heap->block_list_locks[size_class_idx]);

out:;
    BK_ASSERT(mem != NULL,
              "failed to get memory");
    BK_ASSERT(IS_ALIGNED(mem, alignment),
              "failed to align memory");
    BK_ASSERT((u8*)mem >= block->_chunk_space,
              "mem is too low in the block");
    BK_ASSERT(bk_malloc_size(mem) >= n_bytes,
              "did not get enough space for allocation");

    if (zero_mem) {
        if (!bk_mem_is_zero(block, mem)) {
            bk_memzero(mem, n_bytes);
        }
    }

    BK_HOOK(post_alloc, heap, n_bytes, alignment, zero_mem, mem);

    if (bk_config.log_allocs) {
        bk_logf("alloc 0x%X heap %u size %U align %U\n", mem, heap->hid, n_bytes, alignment);
    }

    return mem;
}

BK_ALWAYS_INLINE BK_HOT
static inline void bk_heap_free(bk_Heap *heap, void *addr) {
    bk_Block *block;
    u32       fc;

    BK_HOOK(pre_free, heap, addr);

    block = BK_ADDR_PARENT_BLOCK(addr);

    if (unlikely(block->meta.size_class_idx == BK_BIG_ALLOC_SIZE_CLASS_IDX)) {
        bk_big_free(heap, block, addr);
    } else {
        BK_ASSERT(    (addr >= (void*)block->_chunk_space
                    && addr <  (void*)(block->_chunk_space + BK_CHUNK_SPACE))
                  ||  (addr >= (void*)block->_slot_space
                    && addr <  (void*)(block->_slot_space + BK_SLOT_SPACE)),
                  "addr is not in a valid location within the block");


        if (addr >= block->meta.bump_base) {
            FAA(&block->meta.n_bump_free, 1);
        } else if (addr < (void*)&block->slots) {
            bk_free_chunk(heap, block, BK_CHUNK_FROM_USER_MEM(addr));
        } else {
            bk_spin_lock(&heap->block_list_locks[block->meta.size_class_idx]);
            bk_free_slot(heap, block, addr);
            bk_spin_unlock(&heap->block_list_locks[block->meta.size_class_idx]);
        }


        fc = FAA(&heap->free_count, 1);

        if (bk_config.gc_policy == BK_GC_POL_free_threshold
        &&  fc >= (u32)bk_config.gc_free_threshold) {

            bk_gc_heap(heap);
        }
    }

    if (bk_config.log_frees) {
        bk_logf("free 0x%X heap %u free_count: %u\n", addr, heap->hid, heap->free_count);
    }
}

static void *bk_imalloc(u64 n_bytes) {
    return bk_heap_alloc(bk_global_heap, n_bytes, BK_MIN_ALIGN, 0);
}

static void  bk_ifree(void *addr)    {
    bk_Heap *heap;

    if (likely(addr != NULL)) {
        heap = BK_BLOCK_GET_HEAP_PTR(BK_ADDR_PARENT_BLOCK(addr));

        BK_ASSERT(heap != NULL, "attempting to free from block that doesn't have a heap\n");

        bk_heap_free(heap, addr);
    }
}

static char *bk_istrdup(const char *s) {
    u64   len;
    char *ret;

    len = strlen(s);
    ret = (char*)bk_imalloc(len + 1);

    memcpy(ret, s, len + 1);

    return ret;
}

#endif /* BKMALLOC_HOOK */

/******************************* @@threads *******************************/

#ifndef BKMALLOC_HOOK

typedef struct {
    bk_Heap     *heap;
    bk_Spinlock  lock;
    int          is_valid;
} bk_Thread_Data;


static bk_Thread_Data *_bk_thread_datas;
static u32             _bk_main_thread_cpu;
static u32             _bk_nr_cpus;

static inline void bk_init_threads(void) {
    _bk_main_thread_cpu = bk_getcpu();
    _bk_nr_cpus         = bk_get_nr_cpus();
    _bk_thread_datas    = (bk_Thread_Data*)bk_imalloc(_bk_nr_cpus * sizeof(bk_Thread_Data));
    memset((void*)_bk_thread_datas, 0, _bk_nr_cpus * sizeof(bk_Thread_Data));
}

static BK_THREAD_LOCAL bk_Thread_Data *_bk_local_thr;

BK_ALWAYS_INLINE
static inline bk_Heap *bk_get_this_thread_heap(void) {
    u32 tid;
    u32 cpu;

    if (unlikely(_bk_local_thr == NULL)) {
        tid = syscall(SYS_gettid);
        cpu = tid % _bk_nr_cpus;
        _bk_local_thr = &_bk_thread_datas[cpu];

        if (!_bk_local_thr->is_valid) {
            bk_spin_lock(&_bk_local_thr->lock);

            if (!_bk_local_thr->is_valid) {
                if (cpu == _bk_main_thread_cpu && bk_config.main_thread_use_global) {
                    _bk_local_thr->heap = bk_global_heap;
                } else {
                    _bk_local_thr->heap = (bk_Heap*)bk_imalloc(sizeof(bk_Heap));
                    bk_init_heap(_bk_local_thr->heap, BK_HEAP_THREAD, NULL);
                }

                BK_ASSERT(_bk_local_thr->heap != NULL,
                            "did not get thread heap");

                _bk_local_thr->is_valid = 1;
            }

            bk_spin_unlock(&_bk_local_thr->lock);
        }
    }


    return _bk_local_thr->heap;
}

#endif /* BKMALLOC_HOOK */

/******************************* @@init *******************************/

#ifndef BKMALLOC_HOOK

static s32         bk_is_initialized;
static bk_Spinlock init_lock = BK_STATIC_SPIN_LOCK_INIT;

__attribute__((constructor))
static inline void bk_init(void) {
#ifdef BK_DO_ASSERTIONS
    bk_Block *test_block;
#endif

    /*
     * Thread-unsafe check for performance.
     * Could give a false positive.
     */
    if (unlikely(!bk_is_initialized)) {
        bk_spin_lock(&init_lock);

        /* Thread-safe check. */
        if (unlikely(bk_is_initialized)) {
            bk_spin_unlock(&init_lock);
            return;
        }

#ifdef BK_DO_ASSERTIONS
        test_block = bk_make_block(NULL, 0, BK_BLOCK_SIZE);

        BK_ASSERT(sizeof(bk_Chunk) == 8,
                  "chunk size incorrect");
        BK_ASSERT(sizeof(bk_Block) == BK_BLOCK_SIZE,
                  "bk_Block is not the right size");
        BK_ASSERT(IS_ALIGNED(test_block, BK_BLOCK_ALIGN),
                  "block is not aligned");
        BK_ASSERT((sizeof(bk_Block_Metadata) + BK_CHUNK_SPACE + sizeof(bk_Slots) + BK_SLOT_SPACE) == BK_BLOCK_SIZE,
                  "block packing issues");
        BK_ASSERT(test_block->_chunk_space - (u8*)test_block == sizeof(bk_Block_Metadata),
                  "_chunk_space offset incorrect");

        BK_ASSERT(IS_ALIGNED(test_block->_chunk_space + sizeof(bk_Chunk), BK_MIN_ALIGN),
                  "block's first chunk is not aligned");

        BK_ASSERT(IS_ALIGNED(test_block->_slot_space, BK_MAX_BLOCK_ALLOC_SIZE),
                  "slot space isn't aligned");

        bk_release_block(test_block);
#endif

        if (bk_init_config() != 0) { exit(1); }
        bk_init_threads();
        bk_init_user_heaps();
#ifndef BKMALLOC_HOOK
        bk_init_hooks();
#endif /* BKMALLOC_HOOK */

        bk_is_initialized = 1;

        bk_spin_unlock(&init_lock);
    }
}

#endif /* BKMALLOC_HOOK */


/******************************* @@interface *******************************/

#ifndef BKMALLOC_HOOK

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

void bk_unhook(void) {
    bk_memzero(&bk_hooks, sizeof(bk_hooks));
    bk_hooks.unhooked = 1;

    if (bk_config.log_hooks) {
        bk_logf("hooks-unhook\n");
    }
}

bk_Heap *bk_heap(const char *name) {
    bk_Heap  *heap;
    bk_Heap **heapp;

    heap = NULL;

    bk_spin_lock(&bk_user_heaps_lock);

    heapp = hash_table_get_val(bk_user_heaps, name);
    if (heapp != NULL) {
        heap = *heapp;
    } else {
        heap = (bk_Heap*)bk_imalloc(sizeof(bk_Heap));
        bk_init_heap(heap, BK_HEAP_USER, name);

        hash_table_insert(bk_user_heaps, heap->user_key, heap);
    }

    bk_spin_unlock(&bk_user_heaps_lock);

    return heap;
}

void bk_destroy_heap(const char *name) {
    bk_Heap **heapp;
    bk_Heap  *heap;

    bk_spin_lock(&bk_user_heaps_lock);

    heapp = hash_table_get_val(bk_user_heaps, name);
    if (heapp != NULL) {
        hash_table_delete(bk_user_heaps, name);

        heap = *heapp;
        bk_free_heap(heap);
        bk_ifree(heap);
    }

    bk_spin_unlock(&bk_user_heaps_lock);
}

#define BK_GET_HEAP()                                            \
    (unlikely(!bk_is_initialized || !bk_config.per_thread_heaps) \
        ? bk_global_heap                                         \
        : bk_get_this_thread_heap())

BK_ALWAYS_INLINE
static inline void * _bk_alloc(bk_Heap *heap, u64 n_bytes, u64 alignment, int zero_mem) {
    void *addr;

    addr = bk_heap_alloc(heap, n_bytes, alignment, zero_mem);

    return addr;
}

BK_ALWAYS_INLINE
static inline void * bk_alloc(bk_Heap *heap, u64 n_bytes, u64 alignment) {
    return _bk_alloc(heap, n_bytes, alignment, 0);
}

BK_ALWAYS_INLINE
static inline void * bk_zalloc(bk_Heap *heap, u64 n_bytes, u64 alignment) {
    return _bk_alloc(heap, n_bytes, alignment, 1);
}


BK_ALWAYS_INLINE
static inline size_t _bk_malloc_size(void *addr) {
    bk_Block *block;
    bk_Chunk *chunk;

    if (unlikely(addr == NULL)) { return 0; }

    block = BK_ADDR_PARENT_BLOCK(addr);

    if (unlikely(block->meta.size_class == BK_BIG_ALLOC_SIZE_CLASS)) {
        chunk = BK_CHUNK_FROM_USER_MEM(addr);
        BK_ASSERT(chunk->header.big_flags & BK_CHUNK_IS_BIG,
                  "non-big chunk in big chunk block");
        return chunk->header.big_size;
    } else {
        if ((u8*)addr >= block->_slot_space) {
            return block->meta.size_class;
        } else {
            chunk = BK_CHUNK_FROM_USER_MEM(addr);
            return chunk->header.size;
        }
    }

    return 0;
}

BK_ALWAYS_INLINE
static inline void _bk_free(void *addr) {
    bk_Block *block;
    bk_Heap  *heap;

    if (likely(addr != NULL)) {
        block = BK_ADDR_PARENT_BLOCK(addr);
        heap  = BK_BLOCK_GET_HEAP_PTR(block);

        BK_ASSERT(heap != NULL, "attempting to free from block that doesn't have a heap\n");

        bk_heap_free(heap, addr);
    }
}

BK_ALWAYS_INLINE
static inline void * _bk_malloc(bk_Heap *heap, size_t n_bytes) {
    return bk_alloc(heap, n_bytes, BK_MIN_ALIGN);
}

BK_ALWAYS_INLINE
static inline void * _bk_calloc(bk_Heap *heap, size_t count, size_t n_bytes) {
    return bk_zalloc(heap, count * n_bytes, BK_MIN_ALIGN);
}

BK_ALWAYS_INLINE
static inline void * _bk_realloc(bk_Heap *heap, void *addr, size_t n_bytes) {
    void *new_addr;
    u64   old_size;

    new_addr = NULL;

    if (addr == NULL) {
        new_addr = bk_alloc(heap, n_bytes, BK_MIN_ALIGN);
    } else {
        if (likely(n_bytes > 0)) {
            old_size = _bk_malloc_size(addr);
            /*
             * This is done for us in bk_heap_alloc, but we'll
             * need the aligned value when we get the copy length.
             */
            if (!IS_ALIGNED(n_bytes, BK_MIN_ALIGN)) {
                n_bytes = ALIGN_UP(n_bytes, BK_MIN_ALIGN);
            }

            /*
             * If it's already big enough, just leave it.
             * We won't worry about shrinking it.
             * Saves us an alloc, free, and memcpy.
             * Plus, we don't have to lock anything.
             */
            if (old_size >= n_bytes) {
                return addr;
            }

            new_addr = bk_alloc(heap, n_bytes, BK_MIN_ALIGN);
            memcpy(new_addr, addr, old_size);
        }

        _bk_free(addr);
    }

    return new_addr;
}

BK_ALWAYS_INLINE
static inline void * _bk_reallocf(bk_Heap *heap, void *addr, size_t n_bytes) {
    return _bk_realloc(heap, addr, n_bytes);
}

BK_ALWAYS_INLINE
static inline void * _bk_valloc(bk_Heap *heap, size_t n_bytes) {
    return bk_alloc(heap, n_bytes, PAGE_SIZE);
}

BK_ALWAYS_INLINE
static inline int _bk_posix_memalign(bk_Heap *heap, void **memptr, size_t alignment, size_t n_bytes) {
    if (unlikely(!IS_POWER_OF_TWO(alignment)
    ||  alignment < sizeof(void*))) {
        return EINVAL;
    }

    *memptr = bk_alloc(heap, n_bytes, alignment);

    if (unlikely(*memptr == NULL))    { return ENOMEM; }
    return 0;
}

BK_ALWAYS_INLINE
static inline void * _bk_aligned_alloc(bk_Heap *heap, size_t alignment, size_t size) {
    return bk_alloc(heap, size, alignment);
}

#ifdef BK_RETURN_ADDR
BK_THREAD_LOCAL void *_bk_return_addr;
#endif

void * bk_malloc(struct bk_Heap *heap, size_t n_bytes)                                          { BK_STORE_RA(); return _bk_malloc(heap, n_bytes);                                  }
void * bk_calloc(struct bk_Heap *heap, size_t count, size_t n_bytes)                            { BK_STORE_RA(); return _bk_malloc(heap, n_bytes);                                  }
void * bk_realloc(struct bk_Heap *heap, void *addr, size_t n_bytes)                             { BK_STORE_RA(); return _bk_realloc(heap, addr, n_bytes);                           }
void * bk_reallocf(struct bk_Heap *heap, void *addr, size_t n_bytes)                            { BK_STORE_RA(); return _bk_reallocf(heap, addr, n_bytes);                          }
void * bk_valloc(struct bk_Heap *heap, size_t n_bytes)                                          { BK_STORE_RA(); return _bk_valloc(heap, n_bytes);                                  }
void   bk_free(void *addr)                                                                      { return _bk_free(addr);                                                            }
int    bk_posix_memalign(struct bk_Heap *heap, void **memptr, size_t alignment, size_t n_bytes) { BK_STORE_RA(); return _bk_posix_memalign(heap, memptr, alignment, n_bytes);       }
void * bk_aligned_alloc(struct bk_Heap *heap, size_t alignment, size_t size)                    { BK_STORE_RA(); return _bk_aligned_alloc(heap, alignment, size);                   }
size_t bk_malloc_size(void *addr)                                                               { return _bk_malloc_size(addr);                                                     }

/* This helper does not BK_STORE_RA() since that is done higher up in the new wrappers. */
void * _bk_new(size_t n_bytes) BK_THROW                                                         { return _bk_malloc(BK_GET_HEAP(), n_bytes);                                        }

void * malloc(size_t n_bytes) BK_THROW                                                          { BK_STORE_RA(); return _bk_malloc(BK_GET_HEAP(), n_bytes);                         }
void * calloc(size_t count, size_t n_bytes) BK_THROW                                            { BK_STORE_RA(); return _bk_calloc(BK_GET_HEAP(), count, n_bytes);                  }
void * realloc(void *addr, size_t n_bytes) BK_THROW                                             { BK_STORE_RA(); return _bk_realloc(BK_GET_HEAP(), addr, n_bytes);                  }
void * reallocf(void *addr, size_t n_bytes) BK_THROW                                            { BK_STORE_RA(); return _bk_reallocf(BK_GET_HEAP(), addr, n_bytes);                 }
void * valloc(size_t n_bytes) BK_THROW                                                          { BK_STORE_RA(); return _bk_valloc(BK_GET_HEAP(), n_bytes);                         }
void * pvalloc(size_t n_bytes) BK_THROW                                                         { return NULL;                                                                      }
void   free(void *addr) BK_THROW                                                                { _bk_free(addr);                                                                   }
int    posix_memalign(void **memptr, size_t alignment, size_t size) BK_THROW                    { BK_STORE_RA(); return _bk_posix_memalign(BK_GET_HEAP(), memptr, alignment, size); }
void * aligned_alloc(size_t alignment, size_t size) BK_THROW                                    { BK_STORE_RA(); return _bk_aligned_alloc(BK_GET_HEAP(), alignment, size);          }
void * memalign(size_t alignment, size_t size) BK_THROW                                         { BK_STORE_RA(); return _bk_aligned_alloc(BK_GET_HEAP(), alignment, size);          }
size_t malloc_size(void *addr) BK_THROW                                                         { return _bk_malloc_size(addr);                                                     }
size_t malloc_usable_size(void *addr) BK_THROW                                                  { return 0;                                                                         }

#ifdef BK_MMAP_OVERRIDE
void *mmap(void *addr, size_t length, int prot, int flags, int fd, off_t offset) {
    long  ret;
    void *ret_addr;

    if (!_bk_internal_mmap) { BK_STORE_RA(); }

    ret      = syscall(SYS_mmap, addr, length, prot, flags, fd, offset);
    ret_addr = (void*)ret;

    if (!_bk_internal_mmap && ret_addr != MAP_FAILED) {
        BK_HOOK(post_mmap, addr, length, prot, flags, fd, offset, ret_addr);
    }

    return ret_addr;
}

int munmap(void *addr, size_t length) {
    long ret;

    ret = syscall(SYS_munmap, addr, length);

    if (!_bk_internal_munmap && ret == 0) {
        BK_HOOK(post_munmap, addr, length);
    }

    return ret;
}
#endif /* BK_MMAP_OVERRIDE */


#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif /* BKMALLOC_HOOK */

#undef STR
#undef _STR
#undef CAT2
#undef _CAT2
#undef CAT3
#undef _CAT3
#undef CAT4
#undef _CAT4
#undef hash_table
#undef hash_table_make
#undef hash_table_make_e
#undef hash_table_len
#undef hash_table_free
#undef hash_table_get_key
#undef hash_table_get_val
#undef hash_table_insert
#undef hash_table_delete
#undef hash_table_traverse
#undef _hash_table_slot
#undef hash_table_slot
#undef _hash_table
#undef hash_table
#undef hash_table_pretty_name
#undef _HASH_TABLE_EQU
#undef DEFAULT_START_SIZE_IDX
#undef use_hash_table

#endif /* defined(BKMALLOC_IMPL) || defined(BKMALLOC_HOOK) */

#endif /* __BKMALLOC_H__ */
