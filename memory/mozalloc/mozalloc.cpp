/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * vim: sw=4 ts=4 et :
 */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include <stddef.h>             // for size_t
#include <stdint.h>             // for uint32_t

// Building with USE_STATIC_LIBS = True sets -MT instead of -MD. -MT sets _MT,
// while -MD sets _MT and _DLL.
#if defined(_MT) && !defined(_DLL)
#define MOZ_STATIC_RUNTIME
#endif

#if defined(MOZ_MEMORY) && !defined(MOZ_STATIC_RUNTIME)
// mozalloc.cpp is part of the same library as mozmemory, thus MOZ_MEMORY_IMPL
// is needed.
#define MOZ_MEMORY_IMPL
#include "mozmemory_wrap.h"

#if defined(XP_DARWIN)
#include <malloc/malloc.h> // for malloc_size
#endif

// See mozmemory_wrap.h for more details. This file is part of libmozglue, so
// it needs to use _impl suffixes. However, with libmozglue growing, this is
// becoming cumbersome, so we will likely use a malloc.h wrapper of some sort
// and allow the use of the functions without a _impl suffix.
#define MALLOC_DECL(name, return_type, ...) \
  extern "C" MOZ_MEMORY_API return_type name ## _impl(__VA_ARGS__);
#define MALLOC_FUNCS MALLOC_FUNCS_MALLOC
#include "malloc_decls.h"

#define MALLOC_DECL(name, return_type, ...) \
  extern "C" MFBT_API return_type name ## _impl(__VA_ARGS__);
#define MALLOC_FUNCS MALLOC_FUNCS_EXTRA
#include "malloc_decls.h"

extern "C" MOZ_MEMORY_API char *strdup_impl(const char *);
extern "C" MOZ_MEMORY_API char *strndup_impl(const char *, size_t);

#else
// When jemalloc is disabled, or when building the static runtime variant,
// we need not to use the suffixes.

#if defined(MALLOC_H)
#  include MALLOC_H             // for memalign, valloc, malloc_size, malloc_us
#endif // if defined(MALLOC_H)
#include <stdlib.h>             // for malloc, free
#if defined(XP_UNIX)
#  include <unistd.h>           // for valloc on *BSD
#endif //if defined(XP_UNIX)

#define malloc_impl malloc
#define posix_memalign_impl posix_memalign
#define calloc_impl calloc
#define realloc_impl realloc
#define free_impl free
#define memalign_impl memalign
#define valloc_impl valloc
#define malloc_protect_impl malloc_protect
#define malloc_unprotect_impl malloc_unprotect
#define malloc_usable_size_impl malloc_usable_size
#define strdup_impl strdup
#define strndup_impl strndup

#endif

#include <errno.h>
#include <new>                  // for std::bad_alloc
#include <string.h>

#include <sys/types.h>

#include "mozilla/mozalloc.h"
#include "mozilla/mozalloc_oom.h"  // for mozalloc_handle_oom

#ifdef __GNUC__
#define LIKELY(x)    (__builtin_expect(!!(x), 1))
#define UNLIKELY(x)  (__builtin_expect(!!(x), 0))
#else
#define LIKELY(x)    (x)
#define UNLIKELY(x)  (x)
#endif

#ifndef MOZ_MEMORY
MOZ_BEGIN_EXTERN_C

MFBT_API void
malloc_protect(void* ptr, uint32_t* id)
{
    if (ptr)
        *id = 1;
}

MFBT_API void
malloc_unprotect(void* ptr, uint32_t* id)
{
    *id = 0;
}

MOZ_END_EXTERN_C
#endif

void*
moz_xmalloc(size_t size)
{
    void* ptr = malloc_impl(size);
    if (UNLIKELY(!ptr && size)) {
        mozalloc_handle_oom(size);
        return moz_xmalloc(size);
    }
    return ptr;
}

void*
moz_xcalloc(size_t nmemb, size_t size)
{
    void* ptr = calloc_impl(nmemb, size);
    if (UNLIKELY(!ptr && nmemb && size)) {
        mozalloc_handle_oom(size);
        return moz_xcalloc(nmemb, size);
    }
    return ptr;
}

void*
moz_xrealloc(void* ptr, size_t size)
{
    void* newptr = realloc_impl(ptr, size);
    if (UNLIKELY(!newptr && size)) {
        mozalloc_handle_oom(size);
        return moz_xrealloc(ptr, size);
    }
    return newptr;
}

char*
moz_xstrdup(const char* str)
{
    char* dup = strdup_impl(str);
    if (UNLIKELY(!dup)) {
        mozalloc_handle_oom(0);
        return moz_xstrdup(str);
    }
    return dup;
}

#if defined(HAVE_STRNDUP)
char*
moz_xstrndup(const char* str, size_t strsize)
{
    char* dup = strndup_impl(str, strsize);
    if (UNLIKELY(!dup)) {
        mozalloc_handle_oom(strsize);
        return moz_xstrndup(str, strsize);
    }
    return dup;
}
#endif  // if defined(HAVE_STRNDUP)

#if defined(HAVE_POSIX_MEMALIGN)
int
moz_xposix_memalign(void **ptr, size_t alignment, size_t size)
{
    int err = posix_memalign_impl(ptr, alignment, size);
    if (UNLIKELY(err && ENOMEM == err)) {
        mozalloc_handle_oom(size);
        return moz_xposix_memalign(ptr, alignment, size);
    }
    // else: (0 == err) or (EINVAL == err)
    return err;
}
int
moz_posix_memalign(void **ptr, size_t alignment, size_t size)
{
    int code = posix_memalign_impl(ptr, alignment, size);
    if (code)
        return code;

#if defined(XP_DARWIN)
    // Workaround faulty OSX posix_memalign, which provides memory with the
    // incorrect alignment sometimes, but returns 0 as if nothing was wrong.
    size_t mask = alignment - 1;
    if (((size_t)(*ptr) & mask) != 0) {
        void* old = *ptr;
        code = moz_posix_memalign(ptr, alignment, size);
        free(old);
    }
#endif

    return code;

}
#endif // if defined(HAVE_POSIX_MEMALIGN)

#if defined(HAVE_MEMALIGN)
void*
moz_xmemalign(size_t boundary, size_t size)
{
    void* ptr = memalign_impl(boundary, size);
    if (UNLIKELY(!ptr && EINVAL != errno)) {
        mozalloc_handle_oom(size);
        return moz_xmemalign(boundary, size);
    }
    // non-NULL ptr or errno == EINVAL
    return ptr;
}
#endif // if defined(HAVE_MEMALIGN)

#if defined(HAVE_VALLOC)
void*
moz_xvalloc(size_t size)
{
    void* ptr = valloc_impl(size);
    if (UNLIKELY(!ptr)) {
        mozalloc_handle_oom(size);
        return moz_xvalloc(size);
    }
    return ptr;
}
#endif // if defined(HAVE_VALLOC)

#ifndef MOZ_STATIC_RUNTIME
size_t
moz_malloc_usable_size(void *ptr)
{
    if (!ptr)
        return 0;

#if defined(XP_DARWIN)
    return malloc_size(ptr);
#elif defined(HAVE_MALLOC_USABLE_SIZE) || defined(MOZ_MEMORY)
    return malloc_usable_size_impl(ptr);
#elif defined(XP_WIN)
    return _msize(ptr);
#else
    return 0;
#endif
}

size_t moz_malloc_size_of(const void *ptr)
{
    return moz_malloc_usable_size((void *)ptr);
}
#endif
