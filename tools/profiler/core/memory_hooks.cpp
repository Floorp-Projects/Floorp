/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "memory_hooks.h"

#include "nscore.h"

#include "mozilla/Assertions.h"
#include "mozilla/IntegerPrintfMacros.h"
#include "mozilla/JSONWriter.h"
#include "mozilla/MemoryReporting.h"
#include "mozilla/ProfilerCounts.h"

#include "replace_malloc.h"

#include <ctype.h>
#include <errno.h>
#include <limits.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifdef XP_WIN
#  include <windows.h>
#  include <process.h>
#else
#  include <pthread.h>
#  include <sys/types.h>
#  include <unistd.h>
#endif

#ifdef ANDROID
#  include <android/log.h>
#endif

static mozilla::UniquePtr<ProfilerCounterTotal> sCounter;

namespace mozilla {
namespace profiler {

//---------------------------------------------------------------------------
// Utilities
//---------------------------------------------------------------------------

static malloc_table_t gMallocTable;

// This is only needed because of the |const void*| vs |void*| arg mismatch.
static size_t MallocSizeOf(const void* aPtr) {
  return gMallocTable.malloc_usable_size(const_cast<void*>(aPtr));
}

//---------------------------------------------------------------------------
// malloc/free callbacks
//---------------------------------------------------------------------------

static void AllocCallback(void* aPtr, size_t aReqSize) {
  if (!aPtr) {
    return;
  }
  size_t actualSize = gMallocTable.malloc_usable_size(aPtr);
  if (actualSize > 0) {
    // this never allocates
    sCounter->Add(actualSize);
  }

  // XXX add optional stackwalk here
  // We're ignoring aReqSize here
}

static void FreeCallback(void* aPtr) {
  if (!aPtr) {
    return;
  }

  // this never allocates
  sCounter->Add(-((int64_t)MallocSizeOf(aPtr)));

  // XXX add optional stackwalk here
}

}  // namespace profiler
}  // namespace mozilla

//---------------------------------------------------------------------------
// malloc/free interception
//---------------------------------------------------------------------------

using namespace mozilla::profiler;

static void* replace_malloc(size_t aSize) {
  // This must be a call to malloc from outside.  Intercept it.
  void* ptr = gMallocTable.malloc(aSize);
  AllocCallback(ptr, aSize);
  return ptr;
}

static void* replace_calloc(size_t aCount, size_t aSize) {
  void* ptr = gMallocTable.calloc(aCount, aSize);
  AllocCallback(ptr, aCount * aSize);
  return ptr;
}

static void* replace_realloc(void* aOldPtr, size_t aSize) {
  // If |aOldPtr| is nullptr, the call is equivalent to |malloc(aSize)|.
  if (!aOldPtr) {
    return replace_malloc(aSize);
  }

  FreeCallback(aOldPtr);
  void* ptr = gMallocTable.realloc(aOldPtr, aSize);
  if (ptr) {
    AllocCallback(ptr, aSize);
  } else {
    // If realloc fails, we undo the prior operations by re-inserting the old
    // pointer into the live block table. We don't have to do anything with the
    // dead block list because the dead block hasn't yet been inserted. The
    // block will end up looking like it was allocated for the first time here,
    // which is untrue, and the slop bytes will be zero, which may be untrue.
    // But this case is rare and doing better isn't worth the effort.
    AllocCallback(aOldPtr, gMallocTable.malloc_usable_size(aOldPtr));
  }
  return ptr;
}

static void* replace_memalign(size_t aAlignment, size_t aSize) {
  void* ptr = gMallocTable.memalign(aAlignment, aSize);
  AllocCallback(ptr, aSize);
  return ptr;
}

static void replace_free(void* aPtr) {
  FreeCallback(aPtr);
  gMallocTable.free(aPtr);
}

static void* replace_moz_arena_malloc(arena_id_t aArena, size_t aSize) {
  void* ptr = gMallocTable.moz_arena_malloc(aArena, aSize);
  AllocCallback(ptr, aSize);
  return ptr;
}

static void* replace_moz_arena_calloc(arena_id_t aArena, size_t aCount,
                                      size_t aSize) {
  void* ptr = gMallocTable.moz_arena_calloc(aArena, aCount, aSize);
  AllocCallback(ptr, aCount * aSize);
  return ptr;
}

static void* replace_moz_arena_realloc(arena_id_t aArena, void* aPtr,
                                       size_t aSize) {
  void* ptr = gMallocTable.moz_arena_realloc(aArena, aPtr, aSize);
  AllocCallback(ptr, aSize);
  return ptr;
}

static void replace_moz_arena_free(arena_id_t aArena, void* aPtr) {
  FreeCallback(aPtr);
  gMallocTable.moz_arena_free(aArena, aPtr);
}

static void* replace_moz_arena_memalign(arena_id_t aArena, size_t aAlignment,
                                        size_t aSize) {
  void* ptr = gMallocTable.moz_arena_memalign(aArena, aAlignment, aSize);
  AllocCallback(ptr, aSize);
  return ptr;
}

// we have to replace these or jemalloc will assume we don't implement any
// of the arena replacements!
static arena_id_t replace_moz_create_arena_with_params(
    arena_params_t* aParams) {
  return gMallocTable.moz_create_arena_with_params(aParams);
}

static void replace_moz_dispose_arena(arena_id_t aArenaId) {
  return gMallocTable.moz_dispose_arena(aArenaId);
}

// Must come after all the replace_* funcs
void replace_init(malloc_table_t* aMallocTable, ReplaceMallocBridge** aBridge) {
  gMallocTable = *aMallocTable;
#define MALLOC_FUNCS (MALLOC_FUNCS_MALLOC_BASE | MALLOC_FUNCS_ARENA)
#define MALLOC_DECL(name, ...) aMallocTable->name = replace_##name;
#include "malloc_decls.h"
}

void profiler_replace_remove() {}

namespace mozilla {
namespace profiler {
//---------------------------------------------------------------------------
// Initialization
//---------------------------------------------------------------------------

void install_memory_counter(bool aInstall) {
  if (aInstall) {
    if (!sCounter) {
      sCounter = MakeUnique<ProfilerCounterTotal>("malloc", "Memory",
                                                  "Amount of allocated memory");
    }
    jemalloc_replace_dynamic(replace_init);
  } else {
    jemalloc_replace_dynamic(nullptr);
  }
}

}  // namespace profiler
}  // namespace mozilla
