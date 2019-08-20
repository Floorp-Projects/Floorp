/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "BaseProfiler.h"

#ifdef MOZ_BASE_PROFILER

#  include "mozilla/BlocksRingBuffer.h"
#  include "mozilla/leb128iterator.h"
#  include "mozilla/ModuloBuffer.h"
#  include "mozilla/PowerOfTwo.h"

#  include "mozilla/Attributes.h"
#  include "mozilla/Vector.h"

#  if defined(_MSC_VER)
#    include <windows.h>
#    include <mmsystem.h>
#    include <process.h>
#  else
#    include <time.h>
#    include <unistd.h>
#  endif

#  include <algorithm>
#  include <atomic>
#  include <thread>
#  include <type_traits>

using namespace mozilla;

MOZ_MAYBE_UNUSED static void SleepMilli(unsigned aMilliseconds) {
#  if defined(_MSC_VER)
  Sleep(aMilliseconds);
#  else
  struct timespec ts;
  ts.tv_sec = aMilliseconds / 1000;
  ts.tv_nsec = long(aMilliseconds % 1000) * 1000000;
  struct timespec tr;
  while (nanosleep(&ts, &tr)) {
    if (errno == EINTR) {
      ts = tr;
    } else {
      printf("nanosleep() -> %s\n", strerror(errno));
      exit(1);
    }
  }
#  endif
}

void TestPowerOfTwoMask() {
  printf("TestPowerOfTwoMask...\n");

  static_assert(MakePowerOfTwoMask<uint32_t, 0>().MaskValue() == 0, "");
  constexpr PowerOfTwoMask<uint32_t> c0 = MakePowerOfTwoMask<uint32_t, 0>();
  MOZ_RELEASE_ASSERT(c0.MaskValue() == 0);

  static_assert(MakePowerOfTwoMask<uint32_t, 0xFFu>().MaskValue() == 0xFFu, "");
  constexpr PowerOfTwoMask<uint32_t> cFF =
      MakePowerOfTwoMask<uint32_t, 0xFFu>();
  MOZ_RELEASE_ASSERT(cFF.MaskValue() == 0xFFu);

  static_assert(
      MakePowerOfTwoMask<uint32_t, 0xFFFFFFFFu>().MaskValue() == 0xFFFFFFFFu,
      "");
  constexpr PowerOfTwoMask<uint32_t> cFFFFFFFF =
      MakePowerOfTwoMask<uint32_t, 0xFFFFFFFFu>();
  MOZ_RELEASE_ASSERT(cFFFFFFFF.MaskValue() == 0xFFFFFFFFu);

  struct TestDataU32 {
    uint32_t mInput;
    uint32_t mMask;
  };
  // clang-format off
  TestDataU32 tests[] = {
    { 0, 0 },
    { 1, 1 },
    { 2, 3 },
    { 3, 3 },
    { 4, 7 },
    { 5, 7 },
    { (1u << 31) - 1, (1u << 31) - 1 },
    { (1u << 31), uint32_t(-1) },
    { (1u << 31) + 1, uint32_t(-1) },
    { uint32_t(-1), uint32_t(-1) }
  };
  // clang-format on
  for (const TestDataU32& test : tests) {
    PowerOfTwoMask<uint32_t> p2m(test.mInput);
    MOZ_RELEASE_ASSERT(p2m.MaskValue() == test.mMask);
    for (const TestDataU32& inner : tests) {
      if (p2m.MaskValue() != uint32_t(-1)) {
        MOZ_RELEASE_ASSERT((inner.mInput % p2m) ==
                           (inner.mInput % (p2m.MaskValue() + 1)));
      }
      MOZ_RELEASE_ASSERT((inner.mInput & p2m) == (inner.mInput % p2m));
      MOZ_RELEASE_ASSERT((p2m & inner.mInput) == (inner.mInput & p2m));
    }
  }

  printf("TestPowerOfTwoMask done\n");
}

void TestPowerOfTwo() {
  printf("TestPowerOfTwo...\n");

  static_assert(MakePowerOfTwo<uint32_t, 1>().Value() == 1, "");
  constexpr PowerOfTwo<uint32_t> c1 = MakePowerOfTwo<uint32_t, 1>();
  MOZ_RELEASE_ASSERT(c1.Value() == 1);
  static_assert(MakePowerOfTwo<uint32_t, 1>().Mask().MaskValue() == 0, "");

  static_assert(MakePowerOfTwo<uint32_t, 128>().Value() == 128, "");
  constexpr PowerOfTwo<uint32_t> c128 = MakePowerOfTwo<uint32_t, 128>();
  MOZ_RELEASE_ASSERT(c128.Value() == 128);
  static_assert(MakePowerOfTwo<uint32_t, 128>().Mask().MaskValue() == 127, "");

  static_assert(MakePowerOfTwo<uint32_t, 0x80000000u>().Value() == 0x80000000u,
                "");
  constexpr PowerOfTwo<uint32_t> cMax = MakePowerOfTwo<uint32_t, 0x80000000u>();
  MOZ_RELEASE_ASSERT(cMax.Value() == 0x80000000u);
  static_assert(
      MakePowerOfTwo<uint32_t, 0x80000000u>().Mask().MaskValue() == 0x7FFFFFFFu,
      "");

  struct TestDataU32 {
    uint32_t mInput;
    uint32_t mValue;
    uint32_t mMask;
  };
  // clang-format off
  TestDataU32 tests[] = {
    { 0, 1, 0 },
    { 1, 1, 0 },
    { 2, 2, 1 },
    { 3, 4, 3 },
    { 4, 4, 3 },
    { 5, 8, 7 },
    { (1u << 31) - 1, (1u << 31), (1u << 31) - 1 },
    { (1u << 31), (1u << 31), (1u << 31) - 1 },
    { (1u << 31) + 1, (1u << 31), (1u << 31) - 1 },
    { uint32_t(-1), (1u << 31), (1u << 31) - 1 }
  };
  // clang-format on
  for (const TestDataU32& test : tests) {
    PowerOfTwo<uint32_t> p2(test.mInput);
    MOZ_RELEASE_ASSERT(p2.Value() == test.mValue);
    MOZ_RELEASE_ASSERT(p2.MaskValue() == test.mMask);
    PowerOfTwoMask<uint32_t> p2m = p2.Mask();
    MOZ_RELEASE_ASSERT(p2m.MaskValue() == test.mMask);
    for (const TestDataU32& inner : tests) {
      MOZ_RELEASE_ASSERT((inner.mInput % p2) == (inner.mInput % p2.Value()));
    }
  }

  printf("TestPowerOfTwo done\n");
}

