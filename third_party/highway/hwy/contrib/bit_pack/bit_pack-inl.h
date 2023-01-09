// Copyright 2022 Google LLC
// SPDX-License-Identifier: Apache-2.0
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//      http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

// Per-target include guard
#if defined(HIGHWAY_HWY_CONTRIB_BIT_PACK_INL_H_) == \
    defined(HWY_TARGET_TOGGLE)
#ifdef HIGHWAY_HWY_CONTRIB_BIT_PACK_INL_H_
#undef HIGHWAY_HWY_CONTRIB_BIT_PACK_INL_H_
#else
#define HIGHWAY_HWY_CONTRIB_BIT_PACK_INL_H_
#endif

#include "hwy/highway.h"

HWY_BEFORE_NAMESPACE();
namespace hwy {
namespace HWY_NAMESPACE {

namespace detail {

// Primary template, specialized below for each number of bits
template <size_t kBits>  // <= 8
struct Pack8 {};

template <>
struct Pack8<1> {
  static constexpr size_t kBits = 1;
  static constexpr size_t kRawVectors = 8;
  static constexpr size_t kPackedVectors = 1;

  template <class D8>
  HWY_INLINE void Pack(D8 d8, const uint8_t* HWY_RESTRICT raw,
                       uint8_t* HWY_RESTRICT packed_out) {
    const RepartitionToWide<decltype(d8)> d16;
    using VU16 = Vec<decltype(d16)>;
    const size_t N8 = Lanes(d8);
    // 16-bit shifts avoid masking (bits will not cross 8-bit lanes).
    const VU16 raw0 = BitCast(d16, LoadU(d8, raw + 0 * N8));
    const VU16 raw1 = BitCast(d16, LoadU(d8, raw + 1 * N8));
    const VU16 raw2 = BitCast(d16, LoadU(d8, raw + 2 * N8));
    const VU16 raw3 = BitCast(d16, LoadU(d8, raw + 3 * N8));
    const VU16 raw4 = BitCast(d16, LoadU(d8, raw + 4 * N8));
    const VU16 raw5 = BitCast(d16, LoadU(d8, raw + 5 * N8));
    const VU16 raw6 = BitCast(d16, LoadU(d8, raw + 6 * N8));
    const VU16 raw7 = BitCast(d16, LoadU(d8, raw + 7 * N8));

    const VU16 packed76 = Or(ShiftLeft<7>(raw7), ShiftLeft<6>(raw6));
    const VU16 packed54 = Or(ShiftLeft<5>(raw5), ShiftLeft<4>(raw4));
    const VU16 packed32 = Or(ShiftLeft<3>(raw3), ShiftLeft<2>(raw2));
    const VU16 packed10 = Or(ShiftLeft<1>(raw1), raw0);
    const VU16 packed = Or3(Or(packed76, packed54), packed32, packed10);
    StoreU(BitCast(d8, packed), d8, packed_out);
  }

  template <class D8>
  HWY_INLINE void Unpack(D8 d8, const uint8_t* HWY_RESTRICT packed_in,
                         uint8_t* HWY_RESTRICT raw) {
    const RepartitionToWide<decltype(d8)> d16;
    using VU16 = Vec<decltype(d16)>;
    const size_t N8 = Lanes(d8);
    // We extract the lowest bit from each byte, then shift right.
    const VU16 mask = Set(d16, 0x0101u);

    VU16 packed = BitCast(d16, LoadU(d8, packed_in));

    const VU16 raw0 = And(packed, mask);
    packed = ShiftRight<1>(packed);
    StoreU(BitCast(d8, raw0), d8, raw + 0 * N8);

    const VU16 raw1 = And(packed, mask);
    packed = ShiftRight<1>(packed);
    StoreU(BitCast(d8, raw1), d8, raw + 1 * N8);

    const VU16 raw2 = And(packed, mask);
    packed = ShiftRight<1>(packed);
    StoreU(BitCast(d8, raw2), d8, raw + 2 * N8);

    const VU16 raw3 = And(packed, mask);
    packed = ShiftRight<1>(packed);
    StoreU(BitCast(d8, raw3), d8, raw + 3 * N8);

    const VU16 raw4 = And(packed, mask);
    packed = ShiftRight<1>(packed);
    StoreU(BitCast(d8, raw4), d8, raw + 4 * N8);

    const VU16 raw5 = And(packed, mask);
    packed = ShiftRight<1>(packed);
    StoreU(BitCast(d8, raw5), d8, raw + 5 * N8);

    const VU16 raw6 = And(packed, mask);
    packed = ShiftRight<1>(packed);
    StoreU(BitCast(d8, raw6), d8, raw + 6 * N8);

    const VU16 raw7 = And(packed, mask);
    StoreU(BitCast(d8, raw7), d8, raw + 7 * N8);
  }
};  // Pack8<1>

template <>
struct Pack8<2> {
  static constexpr size_t kBits = 2;
  static constexpr size_t kRawVectors = 4;
  static constexpr size_t kPackedVectors = 1;

  template <class D8>
  HWY_INLINE void Pack(D8 d8, const uint8_t* HWY_RESTRICT raw,
                       uint8_t* HWY_RESTRICT packed_out) {
    const RepartitionToWide<decltype(d8)> d16;
    using VU16 = Vec<decltype(d16)>;
    const size_t N8 = Lanes(d8);
    // 16-bit shifts avoid masking (bits will not cross 8-bit lanes).
    const VU16 raw0 = BitCast(d16, LoadU(d8, raw + 0 * N8));
    const VU16 raw1 = BitCast(d16, LoadU(d8, raw + 1 * N8));
    const VU16 raw2 = BitCast(d16, LoadU(d8, raw + 2 * N8));
    const VU16 raw3 = BitCast(d16, LoadU(d8, raw + 3 * N8));

    const VU16 packed32 = Or(ShiftLeft<6>(raw3), ShiftLeft<4>(raw2));
    const VU16 packed10 = Or(ShiftLeft<2>(raw1), raw0);
    const VU16 packed = Or(packed32, packed10);
    StoreU(BitCast(d8, packed), d8, packed_out);
  }

  template <class D8>
  HWY_INLINE void Unpack(D8 d8, const uint8_t* HWY_RESTRICT packed_in,
                         uint8_t* HWY_RESTRICT raw) {
    const RepartitionToWide<decltype(d8)> d16;
    using VU16 = Vec<decltype(d16)>;
    const size_t N8 = Lanes(d8);
    // We extract the lowest two bits from each byte, then shift right.
    const VU16 mask = Set(d16, 0x0303u);

    VU16 packed = BitCast(d16, LoadU(d8, packed_in));

    const VU16 raw0 = And(packed, mask);
    packed = ShiftRight<2>(packed);
    StoreU(BitCast(d8, raw0), d8, raw + 0 * N8);

    const VU16 raw1 = And(packed, mask);
    packed = ShiftRight<2>(packed);
    StoreU(BitCast(d8, raw1), d8, raw + 1 * N8);

    const VU16 raw2 = And(packed, mask);
    packed = ShiftRight<2>(packed);
    StoreU(BitCast(d8, raw2), d8, raw + 2 * N8);

    const VU16 raw3 = And(packed, mask);
    StoreU(BitCast(d8, raw3), d8, raw + 3 * N8);
  }
};  // Pack8<2>

template <>
struct Pack8<3> {
  static constexpr size_t kBits = 3;
  static constexpr size_t kRawVectors = 8;
  static constexpr size_t kPackedVectors = 3;

  template <class D8>
  HWY_INLINE void Pack(D8 d8, const uint8_t* HWY_RESTRICT raw,
                       uint8_t* HWY_RESTRICT packed_out) {
    const RepartitionToWide<decltype(d8)> d16;
    using VU16 = Vec<decltype(d16)>;
    const size_t N8 = Lanes(d8);
    const VU16 raw0 = BitCast(d16, LoadU(d8, raw + 0 * N8));
    const VU16 raw1 = BitCast(d16, LoadU(d8, raw + 1 * N8));
    const VU16 raw2 = BitCast(d16, LoadU(d8, raw + 2 * N8));
    const VU16 raw3 = BitCast(d16, LoadU(d8, raw + 3 * N8));
    const VU16 raw4 = BitCast(d16, LoadU(d8, raw + 4 * N8));
    const VU16 raw5 = BitCast(d16, LoadU(d8, raw + 5 * N8));
    const VU16 raw6 = BitCast(d16, LoadU(d8, raw + 6 * N8));
    const VU16 raw7 = BitCast(d16, LoadU(d8, raw + 7 * N8));

    // The upper two bits of these three will be filled with packed3 (6 bits).
    VU16 packed0 = Or(ShiftLeft<3>(raw4), raw0);
    VU16 packed1 = Or(ShiftLeft<3>(raw5), raw1);
    VU16 packed2 = Or(ShiftLeft<3>(raw6), raw2);
    const VU16 packed3 = Or(ShiftLeft<3>(raw7), raw3);

    const VU16 hi2 = Set(d16, 0xC0C0u);
    packed0 = OrAnd(packed0, ShiftLeft<2>(packed3), hi2);
    packed1 = OrAnd(packed1, ShiftLeft<4>(packed3), hi2);
    packed2 = OrAnd(packed2, ShiftLeft<6>(packed3), hi2);
    StoreU(BitCast(d8, packed0), d8, packed_out + 0 * N8);
    StoreU(BitCast(d8, packed1), d8, packed_out + 1 * N8);
    StoreU(BitCast(d8, packed2), d8, packed_out + 2 * N8);
  }

