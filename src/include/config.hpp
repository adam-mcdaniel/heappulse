// #define DEBUG

#define INTERVAL_CONFIG {.period_milliseconds = 1000, .clear_soft_dirty_bits=false}

// #define OPTIMIZE
// #define COLLECT_BACKTRACE
#define LOG_FILE "log.txt"

#define MAX_TRACKED_ACCESSES 100000

#define GUARD_ACCESSES
// #define SOFT_GUARD_ACCESSES

// #define DUMMY_TEST
// #define PAGE_TRACKING_TEST
// #define COMPRESSION_TEST
// #define PAGE_LIVENESS_TEST
// #define OBJECT_LIVENESS_TEST
// #define GENERATIONAL_TEST
#define ACCESS_PATTERN_TEST