/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozmemory_wrap.h"

#include <stdlib.h>
#include <mach/mach_types.h>
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
 * Definitions of the following structs in malloc/malloc.h might be too old
 * for the built binary to run on newer versions of OSX. So use the newest
 * possible version of those structs.
 */
typedef struct _malloc_zone_t {
  void *reserved1;
  void *reserved2;
  size_t (*size)(struct _malloc_zone_t *, const void *);
  void *(*malloc)(struct _malloc_zone_t *, size_t);
  void *(*calloc)(struct _malloc_zone_t *, size_t, size_t);
  void *(*valloc)(struct _malloc_zone_t *, size_t);
  void (*free)(struct _malloc_zone_t *, void *);
  void *(*realloc)(struct _malloc_zone_t *, void *, size_t);
  void (*destroy)(struct _malloc_zone_t *);
  const char *zone_name;
  unsigned (*batch_malloc)(struct _malloc_zone_t *, size_t, void **, unsigned);
  void (*batch_free)(struct _malloc_zone_t *, void **, unsigned);
  struct malloc_introspection_t *introspect;
  unsigned version;
  void *(*memalign)(struct _malloc_zone_t *, size_t, size_t);
  void (*free_definite_size)(struct _malloc_zone_t *, void *, size_t);
  size_t (*pressure_relief)(struct _malloc_zone_t *, size_t);
} malloc_zone_t;

typedef struct {
  vm_address_t address;
  vm_size_t size;
} vm_range_t;

typedef struct malloc_statistics_t {
  unsigned blocks_in_use;
  size_t size_in_use;
  size_t max_size_in_use;
  size_t size_allocated;
} malloc_statistics_t;

typedef kern_return_t memory_reader_t(task_t, vm_address_t, vm_size_t, void **);

typedef void vm_range_recorder_t(task_t, void *, unsigned type, vm_range_t *, unsigned);

typedef struct malloc_introspection_t {
  kern_return_t (*enumerator)(task_t, void *, unsigned, vm_address_t, memory_reader_t, vm_range_recorder_t);
  size_t (*good_size)(malloc_zone_t *, size_t);
  boolean_t (*check)(malloc_zone_t *);
  void (*print)(malloc_zone_t *, boolean_t);
  void (*log)(malloc_zone_t *, void *);
  void (*force_lock)(malloc_zone_t *);
  void (*force_unlock)(malloc_zone_t *);
  void (*statistics)(malloc_zone_t *, malloc_statistics_t *);
  boolean_t (*zone_locked)(malloc_zone_t *);
  boolean_t (*enable_discharge_checking)(malloc_zone_t *);
  boolean_t (*disable_discharge_checking)(malloc_zone_t *);
  void (*discharge)(malloc_zone_t *, void *);
#ifdef __BLOCKS__
  void (*enumerate_discharged_pointers)(malloc_zone_t *, void (^)(void *, void *));
#else
  void *enumerate_unavailable_without_blocks;
#endif
  void (*reinit_lock)(malloc_zone_t *);
} malloc_introspection_t;

extern kern_return_t malloc_get_all_zones(task_t, memory_reader_t, vm_address_t **, unsigned *);

extern malloc_zone_t *malloc_default_zone(void);

extern void malloc_zone_register(malloc_zone_t *zone);

extern void malloc_zone_unregister(malloc_zone_t *zone);

extern malloc_zone_t *malloc_default_purgeable_zone(void);

extern malloc_zone_t* malloc_zone_from_ptr(const void* ptr);

extern void malloc_zone_free(malloc_zone_t* zone, void* ptr);

extern void* malloc_zone_realloc(malloc_zone_t* zone, void* ptr, size_t size);

/*
 * The following is a OSX zone allocator implementation.
 * /!\ WARNING. It assumes the underlying malloc implementation's
 * malloc_usable_size returns 0 when the given pointer is not owned by
 * the allocator. Sadly, OSX does call zone_size with pointers not
 * owned by the allocator.
 */

static size_t
zone_size(malloc_zone_t *zone, const void *ptr)
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

  // Sometimes, system libraries call malloc_zone_* functions with the wrong
  // zone (e.g. CoreFoundation does). In that case, we need to find the real
  // one. We can't call libSystem's realloc directly because we're exporting
  // realloc from libmozglue and we'd pick that one, so we manually find the
  // right zone and realloc with it.
  malloc_zone_t* real_zone = malloc_zone_from_ptr(ptr);
  // The system allocator crashes voluntarily by default when a pointer can't
  // be traced back to a zone. Do the same.
  MOZ_RELEASE_ASSERT(real_zone);
  MOZ_RELEASE_ASSERT(real_zone != zone);
  return malloc_zone_realloc(real_zone, ptr, size);
}

