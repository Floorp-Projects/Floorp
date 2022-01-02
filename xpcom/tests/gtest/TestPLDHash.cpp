/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "PLDHashTable.h"
#include "nsCOMPtr.h"
#include "nsICrashReporter.h"
#include "nsServiceManagerUtils.h"
#include "gtest/gtest.h"

// This test mostly focuses on edge cases. But more coverage of normal
// operations wouldn't be a bad thing.

#ifdef XP_UNIX
#  include <unistd.h>
#  include <sys/types.h>
#  include <sys/wait.h>

// This global variable is defined in toolkit/xre/nsSigHandlers.cpp.
extern unsigned int _gdb_sleep_duration;
#endif

// We can test that certain operations cause expected aborts by forking
// and then checking that the child aborted in the expected way (i.e. via
// MOZ_CRASH). We skip this for the following configurations.
// - On Windows, because it doesn't have fork().
// - On non-DEBUG builds, because the crashes cause the crash reporter to pop
//   up when running this test locally, which is surprising and annoying.
// - On ASAN builds, because ASAN alters the way a MOZ_CRASHing process
//   terminates, which makes it harder to test if the right thing has occurred.
static void TestCrashyOperation(const char* label, void (*aCrashyOperation)()) {
#if defined(XP_UNIX) && defined(DEBUG) && !defined(MOZ_ASAN)
  // We're about to trigger a crash. When it happens don't pause to allow GDB
  // to be attached.
  unsigned int old_gdb_sleep_duration = _gdb_sleep_duration;
  _gdb_sleep_duration = 0;

  int pid = fork();
  ASSERT_NE(pid, -1);

  if (pid == 0) {
    // Disable the crashreporter -- writing a crash dump in the child will
    // prevent the parent from writing a subsequent dump. Crashes here are
    // expected, so we don't want their stacks to show up in the log anyway.
    nsCOMPtr<nsICrashReporter> crashreporter =
        do_GetService("@mozilla.org/toolkit/crash-reporter;1");
    if (crashreporter) {
      crashreporter->SetEnabled(false);
    }

    // Child: perform the crashy operation.
    fprintf(stderr,
            "TestCrashyOperation %s: The following crash is expected. Do not "
            "panic.\n",
            label);
    aCrashyOperation();
    fprintf(stderr, "TestCrashyOperation %s: didn't crash?!\n", label);
    ASSERT_TRUE(false);  // shouldn't reach here
  }

  // Parent: check that child crashed as expected.
  int status;
  ASSERT_NE(waitpid(pid, &status, 0), -1);

  // The path taken here depends on the platform and configuration.
  ASSERT_TRUE(WIFEXITED(status) || WTERMSIG(status));
  if (WIFEXITED(status)) {
    // This occurs if the ah_crap_handler() is run, i.e. we caught the crash.
    // It returns the number of the caught signal.
    int signum = WEXITSTATUS(status);
    if (signum != SIGSEGV && signum != SIGBUS) {
      fprintf(stderr, "TestCrashyOperation %s: 'exited' failure: %d\n", label,
              signum);
      ASSERT_TRUE(false);
    }
  } else if (WIFSIGNALED(status)) {
    // This one occurs if we didn't catch the crash. The exit code is the
    // number of the terminating signal.
    int signum = WTERMSIG(status);
    if (signum != SIGSEGV && signum != SIGBUS) {
      fprintf(stderr, "TestCrashyOperation %s: 'signaled' failure: %d\n", label,
              signum);
      ASSERT_TRUE(false);
    }
  }

  _gdb_sleep_duration = old_gdb_sleep_duration;
#endif
}

static void InitCapacityOk_InitialLengthTooBig() {
  PLDHashTable t(PLDHashTable::StubOps(), sizeof(PLDHashEntryStub),
                 PLDHashTable::kMaxInitialLength + 1);
}

