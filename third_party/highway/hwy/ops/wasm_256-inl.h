// Copyright 2021 Google LLC
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

// 256-bit WASM vectors and operations. Experimental.
// External include guard in highway.h - see comment there.

#include <stddef.h>
#include <stdint.h>
#include <wasm_simd128.h>

#include "hwy/base.h"
#include "hwy/ops/shared-inl.h"
#include "hwy/ops/wasm_128-inl.h"

HWY_BEFORE_NAMESPACE();
namespace hwy {
namespace HWY_NAMESPACE {

template <typename T>
using Full256 = Simd<T, 32 / sizeof(T), 0>;

template <typename T>
using Full128 = Simd<T, 16 / sizeof(T), 0>;

// TODO(richardwinterton): add this to DeduceD in wasm_128 similar to x86_128.
template <typename T>
class Vec256 {
 public:
  // Compound assignment. Only usable if there is a corresponding non-member
  // binary operator overload. For example, only f32 and f64 support division.
  HWY_INLINE Vec256& operator*=(const Vec256 other) {
    return *this = (*this * other);
  }
  HWY_INLINE Vec256& operator/=(const Vec256 other) {
    return *this = (*this / other);
  }
  HWY_INLINE Vec256& operator+=(const Vec256 other) {
    return *this = (*this + other);
  }
  HWY_INLINE Vec256& operator-=(const Vec256 other) {
    return *this = (*this - other);
  }
  HWY_INLINE Vec256& operator&=(const Vec256 other) {
    return *this = (*this & other);
  }
  HWY_INLINE Vec256& operator|=(const Vec256 other) {
    return *this = (*this | other);
  }
  HWY_INLINE Vec256& operator^=(const Vec256 other) {
    return *this = (*this ^ other);
  }

