/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * vim: sw=4 ts=4 et :
 */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_mozalloc_h
#define mozilla_mozalloc_h

/*
 * https://bugzilla.mozilla.org/show_bug.cgi?id=427099
 */

#include <stdlib.h>
#include <string.h>
#if defined(__cplusplus)
#  include <new>
#endif
#include "xpcom-config.h"

#if defined(__cplusplus)
#include "mozilla/fallible.h"
#endif
#include "mozilla/Attributes.h"

#define MOZALLOC_HAVE_XMALLOC

#if defined(MOZALLOC_EXPORT)
/* do nothing: it's been defined to __declspec(dllexport) by
 * mozalloc*.cpp on platforms where that's required. */
#elif defined(XP_WIN)
#  define MOZALLOC_EXPORT __declspec(dllimport)
#elif defined(HAVE_VISIBILITY_ATTRIBUTE)
/* Make sure symbols are still exported even if we're wrapped in a
 * |visibility push(hidden)| blanket. */
#  define MOZALLOC_EXPORT __attribute__ ((visibility ("default")))
#else
#  define MOZALLOC_EXPORT
#endif


#if defined(MOZ_ALWAYS_INLINE_EVEN_DEBUG)
#  define MOZALLOC_INLINE MOZ_ALWAYS_INLINE_EVEN_DEBUG
#elif defined(HAVE_FORCEINLINE)
#  define MOZALLOC_INLINE __forceinline
#else
#  define MOZALLOC_INLINE inline
#endif

/* Workaround build problem with Sun Studio 12 */
#if defined(__SUNPRO_C) || defined(__SUNPRO_CC)
#  undef NS_WARN_UNUSED_RESULT
#  define NS_WARN_UNUSED_RESULT
#  undef NS_ATTR_MALLOC
#  define NS_ATTR_MALLOC
#endif

#if defined(__cplusplus)
extern "C" {
#endif /* ifdef __cplusplus */


/*
 * Each pair of declarations below is analogous to a "standard"
 * allocation function, except that the out-of-memory handling is made
 * explicit.  The |moz_x| versions will never return a NULL pointer;
 * if memory is exhausted, they abort.  The |moz_| versions may return
 * NULL pointers if memory is exhausted: their return value must be
 * checked.
 *
 * All these allocation functions are *guaranteed* to return a pointer
 * to memory allocated in such a way that that memory can be freed by
 * passing that pointer to |moz_free()|.
 */

MOZALLOC_EXPORT
void moz_free(void* ptr);

MOZALLOC_EXPORT void* moz_xmalloc(size_t size)
    NS_ATTR_MALLOC NS_WARN_UNUSED_RESULT;

MOZALLOC_EXPORT
void* moz_malloc(size_t size)
    NS_ATTR_MALLOC NS_WARN_UNUSED_RESULT;


MOZALLOC_EXPORT void* moz_xcalloc(size_t nmemb, size_t size)
    NS_ATTR_MALLOC NS_WARN_UNUSED_RESULT;

MOZALLOC_EXPORT void* moz_calloc(size_t nmemb, size_t size)
    NS_ATTR_MALLOC NS_WARN_UNUSED_RESULT;


MOZALLOC_EXPORT void* moz_xrealloc(void* ptr, size_t size)
    NS_ATTR_MALLOC NS_WARN_UNUSED_RESULT;

MOZALLOC_EXPORT void* moz_realloc(void* ptr, size_t size)
    NS_ATTR_MALLOC NS_WARN_UNUSED_RESULT;


MOZALLOC_EXPORT char* moz_xstrdup(const char* str)
    NS_ATTR_MALLOC NS_WARN_UNUSED_RESULT;

MOZALLOC_EXPORT char* moz_strdup(const char* str)
    NS_ATTR_MALLOC NS_WARN_UNUSED_RESULT;

MOZALLOC_EXPORT size_t moz_malloc_usable_size(void *ptr);

MOZALLOC_EXPORT size_t moz_malloc_size_of(const void *ptr);

#if defined(HAVE_STRNDUP)
MOZALLOC_EXPORT char* moz_xstrndup(const char* str, size_t strsize)
    NS_ATTR_MALLOC NS_WARN_UNUSED_RESULT;

MOZALLOC_EXPORT char* moz_strndup(const char* str, size_t strsize)
    NS_ATTR_MALLOC NS_WARN_UNUSED_RESULT;
#endif /* if defined(HAVE_STRNDUP) */


#if defined(HAVE_POSIX_MEMALIGN)
MOZALLOC_EXPORT int moz_xposix_memalign(void **ptr, size_t alignment, size_t size)
    NS_WARN_UNUSED_RESULT;

MOZALLOC_EXPORT int moz_posix_memalign(void **ptr, size_t alignment, size_t size)
    NS_WARN_UNUSED_RESULT;
#endif /* if defined(HAVE_POSIX_MEMALIGN) */


#if defined(HAVE_MEMALIGN)
MOZALLOC_EXPORT void* moz_xmemalign(size_t boundary, size_t size)
    NS_ATTR_MALLOC NS_WARN_UNUSED_RESULT;

MOZALLOC_EXPORT void* moz_memalign(size_t boundary, size_t size)
    NS_ATTR_MALLOC NS_WARN_UNUSED_RESULT;
#endif /* if defined(HAVE_MEMALIGN) */


#if defined(HAVE_VALLOC)
MOZALLOC_EXPORT void* moz_xvalloc(size_t size)
    NS_ATTR_MALLOC NS_WARN_UNUSED_RESULT;

MOZALLOC_EXPORT void* moz_valloc(size_t size)
    NS_ATTR_MALLOC NS_WARN_UNUSED_RESULT;
#endif /* if defined(HAVE_VALLOC) */


#ifdef __cplusplus
} /* extern "C" */
#endif /* ifdef __cplusplus */


