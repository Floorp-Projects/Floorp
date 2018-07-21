/* -*- Mode: C++; tab-width: 9; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

// This is included first to ensure it doesn't implicitly depend on anything
// else.
#include "mozilla/BufferList.h"

// It would be nice if we could use the InfallibleAllocPolicy from mozalloc,
// but MFBT cannot use mozalloc.
class InfallibleAllocPolicy
{
public:
  template <typename T>
  T* pod_malloc(size_t aNumElems)
  {
    if (aNumElems & mozilla::tl::MulOverflowMask<sizeof(T)>::value) {
      MOZ_CRASH("TestBufferList.cpp: overflow");
    }
    T* rv = static_cast<T*>(malloc(aNumElems * sizeof(T)));
    if (!rv) {
      MOZ_CRASH("TestBufferList.cpp: out of memory");
    }
    return rv;
  }

  template <typename T>
  void free_(T* aPtr, size_t aNumElems = 0) { free(aPtr); }

  void reportAllocOverflow() const {}

  bool checkSimulatedOOM() const { return true; }
};

typedef mozilla::BufferList<InfallibleAllocPolicy> BufferList;

int main(void)
{
  const size_t kInitialSize = 16;
  const size_t kInitialCapacity = 24;
  const size_t kStandardCapacity = 32;

  BufferList bl(kInitialSize, kInitialCapacity, kStandardCapacity);

  memset(bl.Start(), 0x0c, kInitialSize);
  MOZ_RELEASE_ASSERT(bl.Size() == kInitialSize);

  // Simple iteration and access.

  BufferList::IterImpl iter(bl.Iter());
  MOZ_RELEASE_ASSERT(iter.RemainingInSegment() == kInitialSize);
  MOZ_RELEASE_ASSERT(iter.HasRoomFor(kInitialSize));
  MOZ_RELEASE_ASSERT(!iter.HasRoomFor(kInitialSize + 1));
  MOZ_RELEASE_ASSERT(!iter.HasRoomFor(size_t(-1)));
  MOZ_RELEASE_ASSERT(*iter.Data() == 0x0c);
  MOZ_RELEASE_ASSERT(!iter.Done());

  iter.Advance(bl, 4);
  MOZ_RELEASE_ASSERT(iter.RemainingInSegment() == kInitialSize - 4);
  MOZ_RELEASE_ASSERT(iter.HasRoomFor(kInitialSize - 4));
  MOZ_RELEASE_ASSERT(*iter.Data() == 0x0c);
  MOZ_RELEASE_ASSERT(!iter.Done());

  iter.Advance(bl, 11);
  MOZ_RELEASE_ASSERT(iter.RemainingInSegment() == kInitialSize - 4 - 11);
  MOZ_RELEASE_ASSERT(iter.HasRoomFor(kInitialSize - 4 - 11));
  MOZ_RELEASE_ASSERT(!iter.HasRoomFor(kInitialSize - 4 - 11 + 1));
  MOZ_RELEASE_ASSERT(*iter.Data() == 0x0c);
  MOZ_RELEASE_ASSERT(!iter.Done());

  iter.Advance(bl, kInitialSize - 4 - 11);
  MOZ_RELEASE_ASSERT(iter.RemainingInSegment() == 0);
  MOZ_RELEASE_ASSERT(!iter.HasRoomFor(1));
  MOZ_RELEASE_ASSERT(iter.Done());

  // Writing to the buffer.

  const size_t kSmallWrite = 16;

  char toWrite[kSmallWrite];
  memset(toWrite, 0x0a, kSmallWrite);
  bl.WriteBytes(toWrite, kSmallWrite);

  MOZ_RELEASE_ASSERT(bl.Size() == kInitialSize + kSmallWrite);

  iter = bl.Iter();
  iter.Advance(bl, kInitialSize);
  MOZ_RELEASE_ASSERT(!iter.Done());
  MOZ_RELEASE_ASSERT(iter.RemainingInSegment() == kInitialCapacity - kInitialSize);
  MOZ_RELEASE_ASSERT(iter.HasRoomFor(kInitialCapacity - kInitialSize));
  MOZ_RELEASE_ASSERT(*iter.Data() == 0x0a);

  // AdvanceAcrossSegments.

  iter = bl.Iter();
  MOZ_RELEASE_ASSERT(iter.AdvanceAcrossSegments(bl, kInitialCapacity - 4));
  MOZ_RELEASE_ASSERT(!iter.Done());
  MOZ_RELEASE_ASSERT(iter.RemainingInSegment() == 4);
  MOZ_RELEASE_ASSERT(iter.HasRoomFor(4));
  MOZ_RELEASE_ASSERT(*iter.Data() == 0x0a);

  iter = bl.Iter();
  MOZ_RELEASE_ASSERT(iter.AdvanceAcrossSegments(bl, kInitialSize + kSmallWrite - 4));
  MOZ_RELEASE_ASSERT(!iter.Done());
  MOZ_RELEASE_ASSERT(iter.RemainingInSegment() == 4);
  MOZ_RELEASE_ASSERT(iter.HasRoomFor(4));
  MOZ_RELEASE_ASSERT(*iter.Data() == 0x0a);

  MOZ_RELEASE_ASSERT(bl.Iter().AdvanceAcrossSegments(bl, kInitialSize + kSmallWrite - 1));
  MOZ_RELEASE_ASSERT(bl.Iter().AdvanceAcrossSegments(bl, kInitialSize + kSmallWrite));
  MOZ_RELEASE_ASSERT(!bl.Iter().AdvanceAcrossSegments(bl, kInitialSize + kSmallWrite + 1));
  MOZ_RELEASE_ASSERT(!bl.Iter().AdvanceAcrossSegments(bl, size_t(-1)));

  // Reading non-contiguous bytes.

  char toRead[kSmallWrite];
  iter = bl.Iter();
  iter.Advance(bl, kInitialSize);
  bl.ReadBytes(iter, toRead, kSmallWrite);
  MOZ_RELEASE_ASSERT(memcmp(toRead, toWrite, kSmallWrite) == 0);
  MOZ_RELEASE_ASSERT(iter.Done());

  // Make sure reading up to the end of a segment advances the iter to the next
  // segment.
  iter = bl.Iter();
  bl.ReadBytes(iter, toRead, kInitialSize);
  MOZ_RELEASE_ASSERT(!iter.Done());
  MOZ_RELEASE_ASSERT(iter.RemainingInSegment() == kInitialCapacity - kInitialSize);

  const size_t kBigWrite = 1024;

  char* toWriteBig = static_cast<char*>(malloc(kBigWrite));
  for (unsigned i = 0; i < kBigWrite; i++) {
    toWriteBig[i] = i % 37;
  }
  bl.WriteBytes(toWriteBig, kBigWrite);

  char* toReadBig = static_cast<char*>(malloc(kBigWrite));
  iter = bl.Iter();
  MOZ_RELEASE_ASSERT(iter.AdvanceAcrossSegments(bl, kInitialSize + kSmallWrite));
  bl.ReadBytes(iter, toReadBig, kBigWrite);
  MOZ_RELEASE_ASSERT(memcmp(toReadBig, toWriteBig, kBigWrite) == 0);
  MOZ_RELEASE_ASSERT(iter.Done());

  free(toReadBig);
  free(toWriteBig);

  // Currently bl contains these segments:
  // #0: offset 0, [0x0c]*16 + [0x0a]*8, size 24
  // #1: offset 24, [0x0a]*8 + [i%37 for i in 0..24], size 32
  // #2: offset 56, [i%37 for i in 24..56, size 32
  // ...
  // #32: offset 1016, [i%37 for i in 984..1016], size 32
  // #33: offset 1048, [i%37 for i in 1016..1024], size 8

  static size_t kTotalSize = kInitialSize + kSmallWrite + kBigWrite;

  MOZ_RELEASE_ASSERT(bl.Size() == kTotalSize);

  static size_t kLastSegmentSize = (kTotalSize - kInitialCapacity) % kStandardCapacity;

  iter = bl.Iter();
  MOZ_RELEASE_ASSERT(iter.AdvanceAcrossSegments(bl, kTotalSize - kLastSegmentSize - kStandardCapacity));
  MOZ_RELEASE_ASSERT(iter.RemainingInSegment() == kStandardCapacity);
  iter.Advance(bl, kStandardCapacity);
  MOZ_RELEASE_ASSERT(iter.RemainingInSegment() == kLastSegmentSize);
  MOZ_RELEASE_ASSERT(unsigned(*iter.Data()) == (kTotalSize - kLastSegmentSize - kInitialSize - kSmallWrite) % 37);

  // Clear.

  bl.Clear();
  MOZ_RELEASE_ASSERT(bl.Size() == 0);
  MOZ_RELEASE_ASSERT(bl.Iter().Done());

  // Move assignment.

  const size_t kSmallCapacity = 8;

  BufferList bl2(0, kSmallCapacity, kSmallCapacity);
  bl2.WriteBytes(toWrite, kSmallWrite);
  bl2.WriteBytes(toWrite, kSmallWrite);
  bl2.WriteBytes(toWrite, kSmallWrite);

  bl = std::move(bl2);
  MOZ_RELEASE_ASSERT(bl2.Size() == 0);
  MOZ_RELEASE_ASSERT(bl2.Iter().Done());

  iter = bl.Iter();
  MOZ_RELEASE_ASSERT(iter.AdvanceAcrossSegments(bl, kSmallWrite * 3));
  MOZ_RELEASE_ASSERT(iter.Done());

  // MoveFallible

  bool success;
  bl2 = bl.MoveFallible<InfallibleAllocPolicy>(&success);
  MOZ_RELEASE_ASSERT(success);
  MOZ_RELEASE_ASSERT(bl.Size() == 0);
  MOZ_RELEASE_ASSERT(bl.Iter().Done());
  MOZ_RELEASE_ASSERT(bl2.Size() == kSmallWrite * 3);

  iter = bl2.Iter();
  MOZ_RELEASE_ASSERT(iter.AdvanceAcrossSegments(bl2, kSmallWrite * 3));
  MOZ_RELEASE_ASSERT(iter.Done());

  bl = bl2.MoveFallible<InfallibleAllocPolicy>(&success);

  // Borrowing.

  const size_t kBorrowStart = 4;
  const size_t kBorrowSize = 24;

  iter = bl.Iter();
  iter.Advance(bl, kBorrowStart);
  bl2 = bl.Borrow<InfallibleAllocPolicy>(iter, kBorrowSize, &success);
  MOZ_RELEASE_ASSERT(success);
  MOZ_RELEASE_ASSERT(bl2.Size() == kBorrowSize);

  MOZ_RELEASE_ASSERT(iter.AdvanceAcrossSegments(bl, kSmallWrite * 3 - kBorrowSize - kBorrowStart));
  MOZ_RELEASE_ASSERT(iter.Done());

  iter = bl2.Iter();
  MOZ_RELEASE_ASSERT(iter.AdvanceAcrossSegments(bl2, kBorrowSize));
  MOZ_RELEASE_ASSERT(iter.Done());

  BufferList::IterImpl iter1(bl.Iter()), iter2(bl2.Iter());
  iter1.Advance(bl, kBorrowStart);
  MOZ_RELEASE_ASSERT(iter1.Data() == iter2.Data());
  MOZ_RELEASE_ASSERT(iter1.AdvanceAcrossSegments(bl, kBorrowSize - 5));
  MOZ_RELEASE_ASSERT(iter2.AdvanceAcrossSegments(bl2, kBorrowSize - 5));
  MOZ_RELEASE_ASSERT(iter1.Data() == iter2.Data());

  // Extracting.

  const size_t kExtractStart = 8;
  const size_t kExtractSize = 24;
  const size_t kExtractOverSize = 1000;

  iter = bl.Iter();
  iter.Advance(bl, kExtractStart);
  bl2 = bl.Extract(iter, kExtractSize, &success);
  MOZ_RELEASE_ASSERT(success);
  MOZ_RELEASE_ASSERT(bl2.Size() == kExtractSize);

  BufferList bl3 = bl.Extract(iter, kExtractOverSize, &success);
  MOZ_RELEASE_ASSERT(!success);

  iter = bl2.Iter();
  MOZ_RELEASE_ASSERT(iter.AdvanceAcrossSegments(bl2, kExtractSize));
  MOZ_RELEASE_ASSERT(iter.Done());

  BufferList bl4(8, 8, 8);
  bl4.WriteBytes("abcd1234", 8);
  iter = bl4.Iter();
  iter.Advance(bl4, 8);

  BufferList bl5 = bl4.Extract(iter, kExtractSize, &success);
  MOZ_RELEASE_ASSERT(!success);

  BufferList bl6(0, 0, 16);
  bl6.WriteBytes("abcdefgh12345678", 16);
  bl6.WriteBytes("ijklmnop87654321", 16);
  iter = bl6.Iter();
  iter.Advance(bl6, 8);
  BufferList bl7 = bl6.Extract(iter, 16, &success);
  MOZ_RELEASE_ASSERT(success);
  char data[16];
  MOZ_RELEASE_ASSERT(bl6.ReadBytes(iter, data, 8));
  MOZ_RELEASE_ASSERT(memcmp(data, "87654321", 8) == 0);
  iter = bl7.Iter();
  MOZ_RELEASE_ASSERT(bl7.ReadBytes(iter, data, 16));
  MOZ_RELEASE_ASSERT(memcmp(data, "12345678ijklmnop", 16) == 0);

  BufferList bl8(0, 0, 16);
  bl8.WriteBytes("abcdefgh12345678", 16);
  iter = bl8.Iter();
  BufferList bl9 = bl8.Extract(iter, 8, &success);
  MOZ_RELEASE_ASSERT(success);
  MOZ_RELEASE_ASSERT(bl9.Size() == 8);
  MOZ_RELEASE_ASSERT(!iter.Done());

  BufferList bl10(0, 0, 8);
  bl10.WriteBytes("abcdefgh", 8);
  bl10.WriteBytes("12345678", 8);
  iter = bl10.Iter();
  BufferList bl11 = bl10.Extract(iter, 16, &success);
  MOZ_RELEASE_ASSERT(success);
  MOZ_RELEASE_ASSERT(bl11.Size() == 16);
  MOZ_RELEASE_ASSERT(iter.Done());
  iter = bl11.Iter();
  MOZ_RELEASE_ASSERT(bl11.ReadBytes(iter, data, 16));
  MOZ_RELEASE_ASSERT(memcmp(data, "abcdefgh12345678", 16) == 0);

  return 0;
}
