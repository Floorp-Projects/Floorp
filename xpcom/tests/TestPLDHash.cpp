/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et cindent: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include <stdio.h>
#include "pldhash.h"

// pldhash is very widely used and so any basic bugs in it are likely to be
// exposed through normal usage.  Therefore, this test currently focusses on
// extreme cases relating to maximum table capacity and potential overflows,
// which are unlikely to be hit during normal execution.

namespace TestPLDHash {

static bool test_pldhash_Init_capacity_ok()
{
  // Try the largest allowed capacity.  With PL_DHASH_MAX_SIZE==1<<26, this
  // will allocate 0.5GB of entry store on 32-bit platforms and 1GB on 64-bit
  // platforms.
  PLDHashTable t;
  bool ok = PL_DHashTableInit(&t, PL_DHashGetStubOps(), nullptr,
                              sizeof(PLDHashEntryStub), PL_DHASH_MAX_SIZE);
  if (ok)
    PL_DHashTableFinish(&t);

  return ok;
}

static bool test_pldhash_Init_capacity_too_large()
{
  // Try the smallest too-large capacity.
  PLDHashTable t;
  bool ok = PL_DHashTableInit(&t, PL_DHashGetStubOps(), nullptr,
                              sizeof(PLDHashEntryStub), PL_DHASH_MAX_SIZE + 1);
  // Don't call PL_DHashTableDestroy(), it's not safe after Init failure.

  return !ok;   // expected to fail
}

static bool test_pldhash_Init_overflow()
{
  // Try an acceptable capacity, but one whose byte size overflows uint32_t.
  //
  // Ideally we'd also try a large-but-ok capacity that almost but doesn't
  // quite overflow, but that would result in allocating just under 4GB of
  // entry storage.  That's very likely to fail on 32-bit platforms, so such a
  // test wouldn't be reliable.

  struct OneKBEntry {
      PLDHashEntryHdr hdr;
      char buf[1024 - sizeof(PLDHashEntryHdr)];
  };

  // |nullptr| for |ops| is ok because it's unused due to the failure.
  PLDHashTable t;
  bool ok = PL_DHashTableInit(&t, /* ops = */nullptr, nullptr,
                              sizeof(OneKBEntry), PL_DHASH_MAX_SIZE);

  return !ok;   // expected to fail
}

// See bug 931062, we skip this test on Android due to OOM.
#ifndef MOZ_WIDGET_ANDROID
// We insert the integers 0.., so this is has function is (a) as simple as
// possible, and (b) collision-free.  Both of which are good, because we want
// this test to be as fast as possible.
static PLDHashNumber
hash(PLDHashTable *table, const void *key)
{
  return (PLDHashNumber)(size_t)key;
}

static bool test_pldhash_grow_to_max_capacity()
{
  static const PLDHashTableOps ops = {
    PL_DHashAllocTable,
    PL_DHashFreeTable,
    hash,
    PL_DHashMatchEntryStub,
    PL_DHashMoveEntryStub,
    PL_DHashClearEntryStub,
    PL_DHashFinalizeStub,
    nullptr
  };

  PLDHashTable t;
  bool ok = PL_DHashTableInit(&t, &ops, nullptr, sizeof(PLDHashEntryStub), 256);
  if (!ok)
    return false;

  // Keep inserting elements until failure occurs because the table is full.
  size_t numInserted = 0;
  while (true) {
    if (!PL_DHashTableOperate(&t, (const void*)numInserted, PL_DHASH_ADD)) {
      break;
    }
    numInserted++;
  }

  // We stop when the element count is 96.875% of PL_DHASH_MAX_SIZE (see
  // MaxLoadOnGrowthFailure()).
  return numInserted == PL_DHASH_MAX_SIZE - (PL_DHASH_MAX_SIZE >> 5);
}
#endif

//----

typedef bool (*TestFunc)();
#define DECL_TEST(name) { #name, name }

static const struct Test {
  const char* name;
  TestFunc    func;
} tests[] = {
  DECL_TEST(test_pldhash_Init_capacity_ok),
  DECL_TEST(test_pldhash_Init_capacity_too_large),
  DECL_TEST(test_pldhash_Init_overflow),
// See bug 931062, we skip this test on Android due to OOM.
#ifndef MOZ_WIDGET_ANDROID
  DECL_TEST(test_pldhash_grow_to_max_capacity),
#endif
  { nullptr, nullptr }
};

} // namespace TestPLDHash

using namespace TestPLDHash;

int main(int argc, char *argv[])
{
  bool success = true;
  for (const Test* t = tests; t->name != nullptr; ++t) {
    bool test_result = t->func();
    printf("%25s : %s\n", t->name, test_result ? "SUCCESS" : "FAILURE");
    if (!test_result)
      success = false;
  }
  return success ? 0 : -1;
}
