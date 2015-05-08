/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et cindent: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include <stdio.h>
#include "pldhash.h"

// This test mostly focuses on edge cases. But more coverage of normal
// operations wouldn't be a bad thing.

namespace TestPLDHash {

static bool test_pldhash_Init_capacity_ok()
{
  PLDHashTable t;

  // Check that the constructor nulls |ops|.
  if (t.IsInitialized()) {
    return false;
  }

  // Try the largest allowed capacity.  With PL_DHASH_MAX_CAPACITY==1<<26, this
  // would allocate (if we added an element) 0.5GB of entry store on 32-bit
  // platforms and 1GB on 64-bit platforms.
  //
  // Ideally we'd also try (a) a too-large capacity, and (b) a large capacity
  // combined with a large entry size that when multipled overflow. But those
  // cases would cause the test to abort immediately.
  //
  // Furthermore, ideally we'd also try a large-but-ok capacity that almost but
  // doesn't quite overflow, but that would result in allocating just under 4GB
  // of entry storage.  That's very likely to fail on 32-bit platforms, so such
  // a test wouldn't be reliable.
  //
  PL_DHashTableInit(&t, PL_DHashGetStubOps(), sizeof(PLDHashEntryStub),
                    PL_DHASH_MAX_INITIAL_LENGTH);

  // Check that Init() sets |ops|.
  if (!t.IsInitialized()) {
    return false;
  }

  // Check that Finish() nulls |ops|.
  PL_DHashTableFinish(&t);
  if (t.IsInitialized()) {
    return false;
  }

  return true;
}

static bool test_pldhash_lazy_storage()
{
  PLDHashTable t(PL_DHashGetStubOps(), sizeof(PLDHashEntryStub));

  // PLDHashTable allocates entry storage lazily. Check that all the non-add
  // operations work appropriately when the table is empty and the storage
  // hasn't yet been allocated.

  if (!t.IsInitialized()) {
    return false;
  }

  if (t.Capacity() != 0) {
    return false;
  }

  if (t.EntrySize() != sizeof(PLDHashEntryStub)) {
    return false;
  }

  if (t.EntryCount() != 0) {
    return false;
  }

  if (t.Generation() != 0) {
    return false;
  }

  if (PL_DHashTableSearch(&t, (const void*)1)) {
    return false;   // search succeeded?
  }

  // No result to check here, but call it to make sure it doesn't crash.
  PL_DHashTableRemove(&t, (const void*)2);

  // Using a null |enumerator| should be fine because it shouldn't be called
  // for an empty table.
  PLDHashEnumerator enumerator = nullptr;
  if (PL_DHashTableEnumerate(&t, enumerator, nullptr) != 0) {
    return false;   // enumeration count is non-zero?
  }

  for (PLDHashTable::Iterator iter = t.Iterate();
       iter.HasMoreEntries();
       iter.NextEntry()) {
    return false; // shouldn't hit this on an empty table
  }

  // Using a null |mallocSizeOf| should be fine because it shouldn't be called
  // for an empty table.
  mozilla::MallocSizeOf mallocSizeOf = nullptr;
  if (PL_DHashTableSizeOfExcludingThis(&t, nullptr, mallocSizeOf) != 0) {
    return false;   // size is non-zero?
  }

  return true;
}

// We insert the integers 0.., so this hash function is (a) as simple as
// possible, and (b) collision-free.  Both of which are good, because we want
// this test to be as fast as possible.
static PLDHashNumber
trivial_hash(PLDHashTable *table, const void *key)
{
  return (PLDHashNumber)(size_t)key;
}

static bool test_pldhash_move_semantics()
{
  static const PLDHashTableOps ops = {
    trivial_hash,
    PL_DHashMatchEntryStub,
    PL_DHashMoveEntryStub,
    PL_DHashClearEntryStub,
    nullptr
  };

  PLDHashTable t1(&ops, sizeof(PLDHashEntryStub));
  PL_DHashTableAdd(&t1, (const void*)88);
  PLDHashTable t2(&ops, sizeof(PLDHashEntryStub));
  PL_DHashTableAdd(&t2, (const void*)99);

  t1 = mozilla::Move(t1);   // self-move

  t1 = mozilla::Move(t2);   // inited overwritten with inited

  PLDHashTable t3, t4;
  PL_DHashTableInit(&t3, &ops, sizeof(PLDHashEntryStub));
  PL_DHashTableAdd(&t3, (const void*)88);

  t3 = mozilla::Move(t4);   // inited overwritten with uninited

  PL_DHashTableFinish(&t3);
  PL_DHashTableFinish(&t4);

  PLDHashTable t5, t6;
  PL_DHashTableInit(&t6, &ops, sizeof(PLDHashEntryStub));
  PL_DHashTableAdd(&t6, (const void*)88);

  t5 = mozilla::Move(t6);   // uninited overwritten with inited

  PL_DHashTableFinish(&t5);
  PL_DHashTableFinish(&t6);

  PLDHashTable t7;
  PLDHashTable t8(mozilla::Move(t7));   // new table constructed with uninited

  PLDHashTable t9(&ops, sizeof(PLDHashEntryStub));
  PL_DHashTableAdd(&t9, (const void*)88);
  PLDHashTable t10(mozilla::Move(t9));  // new table constructed with inited

  return true;
}

// See bug 931062, we skip this test on Android due to OOM.
#ifndef MOZ_WIDGET_ANDROID
static bool test_pldhash_grow_to_max_capacity()
{
  static const PLDHashTableOps ops = {
    trivial_hash,
    PL_DHashMatchEntryStub,
    PL_DHashMoveEntryStub,
    PL_DHashClearEntryStub,
    nullptr
  };

  // This is infallible.
  PLDHashTable* t = new PLDHashTable(&ops, sizeof(PLDHashEntryStub), 128);

  // Check that New() sets |t->ops|.
  if (!t->IsInitialized()) {
    delete t;
    return false;
  }

  // Keep inserting elements until failure occurs because the table is full.
  size_t numInserted = 0;
  while (true) {
    if (!PL_DHashTableAdd(t, (const void*)numInserted, mozilla::fallible)) {
      break;
    }
    numInserted++;
  }

  // We stop when the element count is 96.875% of PL_DHASH_MAX_SIZE (see
  // MaxLoadOnGrowthFailure()).
  if (numInserted != PL_DHASH_MAX_CAPACITY - (PL_DHASH_MAX_CAPACITY >> 5)) {
    return false;
  }

  delete t;

  return true;
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
  DECL_TEST(test_pldhash_lazy_storage),
  DECL_TEST(test_pldhash_move_semantics),
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