  template <class D8>
  HWY_INLINE void Unpack(D8 d8, const uint8_t* HWY_RESTRICT packed_in,
                         uint8_t* HWY_RESTRICT raw) {
    const RepartitionToWide<decltype(d8)> d16;
    using VU16 = Vec<decltype(d16)>;
    const size_t N8 = Lanes(d8);
    // We extract the lowest three bits from each byte.
    const VU16 mask = Set(d16, 0x0707u);

    VU16 packed0 = BitCast(d16, LoadU(d8, packed_in + 0 * N8));
    VU16 packed1 = BitCast(d16, LoadU(d8, packed_in + 1 * N8));
    VU16 packed2 = BitCast(d16, LoadU(d8, packed_in + 2 * N8));

    const VU16 raw0 = And(mask, packed0);
    packed0 = ShiftRight<3>(packed0);
    StoreU(BitCast(d8, raw0), d8, raw + 0 * N8);

    const VU16 raw1 = And(mask, packed1);
    packed1 = ShiftRight<3>(packed1);
    StoreU(BitCast(d8, raw1), d8, raw + 1 * N8);

    const VU16 raw2 = And(mask, packed2);
    packed2 = ShiftRight<3>(packed2);
    StoreU(BitCast(d8, raw2), d8, raw + 2 * N8);

    const VU16 raw4 = And(mask, packed0);
    packed0 = ShiftRight<3>(packed0);
    StoreU(BitCast(d8, raw4), d8, raw + 4 * N8);

    const VU16 raw5 = And(mask, packed1);
    packed1 = ShiftRight<3>(packed1);
    StoreU(BitCast(d8, raw5), d8, raw + 5 * N8);

    const VU16 raw6 = And(mask, packed2);
    packed2 = ShiftRight<3>(packed2);
    StoreU(BitCast(d8, raw6), d8, raw + 6 * N8);

    // packed3 is the concatenation of the low two bits in packed0..2.
    const VU16 mask2 = Set(d16, 0x0303u);
    VU16 packed3 = And(mask2, packed0);  // high bits in lower two bits
    packed3 = ShiftLeft<2>(packed3);
    packed3 = OrAnd(packed3, mask2, packed1);  // insert mid 2 bits
    packed3 = ShiftLeft<2>(packed3);
    packed3 = OrAnd(packed3, mask2, packed2);  // insert low 2 bits

    const VU16 raw3 = And(mask, packed3);
    packed3 = ShiftRight<3>(packed3);
    StoreU(BitCast(d8, raw3), d8, raw + 3 * N8);

    const VU16 raw7 = And(mask, packed3);
    StoreU(BitCast(d8, raw7), d8, raw + 7 * N8);
  }
};  // Pack8<3>

template <>
struct Pack8<4> {
  static constexpr size_t kBits = 4;
  // 2x unrolled (matches size of 2/6 bit cases) for increased efficiency.
  static constexpr size_t kRawVectors = 4;
  static constexpr size_t kPackedVectors = 2;

  template <class D8>
  HWY_INLINE void Pack(D8 d8, const uint8_t* HWY_RESTRICT raw,
                       uint8_t* HWY_RESTRICT packed_out) {
    const RepartitionToWide<decltype(d8)> d16;
    using VU16 = Vec<decltype(d16)>;
    const size_t N8 = Lanes(d8);
    // 16-bit shifts avoid masking (bits will not cross 8-bit lanes).
    const VU16 raw0 = BitCast(d16, LoadU(d8, raw + 0 * N8));
    const VU16 raw1 = BitCast(d16, LoadU(d8, raw + 1 * N8));
    const VU16 raw2 = BitCast(d16, LoadU(d8, raw + 2 * N8));
    const VU16 raw3 = BitCast(d16, LoadU(d8, raw + 3 * N8));

    const VU16 packed0 = Or(ShiftLeft<4>(raw1), raw0);
    const VU16 packed1 = Or(ShiftLeft<4>(raw3), raw2);
    StoreU(BitCast(d8, packed0), d8, packed_out + 0 * N8);
    StoreU(BitCast(d8, packed1), d8, packed_out + 1 * N8);
  }

  template <class D8>
  HWY_INLINE void Unpack(D8 d8, const uint8_t* HWY_RESTRICT packed_in,
                         uint8_t* HWY_RESTRICT raw) {
    const RepartitionToWide<decltype(d8)> d16;
    using VU16 = Vec<decltype(d16)>;
    const size_t N8 = Lanes(d8);
    // We extract the lowest four bits from each byte, then shift right.
    const VU16 mask = Set(d16, 0x0F0Fu);

    VU16 packed0 = BitCast(d16, LoadU(d8, packed_in + 0 * N8));
    VU16 packed1 = BitCast(d16, LoadU(d8, packed_in + 1 * N8));

    const VU16 raw0 = And(packed0, mask);
    packed0 = ShiftRight<4>(packed0);
    StoreU(BitCast(d8, raw0), d8, raw + 0 * N8);

    const VU16 raw1 = And(packed0, mask);
    StoreU(BitCast(d8, raw1), d8, raw + 1 * N8);

    const VU16 raw2 = And(packed1, mask);
    packed1 = ShiftRight<4>(packed1);
    StoreU(BitCast(d8, raw2), d8, raw + 2 * N8);

    const VU16 raw3 = And(packed1, mask);
    StoreU(BitCast(d8, raw3), d8, raw + 3 * N8);
  }
};  // Pack8<4>

template <>
struct Pack8<5> {
  static constexpr size_t kBits = 5;
  static constexpr size_t kRawVectors = 8;
  static constexpr size_t kPackedVectors = 5;

  template <class D8>
  HWY_INLINE void Pack(D8 d8, const uint8_t* HWY_RESTRICT raw,
                       uint8_t* HWY_RESTRICT packed_out) {
    const RepartitionToWide<decltype(d8)> d16;
    using VU16 = Vec<decltype(d16)>;
    const size_t N8 = Lanes(d8);
    const VU16 raw0 = BitCast(d16, LoadU(d8, raw + 0 * N8));
    const VU16 raw1 = BitCast(d16, LoadU(d8, raw + 1 * N8));
    const VU16 raw2 = BitCast(d16, LoadU(d8, raw + 2 * N8));
    const VU16 raw3 = BitCast(d16, LoadU(d8, raw + 3 * N8));
    const VU16 raw4 = BitCast(d16, LoadU(d8, raw + 4 * N8));
    const VU16 raw5 = BitCast(d16, LoadU(d8, raw + 5 * N8));
    const VU16 raw6 = BitCast(d16, LoadU(d8, raw + 6 * N8));
    const VU16 raw7 = BitCast(d16, LoadU(d8, raw + 7 * N8));

    // Fill upper three bits with upper bits from raw4..7.
    const VU16 hi3 = Set(d16, 0xE0E0u);
    const VU16 packed0 = OrAnd(raw0, ShiftLeft<3>(raw4), hi3);
    const VU16 packed1 = OrAnd(raw1, ShiftLeft<3>(raw5), hi3);
    const VU16 packed2 = OrAnd(raw2, ShiftLeft<3>(raw6), hi3);
    const VU16 packed3 = OrAnd(raw3, ShiftLeft<3>(raw7), hi3);
    StoreU(BitCast(d8, packed0), d8, packed_out + 0 * N8);
    StoreU(BitCast(d8, packed1), d8, packed_out + 1 * N8);
    StoreU(BitCast(d8, packed2), d8, packed_out + 2 * N8);
    StoreU(BitCast(d8, packed3), d8, packed_out + 3 * N8);

    // Combine lower two bits of raw4..7 into packed4.
    const VU16 lo2 = Set(d16, 0x0303u);
    VU16 packed4 = And(lo2, raw7);
    packed4 = ShiftLeft<2>(packed4);
    packed4 = OrAnd(packed4, lo2, raw6);
    packed4 = ShiftLeft<2>(packed4);
    packed4 = OrAnd(packed4, lo2, raw5);
    packed4 = ShiftLeft<2>(packed4);
    packed4 = OrAnd(packed4, lo2, raw4);
    StoreU(BitCast(d8, packed4), d8, packed_out + 4 * N8);
  }

