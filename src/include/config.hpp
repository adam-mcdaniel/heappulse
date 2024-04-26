#define DEBUG

#define INTERVAL_CONFIG {.period_milliseconds = 1000, .clear_soft_dirty_bits=false}

// #define OPTIMIZE
// #define COLLECT_BACKTRACE
// #define LOG_FILE "log.txt"

#define MAX_TRACKED_ACCESSES 100000

#define GUARD_ACCESSES
// #define SOFT_GUARD_ACCESSES

// #define CHECK_DYNAMIC_LIBRARIES

// #define DUMMY_TEST
// #define PAGE_TRACKING_TEST
// #define COMPRESSION_TEST
// #define PAGE_LIVENESS_TEST
// #define OBJECT_LIVENESS_TEST
// #define GENERATIONAL_TEST
// #define ACCESS_PATTERN_TEST
#define ACCESS_COMPRESSION_TEST

// #define USE_ZLIB_COMPRESSION // (1.2.11-1)
// #define USE_LZ4_COMPRESSION // (1.9.0)
#define USE_LZO_COMPRESSION // (2.09)
// #define USE_SNAPPY_COMPRESSION // (1.1.4)
// #define USE_ZSTD_COMPRESSION // (1.4.0-1)
// #define USE_LZF_COMPRESSION // (3.6)
// #define USE_LZ4HC_COMPRESSION // (1.9.0)

#ifndef PAGE_SIZE
#define PAGE_SIZE 4096
#endif

#define MPROTECT
// #define PKEYS