static void
other_zone_free(malloc_zone_t* original_zone, void* ptr)
{
  // Sometimes, system libraries call malloc_zone_* functions with the wrong
  // zone (e.g. CoreFoundation does). In that case, we need to find the real
  // one. We can't call libSystem's free directly because we're exporting
  // free from libmozglue and we'd pick that one, so we manually find the
  // right zone and free with it.
  malloc_zone_t* zone = malloc_zone_from_ptr(ptr);
  // The system allocator crashes voluntarily by default when a pointer can't
  // be traced back to a zone. Do the same.
  MOZ_RELEASE_ASSERT(zone);
  MOZ_RELEASE_ASSERT(zone != original_zone);
  return malloc_zone_free(zone, ptr);
}

static void
zone_free(malloc_zone_t *zone, void *ptr)
{
  if (malloc_usable_size_impl(ptr)) {
    free_impl(ptr);
    return;
  }
  other_zone_free(zone, ptr);
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
  other_zone_free(zone, ptr);
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

static void
zone_destroy(malloc_zone_t *zone)
{
  /* This function should never be called. */
  MOZ_CRASH();
}

static unsigned
zone_batch_malloc(malloc_zone_t *zone, size_t size, void **results,
    unsigned num_requested)
{
  unsigned i;

  for (i = 0; i < num_requested; i++) {
    results[i] = malloc_impl(size);
    if (!results[i])
      break;
  }

  return i;
}

static void
zone_batch_free(malloc_zone_t *zone, void **to_be_freed,
    unsigned num_to_be_freed)
{
  unsigned i;

  for (i = 0; i < num_to_be_freed; i++) {
    zone_free(zone, to_be_freed[i]);
    to_be_freed[i] = NULL;
  }
}

static size_t
zone_pressure_relief(malloc_zone_t *zone, size_t goal)
{
  return 0;
}

static size_t
zone_good_size(malloc_zone_t *zone, size_t size)
{
  return malloc_good_size_impl(size);
}

static kern_return_t
zone_enumerator(task_t task, void *data, unsigned type_mask,
    vm_address_t zone_address, memory_reader_t reader,
    vm_range_recorder_t recorder)
{
  return KERN_SUCCESS;
}

static boolean_t
zone_check(malloc_zone_t *zone)
{
  return true;
}

static void
zone_print(malloc_zone_t *zone, boolean_t verbose)
{
}

static void
zone_log(malloc_zone_t *zone, void *address)
{
}

extern void _malloc_prefork(void);
extern void _malloc_postfork_child(void);

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
  _malloc_postfork_child();
}

static void
zone_statistics(malloc_zone_t *zone, malloc_statistics_t *stats)
{
  /* We make no effort to actually fill the values */
  stats->blocks_in_use = 0;
  stats->size_in_use = 0;
  stats->max_size_in_use = 0;
  stats->size_allocated = 0;
}

static boolean_t
zone_locked(malloc_zone_t *zone)
{
  /* Pretend no lock is being held */
  return false;
}

static void
zone_reinit_lock(malloc_zone_t *zone)
{
  /* As of OSX 10.12, this function is only used when force_unlock would
   * be used if the zone version were < 9. So just use force_unlock. */
  zone_force_unlock(zone);
}

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


void
register_zone(void)
{
  malloc_zone_t *default_zone = get_default_zone();

  zone.size = zone_size;
  zone.malloc = zone_malloc;
  zone.calloc = zone_calloc;
  zone.valloc = zone_valloc;
  zone.free = zone_free;
  zone.realloc = zone_realloc;
  zone.destroy = zone_destroy;
#ifdef MOZ_REPLACE_MALLOC
  zone.zone_name = "replace_malloc_zone";
#else
  zone.zone_name = "jemalloc_zone";
#endif
  zone.batch_malloc = zone_batch_malloc;
  zone.batch_free = zone_batch_free;
  zone.introspect = &zone_introspect;
  zone.version = 9;
  zone.memalign = zone_memalign;
  zone.free_definite_size = zone_free_definite_size;
  zone.pressure_relief = zone_pressure_relief;
  zone_introspect.enumerator = zone_enumerator;
  zone_introspect.good_size = zone_good_size;
  zone_introspect.check = zone_check;
  zone_introspect.print = zone_print;
  zone_introspect.log = zone_log;
  zone_introspect.force_lock = zone_force_lock;
  zone_introspect.force_unlock = zone_force_unlock;
  zone_introspect.statistics = zone_statistics;
  zone_introspect.zone_locked = zone_locked;
  zone_introspect.enable_discharge_checking = NULL;
  zone_introspect.disable_discharge_checking = NULL;
  zone_introspect.discharge = NULL;
#ifdef __BLOCKS__
  zone_introspect.enumerate_discharged_pointers = NULL;
#else
  zone_introspect.enumerate_unavailable_without_blocks = NULL;
#endif
  zone_introspect.reinit_lock = zone_reinit_lock;

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