  template <class D8>
  HWY_INLINE void Unpack(D8 d8, const uint8_t* HWY_RESTRICT packed_in,
                         uint8_t* HWY_RESTRICT raw) {
    const RepartitionToWide<decltype(d8)> d16;
    using VU16 = Vec<decltype(d16)>;
    const size_t N8 = Lanes(d8);

    const VU16 packed0 = BitCast(d16, LoadU(d8, packed_in + 0 * N8));
    const VU16 packed1 = BitCast(d16, LoadU(d8, packed_in + 1 * N8));
    const VU16 packed2 = BitCast(d16, LoadU(d8, packed_in + 2 * N8));
    const VU16 packed3 = BitCast(d16, LoadU(d8, packed_in + 3 * N8));
    VU16 packed4 = BitCast(d16, LoadU(d8, packed_in + 4 * N8));

    // We extract the lowest five bits from each byte.
    const VU16 hi3 = Set(d16, 0xE0E0u);

    const VU16 raw0 = AndNot(hi3, packed0);
    StoreU(BitCast(d8, raw0), d8, raw + 0 * N8);

    const VU16 raw1 = AndNot(hi3, packed1);
    StoreU(BitCast(d8, raw1), d8, raw + 1 * N8);

    const VU16 raw2 = AndNot(hi3, packed2);
    StoreU(BitCast(d8, raw2), d8, raw + 2 * N8);

    const VU16 raw3 = AndNot(hi3, packed3);
    StoreU(BitCast(d8, raw3), d8, raw + 3 * N8);

    // The upper bits are the top 3 bits shifted right by three.
    const VU16 top4 = ShiftRight<3>(And(hi3, packed0));
    const VU16 top5 = ShiftRight<3>(And(hi3, packed1));
    const VU16 top6 = ShiftRight<3>(And(hi3, packed2));
    const VU16 top7 = ShiftRight<3>(And(hi3, packed3));

    // Insert the lower 2 bits, which were concatenated into a byte.
    const VU16 lo2 = Set(d16, 0x0303u);
    const VU16 raw4 = OrAnd(top4, lo2, packed4);
    packed4 = ShiftRight<2>(packed4);
    const VU16 raw5 = OrAnd(top5, lo2, packed4);
    packed4 = ShiftRight<2>(packed4);
    const VU16 raw6 = OrAnd(top6, lo2, packed4);
    packed4 = ShiftRight<2>(packed4);
    const VU16 raw7 = OrAnd(top7, lo2, packed4);

    StoreU(BitCast(d8, raw4), d8, raw + 4 * N8);
    StoreU(BitCast(d8, raw5), d8, raw + 5 * N8);
    StoreU(BitCast(d8, raw6), d8, raw + 6 * N8);
    StoreU(BitCast(d8, raw7), d8, raw + 7 * N8);
  }
};  // Pack8<5>

template <>
struct Pack8<6> {
  static constexpr size_t kBits = 6;
  static constexpr size_t kRawVectors = 4;
  static constexpr size_t kPackedVectors = 3;

  template <class D8>
  HWY_INLINE void Pack(D8 d8, const uint8_t* HWY_RESTRICT raw,
                       uint8_t* HWY_RESTRICT packed_out) {
    const RepartitionToWide<decltype(d8)> d16;
    using VU16 = Vec<decltype(d16)>;
    const size_t N8 = Lanes(d8);
    const VU16 raw0 = BitCast(d16, LoadU(d8, raw + 0 * N8));
    const VU16 raw1 = BitCast(d16, LoadU(d8, raw + 1 * N8));
    const VU16 raw2 = BitCast(d16, LoadU(d8, raw + 2 * N8));
    const VU16 raw3 = BitCast(d16, LoadU(d8, raw + 3 * N8));

    // The upper two bits of these three will be filled with raw3 (6 bits).
    VU16 packed0 = raw0;
    VU16 packed1 = raw1;
    VU16 packed2 = raw2;

    const VU16 hi2 = Set(d16, 0xC0C0u);
    packed0 = OrAnd(packed0, ShiftLeft<2>(raw3), hi2);
    packed1 = OrAnd(packed1, ShiftLeft<4>(raw3), hi2);
    packed2 = OrAnd(packed2, ShiftLeft<6>(raw3), hi2);
    StoreU(BitCast(d8, packed0), d8, packed_out + 0 * N8);
    StoreU(BitCast(d8, packed1), d8, packed_out + 1 * N8);
    StoreU(BitCast(d8, packed2), d8, packed_out + 2 * N8);
  }

  template <class D8>
  HWY_INLINE void Unpack(D8 d8, const uint8_t* HWY_RESTRICT packed_in,
                         uint8_t* HWY_RESTRICT raw) {
    const RepartitionToWide<decltype(d8)> d16;
    using VU16 = Vec<decltype(d16)>;
    const size_t N8 = Lanes(d8);
    // We extract the lowest six bits from each byte. Negated mask so we can
    // use OrAnd below.
    const VU16 mask = Set(d16, 0xC0C0u);

    const VU16 packed0 = BitCast(d16, LoadU(d8, packed_in + 0 * N8));
    const VU16 packed1 = BitCast(d16, LoadU(d8, packed_in + 1 * N8));
    const VU16 packed2 = BitCast(d16, LoadU(d8, packed_in + 2 * N8));

    const VU16 raw0 = AndNot(mask, packed0);
    StoreU(BitCast(d8, raw0), d8, raw + 0 * N8);

    const VU16 raw1 = AndNot(mask, packed1);
    StoreU(BitCast(d8, raw1), d8, raw + 1 * N8);

    const VU16 raw2 = AndNot(mask, packed2);
    StoreU(BitCast(d8, raw2), d8, raw + 2 * N8);

    // raw3 is the concatenation of the upper two bits in packed0..2.
    VU16 raw3 = And(mask, packed2);  // low 2 bits in top of byte
    raw3 = ShiftRight<2>(raw3);
    raw3 = OrAnd(raw3, mask, packed1);  // insert mid 2 bits
    raw3 = ShiftRight<2>(raw3);
    raw3 = OrAnd(raw3, mask, packed0);  // insert high 2 bits
    raw3 = ShiftRight<2>(raw3);
    StoreU(BitCast(d8, raw3), d8, raw + 3 * N8);
  }
};  // Pack8<6>

template <>
struct Pack8<7> {
  static constexpr size_t kBits = 7;
  static constexpr size_t kRawVectors = 8;
  static constexpr size_t kPackedVectors = 7;

  template <class D8>
  HWY_INLINE void Pack(D8 d8, const uint8_t* HWY_RESTRICT raw,
                       uint8_t* HWY_RESTRICT packed_out) {
    const RepartitionToWide<decltype(d8)> d16;
    using VU16 = Vec<decltype(d16)>;
    const size_t N8 = Lanes(d8);
    const VU16 raw0 = BitCast(d16, LoadU(d8, raw + 0 * N8));
    const VU16 raw1 = BitCast(d16, LoadU(d8, raw + 1 * N8));
    const VU16 raw2 = BitCast(d16, LoadU(d8, raw + 2 * N8));
    const VU16 raw3 = BitCast(d16, LoadU(d8, raw + 3 * N8));
    const VU16 raw4 = BitCast(d16, LoadU(d8, raw + 4 * N8));
    const VU16 raw5 = BitCast(d16, LoadU(d8, raw + 5 * N8));
    const VU16 raw6 = BitCast(d16, LoadU(d8, raw + 6 * N8));
    // Top bit is inserted into packed0..6 and then shifted left.
    VU16 raw7 = BitCast(d16, LoadU(d8, raw + 7 * N8));

    const VU16 hi1 = Set(d16, 0x8080u);
    raw7 = Add(raw7, raw7);
    const VU16 packed0 = OrAnd(raw0, hi1, raw7);
    raw7 = Add(raw7, raw7);
    const VU16 packed1 = OrAnd(raw1, hi1, raw7);
    raw7 = Add(raw7, raw7);
    const VU16 packed2 = OrAnd(raw2, hi1, raw7);
    raw7 = Add(raw7, raw7);
    const VU16 packed3 = OrAnd(raw3, hi1, raw7);
    raw7 = Add(raw7, raw7);
    const VU16 packed4 = OrAnd(raw4, hi1, raw7);
    raw7 = Add(raw7, raw7);
    const VU16 packed5 = OrAnd(raw5, hi1, raw7);
    raw7 = Add(raw7, raw7);
    const VU16 packed6 = OrAnd(raw6, hi1, raw7);
    StoreU(BitCast(d8, packed0), d8, packed_out + 0 * N8);
    StoreU(BitCast(d8, packed1), d8, packed_out + 1 * N8);
    StoreU(BitCast(d8, packed2), d8, packed_out + 2 * N8);
    StoreU(BitCast(d8, packed3), d8, packed_out + 3 * N8);
    StoreU(BitCast(d8, packed4), d8, packed_out + 4 * N8);
    StoreU(BitCast(d8, packed5), d8, packed_out + 5 * N8);
    StoreU(BitCast(d8, packed6), d8, packed_out + 6 * N8);
  }

