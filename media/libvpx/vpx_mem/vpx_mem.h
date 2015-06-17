/*
 *  Copyright (c) 2010 The WebM project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */


#ifndef VPX_MEM_VPX_MEM_H_
#define VPX_MEM_VPX_MEM_H_

#include "vpx_config.h"
#if defined(__uClinux__)
# include <lddk.h>
#endif

/* vpx_mem version info */
#define vpx_mem_version "2.2.1.5"

#define VPX_MEM_VERSION_CHIEF 2
#define VPX_MEM_VERSION_MAJOR 2
#define VPX_MEM_VERSION_MINOR 1
#define VPX_MEM_VERSION_PATCH 5
/* end - vpx_mem version info */

#ifndef VPX_TRACK_MEM_USAGE
# define VPX_TRACK_MEM_USAGE       0  /* enable memory tracking/integrity checks */
#endif
#ifndef VPX_CHECK_MEM_FUNCTIONS
# define VPX_CHECK_MEM_FUNCTIONS   0  /* enable basic safety checks in _memcpy,
_memset, and _memmove */
#endif
#ifndef REPLACE_BUILTIN_FUNCTIONS
# define REPLACE_BUILTIN_FUNCTIONS 0  /* replace builtin functions with their
vpx_ equivalents */
#endif

#include <stdlib.h>
#include <stddef.h>

#if defined(__cplusplus)
extern "C" {
#endif

  /*
      vpx_mem_get_version()
      provided for runtime version checking. Returns an unsigned int of the form
      CHIEF | MAJOR | MINOR | PATCH, where the chief version number is the high
      order byte.
  */
  unsigned int vpx_mem_get_version(void);

  /*
      vpx_mem_set_heap_size(size_t size)
        size - size in bytes for the memory manager to allocate for its heap
      Sets the memory manager's initial heap size
      Return:
        0: on success
        -1: if memory manager calls have not been included in the vpx_mem lib
        -2: if the memory manager has been compiled to use static memory
        -3: if the memory manager has already allocated its heap
  */
  int vpx_mem_set_heap_size(size_t size);

  void *vpx_memalign(size_t align, size_t size);
  void *vpx_malloc(size_t size);
  void *vpx_calloc(size_t num, size_t size);
  void *vpx_realloc(void *memblk, size_t size);
  void vpx_free(void *memblk);

  void *vpx_memcpy(void *dest, const void *src, size_t length);
  void *vpx_memset(void *dest, int val, size_t length);
#if CONFIG_VP9 && CONFIG_VP9_HIGHBITDEPTH
  void *vpx_memset16(void *dest, int val, size_t length);
#endif
  void *vpx_memmove(void *dest, const void *src, size_t count);

  /* special memory functions */
  void *vpx_mem_alloc(int id, size_t size, size_t align);
  void vpx_mem_free(int id, void *mem, size_t size);

  /* Wrappers to standard library functions. */
  typedef void *(* g_malloc_func)(size_t);
  typedef void *(* g_calloc_func)(size_t, size_t);
  typedef void *(* g_realloc_func)(void *, size_t);
  typedef void (* g_free_func)(void *);
  typedef void *(* g_memcpy_func)(void *, const void *, size_t);
  typedef void *(* g_memset_func)(void *, int, size_t);
  typedef void *(* g_memmove_func)(void *, const void *, size_t);

  int vpx_mem_set_functions(g_malloc_func g_malloc_l
, g_calloc_func g_calloc_l
, g_realloc_func g_realloc_l
, g_free_func g_free_l
, g_memcpy_func g_memcpy_l
, g_memset_func g_memset_l
, g_memmove_func g_memmove_l);
  int vpx_mem_unset_functions(void);


  /* some defines for backward compatibility */
#define DMEM_GENERAL 0

// (*)<

#if REPLACE_BUILTIN_FUNCTIONS
# ifndef __VPX_MEM_C__
#  define memalign vpx_memalign
#  define malloc   vpx_malloc
#  define calloc   vpx_calloc
#  define realloc  vpx_realloc
#  define free     vpx_free
#  define memcpy   vpx_memcpy
#  define memmove  vpx_memmove
#  define memset   vpx_memset
# endif
#endif

#if CONFIG_MEM_TRACKER
#include <stdarg.h>
  /*from vpx_mem/vpx_mem_tracker.c*/
  extern void vpx_memory_tracker_dump();
  extern void vpx_memory_tracker_check_integrity(char *file, unsigned int line);
  extern int vpx_memory_tracker_set_log_type(int type, char *option);
  extern int vpx_memory_tracker_set_log_func(void *userdata,
                                             void(*logfunc)(void *userdata,
                                                            const char *fmt, va_list args));
# ifndef __VPX_MEM_C__
#  define vpx_memalign(align, size) xvpx_memalign((align), (size), __FILE__, __LINE__)
#  define vpx_malloc(size)          xvpx_malloc((size), __FILE__, __LINE__)
#  define vpx_calloc(num, size)     xvpx_calloc(num, size, __FILE__, __LINE__)
#  define vpx_realloc(addr, size)   xvpx_realloc(addr, size, __FILE__, __LINE__)
#  define vpx_free(addr)            xvpx_free(addr, __FILE__, __LINE__)
#  define vpx_memory_tracker_check_integrity() vpx_memory_tracker_check_integrity(__FILE__, __LINE__)
#  define vpx_mem_alloc(id,size,align) xvpx_mem_alloc(id, size, align, __FILE__, __LINE__)
#  define vpx_mem_free(id,mem,size) xvpx_mem_free(id, mem, size, __FILE__, __LINE__)
# endif

  void *xvpx_memalign(size_t align, size_t size, char *file, int line);
  void *xvpx_malloc(size_t size, char *file, int line);
  void *xvpx_calloc(size_t num, size_t size, char *file, int line);
  void *xvpx_realloc(void *memblk, size_t size, char *file, int line);
  void xvpx_free(void *memblk, char *file, int line);
  void *xvpx_mem_alloc(int id, size_t size, size_t align, char *file, int line);
  void xvpx_mem_free(int id, void *mem, size_t size, char *file, int line);

#else
# ifndef __VPX_MEM_C__
#  define vpx_memory_tracker_dump()
#  define vpx_memory_tracker_check_integrity()
#  define vpx_memory_tracker_set_log_type(t,o) 0
#  define vpx_memory_tracker_set_log_func(u,f) 0
# endif
#endif

#if !VPX_CHECK_MEM_FUNCTIONS
# ifndef __VPX_MEM_C__
#  include <string.h>
#  define vpx_memcpy  memcpy
#  define vpx_memset  memset
#  define vpx_memmove memmove
# endif
#endif

#ifdef VPX_MEM_PLTFRM
# include VPX_MEM_PLTFRM
#endif

#if defined(__cplusplus)
}
#endif

#endif  // VPX_MEM_VPX_MEM_H_
