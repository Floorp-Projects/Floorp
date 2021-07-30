// Based on
// https://web.archive.org/web/20191012035921/http://nadeausoftware.com/articles/2012/01/c_c_tip_how_use_compiler_predefined_macros_detect_operating_system#BSD
// Check for windows (non cygwin) environment
#if defined(_WIN32)

#include "wasm-rt-os.h"

#include <errno.h>
#include <inttypes.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <Windows.h>

#ifdef VERBOSE_LOGGING
#define VERBOSE_LOG(...) { printf(__VA_ARGS__); }
#else
#define VERBOSE_LOG(...)
#endif

#define DONT_USE_VIRTUAL_ALLOC2

size_t os_getpagesize() {
  SYSTEM_INFO S;
  GetNativeSystemInfo(&S);
  return S.dwPageSize;
}

static void* win_mmap(void* hint, size_t size, int prot, int flags, DWORD alloc_flag) {
  DWORD flProtect = PAGE_NOACCESS;
  size_t request_size, page_size;
  void* addr;

  page_size = os_getpagesize();
  request_size = (size + page_size - 1) & ~(page_size - 1);

  if (request_size < size)
    /* integer overflow */
    return NULL;

  if (request_size == 0)
    request_size = page_size;

  if (prot & MMAP_PROT_EXEC) {
    if (prot & MMAP_PROT_WRITE)
      flProtect = PAGE_EXECUTE_READWRITE;
    else
      flProtect = PAGE_EXECUTE_READ;
  } else if (prot & MMAP_PROT_WRITE)
    flProtect = PAGE_READWRITE;
  else if (prot & MMAP_PROT_READ)
    flProtect = PAGE_READONLY;

  addr = VirtualAlloc((LPVOID)hint, request_size, alloc_flag, flProtect);
  return addr;
}

void* os_mmap(void* hint, size_t size, int prot, int flags) {
  DWORD alloc_flag = MEM_RESERVE | MEM_COMMIT;
  return win_mmap(hint, size, prot, flags, alloc_flag);
}

#ifndef DONT_USE_VIRTUAL_ALLOC2
static void* win_mmap_aligned(void* hint,
                              size_t size,
                              int prot,
                              int flags,
                              size_t pow2alignment) {
  DWORD alloc_flag = MEM_RESERVE | MEM_COMMIT;
  DWORD flProtect = PAGE_NOACCESS;
  size_t request_size, page_size;
  void* addr;

  page_size = os_getpagesize();
  request_size = (size + page_size - 1) & ~(page_size - 1);

  if (request_size < size)
    /* integer overflow */
    return NULL;

  if (request_size == 0)
    request_size = page_size;

  if (prot & MMAP_PROT_EXEC) {
    if (prot & MMAP_PROT_WRITE)
      flProtect = PAGE_EXECUTE_READWRITE;
    else
      flProtect = PAGE_EXECUTE_READ;
  } else if (prot & MMAP_PROT_WRITE)
    flProtect = PAGE_READWRITE;
  else if (prot & MMAP_PROT_READ)
    flProtect = PAGE_READONLY;

  MEM_ADDRESS_REQUIREMENTS addressReqs = {0};
  MEM_EXTENDED_PARAMETER param = {0};

  addressReqs.Alignment = pow2alignment;
  addressReqs.HighestEndingAddress = 0;
  addressReqs.LowestStartingAddress = 0;

  param.Type = MemExtendedParameterAddressRequirements;
  param.Pointer = &addressReqs;

  addr = VirtualAlloc2(0, (LPVOID)addr, request_size, alloc_flag, flProtect,
                       &param, 1);
  return addr;
}
#endif

void os_munmap(void* addr, size_t size) {
  DWORD alloc_flag = MEM_RELEASE;
  if (addr) {
    if (VirtualFree(addr, 0, alloc_flag) == 0) {
      size_t page_size = os_getpagesize();
      size_t request_size = (size + page_size - 1) & ~(page_size - 1);
      int64_t curr_err = errno;
      printf("os_munmap error addr:%p, size:0x%zx, errno:%" PRId64 "\n", addr,
             request_size, curr_err);
    }
  }
}

int os_mprotect(void* addr, size_t size, int prot) {
  DWORD flProtect = PAGE_NOACCESS;

  if (!addr)
    return 0;

  if (prot & MMAP_PROT_EXEC) {
    if (prot & MMAP_PROT_WRITE)
      flProtect = PAGE_EXECUTE_READWRITE;
    else
      flProtect = PAGE_EXECUTE_READ;
  } else if (prot & MMAP_PROT_WRITE)
    flProtect = PAGE_READWRITE;
  else if (prot & MMAP_PROT_READ)
    flProtect = PAGE_READONLY;

  DWORD old;
  BOOL succeeded = VirtualProtect((LPVOID)addr, size, flProtect, &old);
  return succeeded? 0 : -1;
}

#ifndef DONT_USE_VIRTUAL_ALLOC2
static int IsPowerOfTwoOrZero(size_t x) {
  return (x & (x - 1)) == 0;
}
#endif

