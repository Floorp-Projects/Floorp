/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozmemory_h
#define mozmemory_h

/*
 * This header is meant to be used when the following functions are
 * necessary:
 *   - malloc_good_size (used to be called je_malloc_usable_in_advance)
 *   - jemalloc_stats
 *   - jemalloc_purge_freed_pages
 *   - jemalloc_free_dirty_pages
 */

#ifndef MOZ_MEMORY
#  error Should not include mozmemory.h when MOZ_MEMORY is not set
#endif

#include "mozmemory_wrap.h"
#include "mozilla/Attributes.h"
#include "mozilla/Types.h"
#include "jemalloc_types.h"

MOZ_BEGIN_EXTERN_C

/*
 * On OSX, malloc/malloc.h contains the declaration for malloc_good_size,
 * which will call back in jemalloc, through the zone allocator so just use it.
 */
#ifdef XP_DARWIN
#  include <malloc/malloc.h>
#else
MOZ_MEMORY_API size_t malloc_good_size_impl(size_t size);

/* Note: the MOZ_GLUE_IN_PROGRAM ifdef below is there to avoid -Werror turning
 * the protective if into errors. MOZ_GLUE_IN_PROGRAM is what triggers MFBT_API
 * to use weak imports. */

static MOZ_INLINE size_t _malloc_good_size(size_t size) {
#  if defined(MOZ_GLUE_IN_PROGRAM) && !defined(IMPL_MFBT)
  if (!malloc_good_size)
    return size;
#  endif
  return malloc_good_size_impl(size);
}

#  define malloc_good_size _malloc_good_size
#endif

MOZ_JEMALLOC_API void jemalloc_stats(jemalloc_stats_t *stats);

/*
 * On some operating systems (Mac), we use madvise(MADV_FREE) to hand pages
 * back to the operating system.  On Mac, the operating system doesn't take
 * this memory back immediately; instead, the OS takes it back only when the
 * machine is running out of physical memory.
 *
 * This is great from the standpoint of efficiency, but it makes measuring our
 * actual RSS difficult, because pages which we've MADV_FREE'd shouldn't count
 * against our RSS.
 *
 * This function explicitly purges any MADV_FREE'd pages from physical memory,
 * causing our reported RSS match the amount of memory we're actually using.
 *
 * Note that this call is expensive in two ways.  First, it may be slow to
 * execute, because it may make a number of slow syscalls to free memory.  This
 * function holds the big jemalloc locks, so basically all threads are blocked
 * while this function runs.
 *
 * This function is also expensive in that the next time we go to access a page
 * which we've just explicitly decommitted, the operating system has to attach
 * to it a physical page!  If we hadn't run this function, the OS would have
 * less work to do.
 *
 * If MALLOC_DOUBLE_PURGE is not defined, this function does nothing.
 */
MOZ_JEMALLOC_API void jemalloc_purge_freed_pages();

/*
 * Free all unused dirty pages in all arenas. Calling this function will slow
 * down subsequent allocations so it is recommended to use it only when
 * memory needs to be reclaimed at all costs (see bug 805855). This function
 * provides functionality similar to mallctl("arenas.purge") in jemalloc 3.
 */
MOZ_JEMALLOC_API void jemalloc_free_dirty_pages();

MOZ_END_EXTERN_C

#endif /* mozmemory_h */