  template <class D8>
  HWY_INLINE void Unpack(D8 d8, const uint8_t* HWY_RESTRICT packed_in,
                         uint8_t* HWY_RESTRICT raw) {
    const RepartitionToWide<decltype(d8)> d16;
    using VU16 = Vec<decltype(d16)>;
    const size_t N8 = Lanes(d8);

    const VU16 packed0 = BitCast(d16, LoadU(d8, packed_in + 0 * N8));
    const VU16 packed1 = BitCast(d16, LoadU(d8, packed_in + 1 * N8));
    const VU16 packed2 = BitCast(d16, LoadU(d8, packed_in + 2 * N8));
    const VU16 packed3 = BitCast(d16, LoadU(d8, packed_in + 3 * N8));
    const VU16 packed4 = BitCast(d16, LoadU(d8, packed_in + 4 * N8));
    const VU16 packed5 = BitCast(d16, LoadU(d8, packed_in + 5 * N8));
    const VU16 packed6 = BitCast(d16, LoadU(d8, packed_in + 6 * N8));

    // We extract the lowest seven bits from each byte.
    const VU16 hi1 = Set(d16, 0x8080u);

    const VU16 raw0 = AndNot(hi1, packed0);
    StoreU(BitCast(d8, raw0), d8, raw + 0 * N8);

    const VU16 raw1 = AndNot(hi1, packed1);
    StoreU(BitCast(d8, raw1), d8, raw + 1 * N8);

    const VU16 raw2 = AndNot(hi1, packed2);
    StoreU(BitCast(d8, raw2), d8, raw + 2 * N8);

    const VU16 raw3 = AndNot(hi1, packed3);
    StoreU(BitCast(d8, raw3), d8, raw + 3 * N8);

    const VU16 raw4 = AndNot(hi1, packed4);
    StoreU(BitCast(d8, raw4), d8, raw + 4 * N8);

    const VU16 raw5 = AndNot(hi1, packed5);
    StoreU(BitCast(d8, raw5), d8, raw + 5 * N8);

    const VU16 raw6 = AndNot(hi1, packed6);
    StoreU(BitCast(d8, raw6), d8, raw + 6 * N8);

    VU16 raw7 = And(hi1, packed6);  // will shift this down into LSB
    raw7 = ShiftRight<1>(raw7);
    raw7 = OrAnd(raw7, hi1, packed5);
    raw7 = ShiftRight<1>(raw7);
    raw7 = OrAnd(raw7, hi1, packed4);
    raw7 = ShiftRight<1>(raw7);
    raw7 = OrAnd(raw7, hi1, packed3);
    raw7 = ShiftRight<1>(raw7);
    raw7 = OrAnd(raw7, hi1, packed2);
    raw7 = ShiftRight<1>(raw7);
    raw7 = OrAnd(raw7, hi1, packed1);
    raw7 = ShiftRight<1>(raw7);
    raw7 = OrAnd(raw7, hi1, packed0);
    raw7 = ShiftRight<1>(raw7);
    StoreU(BitCast(d8, raw7), d8, raw + 7 * N8);
  }
};  // Pack8<7>

template <>
struct Pack8<8> {
  static constexpr size_t kBits = 8;
  // 4x unrolled (matches size of 2/6 bit cases) for increased efficiency.
  static constexpr size_t kRawVectors = 4;
  static constexpr size_t kPackedVectors = 4;

  template <class D8>
  HWY_INLINE void Pack(D8 d8, const uint8_t* HWY_RESTRICT raw,
                       uint8_t* HWY_RESTRICT packed_out) {
    using VU8 = Vec<decltype(d8)>;
    const size_t N8 = Lanes(d8);
    const VU8 raw0 = LoadU(d8, raw + 0 * N8);
    const VU8 raw1 = LoadU(d8, raw + 1 * N8);
    const VU8 raw2 = LoadU(d8, raw + 2 * N8);
    const VU8 raw3 = LoadU(d8, raw + 3 * N8);

    StoreU(raw0, d8, packed_out + 0 * N8);
    StoreU(raw1, d8, packed_out + 1 * N8);
    StoreU(raw2, d8, packed_out + 2 * N8);
    StoreU(raw3, d8, packed_out + 3 * N8);
  }

  template <class D8>
  HWY_INLINE void Unpack(D8 d8, const uint8_t* HWY_RESTRICT packed_in,
                         uint8_t* HWY_RESTRICT raw) {
    using VU8 = Vec<decltype(d8)>;
    const size_t N8 = Lanes(d8);
    const VU8 raw0 = LoadU(d8, packed_in + 0 * N8);
    const VU8 raw1 = LoadU(d8, packed_in + 1 * N8);
    const VU8 raw2 = LoadU(d8, packed_in + 2 * N8);
    const VU8 raw3 = LoadU(d8, packed_in + 3 * N8);

    StoreU(raw0, d8, raw + 0 * N8);
    StoreU(raw1, d8, raw + 1 * N8);
    StoreU(raw2, d8, raw + 2 * N8);
    StoreU(raw3, d8, raw + 3 * N8);
  }
};  // Pack8<8>

// Primary template, specialized below for each number of bits
template <size_t kBits>  // <= 16
struct Pack16 {};

template <>
struct Pack16<1> {
  static constexpr size_t kBits = 1;
  static constexpr size_t kRawVectors = 16;
  static constexpr size_t kPackedVectors = 1;

  template <class D>
  HWY_INLINE void Pack(D d, const uint16_t* HWY_RESTRICT raw,
                       uint16_t* HWY_RESTRICT packed_out) {
    using VU16 = Vec<decltype(d)>;
    const size_t N = Lanes(d);
    const VU16 raw0 = LoadU(d, raw + 0 * N);
    const VU16 raw1 = LoadU(d, raw + 1 * N);
    const VU16 raw2 = LoadU(d, raw + 2 * N);
    const VU16 raw3 = LoadU(d, raw + 3 * N);
    const VU16 raw4 = LoadU(d, raw + 4 * N);
    const VU16 raw5 = LoadU(d, raw + 5 * N);
    const VU16 raw6 = LoadU(d, raw + 6 * N);
    const VU16 raw7 = LoadU(d, raw + 7 * N);
    const VU16 raw8 = LoadU(d, raw + 8 * N);
    const VU16 raw9 = LoadU(d, raw + 9 * N);
    const VU16 rawA = LoadU(d, raw + 0xA * N);
    const VU16 rawB = LoadU(d, raw + 0xB * N);
    const VU16 rawC = LoadU(d, raw + 0xC * N);
    const VU16 rawD = LoadU(d, raw + 0xD * N);
    const VU16 rawE = LoadU(d, raw + 0xE * N);
    const VU16 rawF = LoadU(d, raw + 0xF * N);

    const VU16 packed10 = Or(ShiftLeft<1>(raw1), raw0);
    const VU16 packed32 = Or(ShiftLeft<3>(raw3), ShiftLeft<2>(raw2));
    const VU16 packed54 = Or(ShiftLeft<5>(raw5), ShiftLeft<4>(raw4));
    const VU16 packed76 = Or(ShiftLeft<7>(raw7), ShiftLeft<6>(raw6));
    const VU16 packed98 = Or(ShiftLeft<9>(raw9), ShiftLeft<8>(raw8));
    const VU16 packedBA = Or(ShiftLeft<0xB>(rawB), ShiftLeft<0xA>(rawA));
    const VU16 packedDC = Or(ShiftLeft<0xD>(rawD), ShiftLeft<0xC>(rawC));
    const VU16 packedFE = Or(ShiftLeft<0xF>(rawF), ShiftLeft<0xE>(rawE));
    const VU16 packed50 = Or3(packed10, packed32, packed54);
    const VU16 packedB7 = Or3(packed76, packed98, packedBA);
    const VU16 packedFC = Or(packedFE, packedDC);
    const VU16 packed = Or3(packed50, packedB7, packedFC);
    StoreU(packed, d, packed_out);
  }

  template <class D>
  HWY_INLINE void Unpack(D d, const uint16_t* HWY_RESTRICT packed_in,
                         uint16_t* HWY_RESTRICT raw) {
    using VU16 = Vec<decltype(d)>;
    const size_t N = Lanes(d);
    // We extract the lowest bit from each u16, then shift right.
    const VU16 mask = Set(d, 1u);

    VU16 packed = LoadU(d, packed_in);

    const VU16 raw0 = And(packed, mask);
    packed = ShiftRight<1>(packed);
    StoreU(raw0, d, raw + 0 * N);

    const VU16 raw1 = And(packed, mask);
    packed = ShiftRight<1>(packed);
    StoreU(raw1, d, raw + 1 * N);

    const VU16 raw2 = And(packed, mask);
    packed = ShiftRight<1>(packed);
    StoreU(raw2, d, raw + 2 * N);

    const VU16 raw3 = And(packed, mask);
    packed = ShiftRight<1>(packed);
    StoreU(raw3, d, raw + 3 * N);

    const VU16 raw4 = And(packed, mask);
    packed = ShiftRight<1>(packed);
    StoreU(raw4, d, raw + 4 * N);

    const VU16 raw5 = And(packed, mask);
    packed = ShiftRight<1>(packed);
    StoreU(raw5, d, raw + 5 * N);

    const VU16 raw6 = And(packed, mask);
    packed = ShiftRight<1>(packed);
    StoreU(raw6, d, raw + 6 * N);

    const VU16 raw7 = And(packed, mask);
    packed = ShiftRight<1>(packed);
    StoreU(raw7, d, raw + 7 * N);

    const VU16 raw8 = And(packed, mask);
    packed = ShiftRight<1>(packed);
    StoreU(raw8, d, raw + 8 * N);

    const VU16 raw9 = And(packed, mask);
    packed = ShiftRight<1>(packed);
    StoreU(raw9, d, raw + 9 * N);

    const VU16 rawA = And(packed, mask);
    packed = ShiftRight<1>(packed);
    StoreU(rawA, d, raw + 0xA * N);

    const VU16 rawB = And(packed, mask);
    packed = ShiftRight<1>(packed);
    StoreU(rawB, d, raw + 0xB * N);

    const VU16 rawC = And(packed, mask);
    packed = ShiftRight<1>(packed);
    StoreU(rawC, d, raw + 0xC * N);

    const VU16 rawD = And(packed, mask);
    packed = ShiftRight<1>(packed);
    StoreU(rawD, d, raw + 0xD * N);

    const VU16 rawE = And(packed, mask);
    packed = ShiftRight<1>(packed);
    StoreU(rawE, d, raw + 0xE * N);

    const VU16 rawF = And(packed, mask);
    StoreU(rawF, d, raw + 0xF * N);
  }
};  // Pack16<1>

template <>
struct Pack16<2> {
  static constexpr size_t kBits = 2;
  static constexpr size_t kRawVectors = 8;
  static constexpr size_t kPackedVectors = 1;

