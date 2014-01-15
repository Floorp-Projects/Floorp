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

#  include "jemalloc_types.h"

#  if defined(__linux__) && (!defined(MOZ_MEMORY_ANDROID) || ANDROID_VERSION < 19)
typedef void * usable_ptr_t;
#  else
typedef const void * usable_ptr_t;
#  endif

#  define MALLOC_FUNCS_MALLOC 1
#  define MALLOC_FUNCS_JEMALLOC 2
#  define MALLOC_FUNCS_INIT 4
#  define MALLOC_FUNCS_ALL (MALLOC_FUNCS_INIT | MALLOC_FUNCS_MALLOC | MALLOC_FUNCS_JEMALLOC)

#endif /* malloc_decls_h */

#ifndef MALLOC_FUNCS
#  define MALLOC_FUNCS (MALLOC_FUNCS_MALLOC | MALLOC_FUNCS_JEMALLOC)
#endif

#ifdef MALLOC_DECL
#  if MALLOC_FUNCS & MALLOC_FUNCS_INIT
MALLOC_DECL(init, void, const malloc_table_t *)
#  endif
#  if MALLOC_FUNCS & MALLOC_FUNCS_MALLOC
MALLOC_DECL(malloc, void *, size_t)
MALLOC_DECL(posix_memalign, int, void **, size_t, size_t)
MALLOC_DECL(aligned_alloc, void *, size_t, size_t)
MALLOC_DECL(calloc, void *, size_t, size_t)
MALLOC_DECL(realloc, void *, void *, size_t)
MALLOC_DECL(free, void, void *)
MALLOC_DECL(memalign, void *, size_t, size_t)
MALLOC_DECL(valloc, void *, size_t)
MALLOC_DECL(malloc_usable_size, size_t, usable_ptr_t)
MALLOC_DECL(malloc_good_size, size_t, size_t)
#  endif
#  if MALLOC_FUNCS & MALLOC_FUNCS_JEMALLOC
MALLOC_DECL(jemalloc_stats, void, jemalloc_stats_t *)
MALLOC_DECL(jemalloc_purge_freed_pages, void, void)
MALLOC_DECL(jemalloc_free_dirty_pages, void, void)
#  endif
#endif /* MALLOC_DECL */

#undef MALLOC_DECL
#undef MALLOC_FUNCS
