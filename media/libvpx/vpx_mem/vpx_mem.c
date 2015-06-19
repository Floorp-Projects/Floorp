/*
 *  Copyright (c) 2010 The WebM project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */


#define __VPX_MEM_C__

#include "vpx_mem.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "include/vpx_mem_intrnl.h"
#include "vpx/vpx_integer.h"

#if CONFIG_MEM_TRACKER
#ifndef VPX_NO_GLOBALS
static unsigned long g_alloc_count = 0;
#else
#include "vpx_global_handling.h"
#define g_alloc_count vpxglobalm(vpxmem,g_alloc_count)
#endif
#endif

#if CONFIG_MEM_MANAGER
# include "heapmm.h"
# include "hmm_intrnl.h"

# define SHIFT_HMM_ADDR_ALIGN_UNIT 5
# define TOTAL_MEMORY_TO_ALLOCATE  20971520 /* 20 * 1024 * 1024 */

# define MM_DYNAMIC_MEMORY 1
# if MM_DYNAMIC_MEMORY
static unsigned char *g_p_mng_memory_raw = NULL;
static unsigned char *g_p_mng_memory     = NULL;
# else
static unsigned char g_p_mng_memory[TOTAL_MEMORY_TO_ALLOCATE];
# endif

static size_t g_mm_memory_size = TOTAL_MEMORY_TO_ALLOCATE;

static hmm_descriptor hmm_d;
static int g_mng_memory_allocated = 0;

static int vpx_mm_create_heap_memory();
static void *vpx_mm_realloc(void *memblk, size_t size);
#endif /*CONFIG_MEM_MANAGER*/

#if USE_GLOBAL_FUNCTION_POINTERS
struct GLOBAL_FUNC_POINTERS {
  g_malloc_func g_malloc;
  g_calloc_func g_calloc;
  g_realloc_func g_realloc;
  g_free_func g_free;
  g_memcpy_func g_memcpy;
  g_memset_func g_memset;
  g_memmove_func g_memmove;
} *g_func = NULL;

# define VPX_MALLOC_L  g_func->g_malloc
# define VPX_REALLOC_L g_func->g_realloc
# define VPX_FREE_L    g_func->g_free
# define VPX_MEMCPY_L  g_func->g_memcpy
# define VPX_MEMSET_L  g_func->g_memset
# define VPX_MEMMOVE_L g_func->g_memmove
#else
# define VPX_MALLOC_L  malloc
# define VPX_REALLOC_L realloc
# define VPX_FREE_L    free
# define VPX_MEMCPY_L  memcpy
# define VPX_MEMSET_L  memset
# define VPX_MEMMOVE_L memmove
#endif /* USE_GLOBAL_FUNCTION_POINTERS */

unsigned int vpx_mem_get_version() {
  unsigned int ver = ((unsigned int)(unsigned char)VPX_MEM_VERSION_CHIEF << 24 |
                      (unsigned int)(unsigned char)VPX_MEM_VERSION_MAJOR << 16 |
                      (unsigned int)(unsigned char)VPX_MEM_VERSION_MINOR << 8  |
                      (unsigned int)(unsigned char)VPX_MEM_VERSION_PATCH);
  return ver;
}

int vpx_mem_set_heap_size(size_t size) {
  int ret = -1;

#if CONFIG_MEM_MANAGER
#if MM_DYNAMIC_MEMORY

  if (!g_mng_memory_allocated && size) {
    g_mm_memory_size = size;
    ret = 0;
  } else
    ret = -3;

#else
  ret = -2;
#endif
#else
  (void)size;
#endif

  return ret;
}

void *vpx_memalign(size_t align, size_t size) {
  void *addr,
       * x = NULL;

#if CONFIG_MEM_MANAGER
  int number_aau;

  if (vpx_mm_create_heap_memory() < 0) {
    _P(printf("[vpx][mm] ERROR vpx_memalign() Couldn't create memory for Heap.\n");)
  }

  number_aau = ((size + align - 1 + ADDRESS_STORAGE_SIZE) >>
                SHIFT_HMM_ADDR_ALIGN_UNIT) + 1;

  addr = hmm_alloc(&hmm_d, number_aau);
#else
  addr = VPX_MALLOC_L(size + align - 1 + ADDRESS_STORAGE_SIZE);
#endif /*CONFIG_MEM_MANAGER*/

  if (addr) {
    x = align_addr((unsigned char *)addr + ADDRESS_STORAGE_SIZE, (int)align);
    /* save the actual malloc address */
    ((size_t *)x)[-1] = (size_t)addr;
  }

  return x;
}