  template <class D>
  HWY_INLINE void Pack(D d, const uint16_t* HWY_RESTRICT raw,
                       uint16_t* HWY_RESTRICT packed_out) {
    using VU16 = Vec<decltype(d)>;
    const size_t N = Lanes(d);
    const VU16 raw0 = LoadU(d, raw + 0 * N);
    const VU16 raw1 = LoadU(d, raw + 1 * N);
    const VU16 raw2 = LoadU(d, raw + 2 * N);
    const VU16 raw3 = LoadU(d, raw + 3 * N);
    const VU16 raw4 = LoadU(d, raw + 4 * N);
    const VU16 raw5 = LoadU(d, raw + 5 * N);
    const VU16 raw6 = LoadU(d, raw + 6 * N);
    const VU16 raw7 = LoadU(d, raw + 7 * N);

    const VU16 packed10 = Or(ShiftLeft<2>(raw1), raw0);
    const VU16 packed32 = Or(ShiftLeft<6>(raw3), ShiftLeft<4>(raw2));
    const VU16 packed54 = Or(ShiftLeft<10>(raw5), ShiftLeft<8>(raw4));
    const VU16 packed76 = Or(ShiftLeft<14>(raw7), ShiftLeft<12>(raw6));
    const VU16 packed50 = Or3(packed10, packed32, packed54);
    const VU16 packed = Or(packed50, packed76);
    StoreU(packed, d, packed_out);
  }

  template <class D>
  HWY_INLINE void Unpack(D d, const uint16_t* HWY_RESTRICT packed_in,
                         uint16_t* HWY_RESTRICT raw) {
    using VU16 = Vec<decltype(d)>;
    const size_t N = Lanes(d);
    // We extract the lowest two bits, then shift right.
    const VU16 mask = Set(d, 0x3u);

    VU16 packed = LoadU(d, packed_in);

    const VU16 raw0 = And(packed, mask);
    packed = ShiftRight<2>(packed);
    StoreU(raw0, d, raw + 0 * N);

    const VU16 raw1 = And(packed, mask);
    packed = ShiftRight<2>(packed);
    StoreU(raw1, d, raw + 1 * N);

    const VU16 raw2 = And(packed, mask);
    packed = ShiftRight<2>(packed);
    StoreU(raw2, d, raw + 2 * N);

    const VU16 raw3 = And(packed, mask);
    packed = ShiftRight<2>(packed);
    StoreU(raw3, d, raw + 3 * N);

    const VU16 raw4 = And(packed, mask);
    packed = ShiftRight<2>(packed);
    StoreU(raw4, d, raw + 4 * N);

    const VU16 raw5 = And(packed, mask);
    packed = ShiftRight<2>(packed);
    StoreU(raw5, d, raw + 5 * N);

    const VU16 raw6 = And(packed, mask);
    packed = ShiftRight<2>(packed);
    StoreU(raw6, d, raw + 6 * N);

    const VU16 raw7 = And(packed, mask);
    packed = ShiftRight<2>(packed);
    StoreU(raw7, d, raw + 7 * N);
  }
};  // Pack16<2>

template <>
struct Pack16<3> {
  static constexpr size_t kBits = 3;
  static constexpr size_t kRawVectors = 16;
  static constexpr size_t kPackedVectors = 3;

  template <class D>
  HWY_INLINE void Pack(D d, const uint16_t* HWY_RESTRICT raw,
                       uint16_t* HWY_RESTRICT packed_out) {
    using VU16 = Vec<decltype(d)>;
    const size_t N = Lanes(d);
    const VU16 raw0 = LoadU(d, raw + 0 * N);
    const VU16 raw1 = LoadU(d, raw + 1 * N);
    const VU16 raw2 = LoadU(d, raw + 2 * N);
    const VU16 raw3 = LoadU(d, raw + 3 * N);
    const VU16 raw4 = LoadU(d, raw + 4 * N);
    const VU16 raw5 = LoadU(d, raw + 5 * N);
    const VU16 raw6 = LoadU(d, raw + 6 * N);
    const VU16 raw7 = LoadU(d, raw + 7 * N);
    const VU16 raw8 = LoadU(d, raw + 8 * N);
    const VU16 raw9 = LoadU(d, raw + 9 * N);
    const VU16 rawA = LoadU(d, raw + 0xA * N);
    const VU16 rawB = LoadU(d, raw + 0xB * N);
    const VU16 rawC = LoadU(d, raw + 0xC * N);
    const VU16 rawD = LoadU(d, raw + 0xD * N);
    const VU16 rawE = LoadU(d, raw + 0xE * N);
    const VU16 rawF = LoadU(d, raw + 0xF * N);

    // We can fit 15 raw vectors in three packed vectors (five each).
    const VU16 raw630 = Or3(ShiftLeft<6>(raw6), ShiftLeft<3>(raw3), raw0);
    const VU16 raw741 = Or3(ShiftLeft<6>(raw7), ShiftLeft<3>(raw4), raw1);
    const VU16 raw852 = Or3(ShiftLeft<6>(raw8), ShiftLeft<3>(raw5), raw2);

    // rawF will be scattered into the upper bits of these three.
    VU16 packed0 = Or3(raw630, ShiftLeft<12>(rawC), ShiftLeft<9>(raw9));
    VU16 packed1 = Or3(raw741, ShiftLeft<12>(rawD), ShiftLeft<9>(rawA));
    VU16 packed2 = Or3(raw852, ShiftLeft<12>(rawE), ShiftLeft<9>(rawB));

    const VU16 hi1 = Set(d, 0x8000u);
    packed0 = Or(packed0, ShiftLeft<15>(rawF));  // MSB only, no mask
    packed1 = OrAnd(packed1, ShiftLeft<14>(rawF), hi1);
    packed2 = OrAnd(packed2, ShiftLeft<13>(rawF), hi1);
    StoreU(packed0, d, packed_out + 0 * N);
    StoreU(packed1, d, packed_out + 1 * N);
    StoreU(packed2, d, packed_out + 2 * N);
  }

