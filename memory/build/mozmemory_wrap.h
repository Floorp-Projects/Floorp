/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozmemory_wrap_h
#define mozmemory_wrap_h

/*
 * This header contains #defines which tweak the names of various memory
 * allocation functions.
 *
 * There are several types of functions related to memory allocation
 * that are meant to be used publicly by the Gecko codebase:
 *
 * - malloc implementation functions:
 *   - malloc
 *   - posix_memalign
 *   - aligned_alloc
 *   - calloc
 *   - realloc
 *   - free
 *   - memalign
 *   - valloc
 *   - malloc_usable_size
 *   - malloc_good_size
 *   Some of these functions are specific to some systems, but for
 *   convenience, they are treated as being cross-platform, and available
 *   as such.
 *
 * - duplication functions:
 *   - strndup
 *   - strdup
 *   - wcsdup (Windows only)
 *
 * - jemalloc specific functions:
 *   - jemalloc_stats
 *   - jemalloc_purge_freed_pages
 *   - jemalloc_free_dirty_pages
 *   - jemalloc_thread_local_arena
 *   (these functions are native to mozjemalloc)
 *
 * These functions are all exported as part of libmozglue (see
 * $(topsrcdir)/mozglue/build/Makefile.in), with a few implementation
 * peculiarities:
 *
 * - On Windows, the malloc implementation functions are all prefixed with
 *   "je_", the duplication functions are prefixed with "wrap_", and jemalloc
 *   specific functions are left unprefixed. All these functions are however
 *   aliased when exporting them, such that the resulting mozglue.dll exports
 *   them unprefixed (see $(topsrcdir)/mozglue/build/mozglue.def.in). The
 *   prefixed malloc implementation and duplication functions are not
 *   exported.
 *
 * - On MacOSX, the system libc has a zone allocator, which allows us to
 *   hook custom malloc implementation functions without exporting them.
 *   The malloc implementation functions are all prefixed with "je_" and used
 *   this way from the custom zone allocator. They are not exported.
 *   Duplication functions are not included, since they will call the custom
 *   zone allocator anyways. Jemalloc-specific functions are also left
 *   unprefixed.
 *
 * - On Android and Gonk, all functions are left unprefixed. Additionally,
 *   C++ allocation functions (operator new/delete) are also exported and
 *   unprefixed.
 *
 * - On other systems (mostly Linux), all functions are left unprefixed.
 *
 * Only Android and Gonk add C++ allocation functions.
 *
 * Proper exporting of the various functions is done with the MOZ_MEMORY_API
 * and MOZ_JEMALLOC_API macros. MOZ_MEMORY_API is meant to be used for malloc
 * implementation and duplication functions, while MOZ_JEMALLOC_API is
 * dedicated to jemalloc specific functions.
 *
 *
 * All these functions are meant to be called with no prefix from Gecko code.
 * In most cases, this is because that's how they are available at runtime.
 * However, on Android, this relies on faulty.lib (the custom dynamic linker)
 * resolving mozglue symbols before libc symbols, which is guaranteed by the
 * way faulty.lib works (it respects the DT_NEEDED order, and libc always
 * appears after mozglue ; which we double check when building anyways)
 *
 *
 * Within libmozglue (when MOZ_MEMORY_IMPL is defined), all the functions
 * should be suffixed with "_impl" both for declarations and use.
 * That is, the implementation declaration for e.g. strdup would look like:
 *   char* strdup_impl(const char *)
 * That implementation would call malloc by using "malloc_impl".
 *
 *
 * When building with replace-malloc support, the above still holds, but
 * the malloc implementation and jemalloc specific functions are the
 * replace-malloc functions from replace_malloc.c.
 *
 * The actual mozjemalloc implementation is prefixed with "je_".
 *
 * Thus, when MOZ_REPLACE_MALLOC is defined, the "_impl" suffixed macros
 * expand to "je_" prefixed function when building mozjemalloc, where
 * MOZ_JEMALLOC_IMPL is defined.
 *
 * In other cases, the "_impl" suffixed macros follow the original scheme,
 * except on Windows and MacOSX, where they would expand to "je_" prefixed
 * functions. Instead, they are left unmodified (malloc_impl expands to
 * malloc_impl).
 */

#ifndef MOZ_MEMORY
#  error Should only include mozmemory_wrap.h when MOZ_MEMORY is set.
#endif

#if defined(MOZ_JEMALLOC_IMPL) && !defined(MOZ_MEMORY_IMPL)
#  define MOZ_MEMORY_IMPL
#endif
#if defined(MOZ_MEMORY_IMPL) && !defined(IMPL_MFBT)
#  ifdef MFBT_API /* mozilla/Types.h was already included */
#    error mozmemory_wrap.h has to be included before mozilla/Types.h when MOZ_MEMORY_IMPL is set and IMPL_MFBT is not.
#  endif
#  define IMPL_MFBT
#endif

