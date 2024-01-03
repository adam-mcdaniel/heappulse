#define BKMALLOC_HOOK
#include <bkmalloc.h>
#include <stack_io.hpp>
#include <timer.hpp>
#include "stack_map.cpp"
#include "stack_file.cpp"
#include <stack_csv.hpp>
#include <interval_test.hpp>
#include "compression_test.cpp"


static CompressionTest ct;
static IntervalTestSuite its;


static void parse_origin(int pid, u64 origin_inst_addr, char *func_name, char *file_name, u64 *line_number) {
    FILE *f;
    char  buff[4096] = {0};
    char *s;
    char cur[4096] = {0};
    u64   cur_start;
    u64   start;
    u64   end;
    char  cur_copy[4096] = {0};
    FILE *p;

    memset(buff, 0, sizeof(buff));
    memset(cur, 0, sizeof(cur));
    memset(cur_copy, 0, sizeof(cur_copy));

    // origin->func_name   = "???";
    // origin->file_name   = "???";
    // origin->line_number = 0;
    strcpy(func_name, "???");
    strcpy(file_name, "???");
    *line_number = 0;

    // Clear buffer

    stack_sprintf<256>(buff, "/proc/%d/maps", pid);
    // snprintf(buff, sizeof(buff), "/proc/%d/maps", pid);
    f = fopen(buff, "r");

    if (f == NULL) {
        stack_warnf("Failed to open %s\n", buff);
        return;
    }

    strcpy(cur, "");
    cur_start = 0;

    while (fgets(buff, sizeof(buff), f)) {
        if (buff[strlen(buff) - 1] == '\n') { buff[strlen(buff) - 1] = 0; }

        if (*buff == 0) {
            stack_warnf("Empty line\n");
            continue;
        }

        /* range */
        if ((s = strtok(buff, " ")) == NULL) {
            stack_warnf("Skipping range\n");
            continue;
        }
        sscanf(s, "%lx-%lx", &start, &end);

        /* perms */
        if ((s = strtok(NULL, " ")) == NULL) {
            stack_warnf("Skipping perms\n");
            continue;
        }
        /* offset */
        if ((s = strtok(NULL, " ")) == NULL) {
            stack_warnf("Skipping offset\n");
            continue;
        }
        /* dev */
        if ((s = strtok(NULL, " ")) == NULL) {
            stack_warnf("Skipping dev\n");
            continue;
        }
        /* inode */
        if ((s = strtok(NULL, " ")) == NULL) {
            stack_warnf("Skipping inode\n");
            continue;
        }
        /* path */
        if ((s = strtok(NULL, " ")) == NULL) {
            stack_warnf("Skipping path\n");
            continue;
        }
        if (*s != '/') {
            stack_warnf("Skipping path\n");
            continue;
        }

        if (cur == NULL || strcmp(cur, s)) {
            stack_debugf("cur: %s\n", cur);
            // if (cur != NULL) { free(cur); }
            // cur       = strdup(s);
            strcpy(cur, s);
            cur_start = start;
        }

        if (origin_inst_addr >= cur_start
        &&  origin_inst_addr <  end) {
            stack_debugf("Found %s\n", cur);

            snprintf(cur_copy, sizeof(cur_copy), "%s", cur);
            strcpy(file_name, basename(cur_copy));

            // snprintf(buff, sizeof(buff),
            //         "addr2line -f -e %s %lx", cur, origin_inst_addr - cur_start);
            memset(buff, 0, sizeof(buff));
            stack_sprintf<1024>(buff, "bash -c 'env -i addr2line -f -e %s %x'", cur, origin_inst_addr - cur_start);
            stack_debugf("Executing `%s`\n", buff);

            p = popen(buff, "r");

            if (p != NULL) {
                memset(buff, 0, sizeof(buff));
                fread(buff, 1, sizeof(buff) - 1, p);

                stack_debugf("Output: %s\n", buff);

                s = buff;

                while (*s && *s != '\n') {
                    stack_debugf("Skipping %x = '%s'\n", (int)*s, s);
                    s += 1;
                }

                if (*s) {
                    *s  = 0;
                    s  += 1;
                }

                stack_debugf("Function name: %s\n", buff);
                strcpy(func_name, buff);

                if (*s != '?') {
                    if (s[strlen(s) - 1] == '\n') {
                        stack_debugf("Removing newline\n");
                        s[strlen(s) - 1] = 0;
                    }
                    if ((s = strtok(s, ":")) != NULL) {
                        stack_debugf("File name: %s\n", s);
                        // free(origin->file_name);
                        strcpy(file_name, basename(s));


                        if ((s = strtok(NULL, ":")) != NULL) {
                            stack_debugf("Line number: %s\n", s);
                            sscanf(s, "%lu", line_number);
                        }
                    }
                }

                pclose(p);
            }

            break;
        } else {
            stack_debugf("Skipping %s\n", cur);
        }
    }

    // if (cur != NULL) { free(cur); }

    fclose(f);
}

class Hooks {
public:
    Hooks() {
        stack_debugf("Hooks constructor\n");

        stack_debugf("Adding test...\n");
        its.add_test(&ct);
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

            char func_name[1024] = "???";
            char file_name[1024] = "???";
            u64 line_number = 0;
            parse_origin(getpid(), (uintptr_t)BK_GET_RA(), func_name, file_name, &line_number);
            stack_debugf("Origin: %s:%s:%d\n", func_name, file_name, line_number);

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

    ~Hooks() {
        stack_logf("Hooks destructor\n");
    }

private:
    std::mutex hook_lock;
};


static Hooks hooks;

std::mutex bk_lock;


static thread_local bool protection_handler_setup = false;

extern "C"
void bk_post_alloc_hook(bk_Heap *heap, u64 n_bytes, u64 alignment, int zero_mem, void *addr) {
    if (!protection_handler_setup) {
        setup_protection_handler();
        protection_handler_setup = true;
    }
    if (hooks.is_done() || !hooks.can_update()) return;
    if (!bk_lock.try_lock()) {
        stack_debugf("Failed to lock\n");
        return;
    }
    stack_debugf("Entering hook\n");
    hooks.post_alloc(heap, n_bytes, alignment, zero_mem, addr);
    stack_debugf("Leaving hook\n");
    bk_lock.unlock();
}

extern "C"
void bk_pre_free_hook(bk_Heap *heap, void *addr) {
    if (!protection_handler_setup) {
        setup_protection_handler();
        protection_handler_setup = true;
    }
    if (hooks.is_done()) return;
    if (hooks.contains(addr)) {
        stack_debugf("About to block on lock\n");
        bk_lock.lock();
        stack_debugf("Entering hook\n");
        hooks.pre_free(heap, addr);
        stack_debugf("Leaving hook\n");
        bk_lock.unlock();
    }
}

extern "C"
void bk_post_mmap_hook(void *addr, size_t n_bytes, int prot, int flags, int fd, off_t offset, void *ret_addr) {
    if (!protection_handler_setup) {
        setup_protection_handler();
        protection_handler_setup = true;
    }
    if (hooks.is_done() || !hooks.can_update()) return;
    if (!bk_lock.try_lock()) {
        stack_debugf("Failed to lock\n");
        return;
    }
    stack_debugf("Entering hook\n");
    // hooks.post_mmap(addr, n_bytes, prot, flags, fd, offset, ret_addr);
    stack_debugf("Leaving hook\n");
    bk_lock.unlock();
}