void *vpx_malloc(size_t size) {
  return vpx_memalign(DEFAULT_ALIGNMENT, size);
}

void *vpx_calloc(size_t num, size_t size) {
  void *x;

  x = vpx_memalign(DEFAULT_ALIGNMENT, num * size);

  if (x)
    VPX_MEMSET_L(x, 0, num * size);

  return x;
}

void *vpx_realloc(void *memblk, size_t size) {
  void *addr,
       * new_addr = NULL;
  int align = DEFAULT_ALIGNMENT;

  /*
  The realloc() function changes the size of the object pointed to by
  ptr to the size specified by size, and returns a pointer to the
  possibly moved block. The contents are unchanged up to the lesser
  of the new and old sizes. If ptr is null, realloc() behaves like
  malloc() for the specified size. If size is zero (0) and ptr is
  not a null pointer, the object pointed to is freed.
  */
  if (!memblk)
    new_addr = vpx_malloc(size);
  else if (!size)
    vpx_free(memblk);
  else {
    addr   = (void *)(((size_t *)memblk)[-1]);
    memblk = NULL;

#if CONFIG_MEM_MANAGER
    new_addr = vpx_mm_realloc(addr, size + align + ADDRESS_STORAGE_SIZE);
#else
    new_addr = VPX_REALLOC_L(addr, size + align + ADDRESS_STORAGE_SIZE);
#endif

    if (new_addr) {
      addr = new_addr;
      new_addr = (void *)(((size_t)
                           ((unsigned char *)new_addr + ADDRESS_STORAGE_SIZE) + (align - 1)) &
                          (size_t) - align);
      /* save the actual malloc address */
      ((size_t *)new_addr)[-1] = (size_t)addr;
    }
  }

  return new_addr;
}

void vpx_free(void *memblk) {
  if (memblk) {
    void *addr = (void *)(((size_t *)memblk)[-1]);
#if CONFIG_MEM_MANAGER
    hmm_free(&hmm_d, addr);
#else
    VPX_FREE_L(addr);
#endif
  }
}

#if CONFIG_MEM_TRACKER
void *xvpx_memalign(size_t align, size_t size, char *file, int line) {
#if TRY_BOUNDS_CHECK
  unsigned char *x_bounds;
#endif

  void *x;

  if (g_alloc_count == 0) {
#if TRY_BOUNDS_CHECK
    int i_rv = vpx_memory_tracker_init(BOUNDS_CHECK_PAD_SIZE, BOUNDS_CHECK_VALUE);
#else
    int i_rv = vpx_memory_tracker_init(0, 0);
#endif

    if (i_rv < 0) {
      _P(printf("ERROR xvpx_malloc MEM_TRACK_USAGE error vpx_memory_tracker_init().\n");)
    }
  }

#if TRY_BOUNDS_CHECK
  {
    int i;
    unsigned int tempme = BOUNDS_CHECK_VALUE;

    x_bounds = vpx_memalign(align, size + (BOUNDS_CHECK_PAD_SIZE * 2));

    if (x_bounds) {
      /*we're aligning the address twice here but to keep things
        consistent we want to have the padding come before the stored
        address so no matter what free function gets called we will
        attempt to free the correct address*/
      x_bounds = (unsigned char *)(((size_t *)x_bounds)[-1]);
      x = align_addr(x_bounds + BOUNDS_CHECK_PAD_SIZE + ADDRESS_STORAGE_SIZE,
                     (int)align);
      /* save the actual malloc address */
      ((size_t *)x)[-1] = (size_t)x_bounds;

      for (i = 0; i < BOUNDS_CHECK_PAD_SIZE; i += sizeof(unsigned int)) {
        VPX_MEMCPY_L(x_bounds + i, &tempme, sizeof(unsigned int));
        VPX_MEMCPY_L((unsigned char *)x + size + i,
                     &tempme, sizeof(unsigned int));
      }
    } else
      x = NULL;
  }
#else
  x = vpx_memalign(align, size);
#endif /*TRY_BOUNDS_CHECK*/

  g_alloc_count++;

  vpx_memory_tracker_add((size_t)x, (unsigned int)size, file, line, 1);

  return x;
}

void *xvpx_malloc(size_t size, char *file, int line) {
  return xvpx_memalign(DEFAULT_ALIGNMENT, size, file, line);
}

