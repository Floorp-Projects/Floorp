/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/Assertions.h"
#include "mozilla/BitSet.h"

using mozilla::BitSet;

template <typename Storage>
class BitSetSuite {
  template <size_t N>
  using TestBitSet = BitSet<N, Storage>;

  static constexpr size_t kBitsPerWord = sizeof(Storage) * 8;

  static constexpr Storage kAllBitsSet = ~Storage{0};

 public:
  void testLength() {
    MOZ_RELEASE_ASSERT(TestBitSet<1>().Storage().LengthBytes() ==
                       sizeof(Storage));

    MOZ_RELEASE_ASSERT(TestBitSet<1>().Storage().Length() == 1);
    MOZ_RELEASE_ASSERT(TestBitSet<kBitsPerWord>().Storage().Length() == 1);
    MOZ_RELEASE_ASSERT(TestBitSet<kBitsPerWord + 1>().Storage().Length() == 2);
  }

  void testConstruct() {
    MOZ_RELEASE_ASSERT(TestBitSet<1>().Storage()[0] == 0);
    MOZ_RELEASE_ASSERT(TestBitSet<kBitsPerWord>().Storage()[0] == 0);
    MOZ_RELEASE_ASSERT(TestBitSet<kBitsPerWord + 1>().Storage()[0] == 0);
    MOZ_RELEASE_ASSERT(TestBitSet<kBitsPerWord + 1>().Storage()[1] == 0);

    TestBitSet<1> bitset1;
    bitset1.SetAll();
    TestBitSet<kBitsPerWord> bitsetW;
    bitsetW.SetAll();
    TestBitSet<kBitsPerWord + 1> bitsetW1;
    bitsetW1.SetAll();

    MOZ_RELEASE_ASSERT(bitset1.Storage()[0] == 1);
    MOZ_RELEASE_ASSERT(bitsetW.Storage()[0] == kAllBitsSet);
    MOZ_RELEASE_ASSERT(bitsetW1.Storage()[0] == kAllBitsSet);
    MOZ_RELEASE_ASSERT(bitsetW1.Storage()[1] == 1);

    MOZ_RELEASE_ASSERT(TestBitSet<1>(bitset1).Storage()[0] == 1);
    MOZ_RELEASE_ASSERT(TestBitSet<kBitsPerWord>(bitsetW).Storage()[0] ==
                       kAllBitsSet);
    MOZ_RELEASE_ASSERT(TestBitSet<kBitsPerWord + 1>(bitsetW1).Storage()[0] ==
                       kAllBitsSet);
    MOZ_RELEASE_ASSERT(TestBitSet<kBitsPerWord + 1>(bitsetW1).Storage()[1] ==
                       1);

    MOZ_RELEASE_ASSERT(TestBitSet<1>(bitset1.Storage()).Storage()[0] == 1);
    MOZ_RELEASE_ASSERT(
        TestBitSet<kBitsPerWord>(bitsetW.Storage()).Storage()[0] ==
        kAllBitsSet);
    MOZ_RELEASE_ASSERT(
        TestBitSet<kBitsPerWord + 1>(bitsetW1.Storage()).Storage()[0] ==
        kAllBitsSet);
    MOZ_RELEASE_ASSERT(
        TestBitSet<kBitsPerWord + 1>(bitsetW1.Storage()).Storage()[1] == 1);
  }

  void testSetBit() {
    TestBitSet<kBitsPerWord + 2> bitset;
    MOZ_RELEASE_ASSERT(!bitset.Test(3));
    MOZ_RELEASE_ASSERT(!bitset[3]);
    MOZ_RELEASE_ASSERT(!bitset.Test(kBitsPerWord + 1));
    MOZ_RELEASE_ASSERT(!bitset[kBitsPerWord + 1]);

    bitset[3] = true;
    MOZ_RELEASE_ASSERT(bitset.Test(3));
    MOZ_RELEASE_ASSERT(bitset[3]);

    bitset[kBitsPerWord + 1] = true;
    MOZ_RELEASE_ASSERT(bitset.Test(3));
    MOZ_RELEASE_ASSERT(bitset[3]);
    MOZ_RELEASE_ASSERT(bitset.Test(kBitsPerWord + 1));
    MOZ_RELEASE_ASSERT(bitset[kBitsPerWord + 1]);

    bitset.ResetAll();
    for (size_t i = 0; i < decltype(bitset)::Size(); i++) {
      MOZ_RELEASE_ASSERT(!bitset[i]);
    }

    bitset.SetAll();
    for (size_t i = 0; i < decltype(bitset)::Size(); i++) {
      MOZ_RELEASE_ASSERT(bitset[i]);
    }

    // Test trailing unused bits are not set by SetAll().
    MOZ_RELEASE_ASSERT(bitset.Storage()[1] == 3);

    bitset.ResetAll();
    for (size_t i = 0; i < decltype(bitset)::Size(); i++) {
      MOZ_RELEASE_ASSERT(!bitset[i]);
    }
  }

  void runTests() {
    testLength();
    testConstruct();
    testSetBit();
  }
};

int main() {
  BitSetSuite<uint8_t>().runTests();
  BitSetSuite<uint32_t>().runTests();
  BitSetSuite<uint64_t>().runTests();

  return 0;
}
