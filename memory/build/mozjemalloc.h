/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozjemalloc_types.h"

/* Generic interface exposing the whole public allocator API
 * This facilitates the implementation of things like replace-malloc.
 * Note: compilers are expected to be able to optimize out `this`.
 */
template <typename T>
struct Allocator: public T {
#define MALLOC_FUNCS (MALLOC_FUNCS_MALLOC | MALLOC_FUNCS_JEMALLOC)
#define MALLOC_DECL(name, return_type, ...) \
  static return_type name(__VA_ARGS__);
#include "malloc_decls.h"
};

/* The MozJemalloc allocator */
struct MozJemallocBase {};
typedef Allocator<MozJemallocBase> MozJemalloc;

#ifdef MOZ_REPLACE_MALLOC
/* The replace-malloc allocator */
struct ReplaceMallocBase {};
typedef Allocator<ReplaceMallocBase> ReplaceMalloc;

typedef ReplaceMalloc DefaultMalloc;
#else
typedef MozJemalloc DefaultMalloc;
#endif