void TestLEB128() {
  printf("TestLEB128...\n");

  MOZ_RELEASE_ASSERT(ULEB128MaxSize<uint8_t>() == 2);
  MOZ_RELEASE_ASSERT(ULEB128MaxSize<uint16_t>() == 3);
  MOZ_RELEASE_ASSERT(ULEB128MaxSize<uint32_t>() == 5);
  MOZ_RELEASE_ASSERT(ULEB128MaxSize<uint64_t>() == 10);

  struct TestDataU64 {
    uint64_t mValue;
    unsigned mSize;
    const char* mBytes;
  };
  // clang-format off
  TestDataU64 tests[] = {
    // Small numbers should keep their normal byte representation.
    {                  0u,  1, "\0" },
    {                  1u,  1, "\x01" },

    // 0111 1111 (127, or 0x7F) is the highest number that fits into a single
    // LEB128 byte. It gets encoded as 0111 1111, note the most significant bit
    // is off.
    {               0x7Fu,  1, "\x7F" },

    // Next number: 128, or 0x80.
    //   Original data representation:  1000 0000
    //     Broken up into groups of 7:         1  0000000
    // Padded with 0 (msB) or 1 (lsB):  00000001 10000000
    //            Byte representation:  0x01     0x80
    //            Little endian order:  -> 0x80 0x01
    {               0x80u,  2, "\x80\x01" },

    // Next: 129, or 0x81 (showing that we don't lose low bits.)
    //   Original data representation:  1000 0001
    //     Broken up into groups of 7:         1  0000001
    // Padded with 0 (msB) or 1 (lsB):  00000001 10000001
    //            Byte representation:  0x01     0x81
    //            Little endian order:  -> 0x81 0x01
    {               0x81u,  2, "\x81\x01" },

    // Highest 8-bit number: 255, or 0xFF.
    //   Original data representation:  1111 1111
    //     Broken up into groups of 7:         1  1111111
    // Padded with 0 (msB) or 1 (lsB):  00000001 11111111
    //            Byte representation:  0x01     0xFF
    //            Little endian order:  -> 0xFF 0x01
    {               0xFFu,  2, "\xFF\x01" },

    // Next: 256, or 0x100.
    //   Original data representation:  1 0000 0000
    //     Broken up into groups of 7:        10  0000000
    // Padded with 0 (msB) or 1 (lsB):  00000010 10000000
    //            Byte representation:  0x10     0x80
    //            Little endian order:  -> 0x80 0x02
    {              0x100u,  2, "\x80\x02" },

    // Highest 32-bit number: 0xFFFFFFFF (8 bytes, all bits set).
    // Original: 1111 1111 1111 1111 1111 1111 1111 1111
    // Groups:     1111  1111111  1111111  1111111  1111111
    // Padded: 00001111 11111111 11111111 11111111 11111111
    // Bytes:  0x0F     0xFF     0xFF     0xFF     0xFF
    // Little Endian: -> 0xFF 0xFF 0xFF 0xFF 0x0F
    {         0xFFFFFFFFu,  5, "\xFF\xFF\xFF\xFF\x0F" },

    // Highest 64-bit number: 0xFFFFFFFFFFFFFFFF (16 bytes, all bits set).
    // 64 bits, that's 9 groups of 7 bits, plus 1 (most significant) bit.
    { 0xFFFFFFFFFFFFFFFFu, 10, "\xFF\xFF\xFF\xFF\xFF\xFF\xFF\xFF\xFF\x01" }
  };
  // clang-format on

  for (const TestDataU64& test : tests) {
    MOZ_RELEASE_ASSERT(ULEB128Size(test.mValue) == test.mSize);
    // Prepare a buffer that can accomodate the largest-possible LEB128.
    uint8_t buffer[ULEB128MaxSize<uint64_t>()];
    // Use a pointer into the buffer as iterator.
    uint8_t* p = buffer;
    // And write the LEB128.
    WriteULEB128(test.mValue, p);
    // Pointer (iterator) should have advanced just past the expected LEB128
    // size.
    MOZ_RELEASE_ASSERT(p == buffer + test.mSize);
    // Check expected bytes.
    for (unsigned i = 0; i < test.mSize; ++i) {
      MOZ_RELEASE_ASSERT(buffer[i] == uint8_t(test.mBytes[i]));
    }
    // Move pointer (iterator) back to start of buffer.
    p = buffer;
    // And read the LEB128 we wrote above.
    uint64_t read = ReadULEB128<uint64_t>(p);
    // Pointer (iterator) should have also advanced just past the expected
    // LEB128 size.
    MOZ_RELEASE_ASSERT(p == buffer + test.mSize);
    // And check the read value.
    MOZ_RELEASE_ASSERT(read == test.mValue);
  }

  printf("TestLEB128 done\n");
}

static void TestModuloBuffer(ModuloBuffer<>& mb, uint32_t MBSize) {
  using MB = ModuloBuffer<>;

  MOZ_RELEASE_ASSERT(mb.BufferLength().Value() == MBSize);

  // Iterator comparisons.
  MOZ_RELEASE_ASSERT(mb.ReaderAt(2) == mb.ReaderAt(2));
  MOZ_RELEASE_ASSERT(mb.ReaderAt(2) != mb.ReaderAt(3));
  MOZ_RELEASE_ASSERT(mb.ReaderAt(2) < mb.ReaderAt(3));
  MOZ_RELEASE_ASSERT(mb.ReaderAt(2) <= mb.ReaderAt(2));
  MOZ_RELEASE_ASSERT(mb.ReaderAt(2) <= mb.ReaderAt(3));
  MOZ_RELEASE_ASSERT(mb.ReaderAt(3) > mb.ReaderAt(2));
  MOZ_RELEASE_ASSERT(mb.ReaderAt(2) >= mb.ReaderAt(2));
  MOZ_RELEASE_ASSERT(mb.ReaderAt(3) >= mb.ReaderAt(2));

  // Iterators indices don't wrap around (even though they may be pointing at
  // the same location).
  MOZ_RELEASE_ASSERT(mb.ReaderAt(2) != mb.ReaderAt(MBSize + 2));
  MOZ_RELEASE_ASSERT(mb.ReaderAt(MBSize + 2) != mb.ReaderAt(2));

  // Dereference.
  static_assert(std::is_same<decltype(*mb.ReaderAt(0)), const MB::Byte&>::value,
                "Dereferencing from a reader should return const Byte*");
  static_assert(std::is_same<decltype(*mb.WriterAt(0)), MB::Byte&>::value,
                "Dereferencing from a writer should return Byte*");
  // Contiguous between 0 and MBSize-1.
  MOZ_RELEASE_ASSERT(&*mb.ReaderAt(MBSize - 1) ==
                     &*mb.ReaderAt(0) + (MBSize - 1));
  // Wraps around.
  MOZ_RELEASE_ASSERT(&*mb.ReaderAt(MBSize) == &*mb.ReaderAt(0));
  MOZ_RELEASE_ASSERT(&*mb.ReaderAt(MBSize + MBSize - 1) ==
                     &*mb.ReaderAt(MBSize - 1));
  MOZ_RELEASE_ASSERT(&*mb.ReaderAt(MBSize + MBSize) == &*mb.ReaderAt(0));
  // Power of 2 modulo wrapping.
  MOZ_RELEASE_ASSERT(&*mb.ReaderAt(uint32_t(-1)) == &*mb.ReaderAt(MBSize - 1));
  MOZ_RELEASE_ASSERT(&*mb.ReaderAt(static_cast<MB::Index>(-1)) ==
                     &*mb.ReaderAt(MBSize - 1));

  // Arithmetic.
  MB::Reader arit = mb.ReaderAt(0);
  MOZ_RELEASE_ASSERT(++arit == mb.ReaderAt(1));
  MOZ_RELEASE_ASSERT(arit == mb.ReaderAt(1));

  MOZ_RELEASE_ASSERT(--arit == mb.ReaderAt(0));
  MOZ_RELEASE_ASSERT(arit == mb.ReaderAt(0));

  MOZ_RELEASE_ASSERT(arit + 3 == mb.ReaderAt(3));
  MOZ_RELEASE_ASSERT(arit == mb.ReaderAt(0));

  // (Can't have assignments inside asserts, hence the split.)
  const bool checkPlusEq = ((arit += 3) == mb.ReaderAt(3));
  MOZ_RELEASE_ASSERT(checkPlusEq);
  MOZ_RELEASE_ASSERT(arit == mb.ReaderAt(3));

  MOZ_RELEASE_ASSERT((arit - 2) == mb.ReaderAt(1));
  MOZ_RELEASE_ASSERT(arit == mb.ReaderAt(3));

  const bool checkMinusEq = ((arit -= 2) == mb.ReaderAt(1));
  MOZ_RELEASE_ASSERT(checkMinusEq);
  MOZ_RELEASE_ASSERT(arit == mb.ReaderAt(1));

  // Iterator difference.
  MOZ_RELEASE_ASSERT(mb.ReaderAt(3) - mb.ReaderAt(1) == 2);
  MOZ_RELEASE_ASSERT(mb.ReaderAt(1) - mb.ReaderAt(3) == MB::Index(-2));

  // Only testing Writer, as Reader is just a subset with no code differences.
  MB::Writer it = mb.WriterAt(0);
  MOZ_RELEASE_ASSERT(it.CurrentIndex() == 0);

  // Write two characters at the start.
  it.WriteObject('x');
  it.WriteObject('y');

  // Backtrack to read them.
  it -= 2;
  // PeekObject should read without moving.
  MOZ_RELEASE_ASSERT(it.PeekObject<char>() == 'x');
  MOZ_RELEASE_ASSERT(it.CurrentIndex() == 0);
  // ReadObject should read and move past the character.
  MOZ_RELEASE_ASSERT(it.ReadObject<char>() == 'x');
  MOZ_RELEASE_ASSERT(it.CurrentIndex() == 1);
  MOZ_RELEASE_ASSERT(it.PeekObject<char>() == 'y');
  MOZ_RELEASE_ASSERT(it.CurrentIndex() == 1);
  MOZ_RELEASE_ASSERT(it.ReadObject<char>() == 'y');
  MOZ_RELEASE_ASSERT(it.CurrentIndex() == 2);

  // Checking that a reader can be created from a writer.
  MB::Reader it2(it);
  MOZ_RELEASE_ASSERT(it2.CurrentIndex() == 2);
  // Or assigned.
  it2 = it;
  MOZ_RELEASE_ASSERT(it2.CurrentIndex() == 2);

  // Write 4-byte number at index 2.
  it.WriteObject(int32_t(123));
  MOZ_RELEASE_ASSERT(it.CurrentIndex() == 6);
  // And another, which should now wrap around (but index continues on.)
  it.WriteObject(int32_t(456));
  MOZ_RELEASE_ASSERT(it.CurrentIndex() == MBSize + 2);
  // Even though index==MBSize+2, we can read the object we wrote at 2.
  MOZ_RELEASE_ASSERT(it.ReadObject<int32_t>() == 123);
  MOZ_RELEASE_ASSERT(it.CurrentIndex() == MBSize + 6);
  // And similarly, index MBSize+6 points at the same location as index 6.
  MOZ_RELEASE_ASSERT(it.ReadObject<int32_t>() == 456);
  MOZ_RELEASE_ASSERT(it.CurrentIndex() == MBSize + MBSize + 2);
}

void TestModuloBuffer() {
  printf("TestModuloBuffer...\n");

  // Testing ModuloBuffer with default template arguments.
  using MB = ModuloBuffer<>;

  // Only 8-byte buffers, to easily test wrap-around.
  constexpr uint32_t MBSize = 8;

  // MB with self-allocated heap buffer.
  MB mbByLength(MakePowerOfTwo32<MBSize>());
  TestModuloBuffer(mbByLength, MBSize);

  // MB taking ownership of a provided UniquePtr to a buffer.
  auto uniqueBuffer = MakeUnique<uint8_t[]>(MBSize);
  MB mbByUniquePtr(MakeUnique<uint8_t[]>(MBSize), MakePowerOfTwo32<MBSize>());
  TestModuloBuffer(mbByUniquePtr, MBSize);

  // MB using part of a buffer on the stack. The buffer is three times the
  // required size: The middle third is where ModuloBuffer will work, the first
  // and last thirds are only used to later verify that ModuloBuffer didn't go
  // out of its bounds.
  uint8_t buffer[MBSize * 3];
  // Pre-fill the buffer with a known pattern, so we can later see what changed.
  for (size_t i = 0; i < MBSize * 3; ++i) {
    buffer[i] = uint8_t('A' + i);
  }
  MB mbByBuffer(&buffer[MBSize], MakePowerOfTwo32<MBSize>());
  TestModuloBuffer(mbByBuffer, MBSize);

  // Check that only the provided stack-based sub-buffer was modified.
  uint32_t changed = 0;
  for (size_t i = MBSize; i < MBSize * 2; ++i) {
    changed += (buffer[i] == uint8_t('A' + i)) ? 0 : 1;
  }
  // Expect at least 75% changes.
  MOZ_RELEASE_ASSERT(changed >= MBSize * 6 / 8);

  // Everything around the sub-buffer should be unchanged.
  for (size_t i = 0; i < MBSize; ++i) {
    MOZ_RELEASE_ASSERT(buffer[i] == uint8_t('A' + i));
  }
  for (size_t i = MBSize * 2; i < MBSize * 3; ++i) {
    MOZ_RELEASE_ASSERT(buffer[i] == uint8_t('A' + i));
  }

  // Check that move-construction is allowed. This verifies that we do not
  // crash from a double free, when `mbByBuffer` and `mbByStolenBuffer` are both
  // destroyed at the end of this function.
  MB mbByStolenBuffer = std::move(mbByBuffer);
  TestModuloBuffer(mbByStolenBuffer, MBSize);

  // Check that only the provided stack-based sub-buffer was modified.
  changed = 0;
  for (size_t i = MBSize; i < MBSize * 2; ++i) {
    changed += (buffer[i] == uint8_t('A' + i)) ? 0 : 1;
  }
  // Expect at least 75% changes.
  MOZ_RELEASE_ASSERT(changed >= MBSize * 6 / 8);

  // Everything around the sub-buffer should be unchanged.
  for (size_t i = 0; i < MBSize; ++i) {
    MOZ_RELEASE_ASSERT(buffer[i] == uint8_t('A' + i));
  }
  for (size_t i = MBSize * 2; i < MBSize * 3; ++i) {
    MOZ_RELEASE_ASSERT(buffer[i] == uint8_t('A' + i));
  }

  printf("TestModuloBuffer done\n");
}

// Backdoor into value of BlockIndex, only for unit-testing.
static uint64_t ExtractBlockIndex(const BlocksRingBuffer::BlockIndex bi) {
  uint64_t index;
  static_assert(sizeof(bi) == sizeof(index),
                "BlockIndex expected to only contain a uint64_t");
  memcpy(&index, &bi, sizeof(index));
  return index;
};

