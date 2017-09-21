/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozjemalloc_types.h"
#include "mozilla/MacroArgs.h"

/* Macro helpers */

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