void *xvpx_calloc(size_t num, size_t size, char *file, int line) {
  void *x = xvpx_memalign(DEFAULT_ALIGNMENT, num * size, file, line);

  if (x)
    VPX_MEMSET_L(x, 0, num * size);

  return x;
}

void *xvpx_realloc(void *memblk, size_t size, char *file, int line) {
  struct mem_block *p = NULL;
  int orig_size = 0,
      orig_line = 0;
  char *orig_file = NULL;

#if TRY_BOUNDS_CHECK
  unsigned char *x_bounds = memblk ?
                            (unsigned char *)(((size_t *)memblk)[-1]) :
                            NULL;
#endif

  void *x;

  if (g_alloc_count == 0) {
#if TRY_BOUNDS_CHECK

    if (!vpx_memory_tracker_init(BOUNDS_CHECK_PAD_SIZE, BOUNDS_CHECK_VALUE))
#else
    if (!vpx_memory_tracker_init(0, 0))
#endif
    {
      _P(printf("ERROR xvpx_malloc MEM_TRACK_USAGE error vpx_memory_tracker_init().\n");)
    }
  }

  if ((p = vpx_memory_tracker_find((size_t)memblk))) {
    orig_size = p->size;
    orig_file = p->file;
    orig_line = p->line;
  }

#if TRY_BOUNDS_CHECK_ON_FREE
  vpx_memory_tracker_check_integrity(file, line);
#endif

  /* have to do this regardless of success, because
   * the memory that does get realloc'd may change
   * the bounds values of this block
   */
  vpx_memory_tracker_remove((size_t)memblk);

#if TRY_BOUNDS_CHECK
  {
    int i;
    unsigned int tempme = BOUNDS_CHECK_VALUE;

    x_bounds = vpx_realloc(memblk, size + (BOUNDS_CHECK_PAD_SIZE * 2));

    if (x_bounds) {
      x_bounds = (unsigned char *)(((size_t *)x_bounds)[-1]);
      x = align_addr(x_bounds + BOUNDS_CHECK_PAD_SIZE + ADDRESS_STORAGE_SIZE,
                     (int)DEFAULT_ALIGNMENT);
      /* save the actual malloc address */
      ((size_t *)x)[-1] = (size_t)x_bounds;

      for (i = 0; i < BOUNDS_CHECK_PAD_SIZE; i += sizeof(unsigned int)) {
        VPX_MEMCPY_L(x_bounds + i, &tempme, sizeof(unsigned int));
        VPX_MEMCPY_L((unsigned char *)x + size + i,
                     &tempme, sizeof(unsigned int));
      }
    } else
      x = NULL;
  }
#else
  x = vpx_realloc(memblk, size);
#endif /*TRY_BOUNDS_CHECK*/

  if (!memblk) ++g_alloc_count;

  if (x)
    vpx_memory_tracker_add((size_t)x, (unsigned int)size, file, line, 1);
  else
    vpx_memory_tracker_add((size_t)memblk, orig_size, orig_file, orig_line, 1);

  return x;
}

void xvpx_free(void *p_address, char *file, int line) {
#if TRY_BOUNDS_CHECK
  unsigned char *p_bounds_address = (unsigned char *)p_address;
  /*p_bounds_address -= BOUNDS_CHECK_PAD_SIZE;*/
#endif

#if !TRY_BOUNDS_CHECK_ON_FREE
  (void)file;
  (void)line;
#endif

  if (p_address) {
#if TRY_BOUNDS_CHECK_ON_FREE
    vpx_memory_tracker_check_integrity(file, line);
#endif

    /* if the addr isn't found in the list, assume it was allocated via
     * vpx_ calls not xvpx_, therefore it does not contain any padding
     */
    if (vpx_memory_tracker_remove((size_t)p_address) == -2) {
      p_bounds_address = p_address;
      _P(fprintf(stderr, "[vpx_mem][xvpx_free] addr: %p not found in"
                 " list; freed from file:%s"
                 " line:%d\n", p_address, file, line));
    } else
      --g_alloc_count;

#if TRY_BOUNDS_CHECK
    vpx_free(p_bounds_address);
#else
    vpx_free(p_address);
#endif

    if (!g_alloc_count)
      vpx_memory_tracker_destroy();
  }
}

#endif /*CONFIG_MEM_TRACKER*/

#if CONFIG_MEM_CHECKS
#if defined(VXWORKS)
#include <task_lib.h> /*for task_delay()*/
/* This function is only used to get a stack trace of the player
object so we can se where we are having a problem. */
static int get_my_tt(int task) {
  tt(task);

  return 0;
}