static void InitCapacityOk_InitialEntryStoreTooBig() {
  // Try the smallest disallowed power-of-two entry store size, which is 2^32
  // bytes (which overflows to 0). (Note that the 2^23 *length* gets converted
  // to a 2^24 *capacity*.)
  PLDHashTable t(PLDHashTable::StubOps(), (uint32_t)1 << 8, (uint32_t)1 << 23);
}

static void InitCapacityOk_EntrySizeTooBig() {
  // Try the smallest disallowed entry size, which is 256 bytes.
  PLDHashTable t(PLDHashTable::StubOps(), 256);
}

TEST(PLDHashTableTest, InitCapacityOk)
{
  // Try the largest allowed capacity.  With kMaxCapacity==1<<26, this
  // would allocate (if we added an element) 0.5GB of entry store on 32-bit
  // platforms and 1GB on 64-bit platforms.
  PLDHashTable t1(PLDHashTable::StubOps(), sizeof(PLDHashEntryStub),
                  PLDHashTable::kMaxInitialLength);

  // Try the largest allowed power-of-two entry store size, which is 2^31 bytes
  // (Note that the 2^23 *length* gets converted to a 2^24 *capacity*.)
  PLDHashTable t2(PLDHashTable::StubOps(), (uint32_t)1 << 7, (uint32_t)1 << 23);

  // Try a too-large capacity (which aborts).
  TestCrashyOperation("length too big", InitCapacityOk_InitialLengthTooBig);

  // Try a large capacity combined with a large entry size that when multiplied
  // overflow (causing abort).
  TestCrashyOperation("entry store too big",
                      InitCapacityOk_InitialEntryStoreTooBig);

  // Try the largest allowed entry size.
  PLDHashTable t3(PLDHashTable::StubOps(), 255);

  // Try an overly large entry size.
  TestCrashyOperation("entry size too big", InitCapacityOk_EntrySizeTooBig);

  // Ideally we'd also try a large-but-ok capacity that almost but doesn't
  // quite overflow, but that would result in allocating slightly less than 4
  // GiB of entry storage. That would be very likely to fail on 32-bit
  // platforms, so such a test wouldn't be reliable.
}

TEST(PLDHashTableTest, LazyStorage)
{
  PLDHashTable t(PLDHashTable::StubOps(), sizeof(PLDHashEntryStub));

  // PLDHashTable allocates entry storage lazily. Check that all the non-add
  // operations work appropriately when the table is empty and the storage
  // hasn't yet been allocated.

  ASSERT_EQ(t.Capacity(), 0u);
  ASSERT_EQ(t.EntrySize(), sizeof(PLDHashEntryStub));
  ASSERT_EQ(t.EntryCount(), 0u);
  ASSERT_EQ(t.Generation(), 0u);

  ASSERT_TRUE(!t.Search((const void*)1));

  // No result to check here, but call it to make sure it doesn't crash.
  t.Remove((const void*)2);

  for (auto iter = t.Iter(); !iter.Done(); iter.Next()) {
    ASSERT_TRUE(false);  // shouldn't hit this on an empty table
  }

  ASSERT_EQ(t.ShallowSizeOfExcludingThis(moz_malloc_size_of), 0u);
}

// A trivial hash function is good enough here. It's also super-fast for the
// GrowToMaxCapacity test because we insert the integers 0.., which means it's
// collision-free.
static PLDHashNumber TrivialHash(const void* key) {
  return (PLDHashNumber)(size_t)key;
}

static void TrivialInitEntry(PLDHashEntryHdr* aEntry, const void* aKey) {
  auto entry = static_cast<PLDHashEntryStub*>(aEntry);
  entry->key = aKey;
}

static const PLDHashTableOps trivialOps = {
    TrivialHash, PLDHashTable::MatchEntryStub, PLDHashTable::MoveEntryStub,
    PLDHashTable::ClearEntryStub, TrivialInitEntry};

