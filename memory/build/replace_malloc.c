/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef MOZ_MEMORY
#  error Should not compile this file when MOZ_MEMORY is not set
#endif

#ifndef MOZ_REPLACE_MALLOC
#  error Should not compile this file when replace-malloc is disabled
#endif

#include "mozmemory_wrap.h"

/* Declare all je_* functions */
#define MALLOC_DECL(name, return_type, ...) \
  return_type je_ ## name(__VA_ARGS__);
#include "malloc_decls.h"

#include "mozilla/Likely.h"

/*
 * Windows doesn't come with weak imports as they are possible with
 * LD_PRELOAD or DYLD_INSERT_LIBRARIES on Linux/OSX. On this platform,
 * the replacement functions are defined as variable pointers to the
 * function resolved with GetProcAddress() instead of weak definitions
 * of functions. On Android, the same needs to happen as well, because
 * the Android linker doesn't handle weak linking with non LD_PRELOADed
 * libraries, but LD_PRELOADing is not very convenient on Android, with
 * the zygote.
 */
#ifdef XP_DARWIN
#  define MOZ_REPLACE_WEAK __attribute__((weak_import))
#elif defined(XP_WIN) || defined(MOZ_WIDGET_ANDROID)
#  define MOZ_NO_REPLACE_FUNC_DECL
#elif defined(__GNUC__)
#  define MOZ_REPLACE_WEAK __attribute__((weak))
#endif

#include "replace_malloc.h"

#define MALLOC_DECL(name, return_type, ...) \
    je_ ## name,

static const malloc_table_t malloc_table = {
#include "malloc_decls.h"
};

