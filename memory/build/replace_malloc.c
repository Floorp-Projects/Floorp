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
#include "mozilla/MacroArgs.h"
#include <errno.h>
#ifndef XP_WIN
#include <unistd.h>
#endif

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

static malloc_table_t replace_malloc_table;

#ifdef MOZ_NO_REPLACE_FUNC_DECL
#  define MALLOC_DECL(name, return_type, ...) \
    typedef return_type (name ## _impl_t)(__VA_ARGS__); \
    name ## _impl_t* replace_ ## name = NULL;
#  define MALLOC_FUNCS (MALLOC_FUNCS_INIT | MALLOC_FUNCS_BRIDGE)
#  include "malloc_decls.h"
#endif

#ifdef XP_WIN
#  include <windows.h>

typedef HMODULE replace_malloc_handle_t;

static replace_malloc_handle_t
replace_malloc_handle()
{
  char replace_malloc_lib[1024];
  if (GetEnvironmentVariableA("MOZ_REPLACE_MALLOC_LIB", (LPSTR)&replace_malloc_lib,
                              sizeof(replace_malloc_lib)) > 0) {
    return LoadLibraryA(replace_malloc_lib);
  }
  return NULL;
}

#    define REPLACE_MALLOC_GET_FUNC(handle, name) \
      (name ## _impl_t*) GetProcAddress(handle, "replace_" # name)

#elif defined(MOZ_WIDGET_ANDROID)
#  include <dlfcn.h>
#  include <stdlib.h>

typedef void* replace_malloc_handle_t;

static replace_malloc_handle_t
replace_malloc_handle()
{
  const char *replace_malloc_lib = getenv("MOZ_REPLACE_MALLOC_LIB");
  if (replace_malloc_lib && *replace_malloc_lib) {
    return dlopen(replace_malloc_lib, RTLD_LAZY);
  }
  return NULL;
}

#  define REPLACE_MALLOC_GET_FUNC(handle, name) \
    (name ## _impl_t*) dlsym(handle, "replace_" # name)

#else

#  include <stdbool.h>

typedef bool replace_malloc_handle_t;

static replace_malloc_handle_t
replace_malloc_handle()
{
  return true;
}

#  define REPLACE_MALLOC_GET_FUNC(handle, name) \
    replace_ ## name

#endif

static void replace_malloc_init_funcs();

/*
 * Below is the malloc implementation overriding jemalloc and calling the
 * replacement functions if they exist.
 */

static int replace_malloc_initialized = 0;
static void
init()
{
  replace_malloc_init_funcs();
  // Set this *before* calling replace_init, otherwise if replace_init calls
  // malloc() we'll get an infinite loop.
  replace_malloc_initialized = 1;
  if (replace_init)
    replace_init(&malloc_table);
}

/*
 * Malloc implementation functions are MOZ_MEMORY_API, and jemalloc
 * specific functions MOZ_JEMALLOC_API; see mozmemory_wrap.h
 */
#define MACRO_CALL(a, b) a b
/* Can't use macros recursively, so we need another one doing the same as above. */
#define MACRO_CALL2(a, b) a b

#define ARGS_HELPER(name, ...) MACRO_CALL2( \
  MOZ_PASTE_PREFIX_AND_ARG_COUNT(name, ##__VA_ARGS__), \
  (__VA_ARGS__))
#define TYPED_ARGS0()
#define TYPED_ARGS1(t1) t1 arg1
#define TYPED_ARGS2(t1, t2) TYPED_ARGS1(t1), t2 arg2
#define TYPED_ARGS3(t1, t2, t3) TYPED_ARGS2(t1, t2), t3 arg3

#define ARGS0()
#define ARGS1(t1) arg1
#define ARGS2(t1, t2) ARGS1(t1), arg2
#define ARGS3(t1, t2, t3) ARGS2(t1, t2), arg3

#define GENERIC_MALLOC_DECL_HELPER(name, return, return_type, ...) \
  return_type name ## _impl(ARGS_HELPER(TYPED_ARGS, ##__VA_ARGS__)) \
  { \
    if (MOZ_UNLIKELY(!replace_malloc_initialized)) \
      init(); \
    return replace_malloc_table.name(ARGS_HELPER(ARGS, ##__VA_ARGS__)); \
  }

#define GENERIC_MALLOC_DECL(name, return_type, ...) \
  GENERIC_MALLOC_DECL_HELPER(name, return, return_type, ##__VA_ARGS__)
#define GENERIC_MALLOC_DECL_VOID(name, ...) \
  GENERIC_MALLOC_DECL_HELPER(name, , void, ##__VA_ARGS__)

#define MALLOC_DECL(...) MOZ_MEMORY_API MACRO_CALL(GENERIC_MALLOC_DECL, (__VA_ARGS__))
#define MALLOC_DECL_VOID(...) MOZ_MEMORY_API MACRO_CALL(GENERIC_MALLOC_DECL_VOID, (__VA_ARGS__))
#define MALLOC_FUNCS MALLOC_FUNCS_MALLOC
#include "malloc_decls.h"

#define MALLOC_DECL(...) MOZ_JEMALLOC_API MACRO_CALL(GENERIC_MALLOC_DECL, (__VA_ARGS__))
#define MALLOC_DECL_VOID(...) MOZ_JEMALLOC_API MACRO_CALL(GENERIC_MALLOC_DECL_VOID, (__VA_ARGS__))
#define MALLOC_FUNCS MALLOC_FUNCS_JEMALLOC
#include "malloc_decls.h"

MFBT_API struct ReplaceMallocBridge*
get_bridge(void)
{
  if (MOZ_UNLIKELY(!replace_malloc_initialized))
    init();
  if (MOZ_LIKELY(!replace_get_bridge))
    return NULL;
  return replace_get_bridge();
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
 * posix_memalign, aligned_alloc, memalign and valloc all implement some kind
 * of aligned memory allocation. For convenience, a replace-malloc library can
 * skip defining replace_posix_memalign, replace_aligned_alloc and
 * replace_valloc, and default implementations will be automatically derived
 * from replace_memalign.
 */
static int
default_posix_memalign(void** ptr, size_t alignment, size_t size)
{
  if (size == 0) {
    *ptr = NULL;
    return 0;
  }
  /* alignment must be a power of two and a multiple of sizeof(void *) */
  if (((alignment - 1) & alignment) != 0 || (alignment % sizeof(void *)))
    return EINVAL;
  *ptr = replace_malloc_table.memalign(alignment, size);
  return *ptr ? 0 : ENOMEM;
}

static void*
default_aligned_alloc(size_t alignment, size_t size)
{
  /* size should be a multiple of alignment */
  if (size % alignment)
    return NULL;
  return replace_malloc_table.memalign(alignment, size);
}

// Nb: sysconf() is expensive, but valloc is obsolete and rarely used.
static void*
default_valloc(size_t size)
{
#ifdef XP_WIN
  SYSTEM_INFO si;
  GetSystemInfo(&si);
  size_t page_size = si.dwPageSize;
#else
  size_t page_size = sysconf(_SC_PAGE_SIZE);
#endif
  return replace_malloc_table.memalign(page_size, size);
}

static void
replace_malloc_init_funcs()
{
  replace_malloc_handle_t handle = replace_malloc_handle();
  if (handle) {
#ifdef MOZ_NO_REPLACE_FUNC_DECL
#  define MALLOC_DECL(name, ...) \
    replace_ ## name = REPLACE_MALLOC_GET_FUNC(handle, name);

#  define MALLOC_FUNCS (MALLOC_FUNCS_INIT | MALLOC_FUNCS_BRIDGE)
#  include "malloc_decls.h"
#endif

#define MALLOC_DECL(name, ...) \
  replace_malloc_table.name = REPLACE_MALLOC_GET_FUNC(handle, name);
#include "malloc_decls.h"
  }

  if (!replace_malloc_table.posix_memalign && replace_malloc_table.memalign)
    replace_malloc_table.posix_memalign = default_posix_memalign;

  if (!replace_malloc_table.aligned_alloc && replace_malloc_table.memalign)
    replace_malloc_table.aligned_alloc = default_aligned_alloc;

  if (!replace_malloc_table.valloc && replace_malloc_table.memalign)
    replace_malloc_table.valloc = default_valloc;

#define MALLOC_DECL(name, ...) \
  if (!replace_malloc_table.name) { \
    replace_malloc_table.name = je_ ## name; \
  }
#include "malloc_decls.h"
}
