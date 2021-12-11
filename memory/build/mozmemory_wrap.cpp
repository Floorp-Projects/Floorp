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
#define NOTHROW_MALLOC_DECL(name, return_type, ...) \
  MOZ_MEMORY_API return_type name##_impl(__VA_ARGS__) noexcept(true);
#define MALLOC_DECL(name, return_type, ...) \
  MOZ_MEMORY_API return_type name##_impl(__VA_ARGS__);
#define MALLOC_FUNCS MALLOC_FUNCS_MALLOC
#include "malloc_decls.h"

// strndup and strdup may be defined as macros in string.h, which would
// clash with the definitions below.
#undef strndup
#undef strdup

MOZ_MEMORY_API char* strndup_impl(const char* src, size_t len) {
  char* dst = (char*)malloc_impl(len + 1);
  if (dst) {
    strncpy(dst, src, len);
    dst[len] = '\0';
  }
  return dst;
}

MOZ_MEMORY_API char* strdup_impl(const char* src) {
  size_t len = strlen(src);
  return strndup_impl(src, len);
}

#ifdef ANDROID
#  include <stdarg.h>
#  include <stdio.h>

MOZ_MEMORY_API int vasprintf_impl(char** str, const char* fmt, va_list ap) {
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

MOZ_MEMORY_API int asprintf_impl(char** str, const char* fmt, ...) {
  int ret;
  va_list ap;
  va_start(ap, fmt);

  ret = vasprintf_impl(str, fmt, ap);

  va_end(ap);

  return ret;
}
#endif

#ifdef XP_WIN
#  include <wchar.h>

// We also need to provide our own impl of wcsdup so that we don't ask
// the CRT for memory from its heap (which will then be unfreeable).
MOZ_MEMORY_API wchar_t* wcsdup_impl(const wchar_t* src) {
  size_t len = wcslen(src);
  wchar_t* dst = (wchar_t*)malloc_impl((len + 1) * sizeof(wchar_t));
  if (dst) wcsncpy(dst, src, len + 1);
  return dst;
}

MOZ_MEMORY_API void* _aligned_malloc_impl(size_t size, size_t alignment) {
  return memalign_impl(alignment, size);
}

#  ifdef __MINGW32__
MOZ_BEGIN_EXTERN_C
// As in mozjemalloc.cpp, we generate aliases for functions
// redirected in mozglue.def
void* _aligned_malloc(size_t size, size_t alignment)
    __attribute__((alias(MOZ_STRINGIFY(_aligned_malloc_impl))));
void _aligned_free(void* aPtr) __attribute__((alias(MOZ_STRINGIFY(free_impl))));

char* strndup(const char* src, size_t len)
    __attribute__((alias(MOZ_STRINGIFY(strdup_impl))));
char* strdup(const char* src)
    __attribute__((alias(MOZ_STRINGIFY(strdup_impl))));
char* _strdup(const char* src)
    __attribute__((alias(MOZ_STRINGIFY(strdup_impl))));
wchar_t* wcsdup(const wchar_t* src)
    __attribute__((alias(MOZ_STRINGIFY(wcsdup_impl))));
wchar_t* _wcsdup(const wchar_t* src)
    __attribute__((alias(MOZ_STRINGIFY(wcsdup_impl))));

// jemalloc has _aligned_malloc, and friends. libc++.a contains
// references to __imp__aligned_malloc (and friends) because it
// is declared dllimport in the headers.
//
// The linker sees jemalloc's _aligned_malloc symbol in our objects,
// but then libc++.a comes along and needs __imp__aligned_malloc, which
// pulls in those parts of libucrt.a (or libmsvcrt.a in practice),
// which define both __imp__aligned_malloc and _aligned_malloc, and
// this causes a conflict.  (And repeat for each of the symbols defined
// here.)
//
// The fix is to define not only an _aligned_malloc symbol (via an
// alias), but also define the __imp__aligned_malloc pointer to it.
// This prevents those parts of libucrt from being pulled in and causing
// conflicts.
// This is done with __MINGW_IMP_SYMBOL to handle x86/x64 differences.
void (*__MINGW_IMP_SYMBOL(_aligned_free))(void*) = _aligned_free;
void* (*__MINGW_IMP_SYMBOL(_aligned_malloc))(size_t, size_t) = _aligned_malloc;
char* (*__MINGW_IMP_SYMBOL(_strdup))(const char* src) = _strdup;
MOZ_END_EXTERN_C
#  endif
#endif  // XP_WIN
