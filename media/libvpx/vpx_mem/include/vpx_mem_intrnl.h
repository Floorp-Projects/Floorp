/*
 *  Copyright (c) 2010 The WebM project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */


#ifndef VPX_MEM_INCLUDE_VPX_MEM_INTRNL_H_
#define VPX_MEM_INCLUDE_VPX_MEM_INTRNL_H_
#include "./vpx_config.h"

#ifndef CONFIG_MEM_MANAGER
# if defined(VXWORKS)
#  define CONFIG_MEM_MANAGER  1 /*include heap manager functionality,*/
/*default: enabled on vxworks*/
# else
#  define CONFIG_MEM_MANAGER  0 /*include heap manager functionality*/
# endif
#endif /*CONFIG_MEM_MANAGER*/

#ifndef CONFIG_MEM_TRACKER
# define CONFIG_MEM_TRACKER     1 /*include xvpx_* calls in the lib*/
#endif

#ifndef CONFIG_MEM_CHECKS
# define CONFIG_MEM_CHECKS      0 /*include some basic safety checks in
vpx_memcpy, _memset, and _memmove*/
#endif

#ifndef USE_GLOBAL_FUNCTION_POINTERS
# define USE_GLOBAL_FUNCTION_POINTERS   0  /*use function pointers instead of compiled functions.*/
#endif

#if CONFIG_MEM_TRACKER
# include "vpx_mem_tracker.h"
# if VPX_MEM_TRACKER_VERSION_CHIEF != 2 || VPX_MEM_TRACKER_VERSION_MAJOR != 5
#  error "vpx_mem requires memory tracker version 2.5 to track memory usage"
# endif
#endif

#define ADDRESS_STORAGE_SIZE      sizeof(size_t)

#ifndef DEFAULT_ALIGNMENT
# if defined(VXWORKS)
#  define DEFAULT_ALIGNMENT        32        /*default addr alignment to use in
calls to vpx_* functions other
than vpx_memalign*/
# else
#  define DEFAULT_ALIGNMENT        (2 * sizeof(void*))  /* NOLINT */
# endif
#endif

#if CONFIG_MEM_TRACKER
# define TRY_BOUNDS_CHECK         1        /*when set to 1 pads each allocation,
integrity can be checked using
vpx_memory_tracker_check_integrity
or on free by defining*/
/*TRY_BOUNDS_CHECK_ON_FREE*/
#else
# define TRY_BOUNDS_CHECK         0
#endif /*CONFIG_MEM_TRACKER*/

#if TRY_BOUNDS_CHECK
# define TRY_BOUNDS_CHECK_ON_FREE 0          /*checks mem integrity on every
free, very expensive*/
# define BOUNDS_CHECK_VALUE       0xdeadbeef /*value stored before/after ea.
mem addr for bounds checking*/
# define BOUNDS_CHECK_PAD_SIZE    32         /*size of the padding before and
after ea allocation to be filled
with BOUNDS_CHECK_VALUE.
this should be a multiple of 4*/
#else
# define BOUNDS_CHECK_VALUE       0
# define BOUNDS_CHECK_PAD_SIZE    0
#endif /*TRY_BOUNDS_CHECK*/

#ifndef REMOVE_PRINTFS
# define REMOVE_PRINTFS 0
#endif

/* Should probably use a vpx_mem logger function. */
#if REMOVE_PRINTFS
# define _P(x)
#else
# define _P(x) x
#endif

/*returns an addr aligned to the byte boundary specified by align*/
#define align_addr(addr,align) (void*)(((size_t)(addr) + ((align) - 1)) & (size_t)-(align))

#endif  // VPX_MEM_INCLUDE_VPX_MEM_INTRNL_H_