#ifdef __cplusplus

/*
 * We implement the default operators new/delete as part of
 * libmozalloc, replacing their definitions in libstdc++.  The
 * operator new* definitions in libmozalloc will never return a NULL
 * pointer.
 *
 * Each operator new immediately below returns a pointer to memory
 * that can be delete'd by any of
 *
 *   (1) the matching infallible operator delete immediately below
 *   (2) the matching "fallible" operator delete further below
 *   (3) the matching system |operator delete(void*, std::nothrow)|
 *   (4) the matching system |operator delete(void*) throw(std::bad_alloc)|
 *
 * NB: these are declared |throw(std::bad_alloc)|, though they will never
 * throw that exception.  This declaration is consistent with the rule
 * that |::operator new() throw(std::bad_alloc)| will never return NULL.
 */

/* NB: This is defined just to silence vacuous warnings about symbol
 * visibility on OS X/gcc. These symbols are force-inline and not
 * exported. */
#if defined(XP_MACOSX)
#  define MOZALLOC_EXPORT_NEW MOZALLOC_EXPORT
#else
#  define MOZALLOC_EXPORT_NEW
#endif

#if defined(ANDROID)
/*
 * It's important to always specify 'throw()' in GCC because it's used to tell
 * GCC that 'new' may return null. That makes GCC null-check the result before
 * potentially initializing the memory to zero.
 * Also, the Android minimalistic headers don't include std::bad_alloc.
 */
#define MOZALLOC_THROW_IF_HAS_EXCEPTIONS throw()
#define MOZALLOC_THROW_BAD_ALLOC_IF_HAS_EXCEPTIONS
#elif defined(_MSC_VER)
/*
 * Suppress build warning spam (bug 578546).
 */
