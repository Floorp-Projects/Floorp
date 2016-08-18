/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef MOZ_MEMORY
#  error Should not compile this file when MOZ_MEMORY is not set
#endif

#ifndef MOZ_REPLACE_MALLOC
#  error Should not compile this file when replace-malloc is disabled
#endif

#ifdef MOZ_SYSTEM_JEMALLOC
#  error Should not compile this file when we want to use native jemalloc
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
  MFBT_API return_type name ## _impl(__VA_ARGS__);
#define MALLOC_FUNCS MALLOC_FUNCS_EXTRA
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

void
malloc_protect_impl(void* ptr, uint32_t* id)
{
  if (MOZ_UNLIKELY(!replace_malloc_initialized))
    init();
  if (MOZ_LIKELY(!replace_malloc_protect))
    je_malloc_protect(ptr, id);
  else
    replace_malloc_protect(ptr, id);
}

void
malloc_unprotect_impl(void* ptr, uint32_t* id)
{
  if (MOZ_UNLIKELY(!replace_malloc_initialized))
    init();
  if (MOZ_LIKELY(!replace_malloc_unprotect))
    je_malloc_unprotect(ptr, id);
  else
    replace_malloc_unprotect(ptr, id);
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

/*
 * The following is a OSX zone allocator implementation.
 * /!\ WARNING. It assumes the underlying malloc implementation's
 * malloc_usable_size returns 0 when the given pointer is not owned by
 * the allocator. Sadly, OSX does call zone_size with pointers not
 * owned by the allocator.
 */

#ifdef XP_DARWIN
#include <stdlib.h>
#include <malloc/malloc.h>
#include "mozilla/Assertions.h"

static size_t
zone_size(malloc_zone_t *zone, void *ptr)
{
  return malloc_usable_size_impl(ptr);
}

static void *
zone_malloc(malloc_zone_t *zone, size_t size)
{
  return malloc_impl(size);
}

static void *
zone_calloc(malloc_zone_t *zone, size_t num, size_t size)
{
  return calloc_impl(num, size);
}

static void *
zone_realloc(malloc_zone_t *zone, void *ptr, size_t size)
{
  if (malloc_usable_size_impl(ptr))
    return realloc_impl(ptr, size);
  return realloc(ptr, size);
}

static void
zone_free(malloc_zone_t *zone, void *ptr)
{
  if (malloc_usable_size_impl(ptr)) {
    free_impl(ptr);
    return;
  }
  free(ptr);
}

static void
zone_free_definite_size(malloc_zone_t *zone, void *ptr, size_t size)
{
  size_t current_size = malloc_usable_size_impl(ptr);
  if (current_size) {
    MOZ_ASSERT(current_size == size);
    free_impl(ptr);
    return;
  }
  free(ptr);
}

static void *
zone_memalign(malloc_zone_t *zone, size_t alignment, size_t size)
{
  void *ptr;
  if (posix_memalign_impl(&ptr, alignment, size) == 0)
    return ptr;
  return NULL;
}

static void *
zone_valloc(malloc_zone_t *zone, size_t size)
{
  return valloc_impl(size);
}

static void *
zone_destroy(malloc_zone_t *zone)
{
  /* This function should never be called. */
  MOZ_CRASH();
}

static size_t
zone_good_size(malloc_zone_t *zone, size_t size)
{
  return malloc_good_size_impl(size);
}

#ifdef MOZ_JEMALLOC

#include "jemalloc/internal/jemalloc_internal.h"

static void
zone_force_lock(malloc_zone_t *zone)
{
  /* /!\ This calls into jemalloc. It works because we're linked in the
   * same library. Stolen from jemalloc's zone.c. */
  if (isthreaded)
    jemalloc_prefork();
}

static void
zone_force_unlock(malloc_zone_t *zone)
{
  /* /!\ This calls into jemalloc. It works because we're linked in the
   * same library. Stolen from jemalloc's zone.c. */
  if (isthreaded)
    jemalloc_postfork_parent();
}

#else

#define JEMALLOC_ZONE_VERSION 6

/* Empty implementations are needed, because fork() calls zone->force_(un)lock
 * unconditionally. */
static void
zone_force_lock(malloc_zone_t *zone)
{
}

static void
zone_force_unlock(malloc_zone_t *zone)
{
}

#endif

static malloc_zone_t zone;
static struct malloc_introspection_t zone_introspect;

static malloc_zone_t *get_default_zone()
{
  malloc_zone_t **zones = NULL;
  unsigned int num_zones = 0;

  /*
   * On OSX 10.12, malloc_default_zone returns a special zone that is not
   * present in the list of registered zones. That zone uses a "lite zone"
   * if one is present (apparently enabled when malloc stack logging is
   * enabled), or the first registered zone otherwise. In practice this
   * means unless malloc stack logging is enabled, the first registered
   * zone is the default.
   * So get the list of zones to get the first one, instead of relying on
   * malloc_default_zone.
   */
  if (KERN_SUCCESS != malloc_get_all_zones(0, NULL, (vm_address_t**) &zones,
                                           &num_zones)) {
    /* Reset the value in case the failure happened after it was set. */
    num_zones = 0;
  }
  if (num_zones) {
    return zones[0];
  }
  return malloc_default_zone();
}


__attribute__((constructor)) void
register_zone(void)
{
  malloc_zone_t *default_zone = get_default_zone();

  zone.size = (void *)zone_size;
  zone.malloc = (void *)zone_malloc;
  zone.calloc = (void *)zone_calloc;
  zone.valloc = (void *)zone_valloc;
  zone.free = (void *)zone_free;
  zone.realloc = (void *)zone_realloc;
  zone.destroy = (void *)zone_destroy;
  zone.zone_name = "replace_malloc_zone";
  zone.batch_malloc = NULL;
  zone.batch_free = NULL;
  zone.introspect = &zone_introspect;
  zone.version = JEMALLOC_ZONE_VERSION;
  zone.memalign = zone_memalign;
  zone.free_definite_size = zone_free_definite_size;
#if (JEMALLOC_ZONE_VERSION >= 8)
  zone.pressure_relief = NULL;
#endif
  zone_introspect.enumerator = NULL;
  zone_introspect.good_size = (void *)zone_good_size;
  zone_introspect.check = NULL;
  zone_introspect.print = NULL;
  zone_introspect.log = NULL;
  zone_introspect.force_lock = (void *)zone_force_lock;
  zone_introspect.force_unlock = (void *)zone_force_unlock;
  zone_introspect.statistics = NULL;
  zone_introspect.zone_locked = NULL;
#if (JEMALLOC_ZONE_VERSION >= 7)
  zone_introspect.enable_discharge_checking = NULL;
  zone_introspect.disable_discharge_checking = NULL;
  zone_introspect.discharge = NULL;
#ifdef __BLOCKS__
  zone_introspect.enumerate_discharged_pointers = NULL;
#else
  zone_introspect.enumerate_unavailable_without_blocks = NULL;
#endif
#endif

  /*
   * The default purgeable zone is created lazily by OSX's libc.  It uses
   * the default zone when it is created for "small" allocations
   * (< 15 KiB), but assumes the default zone is a scalable_zone.  This
   * obviously fails when the default zone is the jemalloc zone, so
   * malloc_default_purgeable_zone is called beforehand so that the
   * default purgeable zone is created when the default zone is still
   * a scalable_zone.
   */
  malloc_zone_t *purgeable_zone = malloc_default_purgeable_zone();

  /* Register the custom zone.  At this point it won't be the default. */
  malloc_zone_register(&zone);

  do {
    /*
     * Unregister and reregister the default zone.  On OSX >= 10.6,
     * unregistering takes the last registered zone and places it at the
     * location of the specified zone.  Unregistering the default zone thus
     * makes the last registered one the default.  On OSX < 10.6,
     * unregistering shifts all registered zones.  The first registered zone
     * then becomes the default.
     */
    malloc_zone_unregister(default_zone);
    malloc_zone_register(default_zone);
    /*
     * On OSX 10.6, having the default purgeable zone appear before the default
     * zone makes some things crash because it thinks it owns the default
     * zone allocated pointers. We thus unregister/re-register it in order to
     * ensure it's always after the default zone. On OSX < 10.6, as
     * unregistering shifts registered zones, this simply removes the purgeable
     * zone from the list and adds it back at the end, after the default zone.
     * On OSX >= 10.6, unregistering replaces the purgeable zone with the last
     * registered zone above, i.e the default zone. Registering it again then
     * puts it at the end, obviously after the default zone.
     */
    malloc_zone_unregister(purgeable_zone);
    malloc_zone_register(purgeable_zone);
    default_zone = get_default_zone();
  } while (default_zone != &zone);
}
#endif