  template <class D>
  HWY_INLINE void Unpack(D d, const uint16_t* HWY_RESTRICT packed_in,
                         uint16_t* HWY_RESTRICT raw) {
    using VU16 = Vec<decltype(d)>;
    const size_t N = Lanes(d);
    // We extract the lowest three bits.
    const VU16 mask = Set(d, 0x7u);

    VU16 packed0 = LoadU(d, packed_in + 0 * N);
    VU16 packed1 = LoadU(d, packed_in + 1 * N);
    VU16 packed2 = LoadU(d, packed_in + 2 * N);

    const VU16 raw0 = And(mask, packed0);
    packed0 = ShiftRight<3>(packed0);
    StoreU(raw0, d, raw + 0 * N);

    const VU16 raw1 = And(mask, packed1);
    packed1 = ShiftRight<3>(packed1);
    StoreU(raw1, d, raw + 1 * N);

    const VU16 raw2 = And(mask, packed2);
    packed2 = ShiftRight<3>(packed2);
    StoreU(raw2, d, raw + 2 * N);

    const VU16 raw3 = And(mask, packed0);
    packed0 = ShiftRight<3>(packed0);
    StoreU(raw3, d, raw + 3 * N);

    const VU16 raw4 = And(mask, packed1);
    packed1 = ShiftRight<3>(packed1);
    StoreU(raw4, d, raw + 4 * N);

    const VU16 raw5 = And(mask, packed2);
    packed2 = ShiftRight<3>(packed2);
    StoreU(raw5, d, raw + 5 * N);

    const VU16 raw6 = And(mask, packed0);
    packed0 = ShiftRight<3>(packed0);
    StoreU(raw6, d, raw + 6 * N);

    const VU16 raw7 = And(mask, packed1);
    packed1 = ShiftRight<3>(packed1);
    StoreU(raw7, d, raw + 7 * N);

    const VU16 raw8 = And(mask, packed2);
    packed2 = ShiftRight<3>(packed2);
    StoreU(raw8, d, raw + 8 * N);

    const VU16 raw9 = And(mask, packed0);
    packed0 = ShiftRight<3>(packed0);
    StoreU(raw9, d, raw + 9 * N);

    const VU16 rawA = And(mask, packed1);
    packed1 = ShiftRight<3>(packed1);
    StoreU(rawA, d, raw + 0xA * N);

    const VU16 rawB = And(mask, packed2);
    packed2 = ShiftRight<3>(packed2);
    StoreU(rawB, d, raw + 0xB * N);

    const VU16 rawC = And(mask, packed0);
    packed0 = ShiftRight<3>(packed0);
    StoreU(rawC, d, raw + 0xC * N);

    const VU16 rawD = And(mask, packed1);
    packed1 = ShiftRight<3>(packed1);
    StoreU(rawD, d, raw + 0xD * N);

    const VU16 rawE = And(mask, packed2);
    packed2 = ShiftRight<3>(packed2);
    StoreU(rawE, d, raw + 0xE * N);

    // rawF is the concatenation of the lower bit of packed0..2. No masking is
    // required because we have shifted that bit downward from the MSB.
    const VU16 rawF =
        Or3(ShiftLeft<2>(packed2), Add(packed1, packed1), packed0);
    StoreU(rawF, d, raw + 0xF * N);
  }
};  // Pack16<3>

template <>
struct Pack16<4> {
  static constexpr size_t kBits = 4;
  // 2x unrolled (matches size of 2/6 bit cases) for increased efficiency.
  static constexpr size_t kRawVectors = 8;
  static constexpr size_t kPackedVectors = 2;

  template <class D>
  HWY_INLINE void Pack(D d, const uint16_t* HWY_RESTRICT raw,
                       uint16_t* HWY_RESTRICT packed_out) {
    using VU16 = Vec<decltype(d)>;
    const size_t N = Lanes(d);
    const VU16 raw0 = BitCast(d, LoadU(d, raw + 0 * N));
    const VU16 raw1 = BitCast(d, LoadU(d, raw + 1 * N));
    const VU16 raw2 = BitCast(d, LoadU(d, raw + 2 * N));
    const VU16 raw3 = BitCast(d, LoadU(d, raw + 3 * N));
    const VU16 raw4 = BitCast(d, LoadU(d, raw + 4 * N));
    const VU16 raw5 = BitCast(d, LoadU(d, raw + 5 * N));
    const VU16 raw6 = BitCast(d, LoadU(d, raw + 6 * N));
    const VU16 raw7 = BitCast(d, LoadU(d, raw + 7 * N));

    const VU16 raw20 = Or3(ShiftLeft<8>(raw2), ShiftLeft<4>(raw1), raw0);
    const VU16 packed0 = Or(raw20, ShiftLeft<12>(raw3));
    const VU16 raw64 = Or3(ShiftLeft<8>(raw6), ShiftLeft<4>(raw5), raw4);
    const VU16 packed1 = Or(raw64, ShiftLeft<12>(raw7));
    StoreU(packed0, d, packed_out + 0 * N);
    StoreU(packed1, d, packed_out + 1 * N);
  }

  template <class D>
  HWY_INLINE void Unpack(D d, const uint16_t* HWY_RESTRICT packed_in,
                         uint16_t* HWY_RESTRICT raw) {
    using VU16 = Vec<decltype(d)>;
    const size_t N = Lanes(d);
    // We extract the lowest four bits, then shift right.
    const VU16 mask = Set(d, 0xFu);

    VU16 packed0 = LoadU(d, packed_in + 0 * N);
    VU16 packed1 = LoadU(d, packed_in + 1 * N);

    const VU16 raw0 = And(packed0, mask);
    packed0 = ShiftRight<4>(packed0);
    StoreU(raw0, d, raw + 0 * N);

    const VU16 raw1 = And(packed0, mask);
    packed0 = ShiftRight<4>(packed0);
    StoreU(raw1, d, raw + 1 * N);

    const VU16 raw2 = And(packed0, mask);
    packed0 = ShiftRight<4>(packed0);
    StoreU(raw2, d, raw + 2 * N);

    const VU16 raw3 = packed0;  // shifted down, no mask required
    StoreU(raw3, d, raw + 3 * N);

    const VU16 raw4 = And(packed1, mask);
    packed1 = ShiftRight<4>(packed1);
    StoreU(raw4, d, raw + 4 * N);

    const VU16 raw5 = And(packed1, mask);
    packed1 = ShiftRight<4>(packed1);
    StoreU(raw5, d, raw + 5 * N);

    const VU16 raw6 = And(packed1, mask);
    packed1 = ShiftRight<4>(packed1);
    StoreU(raw6, d, raw + 6 * N);

    const VU16 raw7 = packed1;  // shifted down, no mask required
    StoreU(raw7, d, raw + 7 * N);
  }
};  // Pack16<4>

template <>
struct Pack16<5> {
  static constexpr size_t kBits = 5;
  static constexpr size_t kRawVectors = 16;
  static constexpr size_t kPackedVectors = 5;

  template <class D>
  HWY_INLINE void Pack(D d, const uint16_t* HWY_RESTRICT raw,
                       uint16_t* HWY_RESTRICT packed_out) {
    using VU16 = Vec<decltype(d)>;
    const size_t N = Lanes(d);
    const VU16 raw0 = LoadU(d, raw + 0 * N);
    const VU16 raw1 = LoadU(d, raw + 1 * N);
    const VU16 raw2 = LoadU(d, raw + 2 * N);
    const VU16 raw3 = LoadU(d, raw + 3 * N);
    const VU16 raw4 = LoadU(d, raw + 4 * N);
    const VU16 raw5 = LoadU(d, raw + 5 * N);
    const VU16 raw6 = LoadU(d, raw + 6 * N);
    const VU16 raw7 = LoadU(d, raw + 7 * N);
    const VU16 raw8 = LoadU(d, raw + 8 * N);
    const VU16 raw9 = LoadU(d, raw + 9 * N);
    const VU16 rawA = LoadU(d, raw + 0xA * N);
    const VU16 rawB = LoadU(d, raw + 0xB * N);
    const VU16 rawC = LoadU(d, raw + 0xC * N);
    const VU16 rawD = LoadU(d, raw + 0xD * N);
    const VU16 rawE = LoadU(d, raw + 0xE * N);
    const VU16 rawF = LoadU(d, raw + 0xF * N);

    // We can fit 15 raw vectors in five packed vectors (three each).
    const VU16 rawA50 = Or3(ShiftLeft<10>(rawA), ShiftLeft<5>(raw5), raw0);
    const VU16 rawB61 = Or3(ShiftLeft<10>(rawB), ShiftLeft<5>(raw6), raw1);
    const VU16 rawC72 = Or3(ShiftLeft<10>(rawC), ShiftLeft<5>(raw7), raw2);
    const VU16 rawD83 = Or3(ShiftLeft<10>(rawD), ShiftLeft<5>(raw8), raw3);
    const VU16 rawE94 = Or3(ShiftLeft<10>(rawE), ShiftLeft<5>(raw9), raw4);

    // rawF will be scattered into the upper bits of these five.
    const VU16 hi1 = Set(d, 0x8000u);
    const VU16 packed0 = Or(rawA50, ShiftLeft<15>(rawF));  // MSB only, no mask
    const VU16 packed1 = OrAnd(rawB61, ShiftLeft<14>(rawF), hi1);
    const VU16 packed2 = OrAnd(rawC72, ShiftLeft<13>(rawF), hi1);
    const VU16 packed3 = OrAnd(rawD83, ShiftLeft<12>(rawF), hi1);
    const VU16 packed4 = OrAnd(rawE94, ShiftLeft<11>(rawF), hi1);
    StoreU(packed0, d, packed_out + 0 * N);
    StoreU(packed1, d, packed_out + 1 * N);
    StoreU(packed2, d, packed_out + 2 * N);
    StoreU(packed3, d, packed_out + 3 * N);
    StoreU(packed4, d, packed_out + 4 * N);
  }