void* os_mmap_aligned(void* addr,
                      size_t requested_length,
                      int prot,
                      int flags,
                      size_t alignment,
                      size_t alignment_offset) {
#ifndef DONT_USE_VIRTUAL_ALLOC2
  if (IsPowerOfTwoOrZero(alignment) && alignment_offset == 0) {
    return win_mmap_aligned(addr, requested_length, prot, flags, alignment);
  } else
#endif
  {
    size_t padded_length = requested_length + alignment + alignment_offset;
    uintptr_t unaligned = (uintptr_t)win_mmap(addr, padded_length, prot, flags, MEM_RESERVE);

    VERBOSE_LOG("os_mmap_aligned: alignment:%llu, alignment_offset:%llu, requested_length:%llu, padded_length: %llu, initial mapping: %p\n",
      (unsigned long long) alignment,
      (unsigned long long) alignment_offset,
      (unsigned long long) requested_length,
      (unsigned long long) padded_length,
      (void*) unaligned);

    if (!unaligned) {
      return (void*)unaligned;
    }

    // Round up the next address that has addr % alignment = 0
    uintptr_t aligned_nonoffset =
        (unaligned + (alignment - 1)) & ~(alignment - 1);

    // Currently offset 0 is aligned according to alignment
    // Alignment needs to be enforced at the given offset
    uintptr_t aligned = 0;
    if ((aligned_nonoffset - alignment_offset) >= unaligned) {
      aligned = aligned_nonoffset - alignment_offset;
    } else {
      aligned = aligned_nonoffset - alignment_offset + alignment;
    }

    if (aligned == unaligned && padded_length == requested_length) {
      return (void*)aligned;
    }

    // Sanity check
    if (aligned < unaligned ||
        (aligned + (requested_length - 1)) >
            (unaligned + (padded_length - 1)) ||
        (aligned + alignment_offset) % alignment != 0) {
      VERBOSE_LOG("os_mmap_aligned: sanity check fail. aligned: %p\n", (void*) aligned);
      os_munmap((void*)unaligned, padded_length);
      return NULL;
    }

    // windows does not support partial unmapping, so unmap and remap
    os_munmap((void*)unaligned, padded_length);
    aligned =
        (uintptr_t)win_mmap((void*)aligned, requested_length, prot, flags, MEM_RESERVE);
    VERBOSE_LOG("os_mmap_aligned: final mapping: %p\n", (void*) aligned);
    return (void*)aligned;
  }
}

int os_mmap_commit(void* curr_heap_end_pointer, size_t expanded_size, int prot) {
  uintptr_t addr = (uintptr_t)win_mmap(curr_heap_end_pointer, expanded_size, prot, MMAP_MAP_NONE, MEM_COMMIT);
  int ret = addr? 0 : -1;
  return ret;
}

typedef struct {
  LARGE_INTEGER counts_per_sec;
} wasi_win_clock_info_t;

static wasi_win_clock_info_t g_wasi_win_clock_info;
static int g_os_data_initialized = 0;

void os_init() {
  // From here: https://stackoverflow.com/questions/5404277/porting-clock-gettime-to-windows/38212960#38212960
  if (QueryPerformanceFrequency(&g_wasi_win_clock_info.counts_per_sec) == 0) {
    abort();
  }
  g_os_data_initialized = 1;
}

void os_clock_init(void** clock_data_pointer) {
  if (!g_os_data_initialized) {
    os_init();
  }

  wasi_win_clock_info_t* alloc = (wasi_win_clock_info_t*) malloc(sizeof(wasi_win_clock_info_t));
  if (!alloc) {
    abort();
  }
  memcpy(alloc, &g_wasi_win_clock_info, sizeof(wasi_win_clock_info_t));
  *clock_data_pointer = alloc;
}

void os_clock_cleanup(void** clock_data_pointer) {
  if (*clock_data_pointer == 0) {
    free(*clock_data_pointer);
    *clock_data_pointer = 0;
  }
}

int os_clock_gettime(void* clock_data, int clock_id, struct timespec* out_struct) {
  wasi_win_clock_info_t* alloc = (wasi_win_clock_info_t*) clock_data;

  LARGE_INTEGER count;
  (void)clock_id;

  if (alloc->counts_per_sec.QuadPart <= 0 || QueryPerformanceCounter(&count) == 0) {
    return -1;
  }

#define BILLION 1000000000LL
  out_struct->tv_sec = count.QuadPart / alloc->counts_per_sec.QuadPart;
  out_struct->tv_nsec = ((count.QuadPart % alloc->counts_per_sec.QuadPart) * BILLION) / alloc->counts_per_sec.QuadPart;
#undef BILLION

  return 0;
}

int os_clock_getres(void* clock_data, int clock_id, struct timespec* out_struct) {
  (void)clock_id;
  out_struct->tv_sec = 0;
  out_struct->tv_nsec = 1000;
  return 0;
}

#undef VERBOSE_LOG
#undef DONT_USE_VIRTUAL_ALLOC2

#else
  // https://stackoverflow.com/questions/26541150/warning-iso-c-forbids-an-empty-translation-unit
  typedef int make_iso_compilers_happy;
#endif