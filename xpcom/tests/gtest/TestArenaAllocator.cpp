/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/ArenaAllocator.h"
#include "mozilla/ArenaAllocatorExtensions.h"
#include "nsIMemoryReporter.h"  // MOZ_MALLOC_SIZE_OF

#include "gtest/gtest.h"

using mozilla::ArenaAllocator;

TEST(ArenaAllocator, Constructor)
{ ArenaAllocator<4096, 4> a; }

TEST(ArenaAllocator, DefaultAllocate)
{
  // Test default 1-byte alignment.
  ArenaAllocator<1024> a;
  void* x = a.Allocate(101);
  void* y = a.Allocate(101);

  // Given 1-byte aligment, we expect the allocations to follow
  // each other exactly.
  EXPECT_EQ(uintptr_t(x) + 101, uintptr_t(y));
}

TEST(ArenaAllocator, AllocateAlignment)
{
  // Test non-default 8-byte alignment.
  static const size_t kAlignment = 8;
  ArenaAllocator<1024, kAlignment> a;

  // Make sure aligment is correct for 1-8.
  for (size_t i = 1; i <= kAlignment; i++) {
    // All of these should be 8 bytes
    void* x = a.Allocate(i);
    void* y = a.Allocate(i);
    EXPECT_EQ(uintptr_t(x) + kAlignment, uintptr_t(y));
  }

  // Test with slightly larger than specified alignment.
  void* x = a.Allocate(kAlignment + 1);
  void* y = a.Allocate(kAlignment + 1);

  // Given 8-byte aligment, and a non-8-byte aligned request we expect the
  // allocations to be padded.
  EXPECT_NE(uintptr_t(x) + kAlignment, uintptr_t(y));

  // We expect 7 bytes of padding to have been added.
  EXPECT_EQ(uintptr_t(x) + kAlignment * 2, uintptr_t(y));
}

#if 0
TEST(ArenaAllocator, AllocateZeroBytes)
{
  // This would have to be a death test. Since we chose to provide an
  // infallible allocator we can't just return nullptr in the 0 case as
  // there's no way to differentiate that from the OOM case.
  ArenaAllocator<1024> a;
  void* x = a.Allocate(0);
  EXPECT_FALSE(x);
}

TEST(ArenaAllocator, BadAlignment)
{
  // This test causes build failures by triggering the static assert enforcing
  // a power-of-two alignment.
  ArenaAllocator<256, 3> a;
  ArenaAllocator<256, 7> b;
  ArenaAllocator<256, 17> c;
}
#endif

TEST(ArenaAllocator, AllocateMultipleSizes)
{
  // Test non-default 4-byte alignment.
  ArenaAllocator<4096, 4> a;

  for (int i = 1; i < 50; i++) {
    void* x = a.Allocate(i);
    // All the allocations should be aligned properly.
    EXPECT_EQ(uintptr_t(x) % 4, uintptr_t(0));
  }

  // Test a large 64-byte alignment
  ArenaAllocator<8192, 64> b;
  for (int i = 1; i < 100; i++) {
    void* x = b.Allocate(i);
    // All the allocations should be aligned properly.
    EXPECT_EQ(uintptr_t(x) % 64, uintptr_t(0));
  }
}

TEST(ArenaAllocator, AllocateInDifferentChunks)
{
  // Test default 1-byte alignment.
  ArenaAllocator<4096> a;
  void* x = a.Allocate(4000);
  void* y = a.Allocate(4000);
  EXPECT_NE(uintptr_t(x) + 4000, uintptr_t(y));
}

TEST(ArenaAllocator, AllocateLargerThanArenaSize)
{
  // Test default 1-byte alignment.
  ArenaAllocator<256> a;
  void* x = a.Allocate(4000);
  void* y = a.Allocate(4000);
  EXPECT_TRUE(x);
  EXPECT_TRUE(y);

  // Now try a normal allocation, it should behave as expected.
  x = a.Allocate(8);
  y = a.Allocate(8);
  EXPECT_EQ(uintptr_t(x) + 8, uintptr_t(y));
}

