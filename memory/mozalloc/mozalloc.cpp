/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * vim: sw=4 ts=4 et :
 */
/* ***** BEGIN LICENSE BLOCK *****
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public License Version
 * 1.1 (the "License"); you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is
 * Mozilla Foundation
 * Portions created by the Initial Developer are Copyright (C) 2009
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *  Chris Jones <jones.chris.g@gmail.com>
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either of the GNU General Public License Version 2 or later (the "GPL"),
 * or the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

#include <errno.h>
#include <new>                  // for std::bad_alloc
#include <string.h>

#include <sys/types.h>

#if defined(MALLOC_H)
#  include MALLOC_H             // for memalign, valloc where available
#endif // if defined(MALLOC_H)
#include <stddef.h>             // for size_t
#include <stdlib.h>             // for malloc, free
#if defined(XP_UNIX)
#  include <unistd.h>           // for valloc on *BSD
#endif //if defined(XP_UNIX)

#if defined(MOZ_MEMORY)
// jemalloc.h doesn't redeclare symbols if they're provided by the OS
#  include "jemalloc.h"
#endif

#if defined(XP_WIN) || (defined(XP_OS2) && defined(__declspec))
#  define MOZALLOC_EXPORT __declspec(dllexport)
#endif

// Make sure that "malloc" et al. resolve to their libc variants.
#define MOZALLOC_DONT_DEFINE_MACRO_WRAPPERS
#include "mozilla/mozalloc.h"
#include "mozilla/mozalloc_oom.h"  // for mozalloc_handle_oom


#if defined(__GNUC__) && (__GNUC__ > 2)
#define LIKELY(x)    (__builtin_expect(!!(x), 1))
#define UNLIKELY(x)  (__builtin_expect(!!(x), 0))
#else
#define LIKELY(x)    (x)
#define UNLIKELY(x)  (x)
#endif

#ifdef MOZ_MEMORY_DARWIN
#include "jemalloc.h"
#define malloc(a)               je_malloc(a)
#define posix_memalign(a, b, c) je_posix_memalign(a, b, c)
#define valloc(a)               je_valloc(a)
#define calloc(a, b)            je_calloc(a, b)
#define memalign(a, b)          je_memalign(a, b)
#define strdup(a)               je_strdup(a)
#define strndup(a, b)           je_strndup(a, b)
/* We omit functions which could be passed a memory region that was not
 * allocated by jemalloc (realloc, free and malloc_usable_size). Instead,
 * we use the system-provided functions, which will in turn call the
 * jemalloc versions when appropriate */
#endif

void
moz_free(void* ptr)
{
    free(ptr);
}

void*
moz_xmalloc(size_t size)
{
    void* ptr = malloc(size);
    if (UNLIKELY(!ptr)) {
        mozalloc_handle_oom();
        return moz_xmalloc(size);
    }
    return ptr;
}
void*
moz_malloc(size_t size)
{
    return malloc(size);
}

void*
moz_xcalloc(size_t nmemb, size_t size)
{
    void* ptr = calloc(nmemb, size);
    if (UNLIKELY(!ptr)) {
        mozalloc_handle_oom();
        return moz_xcalloc(nmemb, size);
    }
    return ptr;
}
void*
moz_calloc(size_t nmemb, size_t size)
{
    return calloc(nmemb, size);
}

void*
moz_xrealloc(void* ptr, size_t size)
{
    void* newptr = realloc(ptr, size);
    if (UNLIKELY(!newptr)) {
        mozalloc_handle_oom();
        return moz_xrealloc(ptr, size);
    }
    return newptr;
}
void*
moz_realloc(void* ptr, size_t size)
{
    return realloc(ptr, size);
}

char*
moz_xstrdup(const char* str)
{
    char* dup = strdup(str);
    if (UNLIKELY(!dup)) {
        mozalloc_handle_oom();
        return moz_xstrdup(str);
    }
    return dup;
}
char*
moz_strdup(const char* str)
{
    return strdup(str);
}

#if defined(HAVE_STRNDUP)
char*
moz_xstrndup(const char* str, size_t strsize)
{
    char* dup = strndup(str, strsize);
    if (UNLIKELY(!dup)) {
        mozalloc_handle_oom();
        return moz_xstrndup(str, strsize);
    }
    return dup;
}
char*
moz_strndup(const char* str, size_t strsize)
{
    return strndup(str, strsize);
}
#endif  // if defined(HAVE_STRNDUP)

#if defined(HAVE_POSIX_MEMALIGN) || defined(HAVE_JEMALLOC_POSIX_MEMALIGN)
int
moz_xposix_memalign(void **ptr, size_t alignment, size_t size)
{
    int err = posix_memalign(ptr, alignment, size);
    if (UNLIKELY(err && ENOMEM == err)) {
        mozalloc_handle_oom();
        return moz_xposix_memalign(ptr, alignment, size);
    }
    // else: (0 == err) or (EINVAL == err)
    return err;
}
int
moz_posix_memalign(void **ptr, size_t alignment, size_t size)
{
    int code = posix_memalign(ptr, alignment, size);
    if (code)
        return code;

#if defined(XP_MACOSX)
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

#if defined(HAVE_MEMALIGN) || defined(HAVE_JEMALLOC_MEMALIGN)
void*
moz_xmemalign(size_t boundary, size_t size)
{
    void* ptr = memalign(boundary, size);
    if (UNLIKELY(!ptr && EINVAL != errno)) {
        mozalloc_handle_oom();
        return moz_xmemalign(boundary, size);
    }
    // non-NULL ptr or errno == EINVAL
    return ptr;
}
void*
moz_memalign(size_t boundary, size_t size)
{
    return memalign(boundary, size);
}
#endif // if defined(HAVE_MEMALIGN)

#if defined(HAVE_VALLOC) || defined(HAVE_JEMALLOC_VALLOC)
void*
moz_xvalloc(size_t size)
{
    void* ptr = valloc(size);
    if (UNLIKELY(!ptr)) {
        mozalloc_handle_oom();
        return moz_xvalloc(size);
    }
    return ptr;
}
void*
moz_valloc(size_t size)
{
    return valloc(size);
}
#endif // if defined(HAVE_VALLOC)

size_t
moz_malloc_usable_size(void *ptr)
{
    if (!ptr)
        return 0;

#if defined(XP_MACOSX)
    return malloc_size(ptr);
#elif defined(MOZ_MEMORY)
    return malloc_usable_size(ptr);
#elif defined(XP_WIN)
    return _msize(ptr);
#else
    return 0;
#endif
}

namespace mozilla {

const fallible_t fallible = fallible_t();

} // namespace mozilla
