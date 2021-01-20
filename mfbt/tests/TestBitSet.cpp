/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/BitSet.h"

using mozilla::BitSet;

// Work around issue with commas in macro use.
template <size_t N>
using BitSetUint8 = BitSet<N, uint8_t>;
template <size_t N>
using BitSetUint32 = BitSet<N, uint32_t>;

void TestBitSet() {
  MOZ_RELEASE_ASSERT(BitSetUint8<1>().Storage().LengthBytes() == 1);
  MOZ_RELEASE_ASSERT(BitSetUint32<1>().Storage().LengthBytes() == 4);

  MOZ_RELEASE_ASSERT(BitSetUint8<1>().Storage().Length() == 1);
  MOZ_RELEASE_ASSERT(BitSetUint8<8>().Storage().Length() == 1);
  MOZ_RELEASE_ASSERT(BitSetUint8<9>().Storage().Length() == 2);

  MOZ_RELEASE_ASSERT(BitSetUint32<1>().Storage().Length() == 1);
  MOZ_RELEASE_ASSERT(BitSetUint32<32>().Storage().Length() == 1);
  MOZ_RELEASE_ASSERT(BitSetUint32<33>().Storage().Length() == 2);

  BitSetUint8<10> bitset;
  MOZ_RELEASE_ASSERT(!bitset.Test(3));
  MOZ_RELEASE_ASSERT(!bitset[3]);

  bitset[3] = true;
  MOZ_RELEASE_ASSERT(bitset.Test(3));
  MOZ_RELEASE_ASSERT(bitset[3]);

  bitset.ResetAll();
  for (size_t i = 0; i < bitset.Size(); i++) {
    MOZ_RELEASE_ASSERT(!bitset[i]);
  }

  bitset.SetAll();
  for (size_t i = 0; i < bitset.Size(); i++) {
    MOZ_RELEASE_ASSERT(bitset[i]);
  }

  bitset.ResetAll();
  for (size_t i = 0; i < bitset.Size(); i++) {
    MOZ_RELEASE_ASSERT(!bitset[i]);
  }

  // Test trailing unused bits are not set by SetAll().
  bitset.SetAll();
  BitSetUint8<16> bitset2(bitset.Storage());
  MOZ_RELEASE_ASSERT(bitset.Size() < bitset2.Size());
  MOZ_RELEASE_ASSERT(bitset.Storage().Length() == bitset2.Storage().Length());
  for (size_t i = 0; i < bitset.Size(); i++) {
    MOZ_RELEASE_ASSERT(bitset2[i]);
  }
  for (size_t i = bitset.Size(); i < bitset2.Size(); i++) {
    MOZ_RELEASE_ASSERT(!bitset2[i]);
  }
}

int main() {
  TestBitSet();
  return 0;
}
