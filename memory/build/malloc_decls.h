/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

/*
 * Helper header to declare all the supported malloc functions.
 * MALLOC_DECL arguments are:
 *   - function name
 *   - return type
 *   - argument types
 */

#ifndef malloc_decls_h
#  define malloc_decls_h

#  include "mozjemalloc_types.h"

#  define MALLOC_FUNCS_MALLOC_BASE 1
#  define MALLOC_FUNCS_MALLOC_EXTRA 2
#  define MALLOC_FUNCS_MALLOC (MALLOC_FUNCS_MALLOC_BASE | \
                               MALLOC_FUNCS_MALLOC_EXTRA)
#  define MALLOC_FUNCS_JEMALLOC 4
#  define MALLOC_FUNCS_INIT 8
#  define MALLOC_FUNCS_BRIDGE 16
#  define MALLOC_FUNCS_ALL (MALLOC_FUNCS_INIT | MALLOC_FUNCS_BRIDGE | \
                            MALLOC_FUNCS_MALLOC | MALLOC_FUNCS_JEMALLOC)

#endif /* malloc_decls_h */

#ifndef MALLOC_FUNCS
#  define MALLOC_FUNCS (MALLOC_FUNCS_MALLOC | MALLOC_FUNCS_JEMALLOC)
#endif

#ifdef MALLOC_DECL
#  if MALLOC_FUNCS & MALLOC_FUNCS_INIT
MALLOC_DECL(init, void, const malloc_table_t *)
#  endif
#  if MALLOC_FUNCS & MALLOC_FUNCS_BRIDGE
MALLOC_DECL(get_bridge, struct ReplaceMallocBridge*)
#  endif
#  if MALLOC_FUNCS & MALLOC_FUNCS_MALLOC_BASE
MALLOC_DECL(malloc, void *, size_t)
MALLOC_DECL(calloc, void *, size_t, size_t)
MALLOC_DECL(realloc, void *, void *, size_t)
MALLOC_DECL(free, void, void *)
MALLOC_DECL(memalign, void *, size_t, size_t)
#  endif
#  if MALLOC_FUNCS & MALLOC_FUNCS_MALLOC_EXTRA
MALLOC_DECL(posix_memalign, int, void **, size_t, size_t)
MALLOC_DECL(aligned_alloc, void *, size_t, size_t)
MALLOC_DECL(valloc, void *, size_t)
MALLOC_DECL(malloc_usable_size, size_t, usable_ptr_t)
MALLOC_DECL(malloc_good_size, size_t, size_t)
#  endif
#  if MALLOC_FUNCS & MALLOC_FUNCS_JEMALLOC
MALLOC_DECL(jemalloc_stats, void, jemalloc_stats_t *)
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
MALLOC_DECL(jemalloc_purge_freed_pages, void)

/*
 * Free all unused dirty pages in all arenas. Calling this function will slow
 * down subsequent allocations so it is recommended to use it only when
 * memory needs to be reclaimed at all costs (see bug 805855). This function
 * provides functionality similar to mallctl("arenas.purge") in jemalloc 3.
 */
MALLOC_DECL(jemalloc_free_dirty_pages, void)

/*
 * Opt in or out of a thread local arena (bool argument is whether to opt-in
 * (true) or out (false)).
 */
MALLOC_DECL(jemalloc_thread_local_arena, void, bool)

/*
 * Provide information about any allocation enclosing the given address.
 */
MALLOC_DECL(jemalloc_ptr_info, void, const void*, jemalloc_ptr_info_t*)
#  endif

#endif /* MALLOC_DECL */

#undef MALLOC_DECL
#undef MALLOC_FUNCS
