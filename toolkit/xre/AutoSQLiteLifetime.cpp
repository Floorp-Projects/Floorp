/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "nsDebug.h"
#include "AutoSQLiteLifetime.h"
#include "sqlite3.h"

#ifdef MOZ_MEMORY
#  include "mozmemory.h"
#  ifdef MOZ_DMD
#    include "nsIMemoryReporter.h"
#    include "DMD.h"

namespace mozilla {
namespace storage {
extern mozilla::Atomic<size_t> gSqliteMemoryUsed;
}
}  // namespace mozilla

using mozilla::storage::gSqliteMemoryUsed;

#  endif

namespace {

// By default, SQLite tracks the size of all its heap blocks by adding an extra
// 8 bytes at the start of the block to hold the size.  Unfortunately, this
// causes a lot of 2^N-sized allocations to be rounded up by jemalloc
// allocator, wasting memory.  For example, a request for 1024 bytes has 8
// bytes added, becoming a request for 1032 bytes, and jemalloc rounds this up
// to 2048 bytes, wasting 1012 bytes.  (See bug 676189 for more details.)
//
// So we register jemalloc as the malloc implementation, which avoids this
// 8-byte overhead, and thus a lot of waste.  This requires us to provide a
// function, sqliteMemRoundup(), which computes the actual size that will be
// allocated for a given request.  SQLite uses this function before all
// allocations, and may be able to use any excess bytes caused by the rounding.
//
// Note: the wrappers for malloc, realloc and moz_malloc_usable_size are
// necessary because the sqlite_mem_methods type signatures differ slightly
// from the standard ones -- they use int instead of size_t.  But we don't need
// a wrapper for free.

#  ifdef MOZ_DMD

// sqlite does its own memory accounting, and we use its numbers in our memory
// reporters.  But we don't want sqlite's heap blocks to show up in DMD's
// output as unreported, so we mark them as reported when they're allocated and
// mark them as unreported when they are freed.
//
// In other words, we are marking all sqlite heap blocks as reported even
// though we're not reporting them ourselves.  Instead we're trusting that
// sqlite is fully and correctly accounting for all of its heap blocks via its
// own memory accounting.  Well, we don't have to trust it entirely, because
// it's easy to keep track (while doing this DMD-specific marking) of exactly
// how much memory SQLite is using.  And we can compare that against what
// SQLite reports it is using.

MOZ_DEFINE_MALLOC_SIZE_OF_ON_ALLOC(SqliteMallocSizeOfOnAlloc)
MOZ_DEFINE_MALLOC_SIZE_OF_ON_FREE(SqliteMallocSizeOfOnFree)

#  endif

static void* sqliteMemMalloc(int n) {
  void* p = ::malloc(n);
#  ifdef MOZ_DMD
  gSqliteMemoryUsed += SqliteMallocSizeOfOnAlloc(p);
#  endif
  return p;
}

static void sqliteMemFree(void* p) {
#  ifdef MOZ_DMD
  gSqliteMemoryUsed -= SqliteMallocSizeOfOnFree(p);
#  endif
  ::free(p);
}

static void* sqliteMemRealloc(void* p, int n) {
#  ifdef MOZ_DMD
  gSqliteMemoryUsed -= SqliteMallocSizeOfOnFree(p);
  void* pnew = ::realloc(p, n);
  if (pnew) {
    gSqliteMemoryUsed += SqliteMallocSizeOfOnAlloc(pnew);
  } else {
    // realloc failed;  undo the SqliteMallocSizeOfOnFree from above
    gSqliteMemoryUsed += SqliteMallocSizeOfOnAlloc(p);
  }
  return pnew;
#  else
  return ::realloc(p, n);
#  endif
}

static int sqliteMemSize(void* p) { return ::moz_malloc_usable_size(p); }

static int sqliteMemRoundup(int n) {
  n = malloc_good_size(n);

  // jemalloc can return blocks of size 2 and 4, but SQLite requires that all
  // allocations be 8-aligned.  So we round up sub-8 requests to 8.  This
  // wastes a small amount of memory but is obviously safe.
  return n <= 8 ? 8 : n;
}

static int sqliteMemInit(void* p) { return 0; }

static void sqliteMemShutdown(void* p) {}

const sqlite3_mem_methods memMethods = {
    &sqliteMemMalloc,  &sqliteMemFree, &sqliteMemRealloc,  &sqliteMemSize,
    &sqliteMemRoundup, &sqliteMemInit, &sqliteMemShutdown, nullptr};

}  // namespace

#endif  // MOZ_MEMORY

namespace mozilla {

AutoSQLiteLifetime::AutoSQLiteLifetime() {
  if (++AutoSQLiteLifetime::sSingletonEnforcer != 1) {
    MOZ_CRASH("multiple instances of AutoSQLiteLifetime constructed!");
  }

#ifdef MOZ_MEMORY
  sResult = ::sqlite3_config(SQLITE_CONFIG_MALLOC, &memMethods);
#else
  sResult = SQLITE_OK;
#endif

  if (sResult == SQLITE_OK) {
    // TODO (bug 1191405): do not preallocate the connections caches until we
    // have figured the impact on our consumers and memory.
    sqlite3_config(SQLITE_CONFIG_PAGECACHE, NULL, 0, 0);

    // Explicitly initialize sqlite3.  Although this is implicitly called by
    // various sqlite3 functions (and the sqlite3_open calls in our case),
    // the documentation suggests calling this directly.  So we do.
    sResult = ::sqlite3_initialize();
  }
}

AutoSQLiteLifetime::~AutoSQLiteLifetime() {
  // Shutdown the sqlite3 API.  Warn if shutdown did not turn out okay, but
  // there is nothing actionable we can do in that case.
  sResult = ::sqlite3_shutdown();
  NS_WARNING_ASSERTION(sResult == SQLITE_OK,
                       "sqlite3 did not shutdown cleanly.");
}

int AutoSQLiteLifetime::sSingletonEnforcer = 0;
int AutoSQLiteLifetime::sResult = SQLITE_MISUSE;

}  // namespace mozilla