TEST(PLDHashTableTest, MoveSemantics)
{
  PLDHashTable t1(&trivialOps, sizeof(PLDHashEntryStub));
  t1.Add((const void*)88);
  PLDHashTable t2(&trivialOps, sizeof(PLDHashEntryStub));
  t2.Add((const void*)99);

#if defined(__clang__)
#  pragma clang diagnostic push
#  pragma clang diagnostic ignored "-Wself-move"
#endif
  t1 = std::move(t1);  // self-move
#if defined(__clang__)
#  pragma clang diagnostic pop
#endif

  t1 = std::move(t2);  // empty overwritten with empty

  PLDHashTable t3(&trivialOps, sizeof(PLDHashEntryStub));
  PLDHashTable t4(&trivialOps, sizeof(PLDHashEntryStub));
  t3.Add((const void*)88);

  t3 = std::move(t4);  // non-empty overwritten with empty

  PLDHashTable t5(&trivialOps, sizeof(PLDHashEntryStub));
  PLDHashTable t6(&trivialOps, sizeof(PLDHashEntryStub));
  t6.Add((const void*)88);

  t5 = std::move(t6);  // empty overwritten with non-empty

  PLDHashTable t7(&trivialOps, sizeof(PLDHashEntryStub));
  PLDHashTable t8(std::move(t7));  // new table constructed with uninited

  PLDHashTable t9(&trivialOps, sizeof(PLDHashEntryStub));
  t9.Add((const void*)88);
  PLDHashTable t10(std::move(t9));  // new table constructed with inited
}

TEST(PLDHashTableTest, Clear)
{
  PLDHashTable t1(&trivialOps, sizeof(PLDHashEntryStub));

  t1.Clear();
  ASSERT_EQ(t1.EntryCount(), 0u);

  t1.ClearAndPrepareForLength(100);
  ASSERT_EQ(t1.EntryCount(), 0u);

  t1.Add((const void*)77);
  t1.Add((const void*)88);
  t1.Add((const void*)99);
  ASSERT_EQ(t1.EntryCount(), 3u);

  t1.Clear();
  ASSERT_EQ(t1.EntryCount(), 0u);

  t1.Add((const void*)55);
  t1.Add((const void*)66);
  t1.Add((const void*)77);
  t1.Add((const void*)88);
  t1.Add((const void*)99);
  ASSERT_EQ(t1.EntryCount(), 5u);

  t1.ClearAndPrepareForLength(8192);
  ASSERT_EQ(t1.EntryCount(), 0u);
}

TEST(PLDHashTableTest, Iterator)
{
  PLDHashTable t(&trivialOps, sizeof(PLDHashEntryStub));

  // Explicitly test the move constructor. We do this because, due to copy
  // elision, compilers might optimize away move constructor calls for normal
  // iterator use.
  {
    PLDHashTable::Iterator iter1(&t);
    PLDHashTable::Iterator iter2(std::move(iter1));
  }

  // Iterate through the empty table.
  for (PLDHashTable::Iterator iter(&t); !iter.Done(); iter.Next()) {
    (void)iter.Get();
    ASSERT_TRUE(false);  // shouldn't hit this
  }

  // Add three entries.
  t.Add((const void*)77);
  t.Add((const void*)88);
  t.Add((const void*)99);

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
  ASSERT_TRUE(saw77 && saw88 && saw99 && n == 3);

  t.Clear();

  // First, we insert 64 items, which results in a capacity of 128, and a load
  // factor of 50%.
  for (intptr_t i = 0; i < 64; i++) {
    t.Add((const void*)i);
  }
  ASSERT_EQ(t.EntryCount(), 64u);
  ASSERT_EQ(t.Capacity(), 128u);

  // The first removing iterator does no removing; capacity and entry count are
  // unchanged.
  for (PLDHashTable::Iterator iter(&t); !iter.Done(); iter.Next()) {
    (void)iter.Get();
  }
  ASSERT_EQ(t.EntryCount(), 64u);
  ASSERT_EQ(t.Capacity(), 128u);

  // The second removing iterator removes 16 items. This reduces the load
  // factor to 37.5% (48 / 128), which isn't low enough to shrink the table.
  for (auto iter = t.Iter(); !iter.Done(); iter.Next()) {
    auto entry = static_cast<PLDHashEntryStub*>(iter.Get());
    if ((intptr_t)(entry->key) % 4 == 0) {
      iter.Remove();
    }
  }
  ASSERT_EQ(t.EntryCount(), 48u);
  ASSERT_EQ(t.Capacity(), 128u);

  // The third removing iterator removes another 16 items. This reduces
  // the load factor to 25% (32 / 128), so the table is shrunk.
  for (auto iter = t.Iter(); !iter.Done(); iter.Next()) {
    auto entry = static_cast<PLDHashEntryStub*>(iter.Get());
    if ((intptr_t)(entry->key) % 2 == 0) {
      iter.Remove();
    }
  }
  ASSERT_EQ(t.EntryCount(), 32u);
  ASSERT_EQ(t.Capacity(), 64u);

  // The fourth removing iterator removes all remaining items. This reduces
  // the capacity to the minimum.
  for (auto iter = t.Iter(); !iter.Done(); iter.Next()) {
    iter.Remove();
  }
  ASSERT_EQ(t.EntryCount(), 0u);
  ASSERT_EQ(t.Capacity(), unsigned(PLDHashTable::kMinCapacity));
}

