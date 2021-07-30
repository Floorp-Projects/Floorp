#ifndef WASM_RT_OS_H_
#define WASM_RT_OS_H_

#include <stddef.h>
#include <stdint.h>
#include <time.h>

enum {
    MMAP_PROT_NONE = 0,
    MMAP_PROT_READ = 1,
    MMAP_PROT_WRITE = 2,
    MMAP_PROT_EXEC = 4
};

/* Memory map flags */
enum {
    MMAP_MAP_NONE = 0,
    /* Put the mapping into 0 to 2 G, supported only on x86_64 */
    MMAP_MAP_32BIT = 1,
    /* Don't interpret addr as a hint: place the mapping at exactly
       that address. */
    MMAP_MAP_FIXED = 2
};

void os_init();

size_t os_getpagesize();
// Try allocating Memory space.
// Returns pointer to allocated region on success, 0 on failure.
void *os_mmap(void *hint, size_t size, int prot, int flags);
void os_munmap(void *addr, size_t size);
// Set the permissions of the memory region.
// Returns 0 on success, non zero on failure.
int os_mprotect(void *addr, size_t size, int prot);
// Like mmap but returns an aligned region
void* os_mmap_aligned(void *addr, size_t requested_length, int prot, int flags, size_t alignment, size_t alignment_offset);
// Commits and sets the permissions on an already allocated memory region
// Returns 0 on success, non zero on failure.
int os_mmap_commit(void* curr_heap_end_pointer, size_t expanded_size, int prot);

void os_clock_init(void** clock_data_pointer);
void os_clock_cleanup(void** clock_data_pointer);
int os_clock_gettime(void* clock_data, int clock_id, struct timespec* out_struct);
int os_clock_getres(void* clock_data, int clock_id, struct timespec* out_struct);

#endif