#include "mozilla/Types.h"

#ifndef MOZ_EXTERN_C
#ifdef __cplusplus
#define MOZ_EXTERN_C extern "C"
#else
#define MOZ_EXTERN_C
#endif
#endif

#ifdef MOZ_MEMORY_IMPL
#  if defined(MOZ_JEMALLOC_IMPL) && defined(MOZ_REPLACE_MALLOC) && !defined(MOZ_REPLACE_JEMALLOC)
#    define mozmem_malloc_impl(a)     je_ ## a
#    define mozmem_jemalloc_impl(a)   je_ ## a
#  else
#    define MOZ_JEMALLOC_API MOZ_EXTERN_C MFBT_API
#    ifdef MOZ_REPLACE_JEMALLOC
#      define MOZ_MEMORY_API MOZ_EXTERN_C MFBT_API
#      define mozmem_malloc_impl(a)     replace_ ## a
#      define mozmem_jemalloc_impl(a)   replace_ ## a
#    elif (defined(XP_WIN) || defined(XP_DARWIN))
#      if defined(MOZ_REPLACE_MALLOC)
#        define mozmem_malloc_impl(a)   a ## _impl
#      else
#        define mozmem_malloc_impl(a)   je_ ## a
#      endif
#    else
#      define MOZ_MEMORY_API MOZ_EXTERN_C MFBT_API
#      if defined(MOZ_WIDGET_ANDROID) || defined(MOZ_WIDGET_GONK)
#        define MOZ_WRAP_NEW_DELETE
#      endif
#    endif
#  endif
#  ifdef XP_WIN
#    define mozmem_dup_impl(a)      wrap_ ## a
#  endif
#endif

#if !defined(MOZ_MEMORY_IMPL)
#  define MOZ_MEMORY_API MOZ_EXTERN_C MFBT_API
#  define MOZ_JEMALLOC_API MOZ_EXTERN_C MFBT_API
#endif

#ifndef MOZ_MEMORY_API
#  define MOZ_MEMORY_API MOZ_EXTERN_C
#endif
#ifndef MOZ_JEMALLOC_API
#  define MOZ_JEMALLOC_API MOZ_EXTERN_C
#endif

#ifndef mozmem_malloc_impl
#  define mozmem_malloc_impl(a)   a
#endif
#ifndef mozmem_dup_impl
#  define mozmem_dup_impl(a)      a
#endif
#ifndef mozmem_jemalloc_impl
#  define mozmem_jemalloc_impl(a) a
#endif

/* Malloc implementation functions */
#define malloc_impl              mozmem_malloc_impl(malloc)
#define posix_memalign_impl      mozmem_malloc_impl(posix_memalign)
#define aligned_alloc_impl       mozmem_malloc_impl(aligned_alloc)
#define calloc_impl              mozmem_malloc_impl(calloc)
#define realloc_impl             mozmem_malloc_impl(realloc)
#define free_impl                mozmem_malloc_impl(free)
#define memalign_impl            mozmem_malloc_impl(memalign)
#define valloc_impl              mozmem_malloc_impl(valloc)
#define malloc_usable_size_impl  mozmem_malloc_impl(malloc_usable_size)
#define malloc_good_size_impl    mozmem_malloc_impl(malloc_good_size)

/* Duplication functions */
#define strndup_impl   mozmem_dup_impl(strndup)
#define strdup_impl    mozmem_dup_impl(strdup)
#ifdef XP_WIN
#  define wcsdup_impl  mozmem_dup_impl(wcsdup)
#endif

/* String functions */
#ifdef ANDROID
/* Bug 801571 and Bug 879668, libstagefright uses vasprintf, causing malloc()/
 * free() to be mismatched between bionic and mozglue implementation.
 */
#define vasprintf_impl  mozmem_dup_impl(vasprintf)
#define asprintf_impl   mozmem_dup_impl(asprintf)
#endif

/* Jemalloc specific function */
#define jemalloc_stats_impl              mozmem_jemalloc_impl(jemalloc_stats)
#define jemalloc_purge_freed_pages_impl  mozmem_jemalloc_impl(jemalloc_purge_freed_pages)
#define jemalloc_free_dirty_pages_impl   mozmem_jemalloc_impl(jemalloc_free_dirty_pages)
#define jemalloc_thread_local_arena_impl \
          mozmem_jemalloc_impl(jemalloc_thread_local_arena)

#endif /* mozmemory_wrap_h */