  template <class D>
  HWY_INLINE void Unpack(D d, const uint16_t* HWY_RESTRICT packed_in,
                         uint16_t* HWY_RESTRICT raw) {
    using VU16 = Vec<decltype(d)>;
    const size_t N = Lanes(d);

    VU16 packed0 = LoadU(d, packed_in + 0 * N);
    VU16 packed1 = LoadU(d, packed_in + 1 * N);
    VU16 packed2 = LoadU(d, packed_in + 2 * N);
    VU16 packed3 = LoadU(d, packed_in + 3 * N);
    VU16 packed4 = LoadU(d, packed_in + 4 * N);

    // We extract the lowest five bits and shift right.
    const VU16 mask = Set(d, 0x1Fu);

    const VU16 raw0 = And(packed0, mask);
    packed0 = ShiftRight<5>(packed0);
    StoreU(raw0, d, raw + 0 * N);

    const VU16 raw1 = And(packed1, mask);
    packed1 = ShiftRight<5>(packed1);
    StoreU(raw1, d, raw + 1 * N);

    const VU16 raw2 = And(packed2, mask);
    packed2 = ShiftRight<5>(packed2);
    StoreU(raw2, d, raw + 2 * N);

    const VU16 raw3 = And(packed3, mask);
    packed3 = ShiftRight<5>(packed3);
    StoreU(raw3, d, raw + 3 * N);

    const VU16 raw4 = And(packed4, mask);
    packed4 = ShiftRight<5>(packed4);
    StoreU(raw4, d, raw + 4 * N);

    const VU16 raw5 = And(packed0, mask);
    packed0 = ShiftRight<5>(packed0);
    StoreU(raw5, d, raw + 5 * N);

    const VU16 raw6 = And(packed1, mask);
    packed1 = ShiftRight<5>(packed1);
    StoreU(raw6, d, raw + 6 * N);

    const VU16 raw7 = And(packed2, mask);
    packed2 = ShiftRight<5>(packed2);
    StoreU(raw7, d, raw + 7 * N);

    const VU16 raw8 = And(packed3, mask);
    packed3 = ShiftRight<5>(packed3);
    StoreU(raw8, d, raw + 8 * N);

    const VU16 raw9 = And(packed4, mask);
    packed4 = ShiftRight<5>(packed4);
    StoreU(raw9, d, raw + 9 * N);

    const VU16 rawA = And(packed0, mask);
    packed0 = ShiftRight<5>(packed0);
    StoreU(rawA, d, raw + 0xA * N);

    const VU16 rawB = And(packed1, mask);
    packed1 = ShiftRight<5>(packed1);
    StoreU(rawB, d, raw + 0xB * N);

    const VU16 rawC = And(packed2, mask);
    packed2 = ShiftRight<5>(packed2);
    StoreU(rawC, d, raw + 0xC * N);

    const VU16 rawD = And(packed3, mask);
    packed3 = ShiftRight<5>(packed3);
    StoreU(rawD, d, raw + 0xD * N);

    const VU16 rawE = And(packed4, mask);
    packed4 = ShiftRight<5>(packed4);
    StoreU(rawE, d, raw + 0xE * N);

    // rawF is the concatenation of the lower bit of packed0..4. No masking is
    // required because we have shifted that bit downward from the MSB.
    const VU16 bits20 =
        Or3(ShiftLeft<2>(packed2), Add(packed1, packed1), packed0);
    const VU16 rawF = Or3(ShiftLeft<4>(packed4), ShiftLeft<3>(packed3), bits20);
    StoreU(rawF, d, raw + 0xF * N);
  }
};  // Pack16<5>

template <>
struct Pack16<6> {
  static constexpr size_t kBits = 6;
  static constexpr size_t kRawVectors = 8;
  static constexpr size_t kPackedVectors = 3;

  template <class D>
  HWY_INLINE void Pack(D d, const uint16_t* HWY_RESTRICT raw,
                       uint16_t* HWY_RESTRICT packed_out) {
    using VU16 = Vec<decltype(d)>;
    const size_t N = Lanes(d);
    const VU16 raw0 = LoadU(d, raw + 0 * N);
    const VU16 raw1 = LoadU(d, raw + 1 * N);
    const VU16 raw2 = LoadU(d, raw + 2 * N);
    const VU16 raw3 = LoadU(d, raw + 3 * N);
    const VU16 raw4 = LoadU(d, raw + 4 * N);
    const VU16 raw5 = LoadU(d, raw + 5 * N);
    const VU16 raw6 = LoadU(d, raw + 6 * N);
    const VU16 raw7 = LoadU(d, raw + 7 * N);

    // Three vectors, two 6-bit raw each; packed3 (12 bits) is spread over the
    // four remainder bits at the top of each vector.
    VU16 packed0 = Or(ShiftLeft<6>(raw4), raw0);
    VU16 packed1 = Or(ShiftLeft<6>(raw5), raw1);
    VU16 packed2 = Or(ShiftLeft<6>(raw6), raw2);
    const VU16 packed3 = Or(ShiftLeft<6>(raw7), raw3);

    const VU16 hi4 = Set(d, 0xF000u);
    packed0 = Or(packed0, ShiftLeft<12>(packed3));  // shift zeroed LSBs
    packed1 = OrAnd(packed1, ShiftLeft<8>(packed3), hi4);
    packed2 = OrAnd(packed2, ShiftLeft<4>(packed3), hi4);
    StoreU(packed0, d, packed_out + 0 * N);
    StoreU(packed1, d, packed_out + 1 * N);
    StoreU(packed2, d, packed_out + 2 * N);
  }

  template <class D>
  HWY_INLINE void Unpack(D d, const uint16_t* HWY_RESTRICT packed_in,
                         uint16_t* HWY_RESTRICT raw) {
    using VU16 = Vec<decltype(d)>;
    const size_t N = Lanes(d);
    // We extract the lowest six bits and shift right.
    const VU16 mask = Set(d, 0x3Fu);

    VU16 packed0 = LoadU(d, packed_in + 0 * N);
    VU16 packed1 = LoadU(d, packed_in + 1 * N);
    VU16 packed2 = LoadU(d, packed_in + 2 * N);

    const VU16 raw0 = And(packed0, mask);
    packed0 = ShiftRight<6>(packed0);
    StoreU(raw0, d, raw + 0 * N);

    const VU16 raw1 = And(packed1, mask);
    packed1 = ShiftRight<6>(packed1);
    StoreU(raw1, d, raw + 1 * N);

    const VU16 raw2 = And(packed2, mask);
    packed2 = ShiftRight<6>(packed2);
    StoreU(raw2, d, raw + 2 * N);

    const VU16 raw4 = And(packed0, mask);
    packed0 = ShiftRight<6>(packed0);
    StoreU(raw4, d, raw + 4 * N);

    const VU16 raw5 = And(packed1, mask);
    packed1 = ShiftRight<6>(packed1);
    StoreU(raw5, d, raw + 5 * N);

    const VU16 raw6 = And(packed2, mask);
    packed2 = ShiftRight<6>(packed2);
    StoreU(raw6, d, raw + 6 * N);

    // packed3 is the concatenation of the four bits in packed0..2.
    VU16 packed3 = Or3(ShiftLeft<8>(packed2), ShiftLeft<4>(packed1), packed0);
    const VU16 raw3 = And(packed3, mask);
    packed3 = ShiftRight<6>(packed3);
    StoreU(raw3, d, raw + 3 * N);
    const VU16 raw7 = packed3;  // upper bits already zero
    StoreU(raw7, d, raw + 7 * N);
  }
};  // Pack16<6>

template <>
struct Pack16<7> {
  static constexpr size_t kBits = 7;
  static constexpr size_t kRawVectors = 16;
  static constexpr size_t kPackedVectors = 7;

  template <class D>
  HWY_INLINE void Pack(D d, const uint16_t* HWY_RESTRICT raw,
                       uint16_t* HWY_RESTRICT packed_out) {
    using VU16 = Vec<decltype(d)>;
    const size_t N = Lanes(d);
    const VU16 raw0 = LoadU(d, raw + 0 * N);
    const VU16 raw1 = LoadU(d, raw + 1 * N);
    const VU16 raw2 = LoadU(d, raw + 2 * N);
    const VU16 raw3 = LoadU(d, raw + 3 * N);
    const VU16 raw4 = LoadU(d, raw + 4 * N);
    const VU16 raw5 = LoadU(d, raw + 5 * N);
    const VU16 raw6 = LoadU(d, raw + 6 * N);
    const VU16 raw7 = LoadU(d, raw + 7 * N);
    const VU16 raw8 = LoadU(d, raw + 8 * N);
    const VU16 raw9 = LoadU(d, raw + 9 * N);
    const VU16 rawA = LoadU(d, raw + 0xA * N);
    const VU16 rawB = LoadU(d, raw + 0xB * N);
    const VU16 rawC = LoadU(d, raw + 0xC * N);
    const VU16 rawD = LoadU(d, raw + 0xD * N);
    const VU16 rawE = LoadU(d, raw + 0xE * N);
    const VU16 rawF = LoadU(d, raw + 0xF * N);
    // Seven vectors, two 7-bit raw each; packed7 (14 bits) is spread over the
    // two remainder bits at the top of each vector.
    VU16 packed0 = Or(ShiftLeft<7>(raw8), raw0);
    VU16 packed1 = Or(ShiftLeft<7>(raw9), raw1);
    VU16 packed2 = Or(ShiftLeft<7>(rawA), raw2);
    VU16 packed3 = Or(ShiftLeft<7>(rawB), raw3);
    VU16 packed4 = Or(ShiftLeft<7>(rawC), raw4);
    VU16 packed5 = Or(ShiftLeft<7>(rawD), raw5);
    VU16 packed6 = Or(ShiftLeft<7>(rawE), raw6);
    const VU16 packed7 = Or(ShiftLeft<7>(rawF), raw7);

    const VU16 hi2 = Set(d, 0xC000u);
    packed0 = Or(packed0, ShiftLeft<14>(packed7));  // shift zeroed LSBs
    packed1 = OrAnd(packed1, ShiftLeft<12>(packed7), hi2);
    packed2 = OrAnd(packed2, ShiftLeft<10>(packed7), hi2);
    packed3 = OrAnd(packed3, ShiftLeft<8>(packed7), hi2);
    packed4 = OrAnd(packed4, ShiftLeft<6>(packed7), hi2);
    packed5 = OrAnd(packed5, ShiftLeft<4>(packed7), hi2);
    packed6 = OrAnd(packed6, ShiftLeft<2>(packed7), hi2);
    StoreU(packed0, d, packed_out + 0 * N);
    StoreU(packed1, d, packed_out + 1 * N);
    StoreU(packed2, d, packed_out + 2 * N);
    StoreU(packed3, d, packed_out + 3 * N);
    StoreU(packed4, d, packed_out + 4 * N);
    StoreU(packed5, d, packed_out + 5 * N);
    StoreU(packed6, d, packed_out + 6 * N);
  }