static void vx_sleep(int msec) {
  int ticks_to_sleep = 0;

  if (msec) {
    int msec_per_tick = 1000 / sys_clk_rate_get();

    if (msec < msec_per_tick)
      ticks_to_sleep++;
    else
      ticks_to_sleep = msec / msec_per_tick;
  }

  task_delay(ticks_to_sleep);
}
#endif
#endif

void *vpx_memcpy(void *dest, const void *source, size_t length) {
#if CONFIG_MEM_CHECKS

  if (((int)dest < 0x4000) || ((int)source < 0x4000)) {
    _P(printf("WARNING: vpx_memcpy dest:0x%x source:0x%x len:%d\n", (int)dest, (int)source, length);)

#if defined(VXWORKS)
    sp(get_my_tt, task_id_self(), 0, 0, 0, 0, 0, 0, 0, 0);

    vx_sleep(10000);
#endif
  }

#endif

  return VPX_MEMCPY_L(dest, source, length);
}

void *vpx_memset(void *dest, int val, size_t length) {
#if CONFIG_MEM_CHECKS

  if ((int)dest < 0x4000) {
    _P(printf("WARNING: vpx_memset dest:0x%x val:%d len:%d\n", (int)dest, val, length);)

#if defined(VXWORKS)
    sp(get_my_tt, task_id_self(), 0, 0, 0, 0, 0, 0, 0, 0);

    vx_sleep(10000);
#endif
  }

#endif

  return VPX_MEMSET_L(dest, val, length);
}

#if CONFIG_VP9 && CONFIG_VP9_HIGHBITDEPTH
void *vpx_memset16(void *dest, int val, size_t length) {
#if CONFIG_MEM_CHECKS
  if ((int)dest < 0x4000) {
    _P(printf("WARNING: vpx_memset dest:0x%x val:%d len:%d\n",
              (int)dest, val, length);)

#if defined(VXWORKS)
    sp(get_my_tt, task_id_self(), 0, 0, 0, 0, 0, 0, 0, 0);

    vx_sleep(10000);
#endif
  }
#endif
  int i;
  void *orig = dest;
  uint16_t *dest16 = dest;
  for (i = 0; i < length; i++)
    *dest16++ = val;
  return orig;
}
#endif  // CONFIG_VP9 && CONFIG_VP9_HIGHBITDEPTH

void *vpx_memmove(void *dest, const void *src, size_t count) {
#if CONFIG_MEM_CHECKS

  if (((int)dest < 0x4000) || ((int)src < 0x4000)) {
    _P(printf("WARNING: vpx_memmove dest:0x%x src:0x%x count:%d\n", (int)dest, (int)src, count);)

#if defined(VXWORKS)
    sp(get_my_tt, task_id_self(), 0, 0, 0, 0, 0, 0, 0, 0);

    vx_sleep(10000);
#endif
  }

#endif

  return VPX_MEMMOVE_L(dest, src, count);
}

#if CONFIG_MEM_MANAGER

static int vpx_mm_create_heap_memory() {
  int i_rv = 0;

  if (!g_mng_memory_allocated) {
#if MM_DYNAMIC_MEMORY
    g_p_mng_memory_raw =
      (unsigned char *)malloc(g_mm_memory_size + HMM_ADDR_ALIGN_UNIT);

    if (g_p_mng_memory_raw) {
      g_p_mng_memory = (unsigned char *)((((unsigned int)g_p_mng_memory_raw) +
                                          HMM_ADDR_ALIGN_UNIT - 1) &
                                         -(int)HMM_ADDR_ALIGN_UNIT);

      _P(printf("[vpx][mm] total memory size:%d g_p_mng_memory_raw:0x%x g_p_mng_memory:0x%x\n"
, g_mm_memory_size + HMM_ADDR_ALIGN_UNIT
, (unsigned int)g_p_mng_memory_raw
, (unsigned int)g_p_mng_memory);)
    } else {
      _P(printf("[vpx][mm] Couldn't allocate memory:%d for vpx memory manager.\n"
, g_mm_memory_size);)

      i_rv = -1;
    }

    if (g_p_mng_memory)
#endif
    {
      int chunk_size = 0;

      g_mng_memory_allocated = 1;

      hmm_init(&hmm_d);

      chunk_size = g_mm_memory_size >> SHIFT_HMM_ADDR_ALIGN_UNIT;

      chunk_size -= DUMMY_END_BLOCK_BAUS;

      _P(printf("[vpx][mm] memory size:%d for vpx memory manager. g_p_mng_memory:0x%x  chunk_size:%d\n"
, g_mm_memory_size
, (unsigned int)g_p_mng_memory
, chunk_size);)

      hmm_new_chunk(&hmm_d, (void *)g_p_mng_memory, chunk_size);
    }

#if MM_DYNAMIC_MEMORY
    else {
      _P(printf("[vpx][mm] Couldn't allocate memory:%d for vpx memory manager.\n"
, g_mm_memory_size);)

      i_rv = -1;
    }

#endif
  }

  return i_rv;
}

