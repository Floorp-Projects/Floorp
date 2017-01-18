/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozmemory_wrap.h"

#include <stdlib.h>
#include <malloc/malloc.h>
#include "mozilla/Assertions.h"

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

/*
 * The following is a OSX zone allocator implementation.
 * /!\ WARNING. It assumes the underlying malloc implementation's
 * malloc_usable_size returns 0 when the given pointer is not owned by
 * the allocator. Sadly, OSX does call zone_size with pointers not
 * owned by the allocator.
 */

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

extern void _malloc_prefork(void);
extern void _malloc_postfork(void);

static void
zone_force_lock(malloc_zone_t *zone)
{
  /* /!\ This calls into mozjemalloc. It works because we're linked in the
   * same library. */
  _malloc_prefork();
}

static void
zone_force_unlock(malloc_zone_t *zone)
{
  /* /!\ This calls into mozjemalloc. It works because we're linked in the
   * same library. */
  _malloc_postfork();
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