#define MOZALLOC_THROW_IF_HAS_EXCEPTIONS
#define MOZALLOC_THROW_BAD_ALLOC_IF_HAS_EXCEPTIONS
#else
#define MOZALLOC_THROW_IF_HAS_EXCEPTIONS throw()
#define MOZALLOC_THROW_BAD_ALLOC_IF_HAS_EXCEPTIONS throw(std::bad_alloc)
#endif

#define MOZALLOC_THROW_BAD_ALLOC MOZALLOC_THROW_BAD_ALLOC_IF_HAS_EXCEPTIONS

MOZALLOC_EXPORT_NEW MOZALLOC_INLINE
void* operator new(size_t size) MOZALLOC_THROW_BAD_ALLOC
{
    return moz_xmalloc(size);
}

MOZALLOC_EXPORT_NEW MOZALLOC_INLINE
void* operator new(size_t size, const std::nothrow_t&) MOZALLOC_THROW_IF_HAS_EXCEPTIONS
{
    return moz_malloc(size);
}

MOZALLOC_EXPORT_NEW MOZALLOC_INLINE
void* operator new[](size_t size) MOZALLOC_THROW_BAD_ALLOC
{
    return moz_xmalloc(size);
}

MOZALLOC_EXPORT_NEW MOZALLOC_INLINE
void* operator new[](size_t size, const std::nothrow_t&) MOZALLOC_THROW_IF_HAS_EXCEPTIONS
{
    return moz_malloc(size);
}

MOZALLOC_EXPORT_NEW MOZALLOC_INLINE
void operator delete(void* ptr) MOZALLOC_THROW_IF_HAS_EXCEPTIONS
{
    return moz_free(ptr);
}

MOZALLOC_EXPORT_NEW MOZALLOC_INLINE
void operator delete(void* ptr, const std::nothrow_t&) MOZALLOC_THROW_IF_HAS_EXCEPTIONS
{
    return moz_free(ptr);
}

MOZALLOC_EXPORT_NEW MOZALLOC_INLINE
void operator delete[](void* ptr) MOZALLOC_THROW_IF_HAS_EXCEPTIONS
{
    return moz_free(ptr);
}

MOZALLOC_EXPORT_NEW MOZALLOC_INLINE
void operator delete[](void* ptr, const std::nothrow_t&) MOZALLOC_THROW_IF_HAS_EXCEPTIONS
{
    return moz_free(ptr);
}


/*
 * We also add a new allocator variant: "fallible operator new."
 * Unlike libmozalloc's implementations of the standard nofail
 * allocators, this allocator is allowed to return NULL.  It can be used
 * as follows
 *
 *   Foo* f = new (mozilla::fallible) Foo(...);
 *
 * operator delete(fallible) is defined for completeness only.
 *
 * Each operator new below returns a pointer to memory that can be
 * delete'd by any of
 *
 *   (1) the matching "fallible" operator delete below
 *   (2) the matching infallible operator delete above
 *   (3) the matching system |operator delete(void*, std::nothrow)|
 *   (4) the matching system |operator delete(void*) throw(std::bad_alloc)|
 */

MOZALLOC_INLINE
void* operator new(size_t size, const mozilla::fallible_t&) MOZALLOC_THROW_IF_HAS_EXCEPTIONS
{
    return moz_malloc(size);
}

MOZALLOC_INLINE
void* operator new[](size_t size, const mozilla::fallible_t&) MOZALLOC_THROW_IF_HAS_EXCEPTIONS
{
    return moz_malloc(size);
}

MOZALLOC_INLINE
void operator delete(void* ptr, const mozilla::fallible_t&) MOZALLOC_THROW_IF_HAS_EXCEPTIONS
{
    moz_free(ptr);
}

MOZALLOC_INLINE
void operator delete[](void* ptr, const mozilla::fallible_t&) MOZALLOC_THROW_IF_HAS_EXCEPTIONS
{
    moz_free(ptr);
}

#endif  /* ifdef __cplusplus */


#endif /* ifndef mozilla_mozalloc_h */
