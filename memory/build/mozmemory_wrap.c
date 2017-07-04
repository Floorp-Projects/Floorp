/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include <string.h>
#include "mozmemory_wrap.h"
#include "mozilla/Types.h"

/* Declare malloc implementation functions with the right return and
 * argument types. */
#define MALLOC_DECL(name, return_type, ...) \
  MOZ_MEMORY_API return_type name ## _impl(__VA_ARGS__);
#include "malloc_decls.h"

#ifdef MOZ_WRAP_NEW_DELETE
/* operator new(unsigned int) */
MOZ_MEMORY_API void *
mozmem_malloc_impl(_Znwj)(unsigned int size)
{
  return malloc_impl(size);
}
/* operator new[](unsigned int) */
MOZ_MEMORY_API void *
mozmem_malloc_impl(_Znaj)(unsigned int size)
{
  return malloc_impl(size);
}
/* operator delete(void*) */
MOZ_MEMORY_API void
mozmem_malloc_impl(_ZdlPv)(void *ptr)
{
  free_impl(ptr);
}
/* operator delete[](void*) */
MOZ_MEMORY_API void
mozmem_malloc_impl(_ZdaPv)(void *ptr)
{
  free_impl(ptr);
}
/*operator new(unsigned int, std::nothrow_t const&)*/
MOZ_MEMORY_API void *
mozmem_malloc_impl(_ZnwjRKSt9nothrow_t)(unsigned int size)
{
  return malloc_impl(size);
}
/*operator new[](unsigned int, std::nothrow_t const&)*/
MOZ_MEMORY_API void *
mozmem_malloc_impl(_ZnajRKSt9nothrow_t)(unsigned int size)
{
  return malloc_impl(size);
}
/* operator delete(void*, std::nothrow_t const&) */
MOZ_MEMORY_API void
mozmem_malloc_impl(_ZdlPvRKSt9nothrow_t)(void *ptr)
{
  free_impl(ptr);
}
/* operator delete[](void*, std::nothrow_t const&) */
MOZ_MEMORY_API void
mozmem_malloc_impl(_ZdaPvRKSt9nothrow_t)(void *ptr)
{
  free_impl(ptr);
}
#endif

/* strndup and strdup may be defined as macros in string.h, which would
 * clash with the definitions below. */
#undef strndup
#undef strdup

MOZ_MEMORY_API char *
strndup_impl(const char *src, size_t len)
{
  char* dst = (char*) malloc_impl(len + 1);
  if (dst) {
    strncpy(dst, src, len);
    dst[len] = '\0';
  }
  return dst;
}

MOZ_MEMORY_API char *
strdup_impl(const char *src)
{
  size_t len = strlen(src);
  return strndup_impl(src, len);
}

#ifdef ANDROID
#include <stdarg.h>
#include <stdio.h>

MOZ_MEMORY_API int
vasprintf_impl(char **str, const char *fmt, va_list ap)
{
  char* ptr, *_ptr;
  int ret;

  if (str == NULL || fmt == NULL) {
    return -1;
  }

  ptr = (char*)malloc_impl(128);
  if (ptr == NULL) {
    *str = NULL;
    return -1;
  }

  ret = vsnprintf(ptr, 128, fmt, ap);
  if (ret < 0) {
    free_impl(ptr);
    *str = NULL;
    return -1;
  }

  _ptr = realloc_impl(ptr, ret + 1);
  if (_ptr == NULL) {
    free_impl(ptr);
    *str = NULL;
    return -1;
  }

  *str = _ptr;

  return ret;
}

MOZ_MEMORY_API int
asprintf_impl(char **str, const char *fmt, ...)
{
   int ret;
   va_list ap;
   va_start(ap, fmt);

   ret = vasprintf_impl(str, fmt, ap);

   va_end(ap);

   return ret;
}
#endif

#ifdef XP_WIN
/*
 *  There's a fun allocator mismatch in (at least) the VS 2010 CRT
 *  (see the giant comment in $(topsrcdir)/mozglue/build/Makefile.in)
 *  that gets redirected here to avoid a crash on shutdown.
 */
void
dumb_free_thunk(void *ptr)
{
  return; /* shutdown leaks that we don't care about */
}

#include <wchar.h>

/*
 *  We also need to provide our own impl of wcsdup so that we don't ask
 *  the CRT for memory from its heap (which will then be unfreeable).
 */
wchar_t *
wcsdup_impl(const wchar_t *src)
{
  size_t len = wcslen(src);
  wchar_t *dst = (wchar_t*) malloc_impl((len + 1) * sizeof(wchar_t));
  if (dst)
    wcsncpy(dst, src, len + 1);
  return dst;
}

void *
_aligned_malloc(size_t size, size_t alignment)
{
  return memalign_impl(alignment, size);
}
#endif /* XP_WIN */