void TestBlocksRingBufferAPI() {
  printf("TestBlocksRingBufferAPI...\n");

  // Entry destructor will store about-to-be-cleared value in `lastDestroyed`.
  uint32_t lastDestroyed = 0;

  // Create a 16-byte buffer, enough to store up to 3 entries (1 byte size + 4
  // bytes uint64_t).
  constexpr uint32_t MBSize = 16;
  uint8_t buffer[MBSize * 3];
  for (size_t i = 0; i < MBSize * 3; ++i) {
    buffer[i] = uint8_t('A' + i);
  }

  // Start a temporary block to constrain buffer lifetime.
  {
    BlocksRingBuffer rb(&buffer[MBSize], MakePowerOfTwo32<MBSize>(),
                        [&](BlocksRingBuffer::EntryReader& aReader) {
                          lastDestroyed = aReader.ReadObject<uint32_t>();
                        });

#  define VERIFY_START_END_DESTROYED(aStart, aEnd, aLastDestroyed)          \
    {                                                                       \
      BlocksRingBuffer::State state = rb.GetState();                        \
      MOZ_RELEASE_ASSERT(ExtractBlockIndex(state.mRangeStart) == (aStart)); \
      MOZ_RELEASE_ASSERT(ExtractBlockIndex(state.mRangeEnd) == (aEnd));     \
      MOZ_RELEASE_ASSERT(lastDestroyed == (aLastDestroyed));                \
    }

    // All entries will contain one 32-bit number. The resulting blocks will
    // have the following structure:
    // - 1 byte for the LEB128 size of 4
    // - 4 bytes for the number.
    // E.g., if we have entries with `123` and `456`:
    //   .-- Index 0 reserved for empty BlockIndex, nothing there.
    //   | .-- first readable block at index 1
    //   | |.-- first block at index 1
    //   | ||.-- 1 byte for the entry size, which is `4` (32 bits)
    //   | |||  .-- entry starts at index 2, contains 32-bit int
    //   | |||  |             .-- entry and block finish *after* index 5 (so 6)
    //   | |||  |             | .-- second block starts at index 6
    //   | |||  |             | |         etc.
    //   | |||  |             | |                  .-- End readable blocks: 11
    //   v vvv  v             v V                  v
    //   0   1   2   3   4   5   6   7   8   9  10  11  12  13  14  15
    //   - S[4 |   int(123)   ] [4 |   int(456)   ]E

    // Empty buffer to start with.
    // Start&end indices still at 1 (0 is reserved for the default BlockIndex{}
    // that cannot point at a valid entry), nothing destroyed.
    VERIFY_START_END_DESTROYED(1, 1, 0);

    // Default BlockIndex.
    BlocksRingBuffer::BlockIndex bi0;
    if (bi0) {
      MOZ_RELEASE_ASSERT(false, "if (BlockIndex{}) should fail test");
    }
    if (!bi0) {
    } else {
      MOZ_RELEASE_ASSERT(false, "if (!BlockIndex{}) should succeed test");
    }
    MOZ_RELEASE_ASSERT(!bi0);
    MOZ_RELEASE_ASSERT(bi0 == bi0);
    MOZ_RELEASE_ASSERT(bi0 <= bi0);
    MOZ_RELEASE_ASSERT(bi0 >= bi0);
    MOZ_RELEASE_ASSERT(!(bi0 != bi0));
    MOZ_RELEASE_ASSERT(!(bi0 < bi0));
    MOZ_RELEASE_ASSERT(!(bi0 > bi0));

    // Default BlockIndex can be used, but returns no valid entry.
    rb.ReadAt(bi0, [](Maybe<BlocksRingBuffer::EntryReader>&& aMaybeReader) {
      MOZ_RELEASE_ASSERT(aMaybeReader.isNothing());
    });

    // Push `1` directly.
    MOZ_RELEASE_ASSERT(ExtractBlockIndex(rb.PutObject(uint32_t(1))) == 1);
    //   0   1   2   3   4   5   6   7   8   9  10  11  12  13  14  15
    //   - S[4 |    int(1)    ]E
    VERIFY_START_END_DESTROYED(1, 6, 0);

    // Push `2` through ReserveAndPut, check output BlockIndex.
    auto bi2 = rb.ReserveAndPut([]() { return sizeof(uint32_t); },
                                [](BlocksRingBuffer::EntryWriter* aEW) {
                                  MOZ_RELEASE_ASSERT(!!aEW);
                                  aEW->WriteObject(uint32_t(2));
                                  return aEW->CurrentBlockIndex();
                                });
    static_assert(
        std::is_same<decltype(bi2), BlocksRingBuffer::BlockIndex>::value,
        "All index-returning functions should return a "
        "BlocksRingBuffer::BlockIndex");
    MOZ_RELEASE_ASSERT(ExtractBlockIndex(bi2) == 6);
    //   0   1   2   3   4   5   6   7   8   9  10  11  12  13  14  15
    //   - S[4 |    int(1)    ] [4 |    int(2)    ]E
    VERIFY_START_END_DESTROYED(1, 11, 0);

    // Check single entry at bi2, store next block index.
    auto bi2Next =
        rb.ReadAt(bi2, [](Maybe<BlocksRingBuffer::EntryReader>&& aMaybeReader) {
          MOZ_RELEASE_ASSERT(aMaybeReader.isSome());
          MOZ_RELEASE_ASSERT(aMaybeReader->ReadObject<uint32_t>() == 2);
          MOZ_RELEASE_ASSERT(
              aMaybeReader->GetEntryAt(aMaybeReader->NextBlockIndex())
                  .isNothing());
          return aMaybeReader->NextBlockIndex();
        });
    // bi2Next is at the end, nothing to read.
    rb.ReadAt(bi2Next, [](Maybe<BlocksRingBuffer::EntryReader>&& aMaybeReader) {
      MOZ_RELEASE_ASSERT(aMaybeReader.isNothing());
    });

    // BlockIndex tests.
    if (bi2) {
    } else {
      MOZ_RELEASE_ASSERT(false,
                         "if (non-default-BlockIndex) should succeed test");
    }
    if (!bi2) {
      MOZ_RELEASE_ASSERT(false,
                         "if (!non-default-BlockIndex) should fail test");
    }

    MOZ_RELEASE_ASSERT(!!bi2);
    MOZ_RELEASE_ASSERT(bi2 == bi2);
    MOZ_RELEASE_ASSERT(bi2 <= bi2);
    MOZ_RELEASE_ASSERT(bi2 >= bi2);
    MOZ_RELEASE_ASSERT(!(bi2 != bi2));
    MOZ_RELEASE_ASSERT(!(bi2 < bi2));
    MOZ_RELEASE_ASSERT(!(bi2 > bi2));

    MOZ_RELEASE_ASSERT(bi0 != bi2);
    MOZ_RELEASE_ASSERT(bi0 < bi2);
    MOZ_RELEASE_ASSERT(bi0 <= bi2);
    MOZ_RELEASE_ASSERT(!(bi0 == bi2));
    MOZ_RELEASE_ASSERT(!(bi0 > bi2));
    MOZ_RELEASE_ASSERT(!(bi0 >= bi2));

    MOZ_RELEASE_ASSERT(bi2 != bi0);
    MOZ_RELEASE_ASSERT(bi2 > bi0);
    MOZ_RELEASE_ASSERT(bi2 >= bi0);
    MOZ_RELEASE_ASSERT(!(bi2 == bi0));
    MOZ_RELEASE_ASSERT(!(bi2 < bi0));
    MOZ_RELEASE_ASSERT(!(bi2 <= bi0));

    MOZ_RELEASE_ASSERT(bi2 != bi2Next);
    MOZ_RELEASE_ASSERT(bi2 < bi2Next);
    MOZ_RELEASE_ASSERT(bi2 <= bi2Next);
    MOZ_RELEASE_ASSERT(!(bi2 == bi2Next));
    MOZ_RELEASE_ASSERT(!(bi2 > bi2Next));
    MOZ_RELEASE_ASSERT(!(bi2 >= bi2Next));

    MOZ_RELEASE_ASSERT(bi2Next != bi2);
    MOZ_RELEASE_ASSERT(bi2Next > bi2);
    MOZ_RELEASE_ASSERT(bi2Next >= bi2);
    MOZ_RELEASE_ASSERT(!(bi2Next == bi2));
    MOZ_RELEASE_ASSERT(!(bi2Next < bi2));
    MOZ_RELEASE_ASSERT(!(bi2Next <= bi2));

    // Push `3` through Put, check writer output
    // is returned to the initial caller.
    auto put3 =
        rb.Put(sizeof(uint32_t), [&](BlocksRingBuffer::EntryWriter* aEW) {
          MOZ_RELEASE_ASSERT(!!aEW);
          aEW->WriteObject(uint32_t(3));
          return float(ExtractBlockIndex(aEW->CurrentBlockIndex()));
        });
    static_assert(std::is_same<decltype(put3), float>::value,
                  "Expect float as returned by callback.");
    MOZ_RELEASE_ASSERT(put3 == 11.0);
    //   0   1   2   3   4   5   6   7   8   9  10  11  12  13  14  15 (16)
    //   - S[4 |    int(1)    ] [4 |    int(2)    ] [4 |    int(3)    ]E
    VERIFY_START_END_DESTROYED(1, 16, 0);

    // Re-Read single entry at bi2, should now have a next entry.
    rb.ReadAt(bi2, [&](Maybe<BlocksRingBuffer::EntryReader>&& aMaybeReader) {
      MOZ_RELEASE_ASSERT(aMaybeReader.isSome());
      MOZ_RELEASE_ASSERT(aMaybeReader->ReadObject<uint32_t>() == 2);
      MOZ_RELEASE_ASSERT(aMaybeReader->NextBlockIndex() == bi2Next);
      MOZ_RELEASE_ASSERT(aMaybeReader->GetNextEntry().isSome());
      MOZ_RELEASE_ASSERT(
          aMaybeReader->GetEntryAt(aMaybeReader->NextBlockIndex()).isSome());
      MOZ_RELEASE_ASSERT(
          aMaybeReader->GetNextEntry()->CurrentBlockIndex() ==
          aMaybeReader->GetEntryAt(aMaybeReader->NextBlockIndex())
              ->CurrentBlockIndex());
      MOZ_RELEASE_ASSERT(
          aMaybeReader->GetEntryAt(aMaybeReader->NextBlockIndex())
              ->ReadObject<uint32_t>() == 3);
    });

    // Check that we have `1` to `3`.
    uint32_t count = 0;
    rb.ReadEach([&](BlocksRingBuffer::EntryReader& aReader) {
      MOZ_RELEASE_ASSERT(aReader.ReadObject<uint32_t>() == ++count);
    });
    MOZ_RELEASE_ASSERT(count == 3);

    // Push `4`, store its BlockIndex for later.
    // This will wrap around, and destroy the first entry.
    BlocksRingBuffer::BlockIndex bi4 = rb.PutObject(uint32_t(4));
    // Before:
    //   0   1   2   3   4   5   6   7   8   9  10  11  12  13  14  15 (16)
    //   - S[4 |    int(1)    ] [4 |    int(2)    ] [4 |    int(3)    ]E
    // 1. First entry destroyed:
    //   0   1   2   3   4   5   6   7   8   9  10  11  12  13  14  15 (16)
    //   -   ?   ?   ?   ?   ? S[4 |    int(2)    ] [4 |    int(3)    ]E
    // 2. New entry starts at 15 and wraps around: (shown on separate line)
    //   0   1   2   3   4   5   6   7   8   9  10  11  12  13  14  15 (16)
    //   -   ?   ?   ?   ?   ? S[4 |    int(2)    ] [4 |    int(3)    ]
    //  16  17  18  19  20  21  ...
    //  [4 |    int(4)    ]E
    // (collapsed)
    //  16  17  18  19  20  21   6   7   8   9  10  11  12  13  14  15 (16)
    //  [4 |    int(4)    ]E ? S[4 |    int(2)    ] [4 |    int(3)    ]
    VERIFY_START_END_DESTROYED(6, 21, 1);

    // Check that we have `2` to `4`.
    count = 1;
    rb.ReadEach([&](BlocksRingBuffer::EntryReader& aReader) {
      MOZ_RELEASE_ASSERT(aReader.ReadObject<uint32_t>() == ++count);
    });
    MOZ_RELEASE_ASSERT(count == 4);

    // Push 5 through Put, no returns.
    // This will destroy the second entry.
    // Check that the EntryWriter can access bi4 but not bi2.
    auto bi5_6 =
        rb.Put(sizeof(uint32_t), [&](BlocksRingBuffer::EntryWriter* aEW) {
          MOZ_RELEASE_ASSERT(!!aEW);
          aEW->WriteObject(uint32_t(5));
          MOZ_RELEASE_ASSERT(aEW->GetEntryAt(bi2).isNothing());
          MOZ_RELEASE_ASSERT(aEW->GetEntryAt(bi4).isSome());
          MOZ_RELEASE_ASSERT(aEW->GetEntryAt(bi4)->CurrentBlockIndex() == bi4);
          MOZ_RELEASE_ASSERT(aEW->GetEntryAt(bi4)->ReadObject<uint32_t>() == 4);
          return MakePair(aEW->CurrentBlockIndex(), aEW->BlockEndIndex());
        });
    auto& bi5 = bi5_6.first();
    auto& bi6 = bi5_6.second();
    //  16  17  18  19  20  21  22  23  24  25  26  11  12  13  14  15 (16)
    //  [4 |    int(4)    ] [4 |    int(5)    ]E ? S[4 |    int(3)    ]
    VERIFY_START_END_DESTROYED(11, 26, 2);

    // Read single entry at bi2, should now gracefully fail.
    rb.ReadAt(bi2, [](Maybe<BlocksRingBuffer::EntryReader>&& aMaybeReader) {
      MOZ_RELEASE_ASSERT(aMaybeReader.isNothing());
    });

    // Read single entry at bi5.
    rb.ReadAt(bi5, [](Maybe<BlocksRingBuffer::EntryReader>&& aMaybeReader) {
      MOZ_RELEASE_ASSERT(aMaybeReader.isSome());
      MOZ_RELEASE_ASSERT(aMaybeReader->ReadObject<uint32_t>() == 5);
      MOZ_RELEASE_ASSERT(
          aMaybeReader->GetEntryAt(aMaybeReader->NextBlockIndex()).isNothing());
    });

    rb.Read([&](BlocksRingBuffer::Reader* aReader) {
      MOZ_RELEASE_ASSERT(!!aReader);
      // begin() and end() should be at the range edges (verified above).
      MOZ_RELEASE_ASSERT(
          ExtractBlockIndex(aReader->begin().CurrentBlockIndex()) == 11);
      MOZ_RELEASE_ASSERT(
          ExtractBlockIndex(aReader->end().CurrentBlockIndex()) == 26);
      // Null BlockIndex clamped to the beginning.
      MOZ_RELEASE_ASSERT(aReader->At(bi0) == aReader->begin());
      // Cleared block index clamped to the beginning.
      MOZ_RELEASE_ASSERT(aReader->At(bi2) == aReader->begin());
      // At(begin) same as begin().
      MOZ_RELEASE_ASSERT(aReader->At(aReader->begin().CurrentBlockIndex()) ==
                         aReader->begin());
      // bi5 at expected position.
      MOZ_RELEASE_ASSERT(
          ExtractBlockIndex(aReader->At(bi5).CurrentBlockIndex()) == 21);
      // bi6 at expected position at the end.
      MOZ_RELEASE_ASSERT(aReader->At(bi6) == aReader->end());
      // At(end) same as end().
      MOZ_RELEASE_ASSERT(aReader->At(aReader->end().CurrentBlockIndex()) ==
                         aReader->end());
    });

    // Check that we have `3` to `5`.
    count = 2;
    rb.ReadEach([&](BlocksRingBuffer::EntryReader& aReader) {
      MOZ_RELEASE_ASSERT(aReader.ReadObject<uint32_t>() == ++count);
    });
    MOZ_RELEASE_ASSERT(count == 5);

    // Clear everything before `4`, this should destroy `3`.
    rb.ClearBefore(bi4);
    //  16  17  18  19  20  21  22  23  24  25  26  11  12  13  14  15
    // S[4 |    int(4)    ] [4 |    int(5)    ]E ?   ?   ?   ?   ?   ?
    VERIFY_START_END_DESTROYED(16, 26, 3);

    // Check that we have `4` to `5`.
    count = 3;
    rb.ReadEach([&](BlocksRingBuffer::EntryReader& aReader) {
      MOZ_RELEASE_ASSERT(aReader.ReadObject<uint32_t>() == ++count);
    });
    MOZ_RELEASE_ASSERT(count == 5);

    // Clear everything before `4` again, nothing to destroy.
    lastDestroyed = 0;
    rb.ClearBefore(bi4);
    VERIFY_START_END_DESTROYED(16, 26, 0);

    // Clear everything, this should destroy `4` and `5`, and bring the start
    // index where the end index currently is.
    rb.ClearBefore(bi6);
    //  16  17  18  19  20  21  22  23  24  25  26  11  12  13  14  15
    //   ?   ?   ?   ?   ?   ?   ?   ?   ?   ? SE?   ?   ?   ?   ?   ?
    VERIFY_START_END_DESTROYED(26, 26, 5);

    // Check that we have nothing to read.
    rb.ReadEach([&](auto&&) { MOZ_RELEASE_ASSERT(false); });

    // Read single entry at bi5, should now gracefully fail.
    rb.ReadAt(bi5, [](Maybe<BlocksRingBuffer::EntryReader>&& aMaybeReader) {
      MOZ_RELEASE_ASSERT(aMaybeReader.isNothing());
    });

    // Clear everything before now-cleared `4`, nothing to destroy.
    lastDestroyed = 0;
    rb.ClearBefore(bi4);
    VERIFY_START_END_DESTROYED(26, 26, 0);

    // Push `6` directly.
    MOZ_RELEASE_ASSERT(rb.PutObject(uint32_t(6)) == bi6);
    //  16  17  18  19  20  21  22  23  24  25  26  27  28  29  30  31
    //   ?   ?   ?   ?   ?   ?   ?   ?   ?   ? S[4 |    int(6)    ]E ?
    VERIFY_START_END_DESTROYED(26, 31, 0);

    // End of block where rb lives, BlocksRingBuffer destructor should call
    // entry destructor for remaining entries.
  }
  MOZ_RELEASE_ASSERT(lastDestroyed == 6);

  // Check that only the provided stack-based sub-buffer was modified.
  uint32_t changed = 0;
  for (size_t i = MBSize; i < MBSize * 2; ++i) {
    changed += (buffer[i] == uint8_t('A' + i)) ? 0 : 1;
  }
  // Expect at least 75% changes.
  MOZ_RELEASE_ASSERT(changed >= MBSize * 6 / 8);

  // Everything around the sub-buffer should be unchanged.
  for (size_t i = 0; i < MBSize; ++i) {
    MOZ_RELEASE_ASSERT(buffer[i] == uint8_t('A' + i));
  }
  for (size_t i = MBSize * 2; i < MBSize * 3; ++i) {
    MOZ_RELEASE_ASSERT(buffer[i] == uint8_t('A' + i));
  }

  printf("TestBlocksRingBufferAPI done\n");
}

