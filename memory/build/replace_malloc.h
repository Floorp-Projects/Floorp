/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef replace_malloc_h
#define replace_malloc_h

// The replace_malloc facility allows an external library to replace or
// supplement the jemalloc implementation.
//
// The external library may be hooked by setting one of the following
// environment variables to the library path:
//   - LD_PRELOAD on Linux,
//   - DYLD_INSERT_LIBRARIES on OSX,
//   - MOZ_REPLACE_MALLOC_LIB on Windows and Android.
//
// An initialization function is called before any malloc replacement
// function, and has the following declaration:
//
//   void replace_init(malloc_table_t*, ReplaceMallocBridge**)
//
// The malloc_table_t pointer given to that function is a table containing
// pointers to the original allocator implementation, so that replacement
// functions can call them back if they need to. The initialization function
// needs to alter that table to replace the function it wants to replace.
// If it needs the original implementation, it thus needs a copy of the
// original table.
//
// The ReplaceMallocBridge* pointer is an outparam that allows the
// replace_init function to return a pointer to its ReplaceMallocBridge
// (see replace_malloc_bridge.h).
//
// The functions to be implemented in the external library are of the form:
//
//   void* replace_malloc(size_t size)
//   {
//     // Fiddle with the size if necessary.
//     // orig->malloc doesn't have to be called if the external library
//     // provides its own allocator, but in this case it will have to
//     // implement all functions.
//     void *ptr = orig->malloc(size);
//     // Do whatever you want with the ptr.
//     return ptr;
//   }
//
// where "orig" is a pointer to a copy of the table replace_init got.
//
// See malloc_decls.h for a list of functions that can be replaced this
// way. The implementations are all in the form:
//   return_type replace_name(arguments [,...])
//
// They don't all need to be provided.
//
// Building a replace-malloc library is like rocket science. It can end up
// with things blowing up, especially when trying to use complex types, and
// even more especially when these types come from XPCOM or other parts of the
// Mozilla codebase.
// It is recommended to add the following to a replace-malloc implementation's
// moz.build:
//   DISABLE_STL_WRAPPING = True # Avoid STL wrapping
//
// If your replace-malloc implementation lives under memory/replace, these
// are taken care of by memory/replace/defs.mk.

#ifdef replace_malloc_bridge_h
#error Do not include replace_malloc_bridge.h before replace_malloc.h. \
  In fact, you only need the latter.
#endif

#define REPLACE_MALLOC_IMPL

#include "replace_malloc_bridge.h"

// Implementing a replace-malloc library is incompatible with using mozalloc.
#define MOZ_NO_MOZALLOC 1

#include "mozilla/MacroArgs.h"
#include "mozilla/Types.h"

MOZ_BEGIN_EXTERN_C

// MOZ_REPLACE_WEAK is only defined in mozjemalloc.cpp. Normally including
// this header will add function definitions.
#ifndef MOZ_REPLACE_WEAK
#define MOZ_REPLACE_WEAK
#endif

// When building a replace-malloc library for static linking, we want
// each to have a different name for their "public" functions.
// The build system defines MOZ_REPLACE_MALLOC_PREFIX in that case.
#ifdef MOZ_REPLACE_MALLOC_PREFIX
#define replace_init MOZ_CONCAT(MOZ_REPLACE_MALLOC_PREFIX, _init)
#define MOZ_REPLACE_PUBLIC
#else
#define MOZ_REPLACE_PUBLIC MOZ_EXPORT
#endif

// Replace-malloc library initialization function. See top of this file
MOZ_REPLACE_PUBLIC void
replace_init(malloc_table_t*, struct ReplaceMallocBridge**) MOZ_REPLACE_WEAK;

MOZ_END_EXTERN_C

#endif // replace_malloc_h