  template <class D>
  HWY_INLINE void Unpack(D d, const uint16_t* HWY_RESTRICT packed_in,
                         uint16_t* HWY_RESTRICT raw) {
    using VU16 = Vec<decltype(d)>;
    const size_t N = Lanes(d);

    VU16 packed0 = BitCast(d, LoadU(d, packed_in + 0 * N));
    VU16 packed1 = BitCast(d, LoadU(d, packed_in + 1 * N));
    VU16 packed2 = BitCast(d, LoadU(d, packed_in + 2 * N));
    VU16 packed3 = BitCast(d, LoadU(d, packed_in + 3 * N));
    VU16 packed4 = BitCast(d, LoadU(d, packed_in + 4 * N));
    VU16 packed5 = BitCast(d, LoadU(d, packed_in + 5 * N));
    VU16 packed6 = BitCast(d, LoadU(d, packed_in + 6 * N));

    // We extract the lowest seven bits and shift right.
    const VU16 mask = Set(d, 0x7Fu);

    const VU16 raw0 = And(packed0, mask);
    packed0 = ShiftRight<7>(packed0);
    StoreU(raw0, d, raw + 0 * N);

    const VU16 raw1 = And(packed1, mask);
    packed1 = ShiftRight<7>(packed1);
    StoreU(raw1, d, raw + 1 * N);

    const VU16 raw2 = And(packed2, mask);
    packed2 = ShiftRight<7>(packed2);
    StoreU(raw2, d, raw + 2 * N);

    const VU16 raw3 = And(packed3, mask);
    packed3 = ShiftRight<7>(packed3);
    StoreU(raw3, d, raw + 3 * N);

    const VU16 raw4 = And(packed4, mask);
    packed4 = ShiftRight<7>(packed4);
    StoreU(raw4, d, raw + 4 * N);

    const VU16 raw5 = And(packed5, mask);
    packed5 = ShiftRight<7>(packed5);
    StoreU(raw5, d, raw + 5 * N);

    const VU16 raw6 = And(packed6, mask);
    packed6 = ShiftRight<7>(packed6);
    StoreU(raw6, d, raw + 6 * N);

    const VU16 raw8 = And(packed0, mask);
    packed0 = ShiftRight<7>(packed0);
    StoreU(raw8, d, raw + 8 * N);

    const VU16 raw9 = And(packed1, mask);
    packed1 = ShiftRight<7>(packed1);
    StoreU(raw9, d, raw + 9 * N);

    const VU16 rawA = And(packed2, mask);
    packed2 = ShiftRight<7>(packed2);
    StoreU(rawA, d, raw + 0xA * N);

    const VU16 rawB = And(packed3, mask);
    packed3 = ShiftRight<7>(packed3);
    StoreU(rawB, d, raw + 0xB * N);

    const VU16 rawC = And(packed4, mask);
    packed4 = ShiftRight<7>(packed4);
    StoreU(rawC, d, raw + 0xC * N);

    const VU16 rawD = And(packed5, mask);
    packed5 = ShiftRight<7>(packed5);
    StoreU(rawD, d, raw + 0xD * N);

    const VU16 rawE = And(packed6, mask);
    packed6 = ShiftRight<7>(packed6);
    StoreU(rawE, d, raw + 0xE * N);

    // packed7 is the concatenation of the two bits in packed0..6.
    const VU16 bits50 =
        Or3(ShiftLeft<4>(packed2), ShiftLeft<2>(packed1), packed0);
    const VU16 bitsB6 = Or3(ShiftLeft<10>(packed5), ShiftLeft<8>(packed4),
                            ShiftLeft<6>(packed3));
    VU16 packed7 = Or3(ShiftLeft<12>(packed6), bitsB6, bits50);
    const VU16 raw7 = And(packed7, mask);
    packed7 = ShiftRight<7>(packed7);
    StoreU(raw7, d, raw + 7 * N);
    const VU16 rawF = packed7;  // upper bits already zero
    StoreU(rawF, d, raw + 0xF * N);
  }
};  // Pack16<7>

template <>
struct Pack16<8> {
  static constexpr size_t kBits = 8;
  // 4x unrolled (matches size of 2/6 bit cases) for increased efficiency.
  static constexpr size_t kRawVectors = 8;
  static constexpr size_t kPackedVectors = 4;

  template <class D>
  HWY_INLINE void Pack(D d, const uint16_t* HWY_RESTRICT raw,
                       uint16_t* HWY_RESTRICT packed_out) {
    using VU16 = Vec<decltype(d)>;
    const size_t N = Lanes(d);
    const VU16 raw0 = LoadU(d, raw + 0 * N);
    const VU16 raw1 = LoadU(d, raw + 1 * N);
    const VU16 raw2 = LoadU(d, raw + 2 * N);
    const VU16 raw3 = LoadU(d, raw + 3 * N);
    const VU16 raw4 = LoadU(d, raw + 4 * N);
    const VU16 raw5 = LoadU(d, raw + 5 * N);
    const VU16 raw6 = LoadU(d, raw + 6 * N);
    const VU16 raw7 = LoadU(d, raw + 7 * N);
    // This is equivalent to ConcatEven with 8-bit lanes, but much more
    // efficient on RVV and slightly less efficient on SVE2.
    const VU16 packed0 = Or(ShiftLeft<8>(raw1), raw0);
    const VU16 packed1 = Or(ShiftLeft<8>(raw3), raw2);
    const VU16 packed2 = Or(ShiftLeft<8>(raw5), raw4);
    const VU16 packed3 = Or(ShiftLeft<8>(raw7), raw6);
    StoreU(packed0, d, packed_out + 0 * N);
    StoreU(packed1, d, packed_out + 1 * N);
    StoreU(packed2, d, packed_out + 2 * N);
    StoreU(packed3, d, packed_out + 3 * N);
  }

  template <class D>
  HWY_INLINE void Unpack(D d, const uint16_t* HWY_RESTRICT packed_in,
                         uint16_t* HWY_RESTRICT raw) {
    using VU16 = Vec<decltype(d)>;
    const size_t N = Lanes(d);

    VU16 packed0 = BitCast(d, LoadU(d, packed_in + 0 * N));
    VU16 packed1 = BitCast(d, LoadU(d, packed_in + 1 * N));
    VU16 packed2 = BitCast(d, LoadU(d, packed_in + 2 * N));
    VU16 packed3 = BitCast(d, LoadU(d, packed_in + 3 * N));
    // We extract the lowest eight bits and shift right.
    const VU16 mask = Set(d, 0xFFu);

    const VU16 raw0 = And(packed0, mask);
    packed0 = ShiftRight<8>(packed0);
    StoreU(raw0, d, raw + 0 * N);
    const VU16 raw1 = packed0;  // upper bits already zero
    StoreU(raw1, d, raw + 1 * N);

    const VU16 raw2 = And(packed1, mask);
    packed1 = ShiftRight<8>(packed1);
    StoreU(raw2, d, raw + 2 * N);
    const VU16 raw3 = packed1;  // upper bits already zero
    StoreU(raw3, d, raw + 3 * N);

    const VU16 raw4 = And(packed2, mask);
    packed2 = ShiftRight<8>(packed2);
    StoreU(raw4, d, raw + 4 * N);
    const VU16 raw5 = packed2;  // upper bits already zero
    StoreU(raw5, d, raw + 5 * N);

    const VU16 raw6 = And(packed3, mask);
    packed3 = ShiftRight<8>(packed3);
    StoreU(raw6, d, raw + 6 * N);
    const VU16 raw7 = packed3;  // upper bits already zero
    StoreU(raw7, d, raw + 7 * N);
  }
};  // Pack16<8>

}  // namespace detail

// NOLINTNEXTLINE(google-readability-namespace-comments)
}  // namespace HWY_NAMESPACE
}  // namespace hwy
HWY_AFTER_NAMESPACE();

#endif  // HIGHWAY_HWY_CONTRIB_BIT_PACK_INL_H_