void TestBlocksRingBufferUnderlyingBufferChanges() {
  printf("TestBlocksRingBufferUnderlyingBufferChanges...\n");

  // Out-of-session BlocksRingBuffer to start with.
  BlocksRingBuffer rb;

  // Block index to read at. Initially "null", but may be changed below.
  BlocksRingBuffer::BlockIndex bi;

  // Test all rb APIs when rb is out-of-session and therefore doesn't have an
  // underlying buffer.
  auto testOutOfSession = [&]() {
    MOZ_RELEASE_ASSERT(rb.BufferLength().isNothing());
    BlocksRingBuffer::State state = rb.GetState();
    // When out-of-session, range start and ends are the same, and there are no
    // pushed&cleared blocks.
    MOZ_RELEASE_ASSERT(state.mRangeStart == state.mRangeEnd);
    MOZ_RELEASE_ASSERT(state.mPushedBlockCount == 0);
    MOZ_RELEASE_ASSERT(state.mClearedBlockCount == 0);
    // `Put()` functions run the callback with `Nothing`.
    int32_t ran = 0;
    rb.Put(1, [&](BlocksRingBuffer::EntryWriter* aMaybeEntryWriter) {
      MOZ_RELEASE_ASSERT(!aMaybeEntryWriter);
      ++ran;
    });
    MOZ_RELEASE_ASSERT(ran == 1);
    // `PutFrom` won't do anything, and returns the null BlockIndex.
    MOZ_RELEASE_ASSERT(rb.PutFrom(&ran, sizeof(ran)) ==
                       BlocksRingBuffer::BlockIndex{});
    MOZ_RELEASE_ASSERT(rb.PutObject(ran) == BlocksRingBuffer::BlockIndex{});
    // `Read()` functions run the callback with `Nothing`.
    ran = 0;
    rb.Read([&](BlocksRingBuffer::Reader* aReader) {
      MOZ_RELEASE_ASSERT(!aReader);
      ++ran;
    });
    MOZ_RELEASE_ASSERT(ran == 1);
    ran = 0;
    rb.ReadAt(BlocksRingBuffer::BlockIndex{},
              [&](Maybe<BlocksRingBuffer::EntryReader>&& aMaybeEntryReader) {
                MOZ_RELEASE_ASSERT(aMaybeEntryReader.isNothing());
                ++ran;
              });
    MOZ_RELEASE_ASSERT(ran == 1);
    ran = 0;
    rb.ReadAt(bi,
              [&](Maybe<BlocksRingBuffer::EntryReader>&& aMaybeEntryReader) {
                MOZ_RELEASE_ASSERT(aMaybeEntryReader.isNothing());
                ++ran;
              });
    MOZ_RELEASE_ASSERT(ran == 1);
    // `ReadEach` shouldn't run the callback (nothing to read).
    rb.ReadEach([](auto&&) { MOZ_RELEASE_ASSERT(false); });
  };

  // As `testOutOfSession()` attempts to modify the buffer, we run it twice to
  // make sure one run doesn't influence the next one.
  testOutOfSession();
  testOutOfSession();

  rb.ClearBefore(bi);
  testOutOfSession();
  testOutOfSession();

  rb.Clear();
  testOutOfSession();
  testOutOfSession();

  rb.Reset();
  testOutOfSession();
  testOutOfSession();

  constexpr uint32_t MBSize = 32;

  rb.Set(MakePowerOfTwo<BlocksRingBuffer::Length, MBSize>());

  constexpr bool EMPTY = true;
  constexpr bool NOT_EMPTY = false;
  // Test all rb APIs when rb has an underlying buffer.
  auto testInSession = [&](bool aExpectEmpty) {
    MOZ_RELEASE_ASSERT(rb.BufferLength().isSome());
    BlocksRingBuffer::State state = rb.GetState();
    if (aExpectEmpty) {
      MOZ_RELEASE_ASSERT(state.mRangeStart == state.mRangeEnd);
      MOZ_RELEASE_ASSERT(state.mPushedBlockCount == 0);
      MOZ_RELEASE_ASSERT(state.mClearedBlockCount == 0);
    } else {
      MOZ_RELEASE_ASSERT(state.mRangeStart < state.mRangeEnd);
      MOZ_RELEASE_ASSERT(state.mPushedBlockCount > 0);
      MOZ_RELEASE_ASSERT(state.mClearedBlockCount <= state.mPushedBlockCount);
    }
    int32_t ran = 0;
    // The following three `Put...` will write three int32_t of value 1.
    bi = rb.Put(sizeof(ran),
                [&](BlocksRingBuffer::EntryWriter* aMaybeEntryWriter) {
                  MOZ_RELEASE_ASSERT(!!aMaybeEntryWriter);
                  ++ran;
                  aMaybeEntryWriter->WriteObject(ran);
                  return aMaybeEntryWriter->CurrentBlockIndex();
                });
    MOZ_RELEASE_ASSERT(ran == 1);
    MOZ_RELEASE_ASSERT(rb.PutFrom(&ran, sizeof(ran)) !=
                       BlocksRingBuffer::BlockIndex{});
    MOZ_RELEASE_ASSERT(rb.PutObject(ran) != BlocksRingBuffer::BlockIndex{});
    ran = 0;
    rb.Read([&](BlocksRingBuffer::Reader* aReader) {
      MOZ_RELEASE_ASSERT(!!aReader);
      ++ran;
    });
    MOZ_RELEASE_ASSERT(ran == 1);
    ran = 0;
    rb.ReadEach([&](BlocksRingBuffer::EntryReader& aEntryReader) {
      MOZ_RELEASE_ASSERT(aEntryReader.RemainingBytes() == sizeof(ran));
      MOZ_RELEASE_ASSERT(aEntryReader.ReadObject<decltype(ran)>() == 1);
      ++ran;
    });
    MOZ_RELEASE_ASSERT(ran >= 3);
    ran = 0;
    rb.ReadAt(BlocksRingBuffer::BlockIndex{},
              [&](Maybe<BlocksRingBuffer::EntryReader>&& aMaybeEntryReader) {
                MOZ_RELEASE_ASSERT(aMaybeEntryReader.isNothing());
                ++ran;
              });
    MOZ_RELEASE_ASSERT(ran == 1);
    ran = 0;
    rb.ReadAt(bi,
              [&](Maybe<BlocksRingBuffer::EntryReader>&& aMaybeEntryReader) {
                MOZ_RELEASE_ASSERT(aMaybeEntryReader.isNothing() == !bi);
                ++ran;
              });
    MOZ_RELEASE_ASSERT(ran == 1);
  };

  testInSession(EMPTY);
  testInSession(NOT_EMPTY);

  rb.Set(MakePowerOfTwo<BlocksRingBuffer::Length, 32>());
  MOZ_RELEASE_ASSERT(rb.BufferLength().isSome());
  rb.ReadEach([](auto&&) { MOZ_RELEASE_ASSERT(false); });

  testInSession(EMPTY);
  testInSession(NOT_EMPTY);

  rb.Reset();
  testOutOfSession();
  testOutOfSession();

  uint8_t buffer[MBSize * 3];
  for (size_t i = 0; i < MBSize * 3; ++i) {
    buffer[i] = uint8_t('A' + i);
  }

  rb.Set(&buffer[MBSize], MakePowerOfTwo<BlocksRingBuffer::Length, MBSize>());
  MOZ_RELEASE_ASSERT(rb.BufferLength().isSome());
  rb.ReadEach([](auto&&) { MOZ_RELEASE_ASSERT(false); });

  testInSession(EMPTY);
  testInSession(NOT_EMPTY);

  rb.Reset();
  testOutOfSession();
  testOutOfSession();

  int cleared = 0;
  rb.Set(&buffer[MBSize], MakePowerOfTwo<BlocksRingBuffer::Length, MBSize>(),
         [&](auto&&) { ++cleared; });
  MOZ_RELEASE_ASSERT(rb.BufferLength().isSome());
  rb.ReadEach([](auto&&) { MOZ_RELEASE_ASSERT(false); });

  testInSession(EMPTY);
  testInSession(NOT_EMPTY);

  // Remove the current underlying buffer, this should clear all entries.
  rb.Reset();
  // The above should clear all entries (2 tests, three entries each).
  MOZ_RELEASE_ASSERT(cleared == 2 * 3);

  // Check that only the provided stack-based sub-buffer was modified.
  uint32_t changed = 0;
  for (size_t i = MBSize; i < MBSize * 2; ++i) {
    changed += (buffer[i] == uint8_t('A' + i)) ? 0 : 1;
  }
  // Expect at least 75% changes.
  MOZ_RELEASE_ASSERT(changed >= MBSize * 6 / 8);

  // Everything around the sub-buffer should be unchanged.
  for (size_t i = 0; i < MBSize; ++i) {
    MOZ_RELEASE_ASSERT(buffer[i] == uint8_t('A' + i));
  }
  for (size_t i = MBSize * 2; i < MBSize * 3; ++i) {
    MOZ_RELEASE_ASSERT(buffer[i] == uint8_t('A' + i));
  }

  testOutOfSession();
  testOutOfSession();

  printf("TestBlocksRingBufferUnderlyingBufferChanges done\n");
}

