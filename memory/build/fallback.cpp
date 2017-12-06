/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozmemory.h"
#include "mozjemalloc.h"
#include <stdlib.h>

#ifndef HAVE_MEMALIGN
MOZ_MEMORY_API void*
memalign(size_t aAlignment, size_t aSize)
{
#ifdef XP_WIN
  return _aligned_malloc(aSize, aAlignment);
#else
  void* ret;
  if (posix_memalign(&ret, aAlignment, aSize) != 0) {
    return nullptr;
  }
  return ret;
#endif
}
#endif

struct SystemMalloc
{
#define MALLOC_DECL(name, return_type, ...)                                    \
  static inline return_type name(ARGS_HELPER(TYPED_ARGS, ##__VA_ARGS__))       \
  {                                                                            \
    return ::name(ARGS_HELPER(ARGS, ##__VA_ARGS__));                           \
  }
#define MALLOC_FUNCS MALLOC_FUNCS_MALLOC_BASE
#include "malloc_decls.h"
};

#define MALLOC_DECL(name, return_type, ...)                                    \
  MOZ_JEMALLOC_API return_type name(ARGS_HELPER(TYPED_ARGS, ##__VA_ARGS__))    \
  {                                                                            \
    return DummyArenaAllocator<SystemMalloc>::name(                            \
      ARGS_HELPER(ARGS, ##__VA_ARGS__));                                       \
  }
#define MALLOC_FUNCS MALLOC_FUNCS_ARENA
#include "malloc_decls.h"
