#pragma once

#include <stdlib.h>
#include <execinfo.h>

const int BACKTRACE_DEPTH = 100;

class Backtrace {
public:
    Backtrace() {
        backtrace_size = backtrace(backtrace_addrs, BACKTRACE_DEPTH);
    }

    static Backtrace capture() {
        Backtrace bt;
        bt.backtrace_size = backtrace(bt.backtrace_addrs, BACKTRACE_DEPTH);
        return bt;
    }

    // Get the allocation site from the backtrace
    void *get_allocation_site() {
        // Get the first symbol that doesn't have `libbkmalloc` or `hook` in it.
        // This is the allocation site.
        for (int i=0; i<backtrace_size; i++) {
            char **strings = backtrace_symbols(backtrace_addrs + i, 1), *symbol = strings[0];

            if (i == backtrace_size - 1) {
                from_hook = strstr(symbol, "libstdc") != NULL || strstr(symbol, "libc") != NULL || strstr(symbol, "ld") != NULL || strstr(symbol, "libbkmalloc") != NULL || strstr(symbol, "hook") != NULL;
            }

            if (strstr(symbol, "libstdc") == NULL && strstr(symbol, "libbkmalloc") == NULL && strstr(symbol, "hook") == NULL && strstr(symbol, "libc") == NULL && strstr(symbol, "ld") == NULL) {
                free(strings);
                allocation_site = backtrace_addrs[i];
                return allocation_site;
            }
            free(strings);
        }
        return NULL;
    }

    bool contains_allocation_site(void *site) {
        for (int i=0; i<backtrace_size; i++) {
            if (backtrace_addrs[i] == site) {
                return true;
            }
        }
        return false;
    }

    // Check if this backtrace is obviously invalid.
    bool is_invalid() const {
        if (backtrace_size == 0) {
            return true;
        }

        if (from_hook == 1) {
            return true;
        }

        if (backtrace_size == 1) {
            return backtrace_addrs[0] == NULL;
        }

        return false;
    }

    bool is_from_hook() {
        if (from_hook == -1) {
            // If the backtrace starts at `libc` or `ld`, then it's from the hook.
            char **strings = backtrace_symbols(backtrace_addrs + backtrace_size - 1, 1), *symbol = strings[0];
            from_hook = strstr(symbol, "libstdc") != NULL || strstr(symbol, "libc") != NULL || strstr(symbol, "ld") != NULL || strstr(symbol, "libbkmalloc") != NULL || strstr(symbol, "hook") != NULL;
            free(strings);
        }
        return from_hook;
    }

    // friend std::ostream& operator<<(std::ostream& os, Backtrace& bt) {
    //     char **strings = backtrace_symbols(bt.backtrace_addrs, bt.backtrace_size);

    //     bt.get_allocation_site();
    //     if (bt.is_from_hook()) {
    //         os << "[from hook ";
    //         for (int i = 0; i < bt.backtrace_size && strings[i] != NULL; i++)
    //             os << i + 1 << " " << strings[i];
    //         os << " ]";
    //     } else {
    //         if (strings != NULL) {
    //             os << "[allocation site at " << std::hex << (long long int)bt.allocation_site << std::dec << ":";
    //             for (int i = 0; i < bt.backtrace_size && strings[i] != NULL; i++)
    //                 os << i + 1 << " " << strings[i];
    //             os << " ]";
    //         } else {
    //             os << "[error]";
    //         }
    //     }

    //     free(strings);
    //     return os;
    // }

    void print() {
        char **strings = backtrace_symbols(backtrace_addrs, backtrace_size);

        get_allocation_site();
        if (is_from_hook()) {
            stack_printf("[from hook ");
            for (int i = 0; i < backtrace_size && strings[i] != NULL; i++)
                stack_printf("%d %s\n", i + 1, strings[i]);
            stack_printf(" ]\n");
        } else {
            if (strings != NULL) {
                stack_printf("[allocation site at %p:", allocation_site);
                for (int i = 0; i < backtrace_size && strings[i] != NULL; i++)
                    stack_printf("%d %s\n", i + 1, strings[i]);
                stack_printf(" ]\n");
            } else {
                stack_printf("[error]\n");
            }
        }

        free(strings);
    }

private:
    int from_hook = -1;
    void *allocation_site = NULL;
    void *backtrace_addrs[BACKTRACE_DEPTH];
    int backtrace_size = 0;
};