static void *vpx_mm_realloc(void *memblk, size_t size) {
  void *p_ret = NULL;

  if (vpx_mm_create_heap_memory() < 0) {
    _P(printf("[vpx][mm] ERROR vpx_mm_realloc() Couldn't create memory for Heap.\n");)
  } else {
    int i_rv = 0;
    int old_num_aaus;
    int new_num_aaus;

    old_num_aaus = hmm_true_size(memblk);
    new_num_aaus = (size >> SHIFT_HMM_ADDR_ALIGN_UNIT) + 1;

    if (old_num_aaus == new_num_aaus) {
      p_ret = memblk;
    } else {
      i_rv = hmm_resize(&hmm_d, memblk, new_num_aaus);

      if (i_rv == 0) {
        p_ret = memblk;
      } else {
        /* Error. Try to malloc and then copy data. */
        void *p_from_malloc;

        new_num_aaus = (size >> SHIFT_HMM_ADDR_ALIGN_UNIT) + 1;
        p_from_malloc  = hmm_alloc(&hmm_d, new_num_aaus);

        if (p_from_malloc) {
          vpx_memcpy(p_from_malloc, memblk, size);
          hmm_free(&hmm_d, memblk);

          p_ret = p_from_malloc;
        }
      }
    }
  }

  return p_ret;
}
#endif /*CONFIG_MEM_MANAGER*/

#if USE_GLOBAL_FUNCTION_POINTERS
# if CONFIG_MEM_TRACKER
extern int vpx_memory_tracker_set_functions(g_malloc_func g_malloc_l
, g_calloc_func g_calloc_l
, g_realloc_func g_realloc_l
, g_free_func g_free_l
, g_memcpy_func g_memcpy_l
, g_memset_func g_memset_l
, g_memmove_func g_memmove_l);
# endif
#endif /*USE_GLOBAL_FUNCTION_POINTERS*/
int vpx_mem_set_functions(g_malloc_func g_malloc_l
, g_calloc_func g_calloc_l
, g_realloc_func g_realloc_l
, g_free_func g_free_l
, g_memcpy_func g_memcpy_l
, g_memset_func g_memset_l
, g_memmove_func g_memmove_l) {
#if USE_GLOBAL_FUNCTION_POINTERS

  /* If use global functions is turned on then the
  application must set the global functions before
  it does anything else or vpx_mem will have
  unpredictable results. */
  if (!g_func) {
    g_func = (struct GLOBAL_FUNC_POINTERS *)
             g_malloc_l(sizeof(struct GLOBAL_FUNC_POINTERS));

    if (!g_func) {
      return -1;
    }
  }

#if CONFIG_MEM_TRACKER
  {
    int rv = 0;
    rv = vpx_memory_tracker_set_functions(g_malloc_l
, g_calloc_l
, g_realloc_l
, g_free_l
, g_memcpy_l
, g_memset_l
, g_memmove_l);

    if (rv < 0) {
      return rv;
    }
  }
#endif

  g_func->g_malloc  = g_malloc_l;
  g_func->g_calloc  = g_calloc_l;
  g_func->g_realloc = g_realloc_l;
  g_func->g_free    = g_free_l;
  g_func->g_memcpy  = g_memcpy_l;
  g_func->g_memset  = g_memset_l;
  g_func->g_memmove = g_memmove_l;

  return 0;
#else
  (void)g_malloc_l;
  (void)g_calloc_l;
  (void)g_realloc_l;
  (void)g_free_l;
  (void)g_memcpy_l;
  (void)g_memset_l;
  (void)g_memmove_l;
  return -1;
#endif
}

int vpx_mem_unset_functions() {
#if USE_GLOBAL_FUNCTION_POINTERS

  if (g_func) {
    g_free_func temp_free = g_func->g_free;
    temp_free(g_func);
    g_func = NULL;
  }

#endif
  return 0;
}