void TestBlocksRingBufferThreading() {
  printf("TestBlocksRingBufferThreading...\n");

  // Entry destructor will store about-to-be-cleared value in `lastDestroyed`.
  std::atomic<int> lastDestroyed{0};

  constexpr uint32_t MBSize = 8192;
  uint8_t buffer[MBSize * 3];
  for (size_t i = 0; i < MBSize * 3; ++i) {
    buffer[i] = uint8_t('A' + i);
  }
  BlocksRingBuffer rb(&buffer[MBSize], MakePowerOfTwo32<MBSize>(),
                      [&](BlocksRingBuffer::EntryReader& aReader) {
                        lastDestroyed = aReader.ReadObject<int>();
                      });

  // Start reader thread.
  std::atomic<bool> stopReader{false};
  std::thread reader([&]() {
    for (;;) {
      BlocksRingBuffer::State state = rb.GetState();
      printf(
          "Reader: range=%llu..%llu (%llu bytes) pushed=%llu cleared=%llu "
          "(alive=%llu) lastDestroyed=%d\n",
          static_cast<unsigned long long>(ExtractBlockIndex(state.mRangeStart)),
          static_cast<unsigned long long>(ExtractBlockIndex(state.mRangeEnd)),
          static_cast<unsigned long long>(ExtractBlockIndex(state.mRangeEnd)) -
              static_cast<unsigned long long>(
                  ExtractBlockIndex(state.mRangeStart)),
          static_cast<unsigned long long>(state.mPushedBlockCount),
          static_cast<unsigned long long>(state.mClearedBlockCount),
          static_cast<unsigned long long>(state.mPushedBlockCount -
                                          state.mClearedBlockCount),
          int(lastDestroyed));
      if (stopReader) {
        break;
      }
      ::SleepMilli(1);
    }
  });

  // Start writer threads.
  constexpr int ThreadCount = 32;
  std::thread threads[ThreadCount];
  for (int threadNo = 0; threadNo < ThreadCount; ++threadNo) {
    threads[threadNo] = std::thread(
        [&](int aThreadNo) {
          ::SleepMilli(1);
          constexpr int pushCount = 1024;
          for (int push = 0; push < pushCount; ++push) {
            // Reserve as many bytes as the thread number (but at least enough
            // to store an int), and write an increasing int.
            rb.Put(std::max(aThreadNo, int(sizeof(push))),
                   [&](BlocksRingBuffer::EntryWriter* aEW) {
                     MOZ_RELEASE_ASSERT(!!aEW);
                     aEW->WriteObject(aThreadNo * 1000000 + push);
                     *aEW += aEW->RemainingBytes();
                   });
          }
        },
        threadNo);
  }

  // Wait for all writer threads to die.
  for (auto&& thread : threads) {
    thread.join();
  }

  // Stop reader thread.
  stopReader = true;
  reader.join();

  // Check that only the provided stack-based sub-buffer was modified.
  uint32_t changed = 0;
  for (size_t i = MBSize; i < MBSize * 2; ++i) {
    changed += (buffer[i] == uint8_t('A' + i)) ? 0 : 1;
  }
  // Expect at least 75% changes.
  MOZ_RELEASE_ASSERT(changed >= MBSize * 6 / 8);

  // Everything around the sub-buffer should be unchanged.
  for (size_t i = 0; i < MBSize; ++i) {
    MOZ_RELEASE_ASSERT(buffer[i] == uint8_t('A' + i));
  }
  for (size_t i = MBSize * 2; i < MBSize * 3; ++i) {
    MOZ_RELEASE_ASSERT(buffer[i] == uint8_t('A' + i));
  }

  printf("TestBlocksRingBufferThreading done\n");
}