#ifndef MOZ_CODE_COVERAGE
TEST(ArenaAllocator, AllocationsPerChunk)
{
  // Test that expected number of allocations fit in one chunk.
  // We use an alignment of 64-bytes to avoid worrying about differences in
  // the header size on 32 and 64-bit platforms.
  const size_t kArenaSize = 1024;
  const size_t kAlignment = 64;
  ArenaAllocator<kArenaSize, kAlignment> a;

  // With an alignment of 64 bytes we expect the header to take up the first
  // alignment sized slot leaving bytes leaving the rest available for
  // allocation.
  const size_t kAllocationsPerChunk = (kArenaSize / kAlignment) - 1;
  void* x = nullptr;
  void* y = a.Allocate(kAlignment);
  EXPECT_TRUE(y);
  for (size_t i = 1; i < kAllocationsPerChunk; i++) {
    x = y;
    y = a.Allocate(kAlignment);
    EXPECT_EQ(uintptr_t(x) + kAlignment, uintptr_t(y));
  }

  // The next allocation should be in a different chunk.
  x = y;
  y = a.Allocate(kAlignment);
  EXPECT_NE(uintptr_t(x) + kAlignment, uintptr_t(y));
}

TEST(ArenaAllocator, MemoryIsValid)
{
  // Make multiple allocations and actually access the memory. This is
  // expected to trip up ASAN or valgrind if out of bounds memory is
  // accessed.
  static const size_t kArenaSize = 1024;
  static const size_t kAlignment = 64;
  static const char kMark = char(0xBC);
  ArenaAllocator<kArenaSize, kAlignment> a;

  // Single allocation that should fill the arena.
  size_t sz = kArenaSize - kAlignment;
  char* x = (char*)a.Allocate(sz);
  EXPECT_EQ(uintptr_t(x) % kAlignment, uintptr_t(0));
  memset(x, kMark, sz);
  for (size_t i = 0; i < sz; i++) {
    EXPECT_EQ(x[i], kMark);
  }

  // Allocation over arena size.
  sz = kArenaSize * 2;
  x = (char*)a.Allocate(sz);
  EXPECT_EQ(uintptr_t(x) % kAlignment, uintptr_t(0));
  memset(x, kMark, sz);
  for (size_t i = 0; i < sz; i++) {
    EXPECT_EQ(x[i], kMark);
  }

  // Allocation half the arena size.
  sz = kArenaSize / 2;
  x = (char*)a.Allocate(sz);
  EXPECT_EQ(uintptr_t(x) % kAlignment, uintptr_t(0));
  memset(x, kMark, sz);
  for (size_t i = 0; i < sz; i++) {
    EXPECT_EQ(x[i], kMark);
  }

  // Repeat, this should actually end up in a new chunk.
  x = (char*)a.Allocate(sz);
  EXPECT_EQ(uintptr_t(x) % kAlignment, uintptr_t(0));
  memset(x, kMark, sz);
  for (size_t i = 0; i < sz; i++) {
    EXPECT_EQ(x[i], kMark);
  }
}
#endif

MOZ_DEFINE_MALLOC_SIZE_OF(TestSizeOf);

