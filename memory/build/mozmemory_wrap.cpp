/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include <string.h>
#include "mozmemory_wrap.h"
#include "mozilla/Types.h"

// Declare malloc implementation functions with the right return and
// argument types.
#define MALLOC_DECL(name, return_type, ...)                                    \
  MOZ_MEMORY_API return_type name##_impl(__VA_ARGS__);
#define MALLOC_FUNCS MALLOC_FUNCS_MALLOC
#include "malloc_decls.h"

#ifdef MOZ_WRAP_NEW_DELETE
#include <new>

MFBT_API void*
operator new(size_t size)
{
  return malloc_impl(size);
}

MFBT_API void*
operator new[](size_t size)
{
  return malloc_impl(size);
}

MFBT_API void
operator delete(void* ptr) noexcept(true)
{
  free_impl(ptr);
}

MFBT_API void
operator delete[](void* ptr) noexcept(true)
{
  free_impl(ptr);
}

MFBT_API void*
operator new(size_t size, std::nothrow_t const&)
{
  return malloc_impl(size);
}

MFBT_API void*
operator new[](size_t size, std::nothrow_t const&)
{
  return malloc_impl(size);
}

MFBT_API void
operator delete(void* ptr, std::nothrow_t const&) noexcept(true)
{
  free_impl(ptr);
}

MFBT_API void
operator delete[](void* ptr, std::nothrow_t const&) noexcept(true)
{
  free_impl(ptr);
}
#endif

// strndup and strdup may be defined as macros in string.h, which would
// clash with the definitions below.
#undef strndup
#undef strdup

MOZ_MEMORY_API char*
strndup_impl(const char* src, size_t len)
{
  char* dst = (char*)malloc_impl(len + 1);
  if (dst) {
    strncpy(dst, src, len);
    dst[len] = '\0';
  }
  return dst;
}

MOZ_MEMORY_API char*
strdup_impl(const char* src)
{
  size_t len = strlen(src);
  return strndup_impl(src, len);
}

#ifdef ANDROID
#include <stdarg.h>
#include <stdio.h>

MOZ_MEMORY_API int
vasprintf_impl(char** str, const char* fmt, va_list ap)
{
  char *ptr, *_ptr;
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

  _ptr = reinterpret_cast<char*>(realloc_impl(ptr, ret + 1));
  if (_ptr == NULL) {
    free_impl(ptr);
    *str = NULL;
    return -1;
  }

  *str = _ptr;

  return ret;
}

MOZ_MEMORY_API int
asprintf_impl(char** str, const char* fmt, ...)
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
#include <wchar.h>

// We also need to provide our own impl of wcsdup so that we don't ask
// the CRT for memory from its heap (which will then be unfreeable).
MOZ_MEMORY_API wchar_t*
wcsdup_impl(const wchar_t* src)
{
  size_t len = wcslen(src);
  wchar_t* dst = (wchar_t*)malloc_impl((len + 1) * sizeof(wchar_t));
  if (dst)
    wcsncpy(dst, src, len + 1);
  return dst;
}

MOZ_MEMORY_API void*
_aligned_malloc_impl(size_t size, size_t alignment)
{
  return memalign_impl(alignment, size);
}
#endif // XP_WIN
