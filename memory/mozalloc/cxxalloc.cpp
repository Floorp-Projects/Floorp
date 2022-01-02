/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#define MOZ_MEMORY_IMPL
#include "mozmemory_wrap.h"
#define MALLOC_FUNCS MALLOC_FUNCS_MALLOC
// See mozmemory_wrap.h for more details. Files that are part of libmozglue,
// need to use _impl suffixes, which is becoming cumbersome. We'll have to use
// something like a malloc.h wrapper and allow the use of the functions without
// a _impl suffix. In the meanwhile, this is enough to get by for C++ code.
#define MALLOC_DECL(name, return_type, ...) \
  MOZ_MEMORY_API return_type name##_impl(__VA_ARGS__);
#include "malloc_decls.h"

#include "mozilla/Attributes.h"

extern "C" MFBT_API void* moz_xmalloc(size_t size) MOZ_INFALLIBLE_ALLOCATOR;

namespace std {
struct nothrow_t;
}

#define MOZALLOC_EXPORT_NEW MFBT_API

#include "mozilla/cxxalloc.h"