  Vec128<T> v0;
  Vec128<T> v1;
};

template <typename T>
struct Mask256 {
  Mask128<T> m0;
  Mask128<T> m1;
};

// ------------------------------ BitCast

template <typename T, typename FromT>
HWY_API Vec256<T> BitCast(Full256<T> d, Vec256<FromT> v) {
  const Half<decltype(d)> dh;
  Vec256<T> ret;
  ret.v0 = BitCast(dh, v.v0);
  ret.v1 = BitCast(dh, v.v1);
  return ret;

  // TODO(richardwinterton): implement other ops like this
}

// ------------------------------ Zero

// Returns an all-zero vector/part.
template <typename T>
HWY_API Vec256<T> Zero(Full256<T> /* tag */) {
  return Vec256<T>{wasm_i32x4_splat(0)};
}
HWY_API Vec256<float> Zero(Full256<float> /* tag */) {
  return Vec256<float>{wasm_f32x4_splat(0.0f)};
}

template <class D>
using VFromD = decltype(Zero(D()));

// ------------------------------ Set

// Returns a vector/part with all lanes set to "t".
HWY_API Vec256<uint8_t> Set(Full256<uint8_t> /* tag */, const uint8_t t) {
  return Vec256<uint8_t>{wasm_i8x16_splat(static_cast<int8_t>(t))};
}
HWY_API Vec256<uint16_t> Set(Full256<uint16_t> /* tag */, const uint16_t t) {
  return Vec256<uint16_t>{wasm_i16x8_splat(static_cast<int16_t>(t))};
}
HWY_API Vec256<uint32_t> Set(Full256<uint32_t> /* tag */, const uint32_t t) {
  return Vec256<uint32_t>{wasm_i32x4_splat(static_cast<int32_t>(t))};
}
HWY_API Vec256<uint64_t> Set(Full256<uint64_t> /* tag */, const uint64_t t) {
  return Vec256<uint64_t>{wasm_i64x2_splat(static_cast<int64_t>(t))};
}

HWY_API Vec256<int8_t> Set(Full256<int8_t> /* tag */, const int8_t t) {
  return Vec256<int8_t>{wasm_i8x16_splat(t)};
}
HWY_API Vec256<int16_t> Set(Full256<int16_t> /* tag */, const int16_t t) {
  return Vec256<int16_t>{wasm_i16x8_splat(t)};
}
HWY_API Vec256<int32_t> Set(Full256<int32_t> /* tag */, const int32_t t) {
  return Vec256<int32_t>{wasm_i32x4_splat(t)};
}
HWY_API Vec256<int64_t> Set(Full256<int64_t> /* tag */, const int64_t t) {
  return Vec256<int64_t>{wasm_i64x2_splat(t)};
}

HWY_API Vec256<float> Set(Full256<float> /* tag */, const float t) {
  return Vec256<float>{wasm_f32x4_splat(t)};
}

HWY_DIAGNOSTICS(push)
HWY_DIAGNOSTICS_OFF(disable : 4700, ignored "-Wuninitialized")

// Returns a vector with uninitialized elements.
template <typename T>
HWY_API Vec256<T> Undefined(Full256<T> d) {
  return Zero(d);
}

HWY_DIAGNOSTICS(pop)

// Returns a vector with lane i=[0, N) set to "first" + i.
template <typename T, typename T2>
Vec256<T> Iota(const Full256<T> d, const T2 first) {
  HWY_ALIGN T lanes[16 / sizeof(T)];
  for (size_t i = 0; i < 16 / sizeof(T); ++i) {
    lanes[i] = static_cast<T>(first + static_cast<T2>(i));
  }
  return Load(d, lanes);
}

// ================================================== ARITHMETIC

// ------------------------------ Addition

// Unsigned
HWY_API Vec256<uint8_t> operator+(const Vec256<uint8_t> a,
                                  const Vec256<uint8_t> b) {
  return Vec256<uint8_t>{wasm_i8x16_add(a.raw, b.raw)};
}
HWY_API Vec256<uint16_t> operator+(const Vec256<uint16_t> a,
                                   const Vec256<uint16_t> b) {
  return Vec256<uint16_t>{wasm_i16x8_add(a.raw, b.raw)};
}
HWY_API Vec256<uint32_t> operator+(const Vec256<uint32_t> a,
                                   const Vec256<uint32_t> b) {
  return Vec256<uint32_t>{wasm_i32x4_add(a.raw, b.raw)};
}

// Signed
HWY_API Vec256<int8_t> operator+(const Vec256<int8_t> a,
                                 const Vec256<int8_t> b) {
  return Vec256<int8_t>{wasm_i8x16_add(a.raw, b.raw)};
}
HWY_API Vec256<int16_t> operator+(const Vec256<int16_t> a,
                                  const Vec256<int16_t> b) {
  return Vec256<int16_t>{wasm_i16x8_add(a.raw, b.raw)};
}
HWY_API Vec256<int32_t> operator+(const Vec256<int32_t> a,
                                  const Vec256<int32_t> b) {
  return Vec256<int32_t>{wasm_i32x4_add(a.raw, b.raw)};
}

// Float
HWY_API Vec256<float> operator+(const Vec256<float> a, const Vec256<float> b) {
  return Vec256<float>{wasm_f32x4_add(a.raw, b.raw)};
}

// ------------------------------ Subtraction

// Unsigned
HWY_API Vec256<uint8_t> operator-(const Vec256<uint8_t> a,
                                  const Vec256<uint8_t> b) {
  return Vec256<uint8_t>{wasm_i8x16_sub(a.raw, b.raw)};
}
HWY_API Vec256<uint16_t> operator-(Vec256<uint16_t> a, Vec256<uint16_t> b) {
  return Vec256<uint16_t>{wasm_i16x8_sub(a.raw, b.raw)};
}
HWY_API Vec256<uint32_t> operator-(const Vec256<uint32_t> a,
                                   const Vec256<uint32_t> b) {
  return Vec256<uint32_t>{wasm_i32x4_sub(a.raw, b.raw)};
}

// Signed
HWY_API Vec256<int8_t> operator-(const Vec256<int8_t> a,
                                 const Vec256<int8_t> b) {
  return Vec256<int8_t>{wasm_i8x16_sub(a.raw, b.raw)};
}
HWY_API Vec256<int16_t> operator-(const Vec256<int16_t> a,
                                  const Vec256<int16_t> b) {
  return Vec256<int16_t>{wasm_i16x8_sub(a.raw, b.raw)};
}
HWY_API Vec256<int32_t> operator-(const Vec256<int32_t> a,
                                  const Vec256<int32_t> b) {
  return Vec256<int32_t>{wasm_i32x4_sub(a.raw, b.raw)};
}

// Float
HWY_API Vec256<float> operator-(const Vec256<float> a, const Vec256<float> b) {
  return Vec256<float>{wasm_f32x4_sub(a.raw, b.raw)};
}

// ------------------------------ SumsOf8
HWY_API Vec256<uint64_t> SumsOf8(const Vec256<uint8_t> v) {
  HWY_ABORT("not implemented");
}

// ------------------------------ SaturatedAdd

// Returns a + b clamped to the destination range.

// Unsigned
HWY_API Vec256<uint8_t> SaturatedAdd(const Vec256<uint8_t> a,
                                     const Vec256<uint8_t> b) {
  return Vec256<uint8_t>{wasm_u8x16_add_sat(a.raw, b.raw)};
}
HWY_API Vec256<uint16_t> SaturatedAdd(const Vec256<uint16_t> a,
                                      const Vec256<uint16_t> b) {
  return Vec256<uint16_t>{wasm_u16x8_add_sat(a.raw, b.raw)};
}

// Signed
HWY_API Vec256<int8_t> SaturatedAdd(const Vec256<int8_t> a,
                                    const Vec256<int8_t> b) {
  return Vec256<int8_t>{wasm_i8x16_add_sat(a.raw, b.raw)};
}
HWY_API Vec256<int16_t> SaturatedAdd(const Vec256<int16_t> a,
                                     const Vec256<int16_t> b) {
  return Vec256<int16_t>{wasm_i16x8_add_sat(a.raw, b.raw)};
}

// ------------------------------ SaturatedSub

// Returns a - b clamped to the destination range.

// Unsigned
HWY_API Vec256<uint8_t> SaturatedSub(const Vec256<uint8_t> a,
                                     const Vec256<uint8_t> b) {
  return Vec256<uint8_t>{wasm_u8x16_sub_sat(a.raw, b.raw)};
}
HWY_API Vec256<uint16_t> SaturatedSub(const Vec256<uint16_t> a,
                                      const Vec256<uint16_t> b) {
  return Vec256<uint16_t>{wasm_u16x8_sub_sat(a.raw, b.raw)};
}

// Signed
HWY_API Vec256<int8_t> SaturatedSub(const Vec256<int8_t> a,
                                    const Vec256<int8_t> b) {
  return Vec256<int8_t>{wasm_i8x16_sub_sat(a.raw, b.raw)};
}
HWY_API Vec256<int16_t> SaturatedSub(const Vec256<int16_t> a,
                                     const Vec256<int16_t> b) {
  return Vec256<int16_t>{wasm_i16x8_sub_sat(a.raw, b.raw)};
}

// ------------------------------ Average

// Returns (a + b + 1) / 2

// Unsigned
HWY_API Vec256<uint8_t> AverageRound(const Vec256<uint8_t> a,
                                     const Vec256<uint8_t> b) {
  return Vec256<uint8_t>{wasm_u8x16_avgr(a.raw, b.raw)};
}
HWY_API Vec256<uint16_t> AverageRound(const Vec256<uint16_t> a,
                                      const Vec256<uint16_t> b) {
  return Vec256<uint16_t>{wasm_u16x8_avgr(a.raw, b.raw)};
}

// ------------------------------ Absolute value

// Returns absolute value, except that LimitsMin() maps to LimitsMax() + 1.
HWY_API Vec256<int8_t> Abs(const Vec256<int8_t> v) {
  return Vec256<int8_t>{wasm_i8x16_abs(v.raw)};
}
HWY_API Vec256<int16_t> Abs(const Vec256<int16_t> v) {
  return Vec256<int16_t>{wasm_i16x8_abs(v.raw)};
}
HWY_API Vec256<int32_t> Abs(const Vec256<int32_t> v) {
  return Vec256<int32_t>{wasm_i32x4_abs(v.raw)};
}
HWY_API Vec256<int64_t> Abs(const Vec256<int64_t> v) {
  return Vec256<int32_t>{wasm_i62x2_abs(v.raw)};
}

HWY_API Vec256<float> Abs(const Vec256<float> v) {
  return Vec256<float>{wasm_f32x4_abs(v.raw)};
}

// ------------------------------ Shift lanes by constant #bits

// Unsigned
template <int kBits>
HWY_API Vec256<uint16_t> ShiftLeft(const Vec256<uint16_t> v) {
  return Vec256<uint16_t>{wasm_i16x8_shl(v.raw, kBits)};
}
template <int kBits>
HWY_API Vec256<uint16_t> ShiftRight(const Vec256<uint16_t> v) {
  return Vec256<uint16_t>{wasm_u16x8_shr(v.raw, kBits)};
}
template <int kBits>
HWY_API Vec256<uint32_t> ShiftLeft(const Vec256<uint32_t> v) {
  return Vec256<uint32_t>{wasm_i32x4_shl(v.raw, kBits)};
}
template <int kBits>
HWY_API Vec256<uint32_t> ShiftRight(const Vec256<uint32_t> v) {
  return Vec256<uint32_t>{wasm_u32x4_shr(v.raw, kBits)};
}

// Signed
template <int kBits>
HWY_API Vec256<int16_t> ShiftLeft(const Vec256<int16_t> v) {
  return Vec256<int16_t>{wasm_i16x8_shl(v.raw, kBits)};
}
template <int kBits>
HWY_API Vec256<int16_t> ShiftRight(const Vec256<int16_t> v) {
  return Vec256<int16_t>{wasm_i16x8_shr(v.raw, kBits)};
}
template <int kBits>
HWY_API Vec256<int32_t> ShiftLeft(const Vec256<int32_t> v) {
  return Vec256<int32_t>{wasm_i32x4_shl(v.raw, kBits)};
}
template <int kBits>
HWY_API Vec256<int32_t> ShiftRight(const Vec256<int32_t> v) {
  return Vec256<int32_t>{wasm_i32x4_shr(v.raw, kBits)};
}

// 8-bit
template <int kBits, typename T, HWY_IF_LANE_SIZE(T, 1)>
HWY_API Vec256<T> ShiftLeft(const Vec256<T> v) {
  const Full256<T> d8;
  // Use raw instead of BitCast to support N=1.
  const Vec256<T> shifted{ShiftLeft<kBits>(Vec128<MakeWide<T>>{v.raw}).raw};
  return kBits == 1
             ? (v + v)
             : (shifted & Set(d8, static_cast<T>((0xFF << kBits) & 0xFF)));
}

template <int kBits>
HWY_API Vec256<uint8_t> ShiftRight(const Vec256<uint8_t> v) {
  const Full256<uint8_t> d8;
  // Use raw instead of BitCast to support N=1.
  const Vec256<uint8_t> shifted{ShiftRight<kBits>(Vec128<uint16_t>{v.raw}).raw};
  return shifted & Set(d8, 0xFF >> kBits);
}

template <int kBits>
HWY_API Vec256<int8_t> ShiftRight(const Vec256<int8_t> v) {
  const Full256<int8_t> di;
  const Full256<uint8_t> du;
  const auto shifted = BitCast(di, ShiftRight<kBits>(BitCast(du, v)));
  const auto shifted_sign = BitCast(di, Set(du, 0x80 >> kBits));
  return (shifted ^ shifted_sign) - shifted_sign;
}

// ------------------------------ RotateRight (ShiftRight, Or)
template <int kBits, typename T>
HWY_API Vec256<T> RotateRight(const Vec256<T> v) {
  constexpr size_t kSizeInBits = sizeof(T) * 8;
  static_assert(0 <= kBits && kBits < kSizeInBits, "Invalid shift count");
  if (kBits == 0) return v;
  return Or(ShiftRight<kBits>(v), ShiftLeft<kSizeInBits - kBits>(v));
}

// ------------------------------ Shift lanes by same variable #bits

// Unsigned
HWY_API Vec256<uint16_t> ShiftLeftSame(const Vec256<uint16_t> v,
                                       const int bits) {
  return Vec256<uint16_t>{wasm_i16x8_shl(v.raw, bits)};
}
HWY_API Vec256<uint16_t> ShiftRightSame(const Vec256<uint16_t> v,
                                        const int bits) {
  return Vec256<uint16_t>{wasm_u16x8_shr(v.raw, bits)};
}
HWY_API Vec256<uint32_t> ShiftLeftSame(const Vec256<uint32_t> v,
                                       const int bits) {
  return Vec256<uint32_t>{wasm_i32x4_shl(v.raw, bits)};
}
HWY_API Vec256<uint32_t> ShiftRightSame(const Vec256<uint32_t> v,
                                        const int bits) {
  return Vec256<uint32_t>{wasm_u32x4_shr(v.raw, bits)};
}

// Signed
HWY_API Vec256<int16_t> ShiftLeftSame(const Vec256<int16_t> v, const int bits) {
  return Vec256<int16_t>{wasm_i16x8_shl(v.raw, bits)};
}
HWY_API Vec256<int16_t> ShiftRightSame(const Vec256<int16_t> v,
                                       const int bits) {
  return Vec256<int16_t>{wasm_i16x8_shr(v.raw, bits)};
}
HWY_API Vec256<int32_t> ShiftLeftSame(const Vec256<int32_t> v, const int bits) {
  return Vec256<int32_t>{wasm_i32x4_shl(v.raw, bits)};
}
HWY_API Vec256<int32_t> ShiftRightSame(const Vec256<int32_t> v,
                                       const int bits) {
  return Vec256<int32_t>{wasm_i32x4_shr(v.raw, bits)};
}

// 8-bit
template <typename T, HWY_IF_LANE_SIZE(T, 1)>
HWY_API Vec256<T> ShiftLeftSame(const Vec256<T> v, const int bits) {
  const Full256<T> d8;
  // Use raw instead of BitCast to support N=1.
  const Vec256<T> shifted{ShiftLeftSame(Vec128<MakeWide<T>>{v.raw}, bits).raw};
  return shifted & Set(d8, (0xFF << bits) & 0xFF);
}

HWY_API Vec256<uint8_t> ShiftRightSame(Vec256<uint8_t> v, const int bits) {
  const Full256<uint8_t> d8;
  // Use raw instead of BitCast to support N=1.
  const Vec256<uint8_t> shifted{
      ShiftRightSame(Vec128<uint16_t>{v.raw}, bits).raw};
  return shifted & Set(d8, 0xFF >> bits);
}

HWY_API Vec256<int8_t> ShiftRightSame(Vec256<int8_t> v, const int bits) {
  const Full256<int8_t> di;
  const Full256<uint8_t> du;
  const auto shifted = BitCast(di, ShiftRightSame(BitCast(du, v), bits));
  const auto shifted_sign = BitCast(di, Set(du, 0x80 >> bits));
  return (shifted ^ shifted_sign) - shifted_sign;
}

// ------------------------------ Minimum

// Unsigned
HWY_API Vec256<uint8_t> Min(const Vec256<uint8_t> a, const Vec256<uint8_t> b) {
  return Vec256<uint8_t>{wasm_u8x16_min(a.raw, b.raw)};
}
HWY_API Vec256<uint16_t> Min(const Vec256<uint16_t> a,
                             const Vec256<uint16_t> b) {
  return Vec256<uint16_t>{wasm_u16x8_min(a.raw, b.raw)};
}
HWY_API Vec256<uint32_t> Min(const Vec256<uint32_t> a,
                             const Vec256<uint32_t> b) {
  return Vec256<uint32_t>{wasm_u32x4_min(a.raw, b.raw)};
}
HWY_API Vec256<uint64_t> Min(const Vec256<uint64_t> a,
                             const Vec256<uint64_t> b) {
  alignas(32) float min[4];
  min[0] =
      HWY_MIN(wasm_u64x2_extract_lane(a, 0), wasm_u64x2_extract_lane(b, 0));
  min[1] =
      HWY_MIN(wasm_u64x2_extract_lane(a, 1), wasm_u64x2_extract_lane(b, 1));
  return Vec256<uint64_t>{wasm_v128_load(min)};
}

// Signed
HWY_API Vec256<int8_t> Min(const Vec256<int8_t> a, const Vec256<int8_t> b) {
  return Vec256<int8_t>{wasm_i8x16_min(a.raw, b.raw)};
}
HWY_API Vec256<int16_t> Min(const Vec256<int16_t> a, const Vec256<int16_t> b) {
  return Vec256<int16_t>{wasm_i16x8_min(a.raw, b.raw)};
}
HWY_API Vec256<int32_t> Min(const Vec256<int32_t> a, const Vec256<int32_t> b) {
  return Vec256<int32_t>{wasm_i32x4_min(a.raw, b.raw)};
}
HWY_API Vec256<int64_t> Min(const Vec256<int64_t> a, const Vec256<int64_t> b) {
  alignas(32) float min[4];
  min[0] =
      HWY_MIN(wasm_i64x2_extract_lane(a, 0), wasm_i64x2_extract_lane(b, 0));
  min[1] =
      HWY_MIN(wasm_i64x2_extract_lane(a, 1), wasm_i64x2_extract_lane(b, 1));
  return Vec256<int64_t>{wasm_v128_load(min)};
}

// Float
HWY_API Vec256<float> Min(const Vec256<float> a, const Vec256<float> b) {
  return Vec256<float>{wasm_f32x4_min(a.raw, b.raw)};
}

// ------------------------------ Maximum

// Unsigned
HWY_API Vec256<uint8_t> Max(const Vec256<uint8_t> a, const Vec256<uint8_t> b) {
  return Vec256<uint8_t>{wasm_u8x16_max(a.raw, b.raw)};
}
HWY_API Vec256<uint16_t> Max(const Vec256<uint16_t> a,
                             const Vec256<uint16_t> b) {
  return Vec256<uint16_t>{wasm_u16x8_max(a.raw, b.raw)};
}
HWY_API Vec256<uint32_t> Max(const Vec256<uint32_t> a,
                             const Vec256<uint32_t> b) {
  return Vec256<uint32_t>{wasm_u32x4_max(a.raw, b.raw)};
}
HWY_API Vec256<uint64_t> Max(const Vec256<uint64_t> a,
                             const Vec256<uint64_t> b) {
  alignas(32) float max[4];
  max[0] =
      HWY_MAX(wasm_u64x2_extract_lane(a, 0), wasm_u64x2_extract_lane(b, 0));
  max[1] =
      HWY_MAX(wasm_u64x2_extract_lane(a, 1), wasm_u64x2_extract_lane(b, 1));
  return Vec256<int64_t>{wasm_v128_load(max)};
}

// Signed
HWY_API Vec256<int8_t> Max(const Vec256<int8_t> a, const Vec256<int8_t> b) {
  return Vec256<int8_t>{wasm_i8x16_max(a.raw, b.raw)};
}
HWY_API Vec256<int16_t> Max(const Vec256<int16_t> a, const Vec256<int16_t> b) {
  return Vec256<int16_t>{wasm_i16x8_max(a.raw, b.raw)};
}
HWY_API Vec256<int32_t> Max(const Vec256<int32_t> a, const Vec256<int32_t> b) {
  return Vec256<int32_t>{wasm_i32x4_max(a.raw, b.raw)};
}
HWY_API Vec256<int64_t> Max(const Vec256<int64_t> a, const Vec256<int64_t> b) {
  alignas(32) float max[4];
  max[0] =
      HWY_MAX(wasm_i64x2_extract_lane(a, 0), wasm_i64x2_extract_lane(b, 0));
  max[1] =
      HWY_MAX(wasm_i64x2_extract_lane(a, 1), wasm_i64x2_extract_lane(b, 1));
  return Vec256<int64_t>{wasm_v128_load(max)};
}

// Float
HWY_API Vec256<float> Max(const Vec256<float> a, const Vec256<float> b) {
  return Vec256<float>{wasm_f32x4_max(a.raw, b.raw)};
}

// ------------------------------ Integer multiplication

// Unsigned
HWY_API Vec256<uint16_t> operator*(const Vec256<uint16_t> a,
                                   const Vec256<uint16_t> b) {
  return Vec256<uint16_t>{wasm_i16x8_mul(a.raw, b.raw)};
}
HWY_API Vec256<uint32_t> operator*(const Vec256<uint32_t> a,
                                   const Vec256<uint32_t> b) {
  return Vec256<uint32_t>{wasm_i32x4_mul(a.raw, b.raw)};
}

// Signed
HWY_API Vec256<int16_t> operator*(const Vec256<int16_t> a,
                                  const Vec256<int16_t> b) {
  return Vec256<int16_t>{wasm_i16x8_mul(a.raw, b.raw)};
}
HWY_API Vec256<int32_t> operator*(const Vec256<int32_t> a,
                                  const Vec256<int32_t> b) {
  return Vec256<int32_t>{wasm_i32x4_mul(a.raw, b.raw)};
}

// Returns the upper 16 bits of a * b in each lane.
HWY_API Vec256<uint16_t> MulHigh(const Vec256<uint16_t> a,
                                 const Vec256<uint16_t> b) {
  // TODO(eustas): replace, when implemented in WASM.
  const auto al = wasm_u32x4_extend_low_u16x8(a.raw);
  const auto ah = wasm_u32x4_extend_high_u16x8(a.raw);
  const auto bl = wasm_u32x4_extend_low_u16x8(b.raw);
  const auto bh = wasm_u32x4_extend_high_u16x8(b.raw);
  const auto l = wasm_i32x4_mul(al, bl);
  const auto h = wasm_i32x4_mul(ah, bh);
  // TODO(eustas): shift-right + narrow?
  return Vec256<uint16_t>{wasm_i16x8_shuffle(l, h, 1, 3, 5, 7, 9, 11, 13, 15)};
}
HWY_API Vec256<int16_t> MulHigh(const Vec256<int16_t> a,
                                const Vec256<int16_t> b) {
  // TODO(eustas): replace, when implemented in WASM.
  const auto al = wasm_i32x4_extend_low_i16x8(a.raw);
  const auto ah = wasm_i32x4_extend_high_i16x8(a.raw);
  const auto bl = wasm_i32x4_extend_low_i16x8(b.raw);
  const auto bh = wasm_i32x4_extend_high_i16x8(b.raw);
  const auto l = wasm_i32x4_mul(al, bl);
  const auto h = wasm_i32x4_mul(ah, bh);
  // TODO(eustas): shift-right + narrow?
  return Vec256<int16_t>{wasm_i16x8_shuffle(l, h, 1, 3, 5, 7, 9, 11, 13, 15)};
}

HWY_API Vec256<int16_t> MulFixedPoint15(Vec256<int16_t>, Vec256<int16_t>) {
  HWY_ASSERT(0);
}

// Multiplies even lanes (0, 2 ..) and returns the double-width result.
HWY_API Vec256<int64_t> MulEven(const Vec256<int32_t> a,
                                const Vec256<int32_t> b) {
  // TODO(eustas): replace, when implemented in WASM.
  const auto kEvenMask = wasm_i32x4_make(-1, 0, -1, 0);
  const auto ae = wasm_v128_and(a.raw, kEvenMask);
  const auto be = wasm_v128_and(b.raw, kEvenMask);
  return Vec256<int64_t>{wasm_i64x2_mul(ae, be)};
}
HWY_API Vec256<uint64_t> MulEven(const Vec256<uint32_t> a,
                                 const Vec256<uint32_t> b) {
  // TODO(eustas): replace, when implemented in WASM.
  const auto kEvenMask = wasm_i32x4_make(-1, 0, -1, 0);
  const auto ae = wasm_v128_and(a.raw, kEvenMask);
  const auto be = wasm_v128_and(b.raw, kEvenMask);
  return Vec256<uint64_t>{wasm_i64x2_mul(ae, be)};
}

// ------------------------------ Negate

template <typename T, HWY_IF_FLOAT(T)>
HWY_API Vec256<T> Neg(const Vec256<T> v) {
  return Xor(v, SignBit(Full256<T>()));
}

HWY_API Vec256<int8_t> Neg(const Vec256<int8_t> v) {
  return Vec256<int8_t>{wasm_i8x16_neg(v.raw)};
}
HWY_API Vec256<int16_t> Neg(const Vec256<int16_t> v) {
  return Vec256<int16_t>{wasm_i16x8_neg(v.raw)};
}
HWY_API Vec256<int32_t> Neg(const Vec256<int32_t> v) {
  return Vec256<int32_t>{wasm_i32x4_neg(v.raw)};
}
HWY_API Vec256<int64_t> Neg(const Vec256<int64_t> v) {
  return Vec256<int64_t>{wasm_i64x2_neg(v.raw)};
}

// ------------------------------ Floating-point mul / div

HWY_API Vec256<float> operator*(Vec256<float> a, Vec256<float> b) {
  return Vec256<float>{wasm_f32x4_mul(a.raw, b.raw)};
}

HWY_API Vec256<float> operator/(const Vec256<float> a, const Vec256<float> b) {
  return Vec256<float>{wasm_f32x4_div(a.raw, b.raw)};
}

// Approximate reciprocal
HWY_API Vec256<float> ApproximateReciprocal(const Vec256<float> v) {
  const Vec256<float> one = Vec256<float>{wasm_f32x4_splat(1.0f)};
  return one / v;
}

// Absolute value of difference.
HWY_API Vec256<float> AbsDiff(const Vec256<float> a, const Vec256<float> b) {
  return Abs(a - b);
}

// ------------------------------ Floating-point multiply-add variants

// Returns mul * x + add
HWY_API Vec256<float> MulAdd(const Vec256<float> mul, const Vec256<float> x,
                             const Vec256<float> add) {
  // TODO(eustas): replace, when implemented in WASM.
  // TODO(eustas): is it wasm_f32x4_qfma?
  return mul * x + add;
}

// Returns add - mul * x
HWY_API Vec256<float> NegMulAdd(const Vec256<float> mul, const Vec256<float> x,
                                const Vec256<float> add) {
  // TODO(eustas): replace, when implemented in WASM.
  return add - mul * x;
}

// Returns mul * x - sub
HWY_API Vec256<float> MulSub(const Vec256<float> mul, const Vec256<float> x,
                             const Vec256<float> sub) {
  // TODO(eustas): replace, when implemented in WASM.
  // TODO(eustas): is it wasm_f32x4_qfms?
  return mul * x - sub;
}

// Returns -mul * x - sub
HWY_API Vec256<float> NegMulSub(const Vec256<float> mul, const Vec256<float> x,
                                const Vec256<float> sub) {
  // TODO(eustas): replace, when implemented in WASM.
  return Neg(mul) * x - sub;
}

// ------------------------------ Floating-point square root

// Full precision square root
HWY_API Vec256<float> Sqrt(const Vec256<float> v) {
  return Vec256<float>{wasm_f32x4_sqrt(v.raw)};
}

// Approximate reciprocal square root
HWY_API Vec256<float> ApproximateReciprocalSqrt(const Vec256<float> v) {
  // TODO(eustas): find cheaper a way to calculate this.
  const Vec256<float> one = Vec256<float>{wasm_f32x4_splat(1.0f)};
  return one / Sqrt(v);
}

// ------------------------------ Floating-point rounding

// Toward nearest integer, ties to even
HWY_API Vec256<float> Round(const Vec256<float> v) {
  return Vec256<float>{wasm_f32x4_nearest(v.raw)};
}

// Toward zero, aka truncate
HWY_API Vec256<float> Trunc(const Vec256<float> v) {
  return Vec256<float>{wasm_f32x4_trunc(v.raw)};
}

// Toward +infinity, aka ceiling
HWY_API Vec256<float> Ceil(const Vec256<float> v) {
  return Vec256<float>{wasm_f32x4_ceil(v.raw)};
}

// Toward -infinity, aka floor
HWY_API Vec256<float> Floor(const Vec256<float> v) {
  return Vec256<float>{wasm_f32x4_floor(v.raw)};
}

// ------------------------------ Floating-point classification

template <typename T>
HWY_API Mask256<T> IsNaN(const Vec256<T> v) {
  return v != v;
}

template <typename T, HWY_IF_FLOAT(T)>
HWY_API Mask256<T> IsInf(const Vec256<T> v) {
  const Full256<T> d;
  const RebindToSigned<decltype(d)> di;
  const VFromD<decltype(di)> vi = BitCast(di, v);
  // 'Shift left' to clear the sign bit, check for exponent=max and mantissa=0.
  return RebindMask(d, Eq(Add(vi, vi), Set(di, hwy::MaxExponentTimes2<T>())));
}

// Returns whether normal/subnormal/zero.
template <typename T, HWY_IF_FLOAT(T)>
HWY_API Mask256<T> IsFinite(const Vec256<T> v) {
  const Full256<T> d;
  const RebindToUnsigned<decltype(d)> du;
  const RebindToSigned<decltype(d)> di;  // cheaper than unsigned comparison
  const VFromD<decltype(du)> vu = BitCast(du, v);
  // 'Shift left' to clear the sign bit, then right so we can compare with the
  // max exponent (cannot compare with MaxExponentTimes2 directly because it is
  // negative and non-negative floats would be greater).
  const VFromD<decltype(di)> exp =
      BitCast(di, ShiftRight<hwy::MantissaBits<T>() + 1>(Add(vu, vu)));
  return RebindMask(d, Lt(exp, Set(di, hwy::MaxExponentField<T>())));
}

// ================================================== COMPARE

// Comparisons fill a lane with 1-bits if the condition is true, else 0.

template <typename TFrom, typename TTo>
HWY_API Mask256<TTo> RebindMask(Full256<TTo> /*tag*/, Mask256<TFrom> m) {
  static_assert(sizeof(TFrom) == sizeof(TTo), "Must have same size");
  return Mask256<TTo>{m.raw};
}

template <typename T>
HWY_API Mask256<T> TestBit(Vec256<T> v, Vec256<T> bit) {
  static_assert(!hwy::IsFloat<T>(), "Only integer vectors supported");
  return (v & bit) == bit;
}

// ------------------------------ Equality

// Unsigned
HWY_API Mask256<uint8_t> operator==(const Vec256<uint8_t> a,
                                    const Vec256<uint8_t> b) {
  return Mask256<uint8_t>{wasm_i8x16_eq(a.raw, b.raw)};
}
HWY_API Mask256<uint16_t> operator==(const Vec256<uint16_t> a,
                                     const Vec256<uint16_t> b) {
  return Mask256<uint16_t>{wasm_i16x8_eq(a.raw, b.raw)};
}
HWY_API Mask256<uint32_t> operator==(const Vec256<uint32_t> a,
                                     const Vec256<uint32_t> b) {
  return Mask256<uint32_t>{wasm_i32x4_eq(a.raw, b.raw)};
}

// Signed
HWY_API Mask256<int8_t> operator==(const Vec256<int8_t> a,
                                   const Vec256<int8_t> b) {
  return Mask256<int8_t>{wasm_i8x16_eq(a.raw, b.raw)};
}
HWY_API Mask256<int16_t> operator==(Vec256<int16_t> a, Vec256<int16_t> b) {
  return Mask256<int16_t>{wasm_i16x8_eq(a.raw, b.raw)};
}
HWY_API Mask256<int32_t> operator==(const Vec256<int32_t> a,
                                    const Vec256<int32_t> b) {
  return Mask256<int32_t>{wasm_i32x4_eq(a.raw, b.raw)};
}

// Float
HWY_API Mask256<float> operator==(const Vec256<float> a,
                                  const Vec256<float> b) {
  return Mask256<float>{wasm_f32x4_eq(a.raw, b.raw)};
}

// ------------------------------ Inequality

// Unsigned
HWY_API Mask256<uint8_t> operator!=(const Vec256<uint8_t> a,
                                    const Vec256<uint8_t> b) {
  return Mask256<uint8_t>{wasm_i8x16_ne(a.raw, b.raw)};
}
HWY_API Mask256<uint16_t> operator!=(const Vec256<uint16_t> a,
                                     const Vec256<uint16_t> b) {
  return Mask256<uint16_t>{wasm_i16x8_ne(a.raw, b.raw)};
}
HWY_API Mask256<uint32_t> operator!=(const Vec256<uint32_t> a,
                                     const Vec256<uint32_t> b) {
  return Mask256<uint32_t>{wasm_i32x4_ne(a.raw, b.raw)};
}

// Signed
HWY_API Mask256<int8_t> operator!=(const Vec256<int8_t> a,
                                   const Vec256<int8_t> b) {
  return Mask256<int8_t>{wasm_i8x16_ne(a.raw, b.raw)};
}
HWY_API Mask256<int16_t> operator!=(Vec256<int16_t> a, Vec256<int16_t> b) {
  return Mask256<int16_t>{wasm_i16x8_ne(a.raw, b.raw)};
}
HWY_API Mask256<int32_t> operator!=(const Vec256<int32_t> a,
                                    const Vec256<int32_t> b) {
  return Mask256<int32_t>{wasm_i32x4_ne(a.raw, b.raw)};
}

// Float
HWY_API Mask256<float> operator!=(const Vec256<float> a,
                                  const Vec256<float> b) {
  return Mask256<float>{wasm_f32x4_ne(a.raw, b.raw)};
}

// ------------------------------ Strict inequality

HWY_API Mask256<int8_t> operator>(const Vec256<int8_t> a,
                                  const Vec256<int8_t> b) {
  return Mask256<int8_t>{wasm_i8x16_gt(a.raw, b.raw)};
}
HWY_API Mask256<int16_t> operator>(const Vec256<int16_t> a,
                                   const Vec256<int16_t> b) {
  return Mask256<int16_t>{wasm_i16x8_gt(a.raw, b.raw)};
}
HWY_API Mask256<int32_t> operator>(const Vec256<int32_t> a,
                                   const Vec256<int32_t> b) {
  return Mask256<int32_t>{wasm_i32x4_gt(a.raw, b.raw)};
}
HWY_API Mask256<int64_t> operator>(const Vec256<int64_t> a,
                                   const Vec256<int64_t> b) {
  const Rebind < int32_t, DFromV<decltype(a)> d32;
  const auto a32 = BitCast(d32, a);
  const auto b32 = BitCast(d32, b);
  // If the upper half is less than or greater, this is the answer.
  const auto m_gt = a32 < b32;

  // Otherwise, the lower half decides.
  const auto m_eq = a32 == b32;
  const auto lo_in_hi = wasm_i32x4_shuffle(m_gt, m_gt, 2, 2, 0, 0);
  const auto lo_gt = And(m_eq, lo_in_hi);

  const auto gt = Or(lo_gt, m_gt);
  // Copy result in upper 32 bits to lower 32 bits.
  return Mask256<int64_t>{wasm_i32x4_shuffle(gt, gt, 3, 3, 1, 1)};
}

template <typename T, HWY_IF_UNSIGNED(T)>
HWY_API Mask256<T> operator>(Vec256<T> a, Vec256<T> b) {
  const Full256<T> du;
  const RebindToSigned<decltype(du)> di;
  const Vec256<T> msb = Set(du, (LimitsMax<T>() >> 1) + 1);
  return RebindMask(du, BitCast(di, Xor(a, msb)) > BitCast(di, Xor(b, msb)));
}

HWY_API Mask256<float> operator>(const Vec256<float> a, const Vec256<float> b) {
  return Mask256<float>{wasm_f32x4_gt(a.raw, b.raw)};
}

template <typename T>
HWY_API Mask256<T> operator<(const Vec256<T> a, const Vec256<T> b) {
  return operator>(b, a);
}

// ------------------------------ Weak inequality

// Float <= >=
HWY_API Mask256<float> operator<=(const Vec256<float> a,
                                  const Vec256<float> b) {
  return Mask256<float>{wasm_f32x4_le(a.raw, b.raw)};
}
HWY_API Mask256<float> operator>=(const Vec256<float> a,
                                  const Vec256<float> b) {
  return Mask256<float>{wasm_f32x4_ge(a.raw, b.raw)};
}

// ------------------------------ FirstN (Iota, Lt)

template <typename T>
HWY_API Mask256<T> FirstN(const Full256<T> d, size_t num) {
  const RebindToSigned<decltype(d)> di;  // Signed comparisons may be cheaper.
  return RebindMask(d, Iota(di, 0) < Set(di, static_cast<MakeSigned<T>>(num)));
}

// ================================================== LOGICAL

// ------------------------------ Not

template <typename T>
HWY_API Vec256<T> Not(Vec256<T> v) {
  return Vec256<T>{wasm_v128_not(v.raw)};
}

// ------------------------------ And

template <typename T>
HWY_API Vec256<T> And(Vec256<T> a, Vec256<T> b) {
  return Vec256<T>{wasm_v128_and(a.raw, b.raw)};
}

// ------------------------------ AndNot

// Returns ~not_mask & mask.
template <typename T>
HWY_API Vec256<T> AndNot(Vec256<T> not_mask, Vec256<T> mask) {
  return Vec256<T>{wasm_v128_andnot(mask.raw, not_mask.raw)};
}

// ------------------------------ Or

template <typename T>
HWY_API Vec256<T> Or(Vec256<T> a, Vec256<T> b) {
  return Vec256<T>{wasm_v128_or(a.raw, b.raw)};
}

// ------------------------------ Xor

template <typename T>
HWY_API Vec256<T> Xor(Vec256<T> a, Vec256<T> b) {
  return Vec256<T>{wasm_v128_xor(a.raw, b.raw)};
}

// ------------------------------ Or3

template <typename T>
HWY_API Vec256<T> Or3(Vec256<T> o1, Vec256<T> o2, Vec256<T> o3) {
  return Or(o1, Or(o2, o3));
}

// ------------------------------ OrAnd

template <typename T>
HWY_API Vec256<T> OrAnd(Vec256<T> o, Vec256<T> a1, Vec256<T> a2) {
  return Or(o, And(a1, a2));
}

// ------------------------------ IfVecThenElse

template <typename T>
HWY_API Vec256<T> IfVecThenElse(Vec256<T> mask, Vec256<T> yes, Vec256<T> no) {
  return IfThenElse(MaskFromVec(mask), yes, no);
}

// ------------------------------ Operator overloads (internal-only if float)

template <typename T>
HWY_API Vec256<T> operator&(const Vec256<T> a, const Vec256<T> b) {
  return And(a, b);
}

template <typename T>
HWY_API Vec256<T> operator|(const Vec256<T> a, const Vec256<T> b) {
  return Or(a, b);
}

template <typename T>
HWY_API Vec256<T> operator^(const Vec256<T> a, const Vec256<T> b) {
  return Xor(a, b);
}

// ------------------------------ CopySign

template <typename T>
HWY_API Vec256<T> CopySign(const Vec256<T> magn, const Vec256<T> sign) {
  static_assert(IsFloat<T>(), "Only makes sense for floating-point");
  const auto msb = SignBit(Full256<T>());
  return Or(AndNot(msb, magn), And(msb, sign));
}

template <typename T>
HWY_API Vec256<T> CopySignToAbs(const Vec256<T> abs, const Vec256<T> sign) {
  static_assert(IsFloat<T>(), "Only makes sense for floating-point");
  return Or(abs, And(SignBit(Full256<T>()), sign));
}

// ------------------------------ BroadcastSignBit (compare)

template <typename T, HWY_IF_NOT_LANE_SIZE(T, 1)>
HWY_API Vec256<T> BroadcastSignBit(const Vec256<T> v) {
  return ShiftRight<sizeof(T) * 8 - 1>(v);
}
HWY_API Vec256<int8_t> BroadcastSignBit(const Vec256<int8_t> v) {
  return VecFromMask(Full256<int8_t>(), v < Zero(Full256<int8_t>()));
}

// ------------------------------ Mask

// Mask and Vec are the same (true = FF..FF).
template <typename T>
HWY_API Mask256<T> MaskFromVec(const Vec256<T> v) {
  return Mask256<T>{v.raw};
}

template <typename T>
HWY_API Vec256<T> VecFromMask(Full256<T> /* tag */, Mask256<T> v) {
  return Vec256<T>{v.raw};
}

// mask ? yes : no
template <typename T>
HWY_API Vec256<T> IfThenElse(Mask256<T> mask, Vec256<T> yes, Vec256<T> no) {
  return Vec256<T>{wasm_v128_bitselect(yes.raw, no.raw, mask.raw)};
}

// mask ? yes : 0
template <typename T>
HWY_API Vec256<T> IfThenElseZero(Mask256<T> mask, Vec256<T> yes) {
  return yes & VecFromMask(Full256<T>(), mask);
}

// mask ? 0 : no
template <typename T>
HWY_API Vec256<T> IfThenZeroElse(Mask256<T> mask, Vec256<T> no) {
  return AndNot(VecFromMask(Full256<T>(), mask), no);
}

template <typename T>
    HWY_API Vec256 <
    T IfNegativeThenElse(Vec256<T> v, Vec256<T> yes, Vec256<T> no) {
  HWY_ASSERT(0);
}

template <typename T, HWY_IF_FLOAT(T)>
HWY_API Vec256<T> ZeroIfNegative(Vec256<T> v) {
  const Full256<T> d;
  const auto zero = Zero(d);
  return IfThenElse(Mask256<T>{(v > zero).raw}, v, zero);
}

// ------------------------------ Mask logical

template <typename T>
HWY_API Mask256<T> Not(const Mask256<T> m) {
  return MaskFromVec(Not(VecFromMask(Full256<T>(), m)));
}

template <typename T>
HWY_API Mask256<T> And(const Mask256<T> a, Mask256<T> b) {
  const Full256<T> d;
  return MaskFromVec(And(VecFromMask(d, a), VecFromMask(d, b)));
}

template <typename T>
HWY_API Mask256<T> AndNot(const Mask256<T> a, Mask256<T> b) {
  const Full256<T> d;
  return MaskFromVec(AndNot(VecFromMask(d, a), VecFromMask(d, b)));
}

template <typename T>
HWY_API Mask256<T> Or(const Mask256<T> a, Mask256<T> b) {
  const Full256<T> d;
  return MaskFromVec(Or(VecFromMask(d, a), VecFromMask(d, b)));
}

template <typename T>
HWY_API Mask256<T> Xor(const Mask256<T> a, Mask256<T> b) {
  const Full256<T> d;
  return MaskFromVec(Xor(VecFromMask(d, a), VecFromMask(d, b)));
}

// ------------------------------ Shl (BroadcastSignBit, IfThenElse)

// The x86 multiply-by-Pow2() trick will not work because WASM saturates
// float->int correctly to 2^31-1 (not 2^31). Because WASM's shifts take a
// scalar count operand, per-lane shift instructions would require extract_lane
// for each lane, and hoping that shuffle is correctly mapped to a native
// instruction. Using non-vector shifts would incur a store-load forwarding
// stall when loading the result vector. We instead test bits of the shift
// count to "predicate" a shift of the entire vector by a constant.

template <typename T, HWY_IF_LANE_SIZE(T, 2)>
HWY_API Vec256<T> operator<<(Vec256<T> v, const Vec256<T> bits) {
  const Full256<T> d;
  Mask256<T> mask;
  // Need a signed type for BroadcastSignBit.
  auto test = BitCast(RebindToSigned<decltype(d)>(), bits);
  // Move the highest valid bit of the shift count into the sign bit.
  test = ShiftLeft<12>(test);

  mask = RebindMask(d, MaskFromVec(BroadcastSignBit(test)));
  test = ShiftLeft<1>(test);  // next bit (descending order)
  v = IfThenElse(mask, ShiftLeft<8>(v), v);

  mask = RebindMask(d, MaskFromVec(BroadcastSignBit(test)));
  test = ShiftLeft<1>(test);  // next bit (descending order)
  v = IfThenElse(mask, ShiftLeft<4>(v), v);

  mask = RebindMask(d, MaskFromVec(BroadcastSignBit(test)));
  test = ShiftLeft<1>(test);  // next bit (descending order)
  v = IfThenElse(mask, ShiftLeft<2>(v), v);

  mask = RebindMask(d, MaskFromVec(BroadcastSignBit(test)));
  return IfThenElse(mask, ShiftLeft<1>(v), v);
}

template <typename T, HWY_IF_LANE_SIZE(T, 4)>
HWY_API Vec256<T> operator<<(Vec256<T> v, const Vec256<T> bits) {
  const Full256<T> d;
  Mask256<T> mask;
  // Need a signed type for BroadcastSignBit.
  auto test = BitCast(RebindToSigned<decltype(d)>(), bits);
  // Move the highest valid bit of the shift count into the sign bit.
  test = ShiftLeft<27>(test);

  mask = RebindMask(d, MaskFromVec(BroadcastSignBit(test)));
  test = ShiftLeft<1>(test);  // next bit (descending order)
  v = IfThenElse(mask, ShiftLeft<16>(v), v);

  mask = RebindMask(d, MaskFromVec(BroadcastSignBit(test)));
  test = ShiftLeft<1>(test);  // next bit (descending order)
  v = IfThenElse(mask, ShiftLeft<8>(v), v);

  mask = RebindMask(d, MaskFromVec(BroadcastSignBit(test)));
  test = ShiftLeft<1>(test);  // next bit (descending order)
  v = IfThenElse(mask, ShiftLeft<4>(v), v);

  mask = RebindMask(d, MaskFromVec(BroadcastSignBit(test)));
  test = ShiftLeft<1>(test);  // next bit (descending order)
  v = IfThenElse(mask, ShiftLeft<2>(v), v);

  mask = RebindMask(d, MaskFromVec(BroadcastSignBit(test)));
  return IfThenElse(mask, ShiftLeft<1>(v), v);
}

// ------------------------------ Shr (BroadcastSignBit, IfThenElse)

template <typename T, HWY_IF_LANE_SIZE(T, 2)>
HWY_API Vec256<T> operator>>(Vec256<T> v, const Vec256<T> bits) {
  const Full256<T> d;
  Mask256<T> mask;
  // Need a signed type for BroadcastSignBit.
  auto test = BitCast(RebindToSigned<decltype(d)>(), bits);
  // Move the highest valid bit of the shift count into the sign bit.
  test = ShiftLeft<12>(test);

  mask = RebindMask(d, MaskFromVec(BroadcastSignBit(test)));
  test = ShiftLeft<1>(test);  // next bit (descending order)
  v = IfThenElse(mask, ShiftRight<8>(v), v);

  mask = RebindMask(d, MaskFromVec(BroadcastSignBit(test)));
  test = ShiftLeft<1>(test);  // next bit (descending order)
  v = IfThenElse(mask, ShiftRight<4>(v), v);

  mask = RebindMask(d, MaskFromVec(BroadcastSignBit(test)));
  test = ShiftLeft<1>(test);  // next bit (descending order)
  v = IfThenElse(mask, ShiftRight<2>(v), v);

  mask = RebindMask(d, MaskFromVec(BroadcastSignBit(test)));
  return IfThenElse(mask, ShiftRight<1>(v), v);
}

template <typename T, HWY_IF_LANE_SIZE(T, 4)>
HWY_API Vec256<T> operator>>(Vec256<T> v, const Vec256<T> bits) {
  const Full256<T> d;
  Mask256<T> mask;
  // Need a signed type for BroadcastSignBit.
  auto test = BitCast(RebindToSigned<decltype(d)>(), bits);
  // Move the highest valid bit of the shift count into the sign bit.
  test = ShiftLeft<27>(test);

  mask = RebindMask(d, MaskFromVec(BroadcastSignBit(test)));
  test = ShiftLeft<1>(test);  // next bit (descending order)
  v = IfThenElse(mask, ShiftRight<16>(v), v);

  mask = RebindMask(d, MaskFromVec(BroadcastSignBit(test)));
  test = ShiftLeft<1>(test);  // next bit (descending order)
  v = IfThenElse(mask, ShiftRight<8>(v), v);

  mask = RebindMask(d, MaskFromVec(BroadcastSignBit(test)));
  test = ShiftLeft<1>(test);  // next bit (descending order)
  v = IfThenElse(mask, ShiftRight<4>(v), v);

  mask = RebindMask(d, MaskFromVec(BroadcastSignBit(test)));
  test = ShiftLeft<1>(test);  // next bit (descending order)
  v = IfThenElse(mask, ShiftRight<2>(v), v);

  mask = RebindMask(d, MaskFromVec(BroadcastSignBit(test)));
  return IfThenElse(mask, ShiftRight<1>(v), v);
}

// ================================================== MEMORY

// ------------------------------ Load

template <typename T>
HWY_API Vec256<T> Load(Full256<T> /* tag */, const T* HWY_RESTRICT aligned) {
  return Vec256<T>{wasm_v128_load(aligned)};
}

template <typename T>
HWY_API Vec256<T> MaskedLoad(Mask256<T> m, Full256<T> d,
                             const T* HWY_RESTRICT aligned) {
  return IfThenElseZero(m, Load(d, aligned));
}

// LoadU == Load.
template <typename T>
HWY_API Vec256<T> LoadU(Full256<T> d, const T* HWY_RESTRICT p) {
  return Load(d, p);
}

// 128-bit SIMD => nothing to duplicate, same as an unaligned load.
template <typename T>
HWY_API Vec256<T> LoadDup128(Full256<T> d, const T* HWY_RESTRICT p) {
  return Load(d, p);
}

// ------------------------------ Store

template <typename T>
HWY_API void Store(Vec256<T> v, Full256<T> /* tag */, T* HWY_RESTRICT aligned) {
  wasm_v128_store(aligned, v.raw);
}

// StoreU == Store.
template <typename T>
HWY_API void StoreU(Vec256<T> v, Full256<T> d, T* HWY_RESTRICT p) {
  Store(v, d, p);
}

template <typename T>
HWY_API void BlendedStore(Vec256<T> v, Mask256<T> m, Full256<T> d,
                          T* HWY_RESTRICT p) {
  StoreU(IfThenElse(m, v, LoadU(d, p)), d, p);
}

// ------------------------------ Non-temporal stores

// Same as aligned stores on non-x86.

template <typename T>
HWY_API void Stream(Vec256<T> v, Full256<T> /* tag */,
                    T* HWY_RESTRICT aligned) {
  wasm_v128_store(aligned, v.raw);
}

// ------------------------------ Scatter (Store)

template <typename T, typename Offset>
HWY_API void ScatterOffset(Vec256<T> v, Full256<T> d, T* HWY_RESTRICT base,
                           const Vec256<Offset> offset) {
  static_assert(sizeof(T) == sizeof(Offset), "Must match for portability");

  alignas(32) T lanes[32 / sizeof(T)];
  Store(v, d, lanes);

  alignas(32) Offset offset_lanes[32 / sizeof(T)];
  Store(offset, Full256<Offset>(), offset_lanes);

  uint8_t* base_bytes = reinterpret_cast<uint8_t*>(base);
  for (size_t i = 0; i < N; ++i) {
    CopyBytes<sizeof(T)>(&lanes[i], base_bytes + offset_lanes[i]);
  }
}

template <typename T, typename Index>
HWY_API void ScatterIndex(Vec256<T> v, Full256<T> d, T* HWY_RESTRICT base,
                          const Vec256<Index> index) {
  static_assert(sizeof(T) == sizeof(Index), "Must match for portability");

  alignas(32) T lanes[32 / sizeof(T)];
  Store(v, d, lanes);

  alignas(32) Index index_lanes[32 / sizeof(T)];
  Store(index, Full256<Index>(), index_lanes);

  for (size_t i = 0; i < N; ++i) {
    base[index_lanes[i]] = lanes[i];
  }
}

// ------------------------------ Gather (Load/Store)

template <typename T, typename Offset>
HWY_API Vec256<T> GatherOffset(const Full256<T> d, const T* HWY_RESTRICT base,
                               const Vec256<Offset> offset) {
  static_assert(sizeof(T) == sizeof(Offset), "Must match for portability");

  alignas(32) Offset offset_lanes[32 / sizeof(T)];
  Store(offset, Full256<Offset>(), offset_lanes);

  alignas(32) T lanes[32 / sizeof(T)];
  const uint8_t* base_bytes = reinterpret_cast<const uint8_t*>(base);
  for (size_t i = 0; i < N; ++i) {
    CopyBytes<sizeof(T)>(base_bytes + offset_lanes[i], &lanes[i]);
  }
  return Load(d, lanes);
}

template <typename T, typename Index>
HWY_API Vec256<T> GatherIndex(const Full256<T> d, const T* HWY_RESTRICT base,
                              const Vec256<Index> index) {
  static_assert(sizeof(T) == sizeof(Index), "Must match for portability");

  alignas(32) Index index_lanes[32 / sizeof(T)];
  Store(index, Full256<Index>(), index_lanes);

  alignas(32) T lanes[32 / sizeof(T)];
  for (size_t i = 0; i < N; ++i) {
    lanes[i] = base[index_lanes[i]];
  }
  return Load(d, lanes);
}

// ================================================== SWIZZLE

// ------------------------------ ExtractLane
template <typename T, size_t N>
HWY_API T ExtractLane(const Vec128<T, N> v, size_t i) {
  HWY_ASSERT(0);
}

// ------------------------------ InsertLane
template <typename T, size_t N>
HWY_API Vec128<T, N> InsertLane(const Vec128<T, N> v, size_t i, T t) {
  HWY_ASSERT(0);
}

// ------------------------------ GetLane
// Gets the single value stored in a vector/part.
HWY_API uint8_t GetLane(const Vec256<uint8_t> v) {
  return wasm_i8x16_extract_lane(v.raw, 0);
}
HWY_API int8_t GetLane(const Vec256<int8_t> v) {
  return wasm_i8x16_extract_lane(v.raw, 0);
}
HWY_API uint16_t GetLane(const Vec256<uint16_t> v) {
  return wasm_i16x8_extract_lane(v.raw, 0);
}
HWY_API int16_t GetLane(const Vec256<int16_t> v) {
  return wasm_i16x8_extract_lane(v.raw, 0);
}
HWY_API uint32_t GetLane(const Vec256<uint32_t> v) {
  return wasm_i32x4_extract_lane(v.raw, 0);
}
HWY_API int32_t GetLane(const Vec256<int32_t> v) {
  return wasm_i32x4_extract_lane(v.raw, 0);
}
HWY_API uint64_t GetLane(const Vec256<uint64_t> v) {
  return wasm_i64x2_extract_lane(v.raw, 0);
}
HWY_API int64_t GetLane(const Vec256<int64_t> v) {
  return wasm_i64x2_extract_lane(v.raw, 0);
}

HWY_API float GetLane(const Vec256<float> v) {
  return wasm_f32x4_extract_lane(v.raw, 0);
}

// ------------------------------ LowerHalf

template <typename T>
HWY_API Vec128<T> LowerHalf(Full128<T> /* tag */, Vec256<T> v) {
  return Vec128<T>{v.raw};
}

template <typename T>
HWY_API Vec128<T> LowerHalf(Vec256<T> v) {
  return LowerHalf(Full128<T>(), v);
}

// ------------------------------ ShiftLeftBytes

// 0x01..0F, kBytes = 1 => 0x02..0F00
template <int kBytes, typename T>
HWY_API Vec256<T> ShiftLeftBytes(Full256<T> /* tag */, Vec256<T> v) {
  static_assert(0 <= kBytes && kBytes <= 16, "Invalid kBytes");
  const __i8x16 zero = wasm_i8x16_splat(0);
  switch (kBytes) {
    case 0:
      return v;

    case 1:
      return Vec256<T>{wasm_i8x16_shuffle(v.raw, zero, 16, 0, 1, 2, 3, 4, 5, 6,
                                          7, 8, 9, 10, 11, 12, 13, 14)};

    case 2:
      return Vec256<T>{wasm_i8x16_shuffle(v.raw, zero, 16, 16, 0, 1, 2, 3, 4, 5,
                                          6, 7, 8, 9, 10, 11, 12, 13)};

    case 3:
      return Vec256<T>{wasm_i8x16_shuffle(v.raw, zero, 16, 16, 16, 0, 1, 2, 3,
                                          4, 5, 6, 7, 8, 9, 10, 11, 12)};

    case 4:
      return Vec256<T>{wasm_i8x16_shuffle(v.raw, zero, 16, 16, 16, 16, 0, 1, 2,
                                          3, 4, 5, 6, 7, 8, 9, 10, 11)};

    case 5:
      return Vec256<T>{wasm_i8x16_shuffle(v.raw, zero, 16, 16, 16, 16, 16, 0, 1,
                                          2, 3, 4, 5, 6, 7, 8, 9, 10)};

    case 6:
      return Vec256<T>{wasm_i8x16_shuffle(v.raw, zero, 16, 16, 16, 16, 16, 16,
                                          0, 1, 2, 3, 4, 5, 6, 7, 8, 9)};

    case 7:
      return Vec256<T>{wasm_i8x16_shuffle(v.raw, zero, 16, 16, 16, 16, 16, 16,
                                          16, 0, 1, 2, 3, 4, 5, 6, 7, 8)};

    case 8:
      return Vec256<T>{wasm_i8x16_shuffle(v.raw, zero, 16, 16, 16, 16, 16, 16,
                                          16, 16, 0, 1, 2, 3, 4, 5, 6, 7)};

    case 9:
      return Vec256<T>{wasm_i8x16_shuffle(v.raw, zero, 16, 16, 16, 16, 16, 16,
                                          16, 16, 16, 0, 1, 2, 3, 4, 5, 6)};

    case 10:
      return Vec256<T>{wasm_i8x16_shuffle(v.raw, zero, 16, 16, 16, 16, 16, 16,
                                          16, 16, 16, 16, 0, 1, 2, 3, 4, 5)};

    case 11:
      return Vec256<T>{wasm_i8x16_shuffle(v.raw, zero, 16, 16, 16, 16, 16, 16,
                                          16, 16, 16, 16, 16, 0, 1, 2, 3, 4)};

    case 12:
      return Vec256<T>{wasm_i8x16_shuffle(v.raw, zero, 16, 16, 16, 16, 16, 16,
                                          16, 16, 16, 16, 16, 16, 0, 1, 2, 3)};

    case 13:
      return Vec256<T>{wasm_i8x16_shuffle(v.raw, zero, 16, 16, 16, 16, 16, 16,
                                          16, 16, 16, 16, 16, 16, 16, 0, 1, 2)};

    case 14:
      return Vec256<T>{wasm_i8x16_shuffle(v.raw, zero, 16, 16, 16, 16, 16, 16,
                                          16, 16, 16, 16, 16, 16, 16, 16, 0,
                                          1)};

    case 15:
      return Vec256<T>{wasm_i8x16_shuffle(v.raw, zero, 16, 16, 16, 16, 16, 16,
                                          16, 16, 16, 16, 16, 16, 16, 16, 16,
                                          0)};
  }
  return Vec256<T>{zero};
}

template <int kBytes, typename T>
HWY_API Vec256<T> ShiftLeftBytes(Vec256<T> v) {
  return ShiftLeftBytes<kBytes>(Full256<T>(), v);
}

// ------------------------------ ShiftLeftLanes

template <int kLanes, typename T>
HWY_API Vec256<T> ShiftLeftLanes(Full256<T> d, const Vec256<T> v) {
  const Repartition<uint8_t, decltype(d)> d8;
  return BitCast(d, ShiftLeftBytes<kLanes * sizeof(T)>(BitCast(d8, v)));
}

template <int kLanes, typename T>
HWY_API Vec256<T> ShiftLeftLanes(const Vec256<T> v) {
  return ShiftLeftLanes<kLanes>(Full256<T>(), v);
}

// ------------------------------ ShiftRightBytes
namespace detail {

// Helper function allows zeroing invalid lanes in caller.
template <int kBytes, typename T>
HWY_API __i8x16 ShrBytes(const Vec256<T> v) {
  static_assert(0 <= kBytes && kBytes <= 16, "Invalid kBytes");
  const __i8x16 zero = wasm_i8x16_splat(0);

  switch (kBytes) {
    case 0:
      return v.raw;

    case 1:
      return wasm_i8x16_shuffle(v.raw, zero, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11,
                                12, 13, 14, 15, 16);

    case 2:
      return wasm_i8x16_shuffle(v.raw, zero, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12,
                                13, 14, 15, 16, 16);

    case 3:
      return wasm_i8x16_shuffle(v.raw, zero, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12,
                                13, 14, 15, 16, 16, 16);

    case 4:
      return wasm_i8x16_shuffle(v.raw, zero, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13,
                                14, 15, 16, 16, 16, 16);

    case 5:
      return wasm_i8x16_shuffle(v.raw, zero, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14,
                                15, 16, 16, 16, 16, 16);

    case 6:
      return wasm_i8x16_shuffle(v.raw, zero, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15,
                                16, 16, 16, 16, 16, 16);

    case 7:
      return wasm_i8x16_shuffle(v.raw, zero, 7, 8, 9, 10, 11, 12, 13, 14, 15,
                                16, 16, 16, 16, 16, 16, 16);

    case 8:
      return wasm_i8x16_shuffle(v.raw, zero, 8, 9, 10, 11, 12, 13, 14, 15, 16,
                                16, 16, 16, 16, 16, 16, 16);

    case 9:
      return wasm_i8x16_shuffle(v.raw, zero, 9, 10, 11, 12, 13, 14, 15, 16, 16,
                                16, 16, 16, 16, 16, 16, 16);

    case 10:
      return wasm_i8x16_shuffle(v.raw, zero, 10, 11, 12, 13, 14, 15, 16, 16, 16,
                                16, 16, 16, 16, 16, 16, 16);

    case 11:
      return wasm_i8x16_shuffle(v.raw, zero, 11, 12, 13, 14, 15, 16, 16, 16, 16,
                                16, 16, 16, 16, 16, 16, 16);

    case 12:
      return wasm_i8x16_shuffle(v.raw, zero, 12, 13, 14, 15, 16, 16, 16, 16, 16,
                                16, 16, 16, 16, 16, 16, 16);

    case 13:
      return wasm_i8x16_shuffle(v.raw, zero, 13, 14, 15, 16, 16, 16, 16, 16, 16,
                                16, 16, 16, 16, 16, 16, 16);

    case 14:
      return wasm_i8x16_shuffle(v.raw, zero, 14, 15, 16, 16, 16, 16, 16, 16, 16,
                                16, 16, 16, 16, 16, 16, 16);

    case 15:
      return wasm_i8x16_shuffle(v.raw, zero, 15, 16, 16, 16, 16, 16, 16, 16, 16,
                                16, 16, 16, 16, 16, 16, 16);
    case 16:
      return zero;
  }
}

}  // namespace detail

// 0x01..0F, kBytes = 1 => 0x0001..0E
template <int kBytes, typename T>
HWY_API Vec256<T> ShiftRightBytes(Full256<T> /* tag */, Vec256<T> v) {
  return Vec256<T>{detail::ShrBytes<kBytes>(v)};
}

// ------------------------------ ShiftRightLanes
template <int kLanes, typename T>
HWY_API Vec256<T> ShiftRightLanes(Full256<T> d, const Vec256<T> v) {
  const Repartition<uint8_t, decltype(d)> d8;
  return BitCast(d, ShiftRightBytes<kLanes * sizeof(T)>(BitCast(d8, v)));
}

// ------------------------------ UpperHalf (ShiftRightBytes)

// Full input: copy hi into lo (smaller instruction encoding than shifts).
template <typename T>
HWY_API Vec128<T, 8 / sizeof(T)> UpperHalf(Full128<T> /* tag */,
                                           const Vec256<T> v) {
  return Vec128<T, 8 / sizeof(T)>{wasm_i32x4_shuffle(v.raw, v.raw, 2, 3, 2, 3)};
}
HWY_API Vec128<float, 2> UpperHalf(Full128<float> /* tag */,
                                   const Vec128<float> v) {
  return Vec128<float, 2>{wasm_i32x4_shuffle(v.raw, v.raw, 2, 3, 2, 3)};
}

// ------------------------------ CombineShiftRightBytes

template <int kBytes, typename T, class V = Vec256<T>>
HWY_API V CombineShiftRightBytes(Full256<T> /* tag */, V hi, V lo) {
  static_assert(0 <= kBytes && kBytes <= 16, "Invalid kBytes");
  switch (kBytes) {
    case 0:
      return lo;

    case 1:
      return V{wasm_i8x16_shuffle(lo.raw, hi.raw, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10,
                                  11, 12, 13, 14, 15, 16)};

    case 2:
      return V{wasm_i8x16_shuffle(lo.raw, hi.raw, 2, 3, 4, 5, 6, 7, 8, 9, 10,
                                  11, 12, 13, 14, 15, 16, 17)};

    case 3:
      return V{wasm_i8x16_shuffle(lo.raw, hi.raw, 3, 4, 5, 6, 7, 8, 9, 10, 11,
                                  12, 13, 14, 15, 16, 17, 18)};

    case 4:
      return V{wasm_i8x16_shuffle(lo.raw, hi.raw, 4, 5, 6, 7, 8, 9, 10, 11, 12,
                                  13, 14, 15, 16, 17, 18, 19)};

    case 5:
      return V{wasm_i8x16_shuffle(lo.raw, hi.raw, 5, 6, 7, 8, 9, 10, 11, 12, 13,
                                  14, 15, 16, 17, 18, 19, 20)};

    case 6:
      return V{wasm_i8x16_shuffle(lo.raw, hi.raw, 6, 7, 8, 9, 10, 11, 12, 13,
                                  14, 15, 16, 17, 18, 19, 20, 21)};

    case 7:
      return V{wasm_i8x16_shuffle(lo.raw, hi.raw, 7, 8, 9, 10, 11, 12, 13, 14,
                                  15, 16, 17, 18, 19, 20, 21, 22)};

    case 8:
      return V{wasm_i8x16_shuffle(lo.raw, hi.raw, 8, 9, 10, 11, 12, 13, 14, 15,
                                  16, 17, 18, 19, 20, 21, 22, 23)};

    case 9:
      return V{wasm_i8x16_shuffle(lo.raw, hi.raw, 9, 10, 11, 12, 13, 14, 15, 16,
                                  17, 18, 19, 20, 21, 22, 23, 24)};

    case 10:
      return V{wasm_i8x16_shuffle(lo.raw, hi.raw, 10, 11, 12, 13, 14, 15, 16,
                                  17, 18, 19, 20, 21, 22, 23, 24, 25)};

    case 11:
      return V{wasm_i8x16_shuffle(lo.raw, hi.raw, 11, 12, 13, 14, 15, 16, 17,
                                  18, 19, 20, 21, 22, 23, 24, 25, 26)};

    case 12:
      return V{wasm_i8x16_shuffle(lo.raw, hi.raw, 12, 13, 14, 15, 16, 17, 18,
                                  19, 20, 21, 22, 23, 24, 25, 26, 27)};

    case 13:
      return V{wasm_i8x16_shuffle(lo.raw, hi.raw, 13, 14, 15, 16, 17, 18, 19,
                                  20, 21, 22, 23, 24, 25, 26, 27, 28)};

    case 14:
      return V{wasm_i8x16_shuffle(lo.raw, hi.raw, 14, 15, 16, 17, 18, 19, 20,
                                  21, 22, 23, 24, 25, 26, 27, 28, 29)};

    case 15:
      return V{wasm_i8x16_shuffle(lo.raw, hi.raw, 15, 16, 17, 18, 19, 20, 21,
                                  22, 23, 24, 25, 26, 27, 28, 29, 30)};
  }
  return hi;
}

// ------------------------------ Broadcast/splat any lane

// Unsigned
template <int kLane>
HWY_API Vec256<uint16_t> Broadcast(const Vec256<uint16_t> v) {
  static_assert(0 <= kLane && kLane < N, "Invalid lane");
  return Vec256<uint16_t>{wasm_i16x8_shuffle(
      v.raw, v.raw, kLane, kLane, kLane, kLane, kLane, kLane, kLane, kLane)};
}
template <int kLane>
HWY_API Vec256<uint32_t> Broadcast(const Vec256<uint32_t> v) {
  static_assert(0 <= kLane && kLane < N, "Invalid lane");
  return Vec256<uint32_t>{
      wasm_i32x4_shuffle(v.raw, v.raw, kLane, kLane, kLane, kLane)};
}

// Signed
template <int kLane>
HWY_API Vec256<int16_t> Broadcast(const Vec256<int16_t> v) {
  static_assert(0 <= kLane && kLane < N, "Invalid lane");
  return Vec256<int16_t>{wasm_i16x8_shuffle(v.raw, v.raw, kLane, kLane, kLane,
                                            kLane, kLane, kLane, kLane, kLane)};
}
template <int kLane>
HWY_API Vec256<int32_t> Broadcast(const Vec256<int32_t> v) {
  static_assert(0 <= kLane && kLane < N, "Invalid lane");
  return Vec256<int32_t>{
      wasm_i32x4_shuffle(v.raw, v.raw, kLane, kLane, kLane, kLane)};
}

// Float
template <int kLane>
HWY_API Vec256<float> Broadcast(const Vec256<float> v) {
  static_assert(0 <= kLane && kLane < N, "Invalid lane");
  return Vec256<float>{
      wasm_i32x4_shuffle(v.raw, v.raw, kLane, kLane, kLane, kLane)};
}

// ------------------------------ TableLookupBytes

// Returns vector of bytes[from[i]]. "from" is also interpreted as bytes, i.e.
// lane indices in [0, 16).
template <typename T, typename TI>
HWY_API Vec256<TI> TableLookupBytes(const Vec256<T> bytes,
                                    const Vec256<TI> from) {
// Not yet available in all engines, see
// https://github.com/WebAssembly/simd/blob/bdcc304b2d379f4601c2c44ea9b44ed9484fde7e/proposals/simd/ImplementationStatus.md
// V8 implementation of this had a bug, fixed on 2021-04-03:
// https://chromium-review.googlesource.com/c/v8/v8/+/2822951
#if 0
  return Vec256<TI>{wasm_i8x16_swizzle(bytes.raw, from.raw)};
#else
  alignas(32) uint8_t control[16];
  alignas(32) uint8_t input[16];
  alignas(32) uint8_t output[16];
  wasm_v128_store(control, from.raw);
  wasm_v128_store(input, bytes.raw);
  for (size_t i = 0; i < 16; ++i) {
    output[i] = control[i] < 16 ? input[control[i]] : 0;
  }
  return Vec256<TI>{wasm_v128_load(output)};
#endif
}

template <typename T, typename TI>
HWY_API Vec256<TI> TableLookupBytesOr0(const Vec256<T> bytes,
                                       const Vec256<TI> from) {
  const Full256<TI> d;
  // Mask size must match vector type, so cast everything to this type.
  Repartition<int8_t, decltype(d)> di8;
  Repartition<int8_t, Full256<T>> d_bytes8;
  const auto msb = BitCast(di8, from) < Zero(di8);
  const auto lookup =
      TableLookupBytes(BitCast(d_bytes8, bytes), BitCast(di8, from));
  return BitCast(d, IfThenZeroElse(msb, lookup));
}

// ------------------------------ Hard-coded shuffles

// Notation: let Vec128<int32_t> have lanes 3,2,1,0 (0 is least-significant).
// Shuffle0321 rotates one lane to the right (the previous least-significant
// lane is now most-significant). These could also be implemented via
// CombineShiftRightBytes but the shuffle_abcd notation is more convenient.

// Swap 32-bit halves in 64-bit halves.
HWY_API Vec128<uint32_t> Shuffle2301(const Vec128<uint32_t> v) {
  return Vec128<uint32_t>{wasm_i32x4_shuffle(v.raw, v.raw, 1, 0, 3, 2)};
}
HWY_API Vec128<int32_t> Shuffle2301(const Vec128<int32_t> v) {
  return Vec128<int32_t>{wasm_i32x4_shuffle(v.raw, v.raw, 1, 0, 3, 2)};
}
HWY_API Vec128<float> Shuffle2301(const Vec128<float> v) {
  return Vec128<float>{wasm_i32x4_shuffle(v.raw, v.raw, 1, 0, 3, 2)};
}

// Swap 64-bit halves
HWY_API Vec128<uint32_t> Shuffle1032(const Vec128<uint32_t> v) {
  return Vec128<uint32_t>{wasm_i64x2_shuffle(v.raw, v.raw, 1, 0)};
}
HWY_API Vec128<int32_t> Shuffle1032(const Vec128<int32_t> v) {
  return Vec128<int32_t>{wasm_i64x2_shuffle(v.raw, v.raw, 1, 0)};
}
HWY_API Vec128<float> Shuffle1032(const Vec128<float> v) {
  return Vec128<float>{wasm_i64x2_shuffle(v.raw, v.raw, 1, 0)};
}

// Rotate right 32 bits
HWY_API Vec128<uint32_t> Shuffle0321(const Vec128<uint32_t> v) {
  return Vec128<uint32_t>{wasm_i32x4_shuffle(v.raw, v.raw, 1, 2, 3, 0)};
}
HWY_API Vec128<int32_t> Shuffle0321(const Vec128<int32_t> v) {
  return Vec128<int32_t>{wasm_i32x4_shuffle(v.raw, v.raw, 1, 2, 3, 0)};
}
HWY_API Vec128<float> Shuffle0321(const Vec128<float> v) {
  return Vec128<float>{wasm_i32x4_shuffle(v.raw, v.raw, 1, 2, 3, 0)};
}
// Rotate left 32 bits
HWY_API Vec128<uint32_t> Shuffle2103(const Vec128<uint32_t> v) {
  return Vec128<uint32_t>{wasm_i32x4_shuffle(v.raw, v.raw, 3, 0, 1, 2)};
}
HWY_API Vec128<int32_t> Shuffle2103(const Vec128<int32_t> v) {
  return Vec128<int32_t>{wasm_i32x4_shuffle(v.raw, v.raw, 3, 0, 1, 2)};
}
HWY_API Vec128<float> Shuffle2103(const Vec128<float> v) {
  return Vec128<float>{wasm_i32x4_shuffle(v.raw, v.raw, 3, 0, 1, 2)};
}

// Reverse
HWY_API Vec128<uint32_t> Shuffle0123(const Vec128<uint32_t> v) {
  return Vec128<uint32_t>{wasm_i32x4_shuffle(v.raw, v.raw, 3, 2, 1, 0)};
}
HWY_API Vec128<int32_t> Shuffle0123(const Vec128<int32_t> v) {
  return Vec128<int32_t>{wasm_i32x4_shuffle(v.raw, v.raw, 3, 2, 1, 0)};
}
HWY_API Vec128<float> Shuffle0123(const Vec128<float> v) {
  return Vec128<float>{wasm_i32x4_shuffle(v.raw, v.raw, 3, 2, 1, 0)};
}

// ------------------------------ TableLookupLanes

// Returned by SetTableIndices for use by TableLookupLanes.
template <typename T>
struct Indices256 {
  __v128_u raw;
};

template <typename T, typename TI>
HWY_API Indices256<T> IndicesFromVec(Full256<T> d, Vec256<TI> vec) {
  static_assert(sizeof(T) == sizeof(TI), "Index size must match lane");
  return Indices256<T>{};
}

template <typename T, typename TI>
HWY_API Indices256<T> SetTableIndices(Full256<T> d, const TI* idx) {
  const Rebind<TI, decltype(d)> di;
  return IndicesFromVec(d, LoadU(di, idx));
}

template <typename T>
HWY_API Vec256<T> TableLookupLanes(Vec256<T> v, Indices256<T> idx) {
  using TI = MakeSigned<T>;
  const Full256<T> d;
  const Full256<TI> di;
  return BitCast(d, TableLookupBytes(BitCast(di, v), Vec256<TI>{idx.raw}));
}

// ------------------------------ Reverse (Shuffle0123, Shuffle2301, Shuffle01)

template <typename T, HWY_IF_LANE_SIZE(T, 8)>
HWY_API Vec256<T> Reverse(Full256<T> /* tag */, const Vec256<T> v) {
  return Shuffle01(v);
}

// Four lanes: shuffle
template <typename T, HWY_IF_LANE_SIZE(T, 4)>
HWY_API Vec256<T> Reverse(Full256<T> /* tag */, const Vec256<T> v) {
  return Shuffle0123(v);
}

// 16-bit
template <typename T, HWY_IF_LANE_SIZE(T, 2)>
HWY_API Vec256<T> Reverse(Full256<T> d, const Vec256<T> v) {
  const RepartitionToWide<RebindToUnsigned<decltype(d)>> du32;
  return BitCast(d, RotateRight<16>(Reverse(du32, BitCast(du32, v))));
}

// ------------------------------ Reverse2

template <typename T>
HWY_API Vec256<T> Reverse2(Full256<T> d, const Vec256<T> v) {
  HWY_ASSERT(0);
}

// ------------------------------ Reverse4

template <typename T>
HWY_API Vec256<T> Reverse4(Full256<T> d, const Vec256<T> v) {
  HWY_ASSERT(0);
}

// ------------------------------ Reverse8

template <typename T>
HWY_API Vec256<T> Reverse8(Full256<T> d, const Vec256<T> v) {
  HWY_ASSERT(0);
}

// ------------------------------ InterleaveLower

HWY_API Vec256<uint8_t> InterleaveLower(Vec256<uint8_t> a, Vec256<uint8_t> b) {
  return Vec256<uint8_t>{wasm_i8x16_shuffle(a.raw, b.raw, 0, 16, 1, 17, 2, 18,
                                            3, 19, 4, 20, 5, 21, 6, 22, 7, 23)};
}
HWY_API Vec256<uint16_t> InterleaveLower(Vec256<uint16_t> a,
                                         Vec256<uint16_t> b) {
  return Vec256<uint16_t>{
      wasm_i16x8_shuffle(a.raw, b.raw, 0, 8, 1, 9, 2, 10, 3, 11)};
}
HWY_API Vec256<uint32_t> InterleaveLower(Vec256<uint32_t> a,
                                         Vec256<uint32_t> b) {
  return Vec256<uint32_t>{wasm_i32x4_shuffle(a.raw, b.raw, 0, 4, 1, 5)};
}
HWY_API Vec256<uint64_t> InterleaveLower(Vec256<uint64_t> a,
                                         Vec256<uint64_t> b) {
  return Vec256<uint64_t>{wasm_i64x2_shuffle(a.raw, b.raw, 0, 2)};
}

HWY_API Vec256<int8_t> InterleaveLower(Vec256<int8_t> a, Vec256<int8_t> b) {
  return Vec256<int8_t>{wasm_i8x16_shuffle(a.raw, b.raw, 0, 16, 1, 17, 2, 18, 3,
                                           19, 4, 20, 5, 21, 6, 22, 7, 23)};
}
HWY_API Vec256<int16_t> InterleaveLower(Vec256<int16_t> a, Vec256<int16_t> b) {
  return Vec256<int16_t>{
      wasm_i16x8_shuffle(a.raw, b.raw, 0, 8, 1, 9, 2, 10, 3, 11)};
}
HWY_API Vec256<int32_t> InterleaveLower(Vec256<int32_t> a, Vec256<int32_t> b) {
  return Vec256<int32_t>{wasm_i32x4_shuffle(a.raw, b.raw, 0, 4, 1, 5)};
}
HWY_API Vec256<int64_t> InterleaveLower(Vec256<int64_t> a, Vec256<int64_t> b) {
  return Vec256<int64_t>{wasm_i64x2_shuffle(a.raw, b.raw, 0, 2)};
}

HWY_API Vec256<float> InterleaveLower(Vec256<float> a, Vec256<float> b) {
  return Vec256<float>{wasm_i32x4_shuffle(a.raw, b.raw, 0, 4, 1, 5)};
}

// Additional overload for the optional tag.
template <typename T, class V = Vec256<T>>
HWY_API V InterleaveLower(Full256<T> /* tag */, V a, V b) {
  return InterleaveLower(a, b);
}

// ------------------------------ InterleaveUpper (UpperHalf)

// All functions inside detail lack the required D parameter.
namespace detail {

HWY_API Vec256<uint8_t> InterleaveUpper(Vec256<uint8_t> a, Vec256<uint8_t> b) {
  return Vec256<uint8_t>{wasm_i8x16_shuffle(a.raw, b.raw, 8, 24, 9, 25, 10, 26,
                                            11, 27, 12, 28, 13, 29, 14, 30, 15,
                                            31)};
}
HWY_API Vec256<uint16_t> InterleaveUpper(Vec256<uint16_t> a,
                                         Vec256<uint16_t> b) {
  return Vec256<uint16_t>{
      wasm_i16x8_shuffle(a.raw, b.raw, 4, 12, 5, 13, 6, 14, 7, 15)};
}
HWY_API Vec256<uint32_t> InterleaveUpper(Vec256<uint32_t> a,
                                         Vec256<uint32_t> b) {
  return Vec256<uint32_t>{wasm_i32x4_shuffle(a.raw, b.raw, 2, 6, 3, 7)};
}
HWY_API Vec256<uint64_t> InterleaveUpper(Vec256<uint64_t> a,
                                         Vec256<uint64_t> b) {
  return Vec256<uint64_t>{wasm_i64x2_shuffle(a.raw, b.raw, 1, 3)};
}

HWY_API Vec256<int8_t> InterleaveUpper(Vec256<int8_t> a, Vec256<int8_t> b) {
  return Vec256<int8_t>{wasm_i8x16_shuffle(a.raw, b.raw, 8, 24, 9, 25, 10, 26,
                                           11, 27, 12, 28, 13, 29, 14, 30, 15,
                                           31)};
}
HWY_API Vec256<int16_t> InterleaveUpper(Vec256<int16_t> a, Vec256<int16_t> b) {
  return Vec256<int16_t>{
      wasm_i16x8_shuffle(a.raw, b.raw, 4, 12, 5, 13, 6, 14, 7, 15)};
}
HWY_API Vec256<int32_t> InterleaveUpper(Vec256<int32_t> a, Vec256<int32_t> b) {
  return Vec256<int32_t>{wasm_i32x4_shuffle(a.raw, b.raw, 2, 6, 3, 7)};
}
HWY_API Vec256<int64_t> InterleaveUpper(Vec256<int64_t> a, Vec256<int64_t> b) {
  return Vec256<int64_t>{wasm_i64x2_shuffle(a.raw, b.raw, 1, 3)};
}

HWY_API Vec256<float> InterleaveUpper(Vec256<float> a, Vec256<float> b) {
  return Vec256<float>{wasm_i32x4_shuffle(a.raw, b.raw, 2, 6, 3, 7)};
}

}  // namespace detail

template <typename T, class V = Vec256<T>>
HWY_API V InterleaveUpper(Full256<T> /* tag */, V a, V b) {
  return detail::InterleaveUpper(a, b);
}

// ------------------------------ ZipLower/ZipUpper (InterleaveLower)

// Same as Interleave*, except that the return lanes are double-width integers;
// this is necessary because the single-lane scalar cannot return two values.
template <typename T, class DW = RepartitionToWide<Full256<T>>>
HWY_API VFromD<DW> ZipLower(Vec256<T> a, Vec256<T> b) {
  return BitCast(DW(), InterleaveLower(a, b));
}
template <typename T, class D = Full256<T>, class DW = RepartitionToWide<D>>
HWY_API VFromD<DW> ZipLower(DW dw, Vec256<T> a, Vec256<T> b) {
  return BitCast(dw, InterleaveLower(D(), a, b));
}

template <typename T, class D = Full256<T>, class DW = RepartitionToWide<D>>
HWY_API VFromD<DW> ZipUpper(DW dw, Vec256<T> a, Vec256<T> b) {
  return BitCast(dw, InterleaveUpper(D(), a, b));
}

// ================================================== COMBINE

// ------------------------------ Combine (InterleaveLower)

// N = N/2 + N/2 (upper half undefined)
template <typename T>
HWY_API Vec256<T> Combine(Full256<T> d, Vec128<T> hi_half, Vec128<T> lo_half) {
  const Half<decltype(d)> d2;
  const RebindToUnsigned<decltype(d2)> du2;
  // Treat half-width input as one lane, and expand to two lanes.
  using VU = Vec128<UnsignedFromSize<N * sizeof(T) / 2>, 2>;
  const VU lo{BitCast(du2, lo_half).raw};
  const VU hi{BitCast(du2, hi_half).raw};
  return BitCast(d, InterleaveLower(lo, hi));
}

// ------------------------------ ZeroExtendVector (Combine, IfThenElseZero)

template <typename T>
HWY_API Vec256<T> ZeroExtendVector(Full256<T> d, Vec128<T> lo) {
  return IfThenElseZero(FirstN(d, 16 / sizeof(T)), Vec256<T>{lo.raw});
}

// ------------------------------ ConcatLowerLower

// hiH,hiL loH,loL |-> hiL,loL (= lower halves)
template <typename T>
HWY_API Vec256<T> ConcatLowerLower(Full256<T> /* tag */, const Vec256<T> hi,
                                   const Vec256<T> lo) {
  return Vec256<T>{wasm_i64x2_shuffle(lo.raw, hi.raw, 0, 2)};
}

// ------------------------------ ConcatUpperUpper

template <typename T>
HWY_API Vec256<T> ConcatUpperUpper(Full256<T> /* tag */, const Vec256<T> hi,
                                   const Vec256<T> lo) {
  return Vec256<T>{wasm_i64x2_shuffle(lo.raw, hi.raw, 1, 3)};
}

// ------------------------------ ConcatLowerUpper

template <typename T>
HWY_API Vec256<T> ConcatLowerUpper(Full256<T> d, const Vec256<T> hi,
                                   const Vec256<T> lo) {
  return CombineShiftRightBytes<8>(d, hi, lo);
}

// ------------------------------ ConcatUpperLower
template <typename T>
HWY_API Vec256<T> ConcatUpperLower(Full256<T> d, const Vec256<T> hi,
                                   const Vec256<T> lo) {
  return IfThenElse(FirstN(d, Lanes(d) / 2), lo, hi);
}

// ------------------------------ ConcatOdd

// 32-bit
template <typename T, HWY_IF_LANE_SIZE(T, 4)>
HWY_API Vec256<T> ConcatOdd(Full256<T> /* tag */, Vec256<T> hi, Vec256<T> lo) {
  return Vec256<T>{wasm_i32x4_shuffle(lo.raw, hi.raw, 1, 3, 5, 7)};
}

// 64-bit full - no partial because we need at least two inputs to have
// even/odd.
template <typename T, HWY_IF_LANE_SIZE(T, 8)>
HWY_API Vec256<T> ConcatOdd(Full256<T> /* tag */, Vec256<T> hi, Vec256<T> lo) {
  return InterleaveUpper(Full256<T>(), lo, hi);
}

// ------------------------------ ConcatEven (InterleaveLower)

// 32-bit full
template <typename T, HWY_IF_LANE_SIZE(T, 4)>
HWY_API Vec256<T> ConcatEven(Full256<T> /* tag */, Vec256<T> hi, Vec256<T> lo) {
  return Vec256<T>{wasm_i32x4_shuffle(lo.raw, hi.raw, 0, 2, 4, 6)};
}

// 64-bit full - no partial because we need at least two inputs to have
// even/odd.
template <typename T, HWY_IF_LANE_SIZE(T, 8)>
HWY_API Vec256<T> ConcatEven(Full256<T> /* tag */, Vec256<T> hi, Vec256<T> lo) {
  return InterleaveLower(Full256<T>(), lo, hi);
}

// ------------------------------ DupEven
template <typename T>
HWY_API Vec256<T> DupEven(Vec256<T> v) {
  HWY_ASSERT(0);
}

// ------------------------------ DupOdd
template <typename T>
HWY_API Vec256<T> DupOdd(Vec256<T> v) {
  HWY_ASSERT(0);
}

// ------------------------------ OddEven

namespace detail {

template <typename T>
HWY_INLINE Vec256<T> OddEven(hwy::SizeTag<1> /* tag */, const Vec256<T> a,
                             const Vec256<T> b) {
  const Full256<T> d;
  const Repartition<uint8_t, decltype(d)> d8;
  alignas(32) constexpr uint8_t mask[16] = {0xFF, 0, 0xFF, 0, 0xFF, 0, 0xFF, 0,
                                            0xFF, 0, 0xFF, 0, 0xFF, 0, 0xFF, 0};
  return IfThenElse(MaskFromVec(BitCast(d, Load(d8, mask))), b, a);
}
template <typename T>
HWY_INLINE Vec256<T> OddEven(hwy::SizeTag<2> /* tag */, const Vec256<T> a,
                             const Vec256<T> b) {
  return Vec256<T>{wasm_i16x8_shuffle(a.raw, b.raw, 8, 1, 10, 3, 12, 5, 14, 7)};
}
template <typename T>
HWY_INLINE Vec256<T> OddEven(hwy::SizeTag<4> /* tag */, const Vec256<T> a,
                             const Vec256<T> b) {
  return Vec256<T>{wasm_i32x4_shuffle(a.raw, b.raw, 4, 1, 6, 3)};
}
template <typename T>
HWY_INLINE Vec256<T> OddEven(hwy::SizeTag<8> /* tag */, const Vec256<T> a,
                             const Vec256<T> b) {
  return Vec256<T>{wasm_i64x2_shuffle(a.raw, b.raw, 2, 1)};
}

}  // namespace detail

template <typename T>
HWY_API Vec256<T> OddEven(const Vec256<T> a, const Vec256<T> b) {
  return detail::OddEven(hwy::SizeTag<sizeof(T)>(), a, b);
}
HWY_API Vec256<float> OddEven(const Vec256<float> a, const Vec256<float> b) {
  return Vec256<float>{wasm_i32x4_shuffle(a.raw, b.raw, 4, 1, 6, 3)};
}

// ------------------------------ OddEvenBlocks
template <typename T>
HWY_API Vec256<T> OddEvenBlocks(Vec256<T> /* odd */, Vec256<T> even) {
  return even;
}

// ------------------------------ SwapAdjacentBlocks

template <typename T>
HWY_API Vec256<T> SwapAdjacentBlocks(Vec256<T> v) {
  return v;
}

// ------------------------------ ReverseBlocks

template <typename T>
HWY_API Vec256<T> ReverseBlocks(Full256<T> /* tag */, const Vec256<T> v) {
  return v;
}

// ================================================== CONVERT

// ------------------------------ Promotions (part w/ narrow lanes -> full)

// Unsigned: zero-extend.
HWY_API Vec256<uint16_t> PromoteTo(Full256<uint16_t> /* tag */,
                                   const Vec128<uint8_t> v) {
  return Vec256<uint16_t>{wasm_u16x8_extend_low_u8x16(v.raw)};
}
HWY_API Vec256<uint32_t> PromoteTo(Full256<uint32_t> /* tag */,
                                   const Vec128<uint8_t> v) {
  return Vec256<uint32_t>{
      wasm_u32x4_extend_low_u16x8(wasm_u16x8_extend_low_u8x16(v.raw))};
}
HWY_API Vec256<int16_t> PromoteTo(Full256<int16_t> /* tag */,
                                  const Vec128<uint8_t> v) {
  return Vec256<int16_t>{wasm_u16x8_extend_low_u8x16(v.raw)};
}
HWY_API Vec256<int32_t> PromoteTo(Full256<int32_t> /* tag */,
                                  const Vec128<uint8_t> v) {
  return Vec256<int32_t>{
      wasm_u32x4_extend_low_u16x8(wasm_u16x8_extend_low_u8x16(v.raw))};
}
HWY_API Vec256<uint32_t> PromoteTo(Full256<uint32_t> /* tag */,
                                   const Vec128<uint16_t> v) {
  return Vec256<uint32_t>{wasm_u32x4_extend_low_u16x8(v.raw)};
}
HWY_API Vec256<int32_t> PromoteTo(Full256<int32_t> /* tag */,
                                  const Vec128<uint16_t> v) {
  return Vec256<int32_t>{wasm_u32x4_extend_low_u16x8(v.raw)};
}

// Signed: replicate sign bit.
HWY_API Vec256<int16_t> PromoteTo(Full256<int16_t> /* tag */,
                                  const Vec128<int8_t> v) {
  return Vec256<int16_t>{wasm_i16x8_extend_low_i8x16(v.raw)};
}
HWY_API Vec256<int32_t> PromoteTo(Full256<int32_t> /* tag */,
                                  const Vec128<int8_t> v) {
  return Vec256<int32_t>{
      wasm_i32x4_extend_low_i16x8(wasm_i16x8_extend_low_i8x16(v.raw))};
}
HWY_API Vec256<int32_t> PromoteTo(Full256<int32_t> /* tag */,
                                  const Vec128<int16_t> v) {
  return Vec256<int32_t>{wasm_i32x4_extend_low_i16x8(v.raw)};
}

HWY_API Vec256<double> PromoteTo(Full256<double> /* tag */,
                                 const Vec128<int32_t> v) {
  return Vec256<double>{wasm_f64x2_convert_low_i32x4(v.raw)};
}

HWY_API Vec256<float> PromoteTo(Full256<float> /* tag */,
                                const Vec128<float16_t> v) {
  const Full256<int32_t> di32;
  const Full256<uint32_t> du32;
  const Full256<float> df32;
  // Expand to u32 so we can shift.
  const auto bits16 = PromoteTo(du32, Vec256<uint16_t>{v.raw});
  const auto sign = ShiftRight<15>(bits16);
  const auto biased_exp = ShiftRight<10>(bits16) & Set(du32, 0x1F);
  const auto mantissa = bits16 & Set(du32, 0x3FF);
  const auto subnormal =
      BitCast(du32, ConvertTo(df32, BitCast(di32, mantissa)) *
                        Set(df32, 1.0f / 16384 / 1024));

  const auto biased_exp32 = biased_exp + Set(du32, 127 - 15);
  const auto mantissa32 = ShiftLeft<23 - 10>(mantissa);
  const auto normal = ShiftLeft<23>(biased_exp32) | mantissa32;
  const auto bits32 = IfThenElse(biased_exp == Zero(du32), subnormal, normal);
  return BitCast(df32, ShiftLeft<31>(sign) | bits32);
}

HWY_API Vec256<float> PromoteTo(Full256<float> df32,
                                const Vec128<bfloat16_t> v) {
  const Rebind<uint16_t, decltype(df32)> du16;
  const RebindToSigned<decltype(df32)> di32;
  return BitCast(df32, ShiftLeft<16>(PromoteTo(di32, BitCast(du16, v))));
}

// ------------------------------ Demotions (full -> part w/ narrow lanes)

HWY_API Vec128<uint16_t> DemoteTo(Full128<uint16_t> /* tag */,
                                  const Vec256<int32_t> v) {
  return Vec128<uint16_t>{wasm_u16x8_narrow_i32x4(v.raw, v.raw)};
}

HWY_API Vec128<int16_t> DemoteTo(Full128<int16_t> /* tag */,
                                 const Vec256<int32_t> v) {
  return Vec128<int16_t>{wasm_i16x8_narrow_i32x4(v.raw, v.raw)};
}

HWY_API Vec128<uint8_t> DemoteTo(Full128<uint8_t> /* tag */,
                                 const Vec256<int32_t> v) {
  const auto intermediate = wasm_i16x8_narrow_i32x4(v.raw, v.raw);
  return Vec128<uint8_t>{wasm_u8x16_narrow_i16x8(intermediate, intermediate)};
}

HWY_API Vec128<uint8_t> DemoteTo(Full128<uint8_t> /* tag */,
                                 const Vec256<int16_t> v) {
  return Vec128<uint8_t>{wasm_u8x16_narrow_i16x8(v.raw, v.raw)};
}

HWY_API Vec128<int8_t> DemoteTo(Full128<int8_t> /* tag */,
                                const Vec256<int32_t> v) {
  const auto intermediate = wasm_i16x8_narrow_i32x4(v.raw, v.raw);
  return Vec128<int8_t>{wasm_i8x16_narrow_i16x8(intermediate, intermediate)};
}

HWY_API Vec128<int8_t> DemoteTo(Full128<int8_t> /* tag */,
                                const Vec256<int16_t> v) {
  return Vec128<int8_t>{wasm_i8x16_narrow_i16x8(v.raw, v.raw)};
}

HWY_API Vec128<int32_t> DemoteTo(Full128<int32_t> /* di */,
                                 const Vec256<double> v) {
  return Vec128<int32_t>{wasm_i32x4_trunc_sat_f64x2_zero(v.raw)};
}

HWY_API Vec128<float16_t> DemoteTo(Full128<float16_t> /* tag */,
                                   const Vec256<float> v) {
  const Full256<int32_t> di;
  const Full256<uint32_t> du;
  const Full256<uint16_t> du16;
  const auto bits32 = BitCast(du, v);
  const auto sign = ShiftRight<31>(bits32);
  const auto biased_exp32 = ShiftRight<23>(bits32) & Set(du, 0xFF);
  const auto mantissa32 = bits32 & Set(du, 0x7FFFFF);

  const auto k15 = Set(di, 15);
  const auto exp = Min(BitCast(di, biased_exp32) - Set(di, 127), k15);
  const auto is_tiny = exp < Set(di, -24);

  const auto is_subnormal = exp < Set(di, -14);
  const auto biased_exp16 =
      BitCast(du, IfThenZeroElse(is_subnormal, exp + k15));
  const auto sub_exp = BitCast(du, Set(di, -14) - exp);  // [1, 11)
  const auto sub_m = (Set(du, 1) << (Set(du, 10) - sub_exp)) +
                     (mantissa32 >> (Set(du, 13) + sub_exp));
  const auto mantissa16 = IfThenElse(RebindMask(du, is_subnormal), sub_m,
                                     ShiftRight<13>(mantissa32));  // <1024

  const auto sign16 = ShiftLeft<15>(sign);
  const auto normal16 = sign16 | ShiftLeft<10>(biased_exp16) | mantissa16;
  const auto bits16 = IfThenZeroElse(is_tiny, BitCast(di, normal16));
  return Vec128<float16_t>{DemoteTo(du16, bits16).raw};
}

HWY_API Vec128<bfloat16_t> DemoteTo(Full128<bfloat16_t> dbf16,
                                    const Vec256<float> v) {
  const Rebind<int32_t, decltype(dbf16)> di32;
  const Rebind<uint32_t, decltype(dbf16)> du32;  // for logical shift right
  const Rebind<uint16_t, decltype(dbf16)> du16;
  const auto bits_in_32 = BitCast(di32, ShiftRight<16>(BitCast(du32, v)));
  return BitCast(dbf16, DemoteTo(du16, bits_in_32));
}

HWY_API Vec128<bfloat16_t> ReorderDemote2To(Full128<bfloat16_t> dbf16,
                                            Vec256<float> a, Vec256<float> b) {
  const RebindToUnsigned<decltype(dbf16)> du16;
  const Repartition<uint32_t, decltype(dbf16)> du32;
  const Vec256<uint32_t> b_in_even = ShiftRight<16>(BitCast(du32, b));
  return BitCast(dbf16, OddEven(BitCast(du16, a), BitCast(du16, b_in_even)));
}

// For already range-limited input [0, 255].
HWY_API Vec256<uint8_t> U8FromU32(const Vec256<uint32_t> v) {
  const auto intermediate = wasm_i16x8_narrow_i32x4(v.raw, v.raw);
  return Vec256<uint8_t>{wasm_u8x16_narrow_i16x8(intermediate, intermediate)};
}

// ------------------------------ Truncations

HWY_API Vec256<uint8_t, 4> TruncateTo(Simd<uint8_t, 4, 0> /* tag */,
                                      const Vec256<uint64_t> v) {
  return Vec256<uint8_t, 4>{wasm_i8x16_shuffle(v.v0.raw, v.v1.raw, 0, 8, 16, 24,
                                               0, 8, 16, 24, 0, 8, 16, 24, 0, 8,
                                               16, 24)};
}

HWY_API Vec256<uint16_t, 4> TruncateTo(Simd<uint16_t, 4, 0> /* tag */,
                                       const Vec256<uint64_t> v) {
  return Vec256<uint16_t, 4>{wasm_i8x16_shuffle(v.v0.raw, v.v1.raw, 0, 1, 8, 9,
                                                16, 17, 24, 25, 0, 1, 8, 9, 16,
                                                17, 24, 25)};
}

HWY_API Vec256<uint32_t, 4> TruncateTo(Simd<uint32_t, 4, 0> /* tag */,
                                       const Vec256<uint64_t> v) {
  return Vec256<uint32_t, 4>{wasm_i8x16_shuffle(v.v0.raw, v.v1.raw, 0, 1, 2, 3,
                                                8, 9, 10, 11, 16, 17, 18, 19,
                                                24, 25, 26, 27)};
}

HWY_API Vec256<uint8_t, 8> TruncateTo(Simd<uint8_t, 8, 0> /* tag */,
                                      const Vec256<uint32_t> v) {
  return Vec256<uint8_t, 8>{wasm_i8x16_shuffle(v.v0.raw, v.v1.raw, 0, 4, 8, 12,
                                               16, 20, 24, 28, 0, 4, 8, 12, 16,
                                               20, 24, 28)};
}

HWY_API Vec256<uint16_t, 8> TruncateTo(Simd<uint16_t, 8, 0> /* tag */,
                                       const Vec256<uint32_t> v) {
  return Vec256<uint16_t, 8>{wasm_i8x16_shuffle(v.v0.raw, v.v1.raw, 0, 1, 4, 5,
                                                8, 9, 12, 13, 16, 17, 20, 21,
                                                24, 25, 28, 29)};
}

HWY_API Vec256<uint8_t, 16> TruncateTo(Simd<uint8_t, 16, 0> /* tag */,
                                       const Vec256<uint16_t> v) {
  return Vec256<uint8_t, 16>{wasm_i8x16_shuffle(v.v0.raw, v.v1.raw, 0, 2, 4, 6,
                                                8, 10, 12, 14, 16, 18, 20, 22,
                                                24, 26, 28, 30)};
}

// ------------------------------ Convert i32 <=> f32 (Round)

HWY_API Vec256<float> ConvertTo(Full256<float> /* tag */,
                                const Vec256<int32_t> v) {
  return Vec256<float>{wasm_f32x4_convert_i32x4(v.raw)};
}
// Truncates (rounds toward zero).
HWY_API Vec256<int32_t> ConvertTo(Full256<int32_t> /* tag */,
                                  const Vec256<float> v) {
  return Vec256<int32_t>{wasm_i32x4_trunc_sat_f32x4(v.raw)};
}

HWY_API Vec256<int32_t> NearestInt(const Vec256<float> v) {
  return ConvertTo(Full256<int32_t>(), Round(v));
}

// ================================================== MISC

// ------------------------------ LoadMaskBits (TestBit)

namespace detail {

template <typename T, HWY_IF_LANE_SIZE(T, 1)>
HWY_INLINE Mask256<T> LoadMaskBits(Full256<T> d, uint64_t bits) {
  const RebindToUnsigned<decltype(d)> du;
  // Easier than Set(), which would require an >8-bit type, which would not
  // compile for T=uint8_t, N=1.
  const Vec256<T> vbits{wasm_i32x4_splat(static_cast<int32_t>(bits))};

  // Replicate bytes 8x such that each byte contains the bit that governs it.
  alignas(32) constexpr uint8_t kRep8[16] = {0, 0, 0, 0, 0, 0, 0, 0,
                                             1, 1, 1, 1, 1, 1, 1, 1};
  const auto rep8 = TableLookupBytes(vbits, Load(du, kRep8));

  alignas(32) constexpr uint8_t kBit[16] = {1, 2, 4, 8, 16, 32, 64, 128,
                                            1, 2, 4, 8, 16, 32, 64, 128};
  return RebindMask(d, TestBit(rep8, LoadDup128(du, kBit)));
}

template <typename T, HWY_IF_LANE_SIZE(T, 2)>
HWY_INLINE Mask256<T> LoadMaskBits(Full256<T> d, uint64_t bits) {
  const RebindToUnsigned<decltype(d)> du;
  alignas(32) constexpr uint16_t kBit[8] = {1, 2, 4, 8, 16, 32, 64, 128};
  return RebindMask(d, TestBit(Set(du, bits), Load(du, kBit)));
}

template <typename T, HWY_IF_LANE_SIZE(T, 4)>
HWY_INLINE Mask256<T> LoadMaskBits(Full256<T> d, uint64_t bits) {
  const RebindToUnsigned<decltype(d)> du;
  alignas(32) constexpr uint32_t kBit[8] = {1, 2, 4, 8};
  return RebindMask(d, TestBit(Set(du, bits), Load(du, kBit)));
}

template <typename T, HWY_IF_LANE_SIZE(T, 8)>
HWY_INLINE Mask256<T> LoadMaskBits(Full256<T> d, uint64_t bits) {
  const RebindToUnsigned<decltype(d)> du;
  alignas(32) constexpr uint64_t kBit[8] = {1, 2};
  return RebindMask(d, TestBit(Set(du, bits), Load(du, kBit)));
}

}  // namespace detail

// `p` points to at least 8 readable bytes, not all of which need be valid.
template <typename T>
HWY_API Mask256<T> LoadMaskBits(Full256<T> d,
                                const uint8_t* HWY_RESTRICT bits) {
  uint64_t mask_bits = 0;
  CopyBytes<(N + 7) / 8>(bits, &mask_bits);
  return detail::LoadMaskBits(d, mask_bits);
}

// ------------------------------ Mask

namespace detail {

// Full
template <typename T>
HWY_INLINE uint64_t BitsFromMask(hwy::SizeTag<1> /*tag*/,
                                 const Mask128<T> mask) {
  alignas(32) uint64_t lanes[2];
  wasm_v128_store(lanes, mask.raw);

  constexpr uint64_t kMagic = 0x103070F1F3F80ULL;
  const uint64_t lo = ((lanes[0] * kMagic) >> 56);
  const uint64_t hi = ((lanes[1] * kMagic) >> 48) & 0xFF00;
  return (hi + lo);
}

template <typename T>
HWY_INLINE uint64_t BitsFromMask(hwy::SizeTag<2> /*tag*/,
                                 const Mask256<T> mask) {
  // Remove useless lower half of each u16 while preserving the sign bit.
  const __i16x8 zero = wasm_i16x8_splat(0);
  const Mask256<uint8_t> mask8{wasm_i8x16_narrow_i16x8(mask.raw, zero)};
  return BitsFromMask(hwy::SizeTag<1>(), mask8);
}

template <typename T>
HWY_INLINE uint64_t BitsFromMask(hwy::SizeTag<4> /*tag*/,
                                 const Mask256<T> mask) {
  const __i32x4 mask_i = static_cast<__i32x4>(mask.raw);
  const __i32x4 slice = wasm_i32x4_make(1, 2, 4, 8);
  const __i32x4 sliced_mask = wasm_v128_and(mask_i, slice);
  alignas(32) uint32_t lanes[4];
  wasm_v128_store(lanes, sliced_mask);
  return lanes[0] | lanes[1] | lanes[2] | lanes[3];
}

// Returns 0xFF for bytes with index >= N, otherwise 0.
constexpr __i8x16 BytesAbove() {
  return /**/
      (N == 0)    ? wasm_i32x4_make(-1, -1, -1, -1)
      : (N == 4)  ? wasm_i32x4_make(0, -1, -1, -1)
      : (N == 8)  ? wasm_i32x4_make(0, 0, -1, -1)
      : (N == 12) ? wasm_i32x4_make(0, 0, 0, -1)
      : (N == 16) ? wasm_i32x4_make(0, 0, 0, 0)
      : (N == 2)  ? wasm_i16x8_make(0, -1, -1, -1, -1, -1, -1, -1)
      : (N == 6)  ? wasm_i16x8_make(0, 0, 0, -1, -1, -1, -1, -1)
      : (N == 10) ? wasm_i16x8_make(0, 0, 0, 0, 0, -1, -1, -1)
      : (N == 14) ? wasm_i16x8_make(0, 0, 0, 0, 0, 0, 0, -1)
      : (N == 1)  ? wasm_i8x16_make(0, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
                                   -1, -1, -1, -1, -1)
      : (N == 3)  ? wasm_i8x16_make(0, 0, 0, -1, -1, -1, -1, -1, -1, -1, -1, -1,
                                   -1, -1, -1, -1)
      : (N == 5)  ? wasm_i8x16_make(0, 0, 0, 0, 0, -1, -1, -1, -1, -1, -1, -1,
                                   -1, -1, -1, -1)
      : (N == 7)  ? wasm_i8x16_make(0, 0, 0, 0, 0, 0, 0, -1, -1, -1, -1, -1, -1,
                                   -1, -1, -1)
      : (N == 9)  ? wasm_i8x16_make(0, 0, 0, 0, 0, 0, 0, 0, 0, -1, -1, -1, -1,
                                   -1, -1, -1)
      : (N == 11)
          ? wasm_i8x16_make(0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, -1, -1, -1, -1, -1)
      : (N == 13)
          ? wasm_i8x16_make(0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, -1, -1, -1)
          : wasm_i8x16_make(0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, -1);
}

template <typename T>
HWY_INLINE uint64_t BitsFromMask(const Mask256<T> mask) {
  return BitsFromMask(hwy::SizeTag<sizeof(T)>(), mask);
}

template <typename T>
HWY_INLINE size_t CountTrue(hwy::SizeTag<1> tag, const Mask128<T> m) {
  return PopCount(BitsFromMask(tag, m));
}

template <typename T>
HWY_INLINE size_t CountTrue(hwy::SizeTag<2> tag, const Mask128<T> m) {
  return PopCount(BitsFromMask(tag, m));
}

template <typename T>
HWY_INLINE size_t CountTrue(hwy::SizeTag<4> /*tag*/, const Mask128<T> m) {
  const __i32x4 var_shift = wasm_i32x4_make(1, 2, 4, 8);
  const __i32x4 shifted_bits = wasm_v128_and(m.raw, var_shift);
  alignas(32) uint64_t lanes[2];
  wasm_v128_store(lanes, shifted_bits);
  return PopCount(lanes[0] | lanes[1]);
}

}  // namespace detail

// `p` points to at least 8 writable bytes.
template <typename T>
HWY_API size_t StoreMaskBits(const Full256<T> /* tag */, const Mask256<T> mask,
                             uint8_t* bits) {
  const uint64_t mask_bits = detail::BitsFromMask(mask);
  const size_t kNumBytes = (N + 7) / 8;
  CopyBytes<kNumBytes>(&mask_bits, bits);
  return kNumBytes;
}

template <typename T>
HWY_API size_t CountTrue(const Full256<T> /* tag */, const Mask128<T> m) {
  return detail::CountTrue(hwy::SizeTag<sizeof(T)>(), m);
}

template <typename T>
HWY_API bool AllFalse(const Full256<T> d, const Mask128<T> m) {
#if 0
  // Casting followed by wasm_i8x16_any_true results in wasm error:
  // i32.eqz[0] expected type i32, found i8x16.popcnt of type s128
  const auto v8 = BitCast(Full256<int8_t>(), VecFromMask(d, m));
  return !wasm_i8x16_any_true(v8.raw);
#else
  (void)d;
  return (wasm_i64x2_extract_lane(m.raw, 0) |
          wasm_i64x2_extract_lane(m.raw, 1)) == 0;
#endif
}

// Full vector
namespace detail {
template <typename T>
HWY_INLINE bool AllTrue(hwy::SizeTag<1> /*tag*/, const Mask128<T> m) {
  return wasm_i8x16_all_true(m.raw);
}
template <typename T>
HWY_INLINE bool AllTrue(hwy::SizeTag<2> /*tag*/, const Mask128<T> m) {
  return wasm_i16x8_all_true(m.raw);
}
template <typename T>
HWY_INLINE bool AllTrue(hwy::SizeTag<4> /*tag*/, const Mask128<T> m) {
  return wasm_i32x4_all_true(m.raw);
}

}  // namespace detail

template <typename T>
HWY_API bool AllTrue(const Full256<T> /* tag */, const Mask128<T> m) {
  return detail::AllTrue(hwy::SizeTag<sizeof(T)>(), m);
}

template <typename T>
HWY_API intptr_t FindFirstTrue(const Full256<T> /* tag */,
                               const Mask256<T> mask) {
  const uint64_t bits = detail::BitsFromMask(mask);
  return bits ? Num0BitsBelowLS1Bit_Nonzero64(bits) : -1;
}

// ------------------------------ Compress

namespace detail {

template <typename T>
HWY_INLINE Vec256<T> Idx16x8FromBits(const uint64_t mask_bits) {
  HWY_DASSERT(mask_bits < 256);
  const Full256<T> d;
  const Rebind<uint8_t, decltype(d)> d8;
  const Full256<uint16_t> du;

  // We need byte indices for TableLookupBytes (one vector's worth for each of
  // 256 combinations of 8 mask bits). Loading them directly requires 4 KiB. We
  // can instead store lane indices and convert to byte indices (2*lane + 0..1),
  // with the doubling baked into the table. Unpacking nibbles is likely more
  // costly than the higher cache footprint from storing bytes.
  alignas(32) constexpr uint8_t table[256 * 8] = {
      0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  2,  0,
      0,  0,  0,  0,  0,  0,  0,  2,  0,  0,  0,  0,  0,  0,  4,  0,  0,  0,
      0,  0,  0,  0,  0,  4,  0,  0,  0,  0,  0,  0,  2,  4,  0,  0,  0,  0,
      0,  0,  0,  2,  4,  0,  0,  0,  0,  0,  6,  0,  0,  0,  0,  0,  0,  0,
      0,  6,  0,  0,  0,  0,  0,  0,  2,  6,  0,  0,  0,  0,  0,  0,  0,  2,
      6,  0,  0,  0,  0,  0,  4,  6,  0,  0,  0,  0,  0,  0,  0,  4,  6,  0,
      0,  0,  0,  0,  2,  4,  6,  0,  0,  0,  0,  0,  0,  2,  4,  6,  0,  0,
      0,  0,  8,  0,  0,  0,  0,  0,  0,  0,  0,  8,  0,  0,  0,  0,  0,  0,
      2,  8,  0,  0,  0,  0,  0,  0,  0,  2,  8,  0,  0,  0,  0,  0,  4,  8,
      0,  0,  0,  0,  0,  0,  0,  4,  8,  0,  0,  0,  0,  0,  2,  4,  8,  0,
      0,  0,  0,  0,  0,  2,  4,  8,  0,  0,  0,  0,  6,  8,  0,  0,  0,  0,
      0,  0,  0,  6,  8,  0,  0,  0,  0,  0,  2,  6,  8,  0,  0,  0,  0,  0,
      0,  2,  6,  8,  0,  0,  0,  0,  4,  6,  8,  0,  0,  0,  0,  0,  0,  4,
      6,  8,  0,  0,  0,  0,  2,  4,  6,  8,  0,  0,  0,  0,  0,  2,  4,  6,
      8,  0,  0,  0,  10, 0,  0,  0,  0,  0,  0,  0,  0,  10, 0,  0,  0,  0,
      0,  0,  2,  10, 0,  0,  0,  0,  0,  0,  0,  2,  10, 0,  0,  0,  0,  0,
      4,  10, 0,  0,  0,  0,  0,  0,  0,  4,  10, 0,  0,  0,  0,  0,  2,  4,
      10, 0,  0,  0,  0,  0,  0,  2,  4,  10, 0,  0,  0,  0,  6,  10, 0,  0,
      0,  0,  0,  0,  0,  6,  10, 0,  0,  0,  0,  0,  2,  6,  10, 0,  0,  0,
      0,  0,  0,  2,  6,  10, 0,  0,  0,  0,  4,  6,  10, 0,  0,  0,  0,  0,
      0,  4,  6,  10, 0,  0,  0,  0,  2,  4,  6,  10, 0,  0,  0,  0,  0,  2,
      4,  6,  10, 0,  0,  0,  8,  10, 0,  0,  0,  0,  0,  0,  0,  8,  10, 0,
      0,  0,  0,  0,  2,  8,  10, 0,  0,  0,  0,  0,  0,  2,  8,  10, 0,  0,
      0,  0,  4,  8,  10, 0,  0,  0,  0,  0,  0,  4,  8,  10, 0,  0,  0,  0,
      2,  4,  8,  10, 0,  0,  0,  0,  0,  2,  4,  8,  10, 0,  0,  0,  6,  8,
      10, 0,  0,  0,  0,  0,  0,  6,  8,  10, 0,  0,  0,  0,  2,  6,  8,  10,
      0,  0,  0,  0,  0,  2,  6,  8,  10, 0,  0,  0,  4,  6,  8,  10, 0,  0,
      0,  0,  0,  4,  6,  8,  10, 0,  0,  0,  2,  4,  6,  8,  10, 0,  0,  0,
      0,  2,  4,  6,  8,  10, 0,  0,  12, 0,  0,  0,  0,  0,  0,  0,  0,  12,
      0,  0,  0,  0,  0,  0,  2,  12, 0,  0,  0,  0,  0,  0,  0,  2,  12, 0,
      0,  0,  0,  0,  4,  12, 0,  0,  0,  0,  0,  0,  0,  4,  12, 0,  0,  0,
      0,  0,  2,  4,  12, 0,  0,  0,  0,  0,  0,  2,  4,  12, 0,  0,  0,  0,
      6,  12, 0,  0,  0,  0,  0,  0,  0,  6,  12, 0,  0,  0,  0,  0,  2,  6,
      12, 0,  0,  0,  0,  0,  0,  2,  6,  12, 0,  0,  0,  0,  4,  6,  12, 0,
      0,  0,  0,  0,  0,  4,  6,  12, 0,  0,  0,  0,  2,  4,  6,  12, 0,  0,
      0,  0,  0,  2,  4,  6,  12, 0,  0,  0,  8,  12, 0,  0,  0,  0,  0,  0,
      0,  8,  12, 0,  0,  0,  0,  0,  2,  8,  12, 0,  0,  0,  0,  0,  0,  2,
      8,  12, 0,  0,  0,  0,  4,  8,  12, 0,  0,  0,  0,  0,  0,  4,  8,  12,
      0,  0,  0,  0,  2,  4,  8,  12, 0,  0,  0,  0,  0,  2,  4,  8,  12, 0,
      0,  0,  6,  8,  12, 0,  0,  0,  0,  0,  0,  6,  8,  12, 0,  0,  0,  0,
      2,  6,  8,  12, 0,  0,  0,  0,  0,  2,  6,  8,  12, 0,  0,  0,  4,  6,
      8,  12, 0,  0,  0,  0,  0,  4,  6,  8,  12, 0,  0,  0,  2,  4,  6,  8,
      12, 0,  0,  0,  0,  2,  4,  6,  8,  12, 0,  0,  10, 12, 0,  0,  0,  0,
      0,  0,  0,  10, 12, 0,  0,  0,  0,  0,  2,  10, 12, 0,  0,  0,  0,  0,
      0,  2,  10, 12, 0,  0,  0,  0,  4,  10, 12, 0,  0,  0,  0,  0,  0,  4,
      10, 12, 0,  0,  0,  0,  2,  4,  10, 12, 0,  0,  0,  0,  0,  2,  4,  10,
      12, 0,  0,  0,  6,  10, 12, 0,  0,  0,  0,  0,  0,  6,  10, 12, 0,  0,
      0,  0,  2,  6,  10, 12, 0,  0,  0,  0,  0,  2,  6,  10, 12, 0,  0,  0,
      4,  6,  10, 12, 0,  0,  0,  0,  0,  4,  6,  10, 12, 0,  0,  0,  2,  4,
      6,  10, 12, 0,  0,  0,  0,  2,  4,  6,  10, 12, 0,  0,  8,  10, 12, 0,
      0,  0,  0,  0,  0,  8,  10, 12, 0,  0,  0,  0,  2,  8,  10, 12, 0,  0,
      0,  0,  0,  2,  8,  10, 12, 0,  0,  0,  4,  8,  10, 12, 0,  0,  0,  0,
      0,  4,  8,  10, 12, 0,  0,  0,  2,  4,  8,  10, 12, 0,  0,  0,  0,  2,
      4,  8,  10, 12, 0,  0,  6,  8,  10, 12, 0,  0,  0,  0,  0,  6,  8,  10,
      12, 0,  0,  0,  2,  6,  8,  10, 12, 0,  0,  0,  0,  2,  6,  8,  10, 12,
      0,  0,  4,  6,  8,  10, 12, 0,  0,  0,  0,  4,  6,  8,  10, 12, 0,  0,
      2,  4,  6,  8,  10, 12, 0,  0,  0,  2,  4,  6,  8,  10, 12, 0,  14, 0,
      0,  0,  0,  0,  0,  0,  0,  14, 0,  0,  0,  0,  0,  0,  2,  14, 0,  0,
      0,  0,  0,  0,  0,  2,  14, 0,  0,  0,  0,  0,  4,  14, 0,  0,  0,  0,
      0,  0,  0,  4,  14, 0,  0,  0,  0,  0,  2,  4,  14, 0,  0,  0,  0,  0,
      0,  2,  4,  14, 0,  0,  0,  0,  6,  14, 0,  0,  0,  0,  0,  0,  0,  6,
      14, 0,  0,  0,  0,  0,  2,  6,  14, 0,  0,  0,  0,  0,  0,  2,  6,  14,
      0,  0,  0,  0,  4,  6,  14, 0,  0,  0,  0,  0,  0,  4,  6,  14, 0,  0,
      0,  0,  2,  4,  6,  14, 0,  0,  0,  0,  0,  2,  4,  6,  14, 0,  0,  0,
      8,  14, 0,  0,  0,  0,  0,  0,  0,  8,  14, 0,  0,  0,  0,  0,  2,  8,
      14, 0,  0,  0,  0,  0,  0,  2,  8,  14, 0,  0,  0,  0,  4,  8,  14, 0,
      0,  0,  0,  0,  0,  4,  8,  14, 0,  0,  0,  0,  2,  4,  8,  14, 0,  0,
      0,  0,  0,  2,  4,  8,  14, 0,  0,  0,  6,  8,  14, 0,  0,  0,  0,  0,
      0,  6,  8,  14, 0,  0,  0,  0,  2,  6,  8,  14, 0,  0,  0,  0,  0,  2,
      6,  8,  14, 0,  0,  0,  4,  6,  8,  14, 0,  0,  0,  0,  0,  4,  6,  8,
      14, 0,  0,  0,  2,  4,  6,  8,  14, 0,  0,  0,  0,  2,  4,  6,  8,  14,
      0,  0,  10, 14, 0,  0,  0,  0,  0,  0,  0,  10, 14, 0,  0,  0,  0,  0,
      2,  10, 14, 0,  0,  0,  0,  0,  0,  2,  10, 14, 0,  0,  0,  0,  4,  10,
      14, 0,  0,  0,  0,  0,  0,  4,  10, 14, 0,  0,  0,  0,  2,  4,  10, 14,
      0,  0,  0,  0,  0,  2,  4,  10, 14, 0,  0,  0,  6,  10, 14, 0,  0,  0,
      0,  0,  0,  6,  10, 14, 0,  0,  0,  0,  2,  6,  10, 14, 0,  0,  0,  0,
      0,  2,  6,  10, 14, 0,  0,  0,  4,  6,  10, 14, 0,  0,  0,  0,  0,  4,
      6,  10, 14, 0,  0,  0,  2,  4,  6,  10, 14, 0,  0,  0,  0,  2,  4,  6,
      10, 14, 0,  0,  8,  10, 14, 0,  0,  0,  0,  0,  0,  8,  10, 14, 0,  0,
      0,  0,  2,  8,  10, 14, 0,  0,  0,  0,  0,  2,  8,  10, 14, 0,  0,  0,
      4,  8,  10, 14, 0,  0,  0,  0,  0,  4,  8,  10, 14, 0,  0,  0,  2,  4,
      8,  10, 14, 0,  0,  0,  0,  2,  4,  8,  10, 14, 0,  0,  6,  8,  10, 14,
      0,  0,  0,  0,  0,  6,  8,  10, 14, 0,  0,  0,  2,  6,  8,  10, 14, 0,
      0,  0,  0,  2,  6,  8,  10, 14, 0,  0,  4,  6,  8,  10, 14, 0,  0,  0,
      0,  4,  6,  8,  10, 14, 0,  0,  2,  4,  6,  8,  10, 14, 0,  0,  0,  2,
      4,  6,  8,  10, 14, 0,  12, 14, 0,  0,  0,  0,  0,  0,  0,  12, 14, 0,
      0,  0,  0,  0,  2,  12, 14, 0,  0,  0,  0,  0,  0,  2,  12, 14, 0,  0,
      0,  0,  4,  12, 14, 0,  0,  0,  0,  0,  0,  4,  12, 14, 0,  0,  0,  0,
      2,  4,  12, 14, 0,  0,  0,  0,  0,  2,  4,  12, 14, 0,  0,  0,  6,  12,
      14, 0,  0,  0,  0,  0,  0,  6,  12, 14, 0,  0,  0,  0,  2,  6,  12, 14,
      0,  0,  0,  0,  0,  2,  6,  12, 14, 0,  0,  0,  4,  6,  12, 14, 0,  0,
      0,  0,  0,  4,  6,  12, 14, 0,  0,  0,  2,  4,  6,  12, 14, 0,  0,  0,
      0,  2,  4,  6,  12, 14, 0,  0,  8,  12, 14, 0,  0,  0,  0,  0,  0,  8,
      12, 14, 0,  0,  0,  0,  2,  8,  12, 14, 0,  0,  0,  0,  0,  2,  8,  12,
      14, 0,  0,  0,  4,  8,  12, 14, 0,  0,  0,  0,  0,  4,  8,  12, 14, 0,
      0,  0,  2,  4,  8,  12, 14, 0,  0,  0,  0,  2,  4,  8,  12, 14, 0,  0,
      6,  8,  12, 14, 0,  0,  0,  0,  0,  6,  8,  12, 14, 0,  0,  0,  2,  6,
      8,  12, 14, 0,  0,  0,  0,  2,  6,  8,  12, 14, 0,  0,  4,  6,  8,  12,
      14, 0,  0,  0,  0,  4,  6,  8,  12, 14, 0,  0,  2,  4,  6,  8,  12, 14,
      0,  0,  0,  2,  4,  6,  8,  12, 14, 0,  10, 12, 14, 0,  0,  0,  0,  0,
      0,  10, 12, 14, 0,  0,  0,  0,  2,  10, 12, 14, 0,  0,  0,  0,  0,  2,
      10, 12, 14, 0,  0,  0,  4,  10, 12, 14, 0,  0,  0,  0,  0,  4,  10, 12,
      14, 0,  0,  0,  2,  4,  10, 12, 14, 0,  0,  0,  0,  2,  4,  10, 12, 14,
      0,  0,  6,  10, 12, 14, 0,  0,  0,  0,  0,  6,  10, 12, 14, 0,  0,  0,
      2,  6,  10, 12, 14, 0,  0,  0,  0,  2,  6,  10, 12, 14, 0,  0,  4,  6,
      10, 12, 14, 0,  0,  0,  0,  4,  6,  10, 12, 14, 0,  0,  2,  4,  6,  10,
      12, 14, 0,  0,  0,  2,  4,  6,  10, 12, 14, 0,  8,  10, 12, 14, 0,  0,
      0,  0,  0,  8,  10, 12, 14, 0,  0,  0,  2,  8,  10, 12, 14, 0,  0,  0,
      0,  2,  8,  10, 12, 14, 0,  0,  4,  8,  10, 12, 14, 0,  0,  0,  0,  4,
      8,  10, 12, 14, 0,  0,  2,  4,  8,  10, 12, 14, 0,  0,  0,  2,  4,  8,
      10, 12, 14, 0,  6,  8,  10, 12, 14, 0,  0,  0,  0,  6,  8,  10, 12, 14,
      0,  0,  2,  6,  8,  10, 12, 14, 0,  0,  0,  2,  6,  8,  10, 12, 14, 0,
      4,  6,  8,  10, 12, 14, 0,  0,  0,  4,  6,  8,  10, 12, 14, 0,  2,  4,
      6,  8,  10, 12, 14, 0,  0,  2,  4,  6,  8,  10, 12, 14};

  const Vec256<uint8_t> byte_idx{Load(d8, table + mask_bits * 8).raw};
  const Vec256<uint16_t> pairs = ZipLower(byte_idx, byte_idx);
  return BitCast(d, pairs + Set(du, 0x0100));
}

template <typename T>
HWY_INLINE Vec256<T> Idx32x4FromBits(const uint64_t mask_bits) {
  HWY_DASSERT(mask_bits < 16);

  // There are only 4 lanes, so we can afford to load the index vector directly.
  alignas(32) constexpr uint8_t packed_array[16 * 16] = {
      0,  1,  2,  3,  0,  1,  2,  3,  0,  1,  2,  3,  0,  1,  2,  3,  //
      0,  1,  2,  3,  0,  1,  2,  3,  0,  1,  2,  3,  0,  1,  2,  3,  //
      4,  5,  6,  7,  0,  1,  2,  3,  0,  1,  2,  3,  0,  1,  2,  3,  //
      0,  1,  2,  3,  4,  5,  6,  7,  0,  1,  2,  3,  0,  1,  2,  3,  //
      8,  9,  10, 11, 0,  1,  2,  3,  0,  1,  2,  3,  0,  1,  2,  3,  //
      0,  1,  2,  3,  8,  9,  10, 11, 0,  1,  2,  3,  0,  1,  2,  3,  //
      4,  5,  6,  7,  8,  9,  10, 11, 0,  1,  2,  3,  0,  1,  2,  3,  //
      0,  1,  2,  3,  4,  5,  6,  7,  8,  9,  10, 11, 0,  1,  2,  3,  //
      12, 13, 14, 15, 0,  1,  2,  3,  0,  1,  2,  3,  0,  1,  2,  3,  //
      0,  1,  2,  3,  12, 13, 14, 15, 0,  1,  2,  3,  0,  1,  2,  3,  //
      4,  5,  6,  7,  12, 13, 14, 15, 0,  1,  2,  3,  0,  1,  2,  3,  //
      0,  1,  2,  3,  4,  5,  6,  7,  12, 13, 14, 15, 0,  1,  2,  3,  //
      8,  9,  10, 11, 12, 13, 14, 15, 0,  1,  2,  3,  0,  1,  2,  3,  //
      0,  1,  2,  3,  8,  9,  10, 11, 12, 13, 14, 15, 0,  1,  2,  3,  //
      4,  5,  6,  7,  8,  9,  10, 11, 12, 13, 14, 15, 0,  1,  2,  3,  //
      0,  1,  2,  3,  4,  5,  6,  7,  8,  9,  10, 11, 12, 13, 14, 15};

  const Full256<T> d;
  const Repartition<uint8_t, decltype(d)> d8;
  return BitCast(d, Load(d8, packed_array + 16 * mask_bits));
}

#if HWY_HAVE_INTEGER64 || HWY_HAVE_FLOAT64

template <typename T>
HWY_INLINE Vec256<T> Idx64x2FromBits(const uint64_t mask_bits) {
  HWY_DASSERT(mask_bits < 4);

  // There are only 2 lanes, so we can afford to load the index vector directly.
  alignas(32) constexpr uint8_t packed_array[4 * 16] = {
      0, 1, 2,  3,  4,  5,  6,  7,  0, 1, 2,  3,  4,  5,  6,  7,  //
      0, 1, 2,  3,  4,  5,  6,  7,  0, 1, 2,  3,  4,  5,  6,  7,  //
      8, 9, 10, 11, 12, 13, 14, 15, 0, 1, 2,  3,  4,  5,  6,  7,  //
      0, 1, 2,  3,  4,  5,  6,  7,  8, 9, 10, 11, 12, 13, 14, 15};

  const Full256<T> d;
  const Repartition<uint8_t, decltype(d)> d8;
  return BitCast(d, Load(d8, packed_array + 16 * mask_bits));
}

#endif

// Helper functions called by both Compress and CompressStore - avoids a
// redundant BitsFromMask in the latter.

template <typename T>
HWY_INLINE Vec256<T> Compress(hwy::SizeTag<2> /*tag*/, Vec256<T> v,
                              const uint64_t mask_bits) {
  const auto idx = detail::Idx16x8FromBits<T>(mask_bits);
  using D = Full256<T>;
  const RebindToSigned<D> di;
  return BitCast(D(), TableLookupBytes(BitCast(di, v), BitCast(di, idx)));
}

template <typename T>
HWY_INLINE Vec256<T> Compress(hwy::SizeTag<4> /*tag*/, Vec256<T> v,
                              const uint64_t mask_bits) {
  const auto idx = detail::Idx32x4FromBits<T>(mask_bits);
  using D = Full256<T>;
  const RebindToSigned<D> di;
  return BitCast(D(), TableLookupBytes(BitCast(di, v), BitCast(di, idx)));
}

#if HWY_HAVE_INTEGER64 || HWY_HAVE_FLOAT64

template <typename T>
HWY_INLINE Vec256<uint64_t> Compress(hwy::SizeTag<8> /*tag*/,
                                     Vec256<uint64_t> v,
                                     const uint64_t mask_bits) {
  const auto idx = detail::Idx64x2FromBits<uint64_t>(mask_bits);
  using D = Full256<T>;
  const RebindToSigned<D> di;
  return BitCast(D(), TableLookupBytes(BitCast(di, v), BitCast(di, idx)));
}

#endif

}  // namespace detail

template <typename T>
struct CompressIsPartition {
  enum { value = 1 };
};

template <typename T>
HWY_API Vec256<T> Compress(Vec256<T> v, const Mask256<T> mask) {
  const uint64_t mask_bits = detail::BitsFromMask(mask);
  return detail::Compress(hwy::SizeTag<sizeof(T)>(), v, mask_bits);
}

// ------------------------------ CompressNot
template <typename T>
HWY_API Vec256<T> Compress(Vec256<T> v, const Mask256<T> mask) {
  return Compress(v, Not(mask));
}

// ------------------------------ CompressBlocksNot
HWY_API Vec256<uint64_t> CompressBlocksNot(Vec256<uint64_t> v,
                                           Mask256<uint64_t> mask) {
  HWY_ASSERT(0);
}

// ------------------------------ CompressBits

template <typename T>
HWY_API Vec256<T> CompressBits(Vec256<T> v, const uint8_t* HWY_RESTRICT bits) {
  uint64_t mask_bits = 0;
  constexpr size_t kNumBytes = (N + 7) / 8;
  CopyBytes<kNumBytes>(bits, &mask_bits);
  if (N < 8) {
    mask_bits &= (1ull << N) - 1;
  }

  return detail::Compress(hwy::SizeTag<sizeof(T)>(), v, mask_bits);
}

// ------------------------------ CompressStore
template <typename T>
HWY_API size_t CompressStore(Vec256<T> v, const Mask256<T> mask, Full256<T> d,
                             T* HWY_RESTRICT unaligned) {
  const uint64_t mask_bits = detail::BitsFromMask(mask);
  const auto c = detail::Compress(hwy::SizeTag<sizeof(T)>(), v, mask_bits);
  StoreU(c, d, unaligned);
  return PopCount(mask_bits);
}

// ------------------------------ CompressBlendedStore
template <typename T>
HWY_API size_t CompressBlendedStore(Vec256<T> v, Mask256<T> m, Full256<T> d,
                                    T* HWY_RESTRICT unaligned) {
  const RebindToUnsigned<decltype(d)> du;  // so we can support fp16/bf16
  using TU = TFromD<decltype(du)>;
  const uint64_t mask_bits = detail::BitsFromMask(m);
  const size_t count = PopCount(mask_bits);
  const Mask256<TU> store_mask = FirstN(du, count);
  const Vec256<TU> compressed =
      detail::Compress(hwy::SizeTag<sizeof(T)>(), BitCast(du, v), mask_bits);
  const Vec256<TU> prev = BitCast(du, LoadU(d, unaligned));
  StoreU(BitCast(d, IfThenElse(store_mask, compressed, prev)), d, unaligned);
  return count;
}

// ------------------------------ CompressBitsStore

template <typename T>
HWY_API size_t CompressBitsStore(Vec256<T> v, const uint8_t* HWY_RESTRICT bits,
                                 Full256<T> d, T* HWY_RESTRICT unaligned) {
  uint64_t mask_bits = 0;
  constexpr size_t kNumBytes = (N + 7) / 8;
  CopyBytes<kNumBytes>(bits, &mask_bits);
  if (N < 8) {
    mask_bits &= (1ull << N) - 1;
  }

  const auto c = detail::Compress(hwy::SizeTag<sizeof(T)>(), v, mask_bits);
  StoreU(c, d, unaligned);
  return PopCount(mask_bits);
}

// ------------------------------ StoreInterleaved2/3/4

// HWY_NATIVE_LOAD_STORE_INTERLEAVED not set, hence defined in
// generic_ops-inl.h.

// ------------------------------ MulEven/Odd (Load)

HWY_INLINE Vec256<uint64_t> MulEven(const Vec256<uint64_t> a,
                                    const Vec256<uint64_t> b) {
  alignas(32) uint64_t mul[2];
  mul[0] =
      Mul128(static_cast<uint64_t>(wasm_i64x2_extract_lane(a.raw, 0)),
             static_cast<uint64_t>(wasm_i64x2_extract_lane(b.raw, 0)), &mul[1]);
  return Load(Full256<uint64_t>(), mul);
}

HWY_INLINE Vec256<uint64_t> MulOdd(const Vec256<uint64_t> a,
                                   const Vec256<uint64_t> b) {
  alignas(32) uint64_t mul[2];
  mul[0] =
      Mul128(static_cast<uint64_t>(wasm_i64x2_extract_lane(a.raw, 1)),
             static_cast<uint64_t>(wasm_i64x2_extract_lane(b.raw, 1)), &mul[1]);
  return Load(Full256<uint64_t>(), mul);
}

// ------------------------------ ReorderWidenMulAccumulate (MulAdd, ZipLower)

HWY_API Vec256<float> ReorderWidenMulAccumulate(Full256<float> df32,
                                                Vec256<bfloat16_t> a,
                                                Vec256<bfloat16_t> b,
                                                const Vec256<float> sum0,
                                                Vec256<float>& sum1) {
  const Repartition<uint16_t, decltype(df32)> du16;
  const RebindToUnsigned<decltype(df32)> du32;
  const Vec256<uint16_t> zero = Zero(du16);
  const Vec256<uint32_t> a0 = ZipLower(du32, zero, BitCast(du16, a));
  const Vec256<uint32_t> a1 = ZipUpper(du32, zero, BitCast(du16, a));
  const Vec256<uint32_t> b0 = ZipLower(du32, zero, BitCast(du16, b));
  const Vec256<uint32_t> b1 = ZipUpper(du32, zero, BitCast(du16, b));
  sum1 = MulAdd(BitCast(df32, a1), BitCast(df32, b1), sum1);
  return MulAdd(BitCast(df32, a0), BitCast(df32, b0), sum0);
}

// ------------------------------ Reductions

namespace detail {

// u32/i32/f32:

template <typename T>
HWY_INLINE Vec256<T> SumOfLanes(hwy::SizeTag<4> /* tag */,
                                const Vec256<T> v3210) {
  const Vec256<T> v1032 = Shuffle1032(v3210);
  const Vec256<T> v31_20_31_20 = v3210 + v1032;
  const Vec256<T> v20_31_20_31 = Shuffle0321(v31_20_31_20);
  return v20_31_20_31 + v31_20_31_20;
}
template <typename T>
HWY_INLINE Vec256<T> MinOfLanes(hwy::SizeTag<4> /* tag */,
                                const Vec256<T> v3210) {
  const Vec256<T> v1032 = Shuffle1032(v3210);
  const Vec256<T> v31_20_31_20 = Min(v3210, v1032);
  const Vec256<T> v20_31_20_31 = Shuffle0321(v31_20_31_20);
  return Min(v20_31_20_31, v31_20_31_20);
}
template <typename T>
HWY_INLINE Vec256<T> MaxOfLanes(hwy::SizeTag<4> /* tag */,
                                const Vec256<T> v3210) {
  const Vec256<T> v1032 = Shuffle1032(v3210);
  const Vec256<T> v31_20_31_20 = Max(v3210, v1032);
  const Vec256<T> v20_31_20_31 = Shuffle0321(v31_20_31_20);
  return Max(v20_31_20_31, v31_20_31_20);
}

// u64/i64/f64:

template <typename T>
HWY_INLINE Vec256<T> SumOfLanes(hwy::SizeTag<8> /* tag */,
                                const Vec256<T> v10) {
  const Vec256<T> v01 = Shuffle01(v10);
  return v10 + v01;
}
template <typename T>
HWY_INLINE Vec256<T> MinOfLanes(hwy::SizeTag<8> /* tag */,
                                const Vec256<T> v10) {
  const Vec256<T> v01 = Shuffle01(v10);
  return Min(v10, v01);
}
template <typename T>
HWY_INLINE Vec256<T> MaxOfLanes(hwy::SizeTag<8> /* tag */,
                                const Vec256<T> v10) {
  const Vec256<T> v01 = Shuffle01(v10);
  return Max(v10, v01);
}

// u16/i16
template <typename T, HWY_IF_LANE_SIZE(T, 2)>
HWY_API Vec256<T> MinOfLanes(hwy::SizeTag<2> /* tag */, Vec256<T> v) {
  const Repartition<int32_t, Full256<T>> d32;
  const auto even = And(BitCast(d32, v), Set(d32, 0xFFFF));
  const auto odd = ShiftRight<16>(BitCast(d32, v));
  const auto min = MinOfLanes(d32, Min(even, odd));
  // Also broadcast into odd lanes.
  return BitCast(Full256<T>(), Or(min, ShiftLeft<16>(min)));
}
template <typename T, HWY_IF_LANE_SIZE(T, 2)>
HWY_API Vec256<T> MaxOfLanes(hwy::SizeTag<2> /* tag */, Vec256<T> v) {
  const Repartition<int32_t, Full256<T>> d32;
  const auto even = And(BitCast(d32, v), Set(d32, 0xFFFF));
  const auto odd = ShiftRight<16>(BitCast(d32, v));
  const auto min = MaxOfLanes(d32, Max(even, odd));
  // Also broadcast into odd lanes.
  return BitCast(Full256<T>(), Or(min, ShiftLeft<16>(min)));
}

}  // namespace detail

// Supported for u/i/f 32/64. Returns the same value in each lane.
template <typename T>
HWY_API Vec256<T> SumOfLanes(Full256<T> /* tag */, const Vec256<T> v) {
  return detail::SumOfLanes(hwy::SizeTag<sizeof(T)>(), v);
}
template <typename T>
HWY_API Vec256<T> MinOfLanes(Full256<T> /* tag */, const Vec256<T> v) {
  return detail::MinOfLanes(hwy::SizeTag<sizeof(T)>(), v);
}
template <typename T>
HWY_API Vec256<T> MaxOfLanes(Full256<T> /* tag */, const Vec256<T> v) {
  return detail::MaxOfLanes(hwy::SizeTag<sizeof(T)>(), v);
}

// ------------------------------ Lt128

template <typename T>
HWY_INLINE Mask256<T> Lt128(Full256<T> d, Vec256<T> a, Vec256<T> b) {}

template <typename T>
HWY_INLINE Mask256<T> Lt128Upper(Full256<T> d, Vec256<T> a, Vec256<T> b) {}

template <typename T>
HWY_INLINE Mask256<T> Eq128(Full256<T> d, Vec256<T> a, Vec256<T> b) {}

template <typename T>
HWY_INLINE Mask256<T> Eq128Upper(Full256<T> d, Vec256<T> a, Vec256<T> b) {}

template <typename T>
HWY_INLINE Vec256<T> Min128(Full256<T> d, Vec256<T> a, Vec256<T> b) {}

template <typename T>
HWY_INLINE Vec256<T> Max128(Full256<T> d, Vec256<T> a, Vec256<T> b) {}

template <typename T>
HWY_INLINE Vec256<T> Min128Upper(Full256<T> d, Vec256<T> a, Vec256<T> b) {}

template <typename T>
HWY_INLINE Vec256<T> Max128Upper(Full256<T> d, Vec256<T> a, Vec256<T> b) {}

// NOLINTNEXTLINE(google-readability-namespace-comments)
}  // namespace HWY_NAMESPACE
}  // namespace hwy
HWY_AFTER_NAMESPACE();