TEST(ArenaAllocator, SizeOf)
{
  // This tests the sizeof functionality. We can't test for equality as we
  // can't reliably guarantee what sizes the underlying allocator is going to
  // choose, so we just test that things grow (or not) as expected.
  static const size_t kArenaSize = 4096;
  ArenaAllocator<kArenaSize> a;

  // Excluding *this we expect an empty arena allocator to have no overhead.
  size_t sz = a.SizeOfExcludingThis(TestSizeOf);
  EXPECT_EQ(sz, size_t(0));

  // Cause one chunk to be allocated.
  (void)a.Allocate(kArenaSize / 2);
  size_t prev_sz = sz;
  sz = a.SizeOfExcludingThis(TestSizeOf);
  EXPECT_GT(sz, prev_sz);

  // Allocate within the current chunk.
  (void)a.Allocate(kArenaSize / 4);
  prev_sz = sz;
  sz = a.SizeOfExcludingThis(TestSizeOf);
  EXPECT_EQ(sz, prev_sz);

  // Overflow to a new chunk.
  (void)a.Allocate(kArenaSize / 2);
  prev_sz = sz;
  sz = a.SizeOfExcludingThis(TestSizeOf);
  EXPECT_GT(sz, prev_sz);

  // Allocate an oversized chunk with enough room for a header to fit in page
  // size. We expect the underlying allocator to round up to page alignment.
  (void)a.Allocate((kArenaSize * 2) - 64);
  sz = a.SizeOfExcludingThis(TestSizeOf);
  EXPECT_GT(sz, prev_sz);
}

TEST(ArenaAllocator, Clear)
{
  // Tests that the Clear function works as expected. The best proxy for
  // checking if a clear is successful is to measure the size. If it's empty we
  // expect the size to be 0.
  static const size_t kArenaSize = 128;
  ArenaAllocator<kArenaSize> a;

  // Clearing an empty arena should work.
  a.Clear();

  size_t sz = a.SizeOfExcludingThis(TestSizeOf);
  EXPECT_EQ(sz, size_t(0));

  // Allocating should work after clearing an empty arena.
  void* x = a.Allocate(10);
  EXPECT_TRUE(x);

  size_t prev_sz = sz;
  sz = a.SizeOfExcludingThis(TestSizeOf);
  EXPECT_GT(sz, prev_sz);

  // Allocate enough for a few arena chunks to be necessary.
  for (size_t i = 0; i < kArenaSize * 2; i++) {
    x = a.Allocate(1);
    EXPECT_TRUE(x);
  }

  prev_sz = sz;
  sz = a.SizeOfExcludingThis(TestSizeOf);
  EXPECT_GT(sz, prev_sz);

  // Clearing should reduce the size back to zero.
  a.Clear();
  sz = a.SizeOfExcludingThis(TestSizeOf);
  EXPECT_EQ(sz, size_t(0));

  // Allocating should work after clearing an arena with allocations.
  x = a.Allocate(10);
  EXPECT_TRUE(x);

  prev_sz = sz;
  sz = a.SizeOfExcludingThis(TestSizeOf);
  EXPECT_GT(sz, prev_sz);
}

TEST(ArenaAllocator, Extensions)
{
  ArenaAllocator<4096, 8> a;

  // Test with raw strings.
  const char* const kTestCStr = "This is a test string.";
  char* c_dup = mozilla::ArenaStrdup(kTestCStr, a);
  EXPECT_STREQ(c_dup, kTestCStr);

  const char16_t* const kTestStr = u"This is a wide test string.";
  char16_t* dup = mozilla::ArenaStrdup(kTestStr, a);
  EXPECT_TRUE(nsString(dup).Equals(kTestStr));

  // Make sure it works with literal strings.
  NS_NAMED_LITERAL_STRING(wideStr, "A wide string.");
  nsLiteralString::char_type* wide = mozilla::ArenaStrdup(wideStr, a);
  EXPECT_TRUE(wideStr.Equals(wide));

  NS_NAMED_LITERAL_CSTRING(cStr, "A c-string.");
  nsLiteralCString::char_type* cstr = mozilla::ArenaStrdup(cStr, a);
  EXPECT_TRUE(cStr.Equals(cstr));

  // Make sure it works with normal strings.
  nsAutoString x(u"testing wide");
  nsAutoString::char_type* x_copy = mozilla::ArenaStrdup(x, a);
  EXPECT_TRUE(x.Equals(x_copy));

  nsAutoCString y("testing c-string");
  nsAutoCString::char_type* y_copy = mozilla::ArenaStrdup(y, a);
  EXPECT_TRUE(y.Equals(y_copy));
}