#ifdef MOZ_NO_REPLACE_FUNC_DECL
#  define MALLOC_DECL(name, return_type, ...) \
    typedef return_type (replace_ ## name ## _impl_t)(__VA_ARGS__); \
    replace_ ## name ## _impl_t *replace_ ## name = NULL;
#  define MALLOC_FUNCS MALLOC_FUNCS_ALL
#  include "malloc_decls.h"

#  ifdef XP_WIN
#    include <windows.h>
static void
replace_malloc_init_funcs()
{
  char replace_malloc_lib[1024];
  if (GetEnvironmentVariableA("MOZ_REPLACE_MALLOC_LIB", (LPSTR)&replace_malloc_lib,
                              sizeof(replace_malloc_lib)) > 0) {
    HMODULE handle = LoadLibraryA(replace_malloc_lib);
    if (handle) {
#define MALLOC_DECL(name, ...) \
  replace_ ## name = (replace_ ## name ## _impl_t *) GetProcAddress(handle, "replace_" # name);

#  define MALLOC_FUNCS MALLOC_FUNCS_ALL
#include "malloc_decls.h"
    }
  }
}
#  elif defined(MOZ_WIDGET_ANDROID)
#    include <dlfcn.h>
#    include <stdlib.h>
static void
replace_malloc_init_funcs()
{
  const char *replace_malloc_lib = getenv("MOZ_REPLACE_MALLOC_LIB");
  if (replace_malloc_lib && *replace_malloc_lib) {
    void *handle = dlopen(replace_malloc_lib, RTLD_LAZY);
    if (handle) {
#define MALLOC_DECL(name, ...) \
  replace_ ## name = (replace_ ## name ## _impl_t *) dlsym(handle, "replace_" # name);

#  define MALLOC_FUNCS MALLOC_FUNCS_ALL
#include "malloc_decls.h"
    }
  }
}
#  else
#    error No implementation for replace_malloc_init_funcs()
#  endif

#endif /* MOZ_NO_REPLACE_FUNC_DECL */

/*
 * Below is the malloc implementation overriding jemalloc and calling the
 * replacement functions if they exist.
 */

/*
 * Malloc implementation functions are MOZ_MEMORY_API, and jemalloc
 * specific functions MOZ_JEMALLOC_API; see mozmemory_wrap.h
 */
#define MALLOC_DECL(name, return_type, ...) \
  MOZ_MEMORY_API return_type name ## _impl(__VA_ARGS__);
#define MALLOC_FUNCS MALLOC_FUNCS_MALLOC
#include "malloc_decls.h"

#define MALLOC_DECL(name, return_type, ...) \
  MOZ_JEMALLOC_API return_type name ## _impl(__VA_ARGS__);
#define MALLOC_FUNCS MALLOC_FUNCS_JEMALLOC
#include "malloc_decls.h"

static int replace_malloc_initialized = 0;
static void
init()
{
#ifdef MOZ_NO_REPLACE_FUNC_DECL
  replace_malloc_init_funcs();
#endif
  // Set this *before* calling replace_init, otherwise if replace_init calls
  // malloc() we'll get an infinite loop.
  replace_malloc_initialized = 1;
  if (replace_init)
    replace_init(&malloc_table);
}

MFBT_API struct ReplaceMallocBridge*
get_bridge(void)
{
  if (MOZ_UNLIKELY(!replace_malloc_initialized))
    init();
  if (MOZ_LIKELY(!replace_get_bridge))
    return NULL;
  return replace_get_bridge();
}

void*
malloc_impl(size_t size)
{
  if (MOZ_UNLIKELY(!replace_malloc_initialized))
    init();
  if (MOZ_LIKELY(!replace_malloc))
    return je_malloc(size);
  return replace_malloc(size);
}

int
posix_memalign_impl(void **memptr, size_t alignment, size_t size)
{
  if (MOZ_UNLIKELY(!replace_malloc_initialized))
    init();
  if (MOZ_LIKELY(!replace_posix_memalign))
    return je_posix_memalign(memptr, alignment, size);
  return replace_posix_memalign(memptr, alignment, size);
}

void*
aligned_alloc_impl(size_t alignment, size_t size)
{
  if (MOZ_UNLIKELY(!replace_malloc_initialized))
    init();
  if (MOZ_LIKELY(!replace_aligned_alloc))
    return je_aligned_alloc(alignment, size);
  return replace_aligned_alloc(alignment, size);
}

void*
calloc_impl(size_t num, size_t size)
{
  if (MOZ_UNLIKELY(!replace_malloc_initialized))
    init();
  if (MOZ_LIKELY(!replace_calloc))
    return je_calloc(num, size);
  return replace_calloc(num, size);
}

void*
realloc_impl(void *ptr, size_t size)
{
  if (MOZ_UNLIKELY(!replace_malloc_initialized))
    init();
  if (MOZ_LIKELY(!replace_realloc))
    return je_realloc(ptr, size);
  return replace_realloc(ptr, size);
}

void
free_impl(void *ptr)
{
  if (MOZ_UNLIKELY(!replace_malloc_initialized))
    init();
  if (MOZ_LIKELY(!replace_free))
    je_free(ptr);
  else
    replace_free(ptr);
}

void*
memalign_impl(size_t alignment, size_t size)
{
  if (MOZ_UNLIKELY(!replace_malloc_initialized))
    init();
  if (MOZ_LIKELY(!replace_memalign))
    return je_memalign(alignment, size);
  return replace_memalign(alignment, size);
}

void*
valloc_impl(size_t size)
{
  if (MOZ_UNLIKELY(!replace_malloc_initialized))
    init();
  if (MOZ_LIKELY(!replace_valloc))
    return je_valloc(size);
  return replace_valloc(size);
}

size_t
malloc_usable_size_impl(usable_ptr_t ptr)
{
  if (MOZ_UNLIKELY(!replace_malloc_initialized))
    init();
  if (MOZ_LIKELY(!replace_malloc_usable_size))
    return je_malloc_usable_size(ptr);
  return replace_malloc_usable_size(ptr);
}

size_t
malloc_good_size_impl(size_t size)
{
  if (MOZ_UNLIKELY(!replace_malloc_initialized))
    init();
  if (MOZ_LIKELY(!replace_malloc_good_size))
    return je_malloc_good_size(size);
  return replace_malloc_good_size(size);
}

void
jemalloc_stats_impl(jemalloc_stats_t *stats)
{
  if (MOZ_UNLIKELY(!replace_malloc_initialized))
    init();
  if (MOZ_LIKELY(!replace_jemalloc_stats))
    je_jemalloc_stats(stats);
  else
    replace_jemalloc_stats(stats);
}

void
jemalloc_purge_freed_pages_impl()
{
  if (MOZ_UNLIKELY(!replace_malloc_initialized))
    init();
  if (MOZ_LIKELY(!replace_jemalloc_purge_freed_pages))
    je_jemalloc_purge_freed_pages();
  else
    replace_jemalloc_purge_freed_pages();
}

void
jemalloc_free_dirty_pages_impl()
{
  if (MOZ_UNLIKELY(!replace_malloc_initialized))
    init();
  if (MOZ_LIKELY(!replace_jemalloc_free_dirty_pages))
    je_jemalloc_free_dirty_pages();
  else
    replace_jemalloc_free_dirty_pages();
}

void
jemalloc_thread_local_arena_impl(jemalloc_bool enabled)
{
  if (MOZ_UNLIKELY(!replace_malloc_initialized))
    init();
  if (MOZ_LIKELY(!replace_jemalloc_thread_local_arena))
    je_jemalloc_thread_local_arena(enabled);
  else
    replace_jemalloc_thread_local_arena(enabled);
}

/* The following comment and definitions are from jemalloc.c: */
#if defined(__GLIBC__) && !defined(__UCLIBC__)

/*
 * glibc provides the RTLD_DEEPBIND flag for dlopen which can make it possible
 * to inconsistently reference libc's malloc(3)-compatible functions
 * (https://bugzilla.mozilla.org/show_bug.cgi?id=493541).
 *
 * These definitions interpose hooks in glibc.  The functions are actually
 * passed an extra argument for the caller return address, which will be
 * ignored.
 */

typedef void (* __free_hook_type)(void *ptr);
typedef void *(* __malloc_hook_type)(size_t size);
typedef void *(* __realloc_hook_type)(void *ptr, size_t size);
typedef void *(* __memalign_hook_type)(size_t alignment, size_t size);

MOZ_MEMORY_API __free_hook_type __free_hook = free_impl;
MOZ_MEMORY_API __malloc_hook_type __malloc_hook = malloc_impl;
MOZ_MEMORY_API __realloc_hook_type __realloc_hook = realloc_impl;
MOZ_MEMORY_API __memalign_hook_type __memalign_hook = memalign_impl;

#endif