// Increase the depth, to a maximum (to avoid too-deep recursion).
static constexpr size_t NextDepth(size_t aDepth) {
  constexpr size_t MAX_DEPTH = 128;
  return (aDepth < MAX_DEPTH) ? (aDepth + 1) : aDepth;
}

Atomic<bool, Relaxed, recordreplay::Behavior::DontPreserve> sStopFibonacci;

// Compute fibonacci the hard way (recursively: `f(n)=f(n-1)+f(n-2)`), and
// prevent inlining.
// The template parameter makes each depth be a separate function, to better
// distinguish them in the profiler output.
template <size_t DEPTH = 0>
MOZ_NEVER_INLINE unsigned long long Fibonacci(unsigned long long n) {
  AUTO_BASE_PROFILER_LABEL_DYNAMIC_STRING("fib", OTHER, std::to_string(DEPTH));
  if (n == 0) {
    return 0;
  }
  if (n == 1) {
    return 1;
  }
  if (DEPTH < 5 && sStopFibonacci) {
    return 1'000'000'000;
  }
  TimeStamp start = TimeStamp::NowUnfuzzed();
  static constexpr size_t MAX_MARKER_DEPTH = 10;
  unsigned long long f2 = Fibonacci<NextDepth(DEPTH)>(n - 2);
  if (DEPTH == 0) {
    BASE_PROFILER_ADD_MARKER("Half-way through Fibonacci", OTHER);
  }
  unsigned long long f1 = Fibonacci<NextDepth(DEPTH)>(n - 1);
  if (DEPTH < MAX_MARKER_DEPTH) {
    baseprofiler::profiler_add_text_marker(
        "fib", std::to_string(DEPTH),
        baseprofiler::ProfilingCategoryPair::OTHER, start,
        TimeStamp::NowUnfuzzed());
  }
  return f2 + f1;
}

