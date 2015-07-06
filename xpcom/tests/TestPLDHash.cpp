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
  PLDHashTable t(PL_DHashGetStubOps(), sizeof(PLDHashEntryStub),
                 PL_DHASH_MAX_INITIAL_LENGTH);

  return true;
}

static bool test_pldhash_lazy_storage()
{
  PLDHashTable t(PL_DHashGetStubOps(), sizeof(PLDHashEntryStub));

  // PLDHashTable allocates entry storage lazily. Check that all the non-add
  // operations work appropriately when the table is empty and the storage
  // hasn't yet been allocated.

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

  for (auto iter = t.Iter(); !iter.Done(); iter.Next()) {
    return false; // shouldn't hit this on an empty table
  }

  for (auto iter = t.RemovingIter(); !iter.Done(); iter.Next()) {
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

// A trivial hash function is good enough here. It's also super-fast for
// test_pldhash_grow_to_max_capacity() because we insert the integers 0..,
// which means it's collision-free.
static PLDHashNumber
TrivialHash(PLDHashTable *table, const void *key)
{
  return (PLDHashNumber)(size_t)key;
}

static void
TrivialInitEntry(PLDHashEntryHdr* aEntry, const void* aKey)
{
  auto entry = static_cast<PLDHashEntryStub*>(aEntry);
  entry->key = aKey;
}

static const PLDHashTableOps trivialOps = {
  TrivialHash,
  PL_DHashMatchEntryStub,
  PL_DHashMoveEntryStub,
  PL_DHashClearEntryStub,
  TrivialInitEntry
};

static bool test_pldhash_move_semantics()
{
  PLDHashTable t1(&trivialOps, sizeof(PLDHashEntryStub));
  PL_DHashTableAdd(&t1, (const void*)88);
  PLDHashTable t2(&trivialOps, sizeof(PLDHashEntryStub));
  PL_DHashTableAdd(&t2, (const void*)99);

  t1 = mozilla::Move(t1);   // self-move

  t1 = mozilla::Move(t2);   // empty overwritten with empty

  PLDHashTable t3(&trivialOps, sizeof(PLDHashEntryStub));
  PLDHashTable t4(&trivialOps, sizeof(PLDHashEntryStub));
  PL_DHashTableAdd(&t3, (const void*)88);

  t3 = mozilla::Move(t4);   // non-empty overwritten with empty

  PLDHashTable t5(&trivialOps, sizeof(PLDHashEntryStub));
  PLDHashTable t6(&trivialOps, sizeof(PLDHashEntryStub));
  PL_DHashTableAdd(&t6, (const void*)88);

  t5 = mozilla::Move(t6);   // empty overwritten with non-empty

  PLDHashTable t7(&trivialOps, sizeof(PLDHashEntryStub));
  PLDHashTable t8(mozilla::Move(t7));  // new table constructed with uninited

  PLDHashTable t9(&trivialOps, sizeof(PLDHashEntryStub));
  PL_DHashTableAdd(&t9, (const void*)88);
  PLDHashTable t10(mozilla::Move(t9));  // new table constructed with inited

  return true;
}

static bool test_pldhash_Clear()
{
  PLDHashTable t1(&trivialOps, sizeof(PLDHashEntryStub));

  t1.Clear();
  if (t1.EntryCount() != 0) {
    return false;
  }

  t1.ClearAndPrepareForLength(100);
  if (t1.EntryCount() != 0) {
    return false;
  }

  PL_DHashTableAdd(&t1, (const void*)77);
  PL_DHashTableAdd(&t1, (const void*)88);
  PL_DHashTableAdd(&t1, (const void*)99);
  if (t1.EntryCount() != 3) {
    return false;
  }

  t1.Clear();
  if (t1.EntryCount() != 0) {
    return false;
  }

  PL_DHashTableAdd(&t1, (const void*)55);
  PL_DHashTableAdd(&t1, (const void*)66);
  PL_DHashTableAdd(&t1, (const void*)77);
  PL_DHashTableAdd(&t1, (const void*)88);
  PL_DHashTableAdd(&t1, (const void*)99);
  if (t1.EntryCount() != 5) {
    return false;
  }

  t1.ClearAndPrepareForLength(8192);
  if (t1.EntryCount() != 0) {
    return false;
  }

  return true;
}

static bool test_pldhash_Iterator()
{
  PLDHashTable t(&trivialOps, sizeof(PLDHashEntryStub));

  // Explicitly test the move constructor. We do this because, due to copy
  // elision, compilers might optimize away move constructor calls for normal
  // iterator use.
  {
    PLDHashTable::Iterator iter1(&t);
    PLDHashTable::Iterator iter2(mozilla::Move(iter1));
  }

  // Iterate through the empty table.
  for (PLDHashTable::Iterator iter(&t); !iter.Done(); iter.Next()) {
    (void) iter.Get();
    return false;   // shouldn't hit this
  }

  // Add three entries.
  PL_DHashTableAdd(&t, (const void*)77);
  PL_DHashTableAdd(&t, (const void*)88);
  PL_DHashTableAdd(&t, (const void*)99);

  // Check the iterator goes through each entry once.
  bool saw77 = false, saw88 = false, saw99 = false;
  int n = 0;
  for (auto iter(t.Iter()); !iter.Done(); iter.Next()) {
    auto entry = static_cast<PLDHashEntryStub*>(iter.Get());
    if (entry->key == (const void*)77) {
      saw77 = true;
    }
    if (entry->key == (const void*)88) {
      saw88 = true;
    }
    if (entry->key == (const void*)99) {
      saw99 = true;
    }
    n++;
  }
  if (!saw77 || !saw88 || !saw99 || n != 3) {
    return false;
  }

  return true;
}

static bool test_pldhash_RemovingIterator()
{
  PLDHashTable t(&trivialOps, sizeof(PLDHashEntryStub));

  // Explicitly test the move constructor. We do this because, due to copy
  // elision, compilers might optimize away move constructor calls for normal
  // iterator use.
  {
    PLDHashTable::RemovingIterator iter1(&t);
    PLDHashTable::RemovingIterator iter2(mozilla::Move(iter1));
  }

  // First, we insert 64 items, which results in a capacity of 128, and a load
  // factor of 50%.
  for (intptr_t i = 0; i < 64; i++) {
    PL_DHashTableAdd(&t, (const void*)i);
  }
  if (t.EntryCount() != 64 || t.Capacity() != 128) {
    return false;
  }

  // The first removing iterator does no removing; capacity and entry count are
  // unchanged.
  for (PLDHashTable::RemovingIterator iter(&t); !iter.Done(); iter.Next()) {
    (void) iter.Get();
  }
  if (t.EntryCount() != 64 || t.Capacity() != 128) {
    return false;
  }

  // The second removing iterator removes 16 items. This reduces the load
  // factor to 37.5% (48 / 128), which isn't low enough to shrink the table.
  for (auto iter = t.RemovingIter(); !iter.Done(); iter.Next()) {
    auto entry = static_cast<PLDHashEntryStub*>(iter.Get());
    if ((intptr_t)(entry->key) % 4 == 0) {
      iter.Remove();
    }
  }
  if (t.EntryCount() != 48 || t.Capacity() != 128) {
    return false;
  }

  // The third removing iterator removes another 16 items. This reduces
  // the load factor to 25% (32 / 128), so the table is shrunk.
  for (auto iter = t.RemovingIter(); !iter.Done(); iter.Next()) {
    auto entry = static_cast<PLDHashEntryStub*>(iter.Get());
    if ((intptr_t)(entry->key) % 2 == 0) {
      iter.Remove();
    }
  }
  if (t.EntryCount() != 32 || t.Capacity() != 64) {
    return false;
  }

  // The fourth removing iterator removes all remaining items. This reduces
  // the capacity to the minimum.
  for (auto iter = t.RemovingIter(); !iter.Done(); iter.Next()) {
    iter.Remove();
  }
  if (t.EntryCount() != 0 || t.Capacity() != PL_DHASH_MIN_CAPACITY) {
    return false;
  }

  return true;
}

// See bug 931062, we skip this test on Android due to OOM.
#ifndef MOZ_WIDGET_ANDROID
static bool test_pldhash_grow_to_max_capacity()
{
  // This is infallible.
  PLDHashTable* t =
    new PLDHashTable(&trivialOps, sizeof(PLDHashEntryStub), 128);

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
    delete t;
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
  DECL_TEST(test_pldhash_Clear),
  DECL_TEST(test_pldhash_Iterator),
  DECL_TEST(test_pldhash_RemovingIterator),
// See bug 931062, we skip this test on Android due to OOM. Also, it's slow,
// and so should always be last.
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
    printf("%35s : %s\n", t->name, test_result ? "SUCCESS" : "FAILURE");
    if (!test_result)
      success = false;
  }
  return success ? 0 : -1;
}