TEST(PLDHashTableTest, WithEntryHandle)
{
  PLDHashTable t(&trivialOps, sizeof(PLDHashEntryStub));

  PLDHashEntryHdr* entry1 =
      t.WithEntryHandle((const void*)88, [](auto entryHandle) {
        EXPECT_FALSE(entryHandle);

        bool initEntryCalled = false;
        PLDHashEntryHdr* entry =
            entryHandle.OrInsert([&initEntryCalled](PLDHashEntryHdr* entry) {
              EXPECT_TRUE(entry);
              TrivialInitEntry(entry, (const void*)88);
              initEntryCalled = true;
            });
        EXPECT_TRUE(initEntryCalled);
        EXPECT_EQ(entryHandle.Entry(), entry);

        return entry;
      });
  ASSERT_TRUE(entry1);
  ASSERT_EQ(t.EntryCount(), 1u);

  PLDHashEntryHdr* entry2 =
      t.WithEntryHandle((const void*)88, [](auto entryHandle) {
        EXPECT_TRUE(entryHandle);

        bool initEntryCalled = false;
        PLDHashEntryHdr* entry =
            entryHandle.OrInsert([&initEntryCalled](PLDHashEntryHdr* entry) {
              EXPECT_TRUE(entry);
              TrivialInitEntry(entry, (const void*)88);
              initEntryCalled = true;
            });
        EXPECT_FALSE(initEntryCalled);
        EXPECT_EQ(entryHandle.Entry(), entry);

        return entry;
      });
  ASSERT_TRUE(entry2);
  ASSERT_EQ(t.EntryCount(), 1u);

  ASSERT_EQ(entry1, entry2);
}

// This test involves resizing a table repeatedly up to 512 MiB in size. On
// 32-bit platforms (Win32, Android) it sometimes OOMs, causing the test to
// fail. (See bug 931062 and bug 1267227.) Therefore, we only run it on 64-bit
// platforms where OOM is much less likely.
//
// Also, it's slow, and so should always be last.
#ifdef HAVE_64BIT_BUILD
TEST(PLDHashTableTest, GrowToMaxCapacity)
{
  // This is infallible.
  PLDHashTable* t =
      new PLDHashTable(&trivialOps, sizeof(PLDHashEntryStub), 128);

  // Keep inserting elements until failure occurs because the table is full.
  size_t numInserted = 0;
  while (true) {
    if (!t->Add((const void*)numInserted, mozilla::fallible)) {
      break;
    }
    numInserted++;
  }

  // We stop when the element count is 96.875% of PLDHashTable::kMaxCapacity
  // (see MaxLoadOnGrowthFailure()).
  if (numInserted !=
      PLDHashTable::kMaxCapacity - (PLDHashTable::kMaxCapacity >> 5)) {
    delete t;
    ASSERT_TRUE(false);
  }

  delete t;
}
#endif