void TestProfiler() {
  printf("TestProfiler starting -- pid: %d, tid: %d\n",
         baseprofiler::profiler_current_process_id(),
         baseprofiler::profiler_current_thread_id());
  // ::SleepMilli(10000);

  // Test dependencies.
  TestPowerOfTwoMask();
  TestPowerOfTwo();
  TestLEB128();
  TestModuloBuffer();
  TestBlocksRingBufferAPI();
  TestBlocksRingBufferUnderlyingBufferChanges();
  TestBlocksRingBufferThreading();

  {
    printf("profiler_init()...\n");
    AUTO_BASE_PROFILER_INIT;

    MOZ_RELEASE_ASSERT(!baseprofiler::profiler_is_active());
    MOZ_RELEASE_ASSERT(!baseprofiler::profiler_thread_is_being_profiled());
    MOZ_RELEASE_ASSERT(!baseprofiler::profiler_thread_is_sleeping());

    printf("profiler_start()...\n");
    Vector<const char*> filters;
    // Profile all registered threads.
    MOZ_RELEASE_ASSERT(filters.append(""));
    const uint32_t features = baseprofiler::ProfilerFeature::Leaf |
                              baseprofiler::ProfilerFeature::StackWalk |
                              baseprofiler::ProfilerFeature::Threads;
    baseprofiler::profiler_start(baseprofiler::BASE_PROFILER_DEFAULT_ENTRIES,
                                 BASE_PROFILER_DEFAULT_INTERVAL, features,
                                 filters.begin(), filters.length());

    MOZ_RELEASE_ASSERT(baseprofiler::profiler_is_active());
    MOZ_RELEASE_ASSERT(baseprofiler::profiler_thread_is_being_profiled());
    MOZ_RELEASE_ASSERT(!baseprofiler::profiler_thread_is_sleeping());

    sStopFibonacci = false;

    std::thread threadFib([]() {
      AUTO_BASE_PROFILER_REGISTER_THREAD("fibonacci");
      SleepMilli(5);
      AUTO_BASE_PROFILER_TEXT_MARKER_CAUSE("fibonacci", "First leaf call",
                                           OTHER, nullptr);
      static const unsigned long long fibStart = 37;
      printf("Fibonacci(%llu)...\n", fibStart);
      AUTO_BASE_PROFILER_LABEL("Label around Fibonacci", OTHER);
      unsigned long long f = Fibonacci(fibStart);
      printf("Fibonacci(%llu) = %llu\n", fibStart, f);
    });

    std::thread threadCancelFib([]() {
      AUTO_BASE_PROFILER_REGISTER_THREAD("fibonacci canceller");
      SleepMilli(5);
      AUTO_BASE_PROFILER_TEXT_MARKER_CAUSE("fibonacci", "Canceller", OTHER,
                                           nullptr);
      static const int waitMaxSeconds = 10;
      for (int i = 0; i < waitMaxSeconds; ++i) {
        if (sStopFibonacci) {
          AUTO_BASE_PROFILER_LABEL_DYNAMIC_STRING("fibCancel", OTHER,
                                                  std::to_string(i));
          return;
        }
        AUTO_BASE_PROFILER_THREAD_SLEEP;
        SleepMilli(1000);
      }
      AUTO_BASE_PROFILER_LABEL_DYNAMIC_STRING("fibCancel", OTHER,
                                              "Cancelling!");
      sStopFibonacci = true;
    });

    {
      AUTO_BASE_PROFILER_TEXT_MARKER_CAUSE(
          "main thread", "joining fibonacci thread", OTHER, nullptr);
      AUTO_BASE_PROFILER_THREAD_SLEEP;
      threadFib.join();
    }

    {
      AUTO_BASE_PROFILER_TEXT_MARKER_CAUSE(
          "main thread", "joining fibonacci-canceller thread", OTHER, nullptr);
      sStopFibonacci = true;
      AUTO_BASE_PROFILER_THREAD_SLEEP;
      threadCancelFib.join();
    }

    printf("Sleep 1s...\n");
    {
      AUTO_BASE_PROFILER_THREAD_SLEEP;
      SleepMilli(1000);
    }

    Maybe<baseprofiler::ProfilerBufferInfo> info =
        baseprofiler::profiler_get_buffer_info();
    MOZ_RELEASE_ASSERT(info.isSome());
    printf("Profiler buffer range: %llu .. %llu (%llu bytes)\n",
           static_cast<unsigned long long>(info->mRangeStart),
           static_cast<unsigned long long>(info->mRangeEnd),
           // sizeof(ProfileBufferEntry) == 9
           (static_cast<unsigned long long>(info->mRangeEnd) -
            static_cast<unsigned long long>(info->mRangeStart)) *
               9);
    printf("Stats:         min(ns) .. mean(ns) .. max(ns)  [count]\n");
    printf("- Intervals:   %7.1f .. %7.1f  .. %7.1f  [%u]\n",
           info->mIntervalsNs.min,
           info->mIntervalsNs.sum / info->mIntervalsNs.n,
           info->mIntervalsNs.max, info->mIntervalsNs.n);
    printf("- Overheads:   %7.1f .. %7.1f  .. %7.1f  [%u]\n",
           info->mOverheadsNs.min,
           info->mOverheadsNs.sum / info->mOverheadsNs.n,
           info->mOverheadsNs.max, info->mOverheadsNs.n);
    printf("  - Locking:   %7.1f .. %7.1f  .. %7.1f  [%u]\n",
           info->mLockingsNs.min, info->mLockingsNs.sum / info->mLockingsNs.n,
           info->mLockingsNs.max, info->mLockingsNs.n);
    printf("  - Clearning: %7.1f .. %7.1f  .. %7.1f  [%u]\n",
           info->mCleaningsNs.min,
           info->mCleaningsNs.sum / info->mCleaningsNs.n,
           info->mCleaningsNs.max, info->mCleaningsNs.n);
    printf("  - Counters:  %7.1f .. %7.1f  .. %7.1f  [%u]\n",
           info->mCountersNs.min, info->mCountersNs.sum / info->mCountersNs.n,
           info->mCountersNs.max, info->mCountersNs.n);
    printf("  - Threads:   %7.1f .. %7.1f  .. %7.1f  [%u]\n",
           info->mThreadsNs.min, info->mThreadsNs.sum / info->mThreadsNs.n,
           info->mThreadsNs.max, info->mThreadsNs.n);

    printf("baseprofiler_save_profile_to_file()...\n");
    baseprofiler::profiler_save_profile_to_file("TestProfiler_profile.json");

    printf("profiler_stop()...\n");
    baseprofiler::profiler_stop();

    MOZ_RELEASE_ASSERT(!baseprofiler::profiler_is_active());
    MOZ_RELEASE_ASSERT(!baseprofiler::profiler_thread_is_being_profiled());
    MOZ_RELEASE_ASSERT(!baseprofiler::profiler_thread_is_sleeping());

    printf("profiler_shutdown()...\n");
  }

  printf("TestProfiler done\n");
}

#else  // MOZ_BASE_PROFILER

// Testing that macros are still #defined (but do nothing) when
// MOZ_BASE_PROFILER is disabled.
void TestProfiler() {
  // These don't need to make sense, we just want to know that they're defined
  // and don't do anything.
  AUTO_BASE_PROFILER_INIT;

  // This wouldn't build if the macro did output its arguments.
  AUTO_BASE_PROFILER_TEXT_MARKER_CAUSE(catch, catch, catch, catch);

  AUTO_BASE_PROFILER_LABEL(catch, catch);

  AUTO_BASE_PROFILER_THREAD_SLEEP;
}

#endif  // MOZ_BASE_PROFILER else

int main() {
  // Note that there are two `TestProfiler` functions above, depending on
  // whether MOZ_BASE_PROFILER is #defined.
  TestProfiler();

  return 0;
}
