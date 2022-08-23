// Copyright 2019 Google LLC
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

// 128-bit WASM vectors and operations.
// External include guard in highway.h - see comment there.

#include <stddef.h>
#include <stdint.h>
#include <wasm_simd128.h>

#include "hwy/base.h"
#include "hwy/ops/shared-inl.h"

#ifdef HWY_WASM_OLD_NAMES
#define wasm_i8x16_shuffle wasm_v8x16_shuffle
#define wasm_i16x8_shuffle wasm_v16x8_shuffle
#define wasm_i32x4_shuffle wasm_v32x4_shuffle
#define wasm_i64x2_shuffle wasm_v64x2_shuffle
#define wasm_u16x8_extend_low_u8x16 wasm_i16x8_widen_low_u8x16
#define wasm_u32x4_extend_low_u16x8 wasm_i32x4_widen_low_u16x8
#define wasm_i32x4_extend_low_i16x8 wasm_i32x4_widen_low_i16x8
#define wasm_i16x8_extend_low_i8x16 wasm_i16x8_widen_low_i8x16
#define wasm_u32x4_extend_high_u16x8 wasm_i32x4_widen_high_u16x8
#define wasm_i32x4_extend_high_i16x8 wasm_i32x4_widen_high_i16x8
#define wasm_i32x4_trunc_sat_f32x4 wasm_i32x4_trunc_saturate_f32x4
#define wasm_u8x16_add_sat wasm_u8x16_add_saturate
#define wasm_u8x16_sub_sat wasm_u8x16_sub_saturate
#define wasm_u16x8_add_sat wasm_u16x8_add_saturate
#define wasm_u16x8_sub_sat wasm_u16x8_sub_saturate
#define wasm_i8x16_add_sat wasm_i8x16_add_saturate
#define wasm_i8x16_sub_sat wasm_i8x16_sub_saturate
#define wasm_i16x8_add_sat wasm_i16x8_add_saturate
#define wasm_i16x8_sub_sat wasm_i16x8_sub_saturate
#endif

HWY_BEFORE_NAMESPACE();
namespace hwy {
namespace HWY_NAMESPACE {

namespace detail {

template <typename T>
struct Raw128 {
  using type = __v128_u;
};
template <>
struct Raw128<float> {
  using type = __f32x4;
};

}  // namespace detail

template <typename T, size_t N = 16 / sizeof(T)>
class Vec128 {
  using Raw = typename detail::Raw128<T>::type;

 public:
  // Compound assignment. Only usable if there is a corresponding non-member
  // binary operator overload. For example, only f32 and f64 support division.
  HWY_INLINE Vec128& operator*=(const Vec128 other) {
    return *this = (*this * other);
  }
  HWY_INLINE Vec128& operator/=(const Vec128 other) {
    return *this = (*this / other);
  }
  HWY_INLINE Vec128& operator+=(const Vec128 other) {
    return *this = (*this + other);
  }
  HWY_INLINE Vec128& operator-=(const Vec128 other) {
    return *this = (*this - other);
  }
  HWY_INLINE Vec128& operator&=(const Vec128 other) {
    return *this = (*this & other);
  }
  HWY_INLINE Vec128& operator|=(const Vec128 other) {
    return *this = (*this | other);
  }
  HWY_INLINE Vec128& operator^=(const Vec128 other) {
    return *this = (*this ^ other);
  }

  Raw raw;
};

template <typename T>
using Vec64 = Vec128<T, 8 / sizeof(T)>;

template <typename T>
using Vec32 = Vec128<T, 4 / sizeof(T)>;

// FF..FF or 0.
template <typename T, size_t N = 16 / sizeof(T)>
struct Mask128 {
  typename detail::Raw128<T>::type raw;
};

namespace detail {

// Deduce Simd<T, N, 0> from Vec128<T, N>
struct DeduceD {
  template <typename T, size_t N>
  Simd<T, N, 0> operator()(Vec128<T, N>) const {
    return Simd<T, N, 0>();
  }
};

}  // namespace detail

template <class V>
using DFromV = decltype(detail::DeduceD()(V()));

template <class V>
using TFromV = TFromD<DFromV<V>>;

// ------------------------------ BitCast

namespace detail {

HWY_INLINE __v128_u BitCastToInteger(__v128_u v) { return v; }
HWY_INLINE __v128_u BitCastToInteger(__f32x4 v) {
  return static_cast<__v128_u>(v);
}
HWY_INLINE __v128_u BitCastToInteger(__f64x2 v) {
  return static_cast<__v128_u>(v);
}

template <typename T, size_t N>
HWY_INLINE Vec128<uint8_t, N * sizeof(T)> BitCastToByte(Vec128<T, N> v) {
  return Vec128<uint8_t, N * sizeof(T)>{BitCastToInteger(v.raw)};
}

// Cannot rely on function overloading because return types differ.
template <typename T>
struct BitCastFromInteger128 {
  HWY_INLINE __v128_u operator()(__v128_u v) { return v; }
};
template <>
struct BitCastFromInteger128<float> {
  HWY_INLINE __f32x4 operator()(__v128_u v) { return static_cast<__f32x4>(v); }
};

template <typename T, size_t N>
HWY_INLINE Vec128<T, N> BitCastFromByte(Simd<T, N, 0> /* tag */,
                                        Vec128<uint8_t, N * sizeof(T)> v) {
  return Vec128<T, N>{BitCastFromInteger128<T>()(v.raw)};
}

}  // namespace detail

template <typename T, size_t N, typename FromT>
HWY_API Vec128<T, N> BitCast(Simd<T, N, 0> d,
                             Vec128<FromT, N * sizeof(T) / sizeof(FromT)> v) {
  return detail::BitCastFromByte(d, detail::BitCastToByte(v));
}

// ------------------------------ Zero

// Returns an all-zero vector/part.
template <typename T, size_t N, HWY_IF_LE128(T, N)>
HWY_API Vec128<T, N> Zero(Simd<T, N, 0> /* tag */) {
  return Vec128<T, N>{wasm_i32x4_splat(0)};
}
template <size_t N, HWY_IF_LE128(float, N)>
HWY_API Vec128<float, N> Zero(Simd<float, N, 0> /* tag */) {
  return Vec128<float, N>{wasm_f32x4_splat(0.0f)};
}

template <class D>
using VFromD = decltype(Zero(D()));

// ------------------------------ Set

// Returns a vector/part with all lanes set to "t".
template <size_t N, HWY_IF_LE128(uint8_t, N)>
HWY_API Vec128<uint8_t, N> Set(Simd<uint8_t, N, 0> /* tag */, const uint8_t t) {
  return Vec128<uint8_t, N>{wasm_i8x16_splat(static_cast<int8_t>(t))};
}
template <size_t N, HWY_IF_LE128(uint16_t, N)>
HWY_API Vec128<uint16_t, N> Set(Simd<uint16_t, N, 0> /* tag */,
                                const uint16_t t) {
  return Vec128<uint16_t, N>{wasm_i16x8_splat(static_cast<int16_t>(t))};
}
template <size_t N, HWY_IF_LE128(uint32_t, N)>
HWY_API Vec128<uint32_t, N> Set(Simd<uint32_t, N, 0> /* tag */,
                                const uint32_t t) {
  return Vec128<uint32_t, N>{wasm_i32x4_splat(static_cast<int32_t>(t))};
}
template <size_t N, HWY_IF_LE128(uint64_t, N)>
HWY_API Vec128<uint64_t, N> Set(Simd<uint64_t, N, 0> /* tag */,
                                const uint64_t t) {
  return Vec128<uint64_t, N>{wasm_i64x2_splat(static_cast<int64_t>(t))};
}

template <size_t N, HWY_IF_LE128(int8_t, N)>
HWY_API Vec128<int8_t, N> Set(Simd<int8_t, N, 0> /* tag */, const int8_t t) {
  return Vec128<int8_t, N>{wasm_i8x16_splat(t)};
}
template <size_t N, HWY_IF_LE128(int16_t, N)>
HWY_API Vec128<int16_t, N> Set(Simd<int16_t, N, 0> /* tag */, const int16_t t) {
  return Vec128<int16_t, N>{wasm_i16x8_splat(t)};
}
template <size_t N, HWY_IF_LE128(int32_t, N)>
HWY_API Vec128<int32_t, N> Set(Simd<int32_t, N, 0> /* tag */, const int32_t t) {
  return Vec128<int32_t, N>{wasm_i32x4_splat(t)};
}
template <size_t N, HWY_IF_LE128(int64_t, N)>
HWY_API Vec128<int64_t, N> Set(Simd<int64_t, N, 0> /* tag */, const int64_t t) {
  return Vec128<int64_t, N>{wasm_i64x2_splat(t)};
}

template <size_t N, HWY_IF_LE128(float, N)>
HWY_API Vec128<float, N> Set(Simd<float, N, 0> /* tag */, const float t) {
  return Vec128<float, N>{wasm_f32x4_splat(t)};
}

HWY_DIAGNOSTICS(push)
HWY_DIAGNOSTICS_OFF(disable : 4700, ignored "-Wuninitialized")

// Returns a vector with uninitialized elements.
template <typename T, size_t N, HWY_IF_LE128(T, N)>
HWY_API Vec128<T, N> Undefined(Simd<T, N, 0> d) {
  return Zero(d);
}

HWY_DIAGNOSTICS(pop)

// Returns a vector with lane i=[0, N) set to "first" + i.
template <typename T, size_t N, typename T2>
Vec128<T, N> Iota(const Simd<T, N, 0> d, const T2 first) {
  HWY_ALIGN T lanes[16 / sizeof(T)];
  for (size_t i = 0; i < 16 / sizeof(T); ++i) {
    lanes[i] = static_cast<T>(first + static_cast<T2>(i));
  }
  return Load(d, lanes);
}

// ================================================== ARITHMETIC

// ------------------------------ Addition

// Unsigned
template <size_t N>
HWY_API Vec128<uint8_t, N> operator+(const Vec128<uint8_t, N> a,
                                     const Vec128<uint8_t, N> b) {
  return Vec128<uint8_t, N>{wasm_i8x16_add(a.raw, b.raw)};
}
template <size_t N>
HWY_API Vec128<uint16_t, N> operator+(const Vec128<uint16_t, N> a,
                                      const Vec128<uint16_t, N> b) {
  return Vec128<uint16_t, N>{wasm_i16x8_add(a.raw, b.raw)};
}
template <size_t N>
HWY_API Vec128<uint32_t, N> operator+(const Vec128<uint32_t, N> a,
                                      const Vec128<uint32_t, N> b) {
  return Vec128<uint32_t, N>{wasm_i32x4_add(a.raw, b.raw)};
}
template <size_t N>
HWY_API Vec128<uint64_t, N> operator+(const Vec128<uint64_t, N> a,
                                      const Vec128<uint64_t, N> b) {
  return Vec128<uint64_t, N>{wasm_i64x2_add(a.raw, b.raw)};
}

// Signed
template <size_t N>
HWY_API Vec128<int8_t, N> operator+(const Vec128<int8_t, N> a,
                                    const Vec128<int8_t, N> b) {
  return Vec128<int8_t, N>{wasm_i8x16_add(a.raw, b.raw)};
}
template <size_t N>
HWY_API Vec128<int16_t, N> operator+(const Vec128<int16_t, N> a,
                                     const Vec128<int16_t, N> b) {
  return Vec128<int16_t, N>{wasm_i16x8_add(a.raw, b.raw)};
}
template <size_t N>
HWY_API Vec128<int32_t, N> operator+(const Vec128<int32_t, N> a,
                                     const Vec128<int32_t, N> b) {
  return Vec128<int32_t, N>{wasm_i32x4_add(a.raw, b.raw)};
}
template <size_t N>
HWY_API Vec128<int64_t, N> operator+(const Vec128<int64_t, N> a,
                                     const Vec128<int64_t, N> b) {
  return Vec128<int64_t, N>{wasm_i64x2_add(a.raw, b.raw)};
}

// Float
template <size_t N>
HWY_API Vec128<float, N> operator+(const Vec128<float, N> a,
                                   const Vec128<float, N> b) {
  return Vec128<float, N>{wasm_f32x4_add(a.raw, b.raw)};
}

// ------------------------------ Subtraction

// Unsigned
template <size_t N>
HWY_API Vec128<uint8_t, N> operator-(const Vec128<uint8_t, N> a,
                                     const Vec128<uint8_t, N> b) {
  return Vec128<uint8_t, N>{wasm_i8x16_sub(a.raw, b.raw)};
}
template <size_t N>
HWY_API Vec128<uint16_t, N> operator-(Vec128<uint16_t, N> a,
                                      Vec128<uint16_t, N> b) {
  return Vec128<uint16_t, N>{wasm_i16x8_sub(a.raw, b.raw)};
}
template <size_t N>
HWY_API Vec128<uint32_t, N> operator-(const Vec128<uint32_t, N> a,
                                      const Vec128<uint32_t, N> b) {
  return Vec128<uint32_t, N>{wasm_i32x4_sub(a.raw, b.raw)};
}
template <size_t N>
HWY_API Vec128<uint64_t, N> operator-(const Vec128<uint64_t, N> a,
                                      const Vec128<uint64_t, N> b) {
  return Vec128<uint64_t, N>{wasm_i64x2_sub(a.raw, b.raw)};
}

// Signed
template <size_t N>
HWY_API Vec128<int8_t, N> operator-(const Vec128<int8_t, N> a,
                                    const Vec128<int8_t, N> b) {
  return Vec128<int8_t, N>{wasm_i8x16_sub(a.raw, b.raw)};
}
template <size_t N>
HWY_API Vec128<int16_t, N> operator-(const Vec128<int16_t, N> a,
                                     const Vec128<int16_t, N> b) {
  return Vec128<int16_t, N>{wasm_i16x8_sub(a.raw, b.raw)};
}
template <size_t N>
HWY_API Vec128<int32_t, N> operator-(const Vec128<int32_t, N> a,
                                     const Vec128<int32_t, N> b) {
  return Vec128<int32_t, N>{wasm_i32x4_sub(a.raw, b.raw)};
}
template <size_t N>
HWY_API Vec128<int64_t, N> operator-(const Vec128<int64_t, N> a,
                                     const Vec128<int64_t, N> b) {
  return Vec128<int64_t, N>{wasm_i64x2_sub(a.raw, b.raw)};
}

// Float
template <size_t N>
HWY_API Vec128<float, N> operator-(const Vec128<float, N> a,
                                   const Vec128<float, N> b) {
  return Vec128<float, N>{wasm_f32x4_sub(a.raw, b.raw)};
}

// ------------------------------ SaturatedAdd

// Returns a + b clamped to the destination range.

// Unsigned
template <size_t N>
HWY_API Vec128<uint8_t, N> SaturatedAdd(const Vec128<uint8_t, N> a,
                                        const Vec128<uint8_t, N> b) {
  return Vec128<uint8_t, N>{wasm_u8x16_add_sat(a.raw, b.raw)};
}
template <size_t N>
HWY_API Vec128<uint16_t, N> SaturatedAdd(const Vec128<uint16_t, N> a,
                                         const Vec128<uint16_t, N> b) {
  return Vec128<uint16_t, N>{wasm_u16x8_add_sat(a.raw, b.raw)};
}

// Signed
template <size_t N>
HWY_API Vec128<int8_t, N> SaturatedAdd(const Vec128<int8_t, N> a,
                                       const Vec128<int8_t, N> b) {
  return Vec128<int8_t, N>{wasm_i8x16_add_sat(a.raw, b.raw)};
}
template <size_t N>
HWY_API Vec128<int16_t, N> SaturatedAdd(const Vec128<int16_t, N> a,
                                        const Vec128<int16_t, N> b) {
  return Vec128<int16_t, N>{wasm_i16x8_add_sat(a.raw, b.raw)};
}

// ------------------------------ SaturatedSub

// Returns a - b clamped to the destination range.

// Unsigned
template <size_t N>
HWY_API Vec128<uint8_t, N> SaturatedSub(const Vec128<uint8_t, N> a,
                                        const Vec128<uint8_t, N> b) {
  return Vec128<uint8_t, N>{wasm_u8x16_sub_sat(a.raw, b.raw)};
}
template <size_t N>
HWY_API Vec128<uint16_t, N> SaturatedSub(const Vec128<uint16_t, N> a,
                                         const Vec128<uint16_t, N> b) {
  return Vec128<uint16_t, N>{wasm_u16x8_sub_sat(a.raw, b.raw)};
}

// Signed
template <size_t N>
HWY_API Vec128<int8_t, N> SaturatedSub(const Vec128<int8_t, N> a,
                                       const Vec128<int8_t, N> b) {
  return Vec128<int8_t, N>{wasm_i8x16_sub_sat(a.raw, b.raw)};
}
template <size_t N>
HWY_API Vec128<int16_t, N> SaturatedSub(const Vec128<int16_t, N> a,
                                        const Vec128<int16_t, N> b) {
  return Vec128<int16_t, N>{wasm_i16x8_sub_sat(a.raw, b.raw)};
}

// ------------------------------ Average

// Returns (a + b + 1) / 2

// Unsigned
template <size_t N>
HWY_API Vec128<uint8_t, N> AverageRound(const Vec128<uint8_t, N> a,
                                        const Vec128<uint8_t, N> b) {
  return Vec128<uint8_t, N>{wasm_u8x16_avgr(a.raw, b.raw)};
}
template <size_t N>
HWY_API Vec128<uint16_t, N> AverageRound(const Vec128<uint16_t, N> a,
                                         const Vec128<uint16_t, N> b) {
  return Vec128<uint16_t, N>{wasm_u16x8_avgr(a.raw, b.raw)};
}

// ------------------------------ Absolute value

// Returns absolute value, except that LimitsMin() maps to LimitsMax() + 1.
template <size_t N>
HWY_API Vec128<int8_t, N> Abs(const Vec128<int8_t, N> v) {
  return Vec128<int8_t, N>{wasm_i8x16_abs(v.raw)};
}
template <size_t N>
HWY_API Vec128<int16_t, N> Abs(const Vec128<int16_t, N> v) {
  return Vec128<int16_t, N>{wasm_i16x8_abs(v.raw)};
}
template <size_t N>
HWY_API Vec128<int32_t, N> Abs(const Vec128<int32_t, N> v) {
  return Vec128<int32_t, N>{wasm_i32x4_abs(v.raw)};
}
template <size_t N>
HWY_API Vec128<int64_t, N> Abs(const Vec128<int64_t, N> v) {
  return Vec128<int64_t, N>{wasm_i64x2_abs(v.raw)};
}

template <size_t N>
HWY_API Vec128<float, N> Abs(const Vec128<float, N> v) {
  return Vec128<float, N>{wasm_f32x4_abs(v.raw)};
}

// ------------------------------ Shift lanes by constant #bits

// Unsigned
template <int kBits, size_t N>
HWY_API Vec128<uint16_t, N> ShiftLeft(const Vec128<uint16_t, N> v) {
  return Vec128<uint16_t, N>{wasm_i16x8_shl(v.raw, kBits)};
}
template <int kBits, size_t N>
HWY_API Vec128<uint16_t, N> ShiftRight(const Vec128<uint16_t, N> v) {
  return Vec128<uint16_t, N>{wasm_u16x8_shr(v.raw, kBits)};
}
template <int kBits, size_t N>
HWY_API Vec128<uint32_t, N> ShiftLeft(const Vec128<uint32_t, N> v) {
  return Vec128<uint32_t, N>{wasm_i32x4_shl(v.raw, kBits)};
}
template <int kBits, size_t N>
HWY_API Vec128<uint64_t, N> ShiftLeft(const Vec128<uint64_t, N> v) {
  return Vec128<uint64_t, N>{wasm_i64x2_shl(v.raw, kBits)};
}
template <int kBits, size_t N>
HWY_API Vec128<uint32_t, N> ShiftRight(const Vec128<uint32_t, N> v) {
  return Vec128<uint32_t, N>{wasm_u32x4_shr(v.raw, kBits)};
}
template <int kBits, size_t N>
HWY_API Vec128<uint64_t, N> ShiftRight(const Vec128<uint64_t, N> v) {
  return Vec128<uint64_t, N>{wasm_u64x2_shr(v.raw, kBits)};
}

// Signed
template <int kBits, size_t N>
HWY_API Vec128<int16_t, N> ShiftLeft(const Vec128<int16_t, N> v) {
  return Vec128<int16_t, N>{wasm_i16x8_shl(v.raw, kBits)};
}
template <int kBits, size_t N>
HWY_API Vec128<int16_t, N> ShiftRight(const Vec128<int16_t, N> v) {
  return Vec128<int16_t, N>{wasm_i16x8_shr(v.raw, kBits)};
}
template <int kBits, size_t N>
HWY_API Vec128<int32_t, N> ShiftLeft(const Vec128<int32_t, N> v) {
  return Vec128<int32_t, N>{wasm_i32x4_shl(v.raw, kBits)};
}
template <int kBits, size_t N>
HWY_API Vec128<int64_t, N> ShiftLeft(const Vec128<int64_t, N> v) {
  return Vec128<int64_t, N>{wasm_i64x2_shl(v.raw, kBits)};
}
template <int kBits, size_t N>
HWY_API Vec128<int32_t, N> ShiftRight(const Vec128<int32_t, N> v) {
  return Vec128<int32_t, N>{wasm_i32x4_shr(v.raw, kBits)};
}
template <int kBits, size_t N>
HWY_API Vec128<int64_t, N> ShiftRight(const Vec128<int64_t, N> v) {
  return Vec128<int64_t, N>{wasm_i64x2_shr(v.raw, kBits)};
}

// 8-bit
template <int kBits, typename T, size_t N, HWY_IF_LANE_SIZE(T, 1)>
HWY_API Vec128<T, N> ShiftLeft(const Vec128<T, N> v) {
  const DFromV<decltype(v)> d8;
  // Use raw instead of BitCast to support N=1.
  const Vec128<T, N> shifted{ShiftLeft<kBits>(Vec128<MakeWide<T>>{v.raw}).raw};
  return kBits == 1
             ? (v + v)
             : (shifted & Set(d8, static_cast<T>((0xFF << kBits) & 0xFF)));
}

template <int kBits, size_t N>
HWY_API Vec128<uint8_t, N> ShiftRight(const Vec128<uint8_t, N> v) {
  const DFromV<decltype(v)> d8;
  // Use raw instead of BitCast to support N=1.
  const Vec128<uint8_t, N> shifted{
      ShiftRight<kBits>(Vec128<uint16_t>{v.raw}).raw};
  return shifted & Set(d8, 0xFF >> kBits);
}

template <int kBits, size_t N>
HWY_API Vec128<int8_t, N> ShiftRight(const Vec128<int8_t, N> v) {
  const DFromV<decltype(v)> di;
  const RebindToUnsigned<decltype(di)> du;
  const auto shifted = BitCast(di, ShiftRight<kBits>(BitCast(du, v)));
  const auto shifted_sign = BitCast(di, Set(du, 0x80 >> kBits));
  return (shifted ^ shifted_sign) - shifted_sign;
}

// ------------------------------ RotateRight (ShiftRight, Or)
template <int kBits, typename T, size_t N>
HWY_API Vec128<T, N> RotateRight(const Vec128<T, N> v) {
  constexpr size_t kSizeInBits = sizeof(T) * 8;
  static_assert(0 <= kBits && kBits < kSizeInBits, "Invalid shift count");
  if (kBits == 0) return v;
  return Or(ShiftRight<kBits>(v), ShiftLeft<kSizeInBits - kBits>(v));
}

// ------------------------------ Shift lanes by same variable #bits

// After https://reviews.llvm.org/D108415 shift argument became unsigned.
HWY_DIAGNOSTICS(push)
HWY_DIAGNOSTICS_OFF(disable : 4245 4365, ignored "-Wsign-conversion")

// Unsigned
template <size_t N>
HWY_API Vec128<uint16_t, N> ShiftLeftSame(const Vec128<uint16_t, N> v,
                                          const int bits) {
  return Vec128<uint16_t, N>{wasm_i16x8_shl(v.raw, bits)};
}
template <size_t N>
HWY_API Vec128<uint16_t, N> ShiftRightSame(const Vec128<uint16_t, N> v,
                                           const int bits) {
  return Vec128<uint16_t, N>{wasm_u16x8_shr(v.raw, bits)};
}
template <size_t N>
HWY_API Vec128<uint32_t, N> ShiftLeftSame(const Vec128<uint32_t, N> v,
                                          const int bits) {
  return Vec128<uint32_t, N>{wasm_i32x4_shl(v.raw, bits)};
}
template <size_t N>
HWY_API Vec128<uint32_t, N> ShiftRightSame(const Vec128<uint32_t, N> v,
                                           const int bits) {
  return Vec128<uint32_t, N>{wasm_u32x4_shr(v.raw, bits)};
}
template <size_t N>
HWY_API Vec128<uint64_t, N> ShiftLeftSame(const Vec128<uint64_t, N> v,
                                          const int bits) {
  return Vec128<uint64_t, N>{wasm_i64x2_shl(v.raw, bits)};
}
template <size_t N>
HWY_API Vec128<uint64_t, N> ShiftRightSame(const Vec128<uint64_t, N> v,
                                           const int bits) {
  return Vec128<uint64_t, N>{wasm_u64x2_shr(v.raw, bits)};
}

// Signed
template <size_t N>
HWY_API Vec128<int16_t, N> ShiftLeftSame(const Vec128<int16_t, N> v,
                                         const int bits) {
  return Vec128<int16_t, N>{wasm_i16x8_shl(v.raw, bits)};
}
template <size_t N>
HWY_API Vec128<int16_t, N> ShiftRightSame(const Vec128<int16_t, N> v,
                                          const int bits) {
  return Vec128<int16_t, N>{wasm_i16x8_shr(v.raw, bits)};
}
template <size_t N>
HWY_API Vec128<int32_t, N> ShiftLeftSame(const Vec128<int32_t, N> v,
                                         const int bits) {
  return Vec128<int32_t, N>{wasm_i32x4_shl(v.raw, bits)};
}
template <size_t N>
HWY_API Vec128<int32_t, N> ShiftRightSame(const Vec128<int32_t, N> v,
                                          const int bits) {
  return Vec128<int32_t, N>{wasm_i32x4_shr(v.raw, bits)};
}
template <size_t N>
HWY_API Vec128<int64_t, N> ShiftLeftSame(const Vec128<int64_t, N> v,
                                         const int bits) {
  return Vec128<int64_t, N>{wasm_i64x2_shl(v.raw, bits)};
}
template <size_t N>
HWY_API Vec128<int64_t, N> ShiftRightSame(const Vec128<int64_t, N> v,
                                          const int bits) {
  return Vec128<int64_t, N>{wasm_i64x2_shr(v.raw, bits)};
}

// 8-bit
template <typename T, size_t N, HWY_IF_LANE_SIZE(T, 1)>
HWY_API Vec128<T, N> ShiftLeftSame(const Vec128<T, N> v, const int bits) {
  const DFromV<decltype(v)> d8;
  // Use raw instead of BitCast to support N=1.
  const Vec128<T, N> shifted{
      ShiftLeftSame(Vec128<MakeWide<T>>{v.raw}, bits).raw};
  return shifted & Set(d8, static_cast<T>((0xFF << bits) & 0xFF));
}

template <size_t N>
HWY_API Vec128<uint8_t, N> ShiftRightSame(Vec128<uint8_t, N> v,
                                          const int bits) {
  const DFromV<decltype(v)> d8;
  // Use raw instead of BitCast to support N=1.
  const Vec128<uint8_t, N> shifted{
      ShiftRightSame(Vec128<uint16_t>{v.raw}, bits).raw};
  return shifted & Set(d8, 0xFF >> bits);
}

template <size_t N>
HWY_API Vec128<int8_t, N> ShiftRightSame(Vec128<int8_t, N> v, const int bits) {
  const DFromV<decltype(v)> di;
  const RebindToUnsigned<decltype(di)> du;
  const auto shifted = BitCast(di, ShiftRightSame(BitCast(du, v), bits));
  const auto shifted_sign = BitCast(di, Set(du, 0x80 >> bits));
  return (shifted ^ shifted_sign) - shifted_sign;
}

// ignore Wsign-conversion
HWY_DIAGNOSTICS(pop)

// ------------------------------ Minimum

// Unsigned
template <size_t N>
HWY_API Vec128<uint8_t, N> Min(Vec128<uint8_t, N> a, Vec128<uint8_t, N> b) {
  return Vec128<uint8_t, N>{wasm_u8x16_min(a.raw, b.raw)};
}
template <size_t N>
HWY_API Vec128<uint16_t, N> Min(Vec128<uint16_t, N> a, Vec128<uint16_t, N> b) {
  return Vec128<uint16_t, N>{wasm_u16x8_min(a.raw, b.raw)};
}
template <size_t N>
HWY_API Vec128<uint32_t, N> Min(Vec128<uint32_t, N> a, Vec128<uint32_t, N> b) {
  return Vec128<uint32_t, N>{wasm_u32x4_min(a.raw, b.raw)};
}
template <size_t N>
HWY_API Vec128<uint64_t, N> Min(Vec128<uint64_t, N> a, Vec128<uint64_t, N> b) {
  // Avoid wasm_u64x2_extract_lane - not all implementations have it yet.
  const uint64_t a0 = static_cast<uint64_t>(wasm_i64x2_extract_lane(a.raw, 0));
  const uint64_t b0 = static_cast<uint64_t>(wasm_i64x2_extract_lane(b.raw, 0));
  const uint64_t a1 = static_cast<uint64_t>(wasm_i64x2_extract_lane(a.raw, 1));
  const uint64_t b1 = static_cast<uint64_t>(wasm_i64x2_extract_lane(b.raw, 1));
  alignas(16) uint64_t min[2] = {HWY_MIN(a0, b0), HWY_MIN(a1, b1)};
  return Vec128<uint64_t, N>{wasm_v128_load(min)};
}

// Signed
template <size_t N>
HWY_API Vec128<int8_t, N> Min(Vec128<int8_t, N> a, Vec128<int8_t, N> b) {
  return Vec128<int8_t, N>{wasm_i8x16_min(a.raw, b.raw)};
}
template <size_t N>
HWY_API Vec128<int16_t, N> Min(Vec128<int16_t, N> a, Vec128<int16_t, N> b) {
  return Vec128<int16_t, N>{wasm_i16x8_min(a.raw, b.raw)};
}
template <size_t N>
HWY_API Vec128<int32_t, N> Min(Vec128<int32_t, N> a, Vec128<int32_t, N> b) {
  return Vec128<int32_t, N>{wasm_i32x4_min(a.raw, b.raw)};
}
template <size_t N>
HWY_API Vec128<int64_t, N> Min(Vec128<int64_t, N> a, Vec128<int64_t, N> b) {
  alignas(16) int64_t min[4];
  min[0] = HWY_MIN(wasm_i64x2_extract_lane(a.raw, 0),
                   wasm_i64x2_extract_lane(b.raw, 0));
  min[1] = HWY_MIN(wasm_i64x2_extract_lane(a.raw, 1),
                   wasm_i64x2_extract_lane(b.raw, 1));
  return Vec128<int64_t, N>{wasm_v128_load(min)};
}

// Float
template <size_t N>
HWY_API Vec128<float, N> Min(Vec128<float, N> a, Vec128<float, N> b) {
  return Vec128<float, N>{wasm_f32x4_min(a.raw, b.raw)};
}

// ------------------------------ Maximum

// Unsigned
template <size_t N>
HWY_API Vec128<uint8_t, N> Max(Vec128<uint8_t, N> a, Vec128<uint8_t, N> b) {
  return Vec128<uint8_t, N>{wasm_u8x16_max(a.raw, b.raw)};
}
template <size_t N>
HWY_API Vec128<uint16_t, N> Max(Vec128<uint16_t, N> a, Vec128<uint16_t, N> b) {
  return Vec128<uint16_t, N>{wasm_u16x8_max(a.raw, b.raw)};
}
template <size_t N>
HWY_API Vec128<uint32_t, N> Max(Vec128<uint32_t, N> a, Vec128<uint32_t, N> b) {
  return Vec128<uint32_t, N>{wasm_u32x4_max(a.raw, b.raw)};
}
template <size_t N>
HWY_API Vec128<uint64_t, N> Max(Vec128<uint64_t, N> a, Vec128<uint64_t, N> b) {
  // Avoid wasm_u64x2_extract_lane - not all implementations have it yet.
  const uint64_t a0 = static_cast<uint64_t>(wasm_i64x2_extract_lane(a.raw, 0));
  const uint64_t b0 = static_cast<uint64_t>(wasm_i64x2_extract_lane(b.raw, 0));
  const uint64_t a1 = static_cast<uint64_t>(wasm_i64x2_extract_lane(a.raw, 1));
  const uint64_t b1 = static_cast<uint64_t>(wasm_i64x2_extract_lane(b.raw, 1));
  alignas(16) uint64_t max[2] = {HWY_MAX(a0, b0), HWY_MAX(a1, b1)};
  return Vec128<uint64_t, N>{wasm_v128_load(max)};
}

// Signed
template <size_t N>
HWY_API Vec128<int8_t, N> Max(Vec128<int8_t, N> a, Vec128<int8_t, N> b) {
  return Vec128<int8_t, N>{wasm_i8x16_max(a.raw, b.raw)};
}
template <size_t N>
HWY_API Vec128<int16_t, N> Max(Vec128<int16_t, N> a, Vec128<int16_t, N> b) {
  return Vec128<int16_t, N>{wasm_i16x8_max(a.raw, b.raw)};
}
template <size_t N>
HWY_API Vec128<int32_t, N> Max(Vec128<int32_t, N> a, Vec128<int32_t, N> b) {
  return Vec128<int32_t, N>{wasm_i32x4_max(a.raw, b.raw)};
}
template <size_t N>
HWY_API Vec128<int64_t, N> Max(Vec128<int64_t, N> a, Vec128<int64_t, N> b) {
  alignas(16) int64_t max[2];
  max[0] = HWY_MAX(wasm_i64x2_extract_lane(a.raw, 0),
                   wasm_i64x2_extract_lane(b.raw, 0));
  max[1] = HWY_MAX(wasm_i64x2_extract_lane(a.raw, 1),
                   wasm_i64x2_extract_lane(b.raw, 1));
  return Vec128<int64_t, N>{wasm_v128_load(max)};
}

// Float
template <size_t N>
HWY_API Vec128<float, N> Max(Vec128<float, N> a, Vec128<float, N> b) {
  return Vec128<float, N>{wasm_f32x4_max(a.raw, b.raw)};
}

// ------------------------------ Integer multiplication

// Unsigned
template <size_t N>
HWY_API Vec128<uint16_t, N> operator*(const Vec128<uint16_t, N> a,
                                      const Vec128<uint16_t, N> b) {
  return Vec128<uint16_t, N>{wasm_i16x8_mul(a.raw, b.raw)};
}
template <size_t N>
HWY_API Vec128<uint32_t, N> operator*(const Vec128<uint32_t, N> a,
                                      const Vec128<uint32_t, N> b) {
  return Vec128<uint32_t, N>{wasm_i32x4_mul(a.raw, b.raw)};
}

// Signed
template <size_t N>
HWY_API Vec128<int16_t, N> operator*(const Vec128<int16_t, N> a,
                                     const Vec128<int16_t, N> b) {
  return Vec128<int16_t, N>{wasm_i16x8_mul(a.raw, b.raw)};
}
template <size_t N>
HWY_API Vec128<int32_t, N> operator*(const Vec128<int32_t, N> a,
                                     const Vec128<int32_t, N> b) {
  return Vec128<int32_t, N>{wasm_i32x4_mul(a.raw, b.raw)};
}

// Returns the upper 16 bits of a * b in each lane.
template <size_t N>
HWY_API Vec128<uint16_t, N> MulHigh(const Vec128<uint16_t, N> a,
                                    const Vec128<uint16_t, N> b) {
  // TODO(eustas): replace, when implemented in WASM.
  const auto al = wasm_u32x4_extend_low_u16x8(a.raw);
  const auto ah = wasm_u32x4_extend_high_u16x8(a.raw);
  const auto bl = wasm_u32x4_extend_low_u16x8(b.raw);
  const auto bh = wasm_u32x4_extend_high_u16x8(b.raw);
  const auto l = wasm_i32x4_mul(al, bl);
  const auto h = wasm_i32x4_mul(ah, bh);
  // TODO(eustas): shift-right + narrow?
  return Vec128<uint16_t, N>{
      wasm_i16x8_shuffle(l, h, 1, 3, 5, 7, 9, 11, 13, 15)};
}
template <size_t N>
HWY_API Vec128<int16_t, N> MulHigh(const Vec128<int16_t, N> a,
                                   const Vec128<int16_t, N> b) {
  // TODO(eustas): replace, when implemented in WASM.
  const auto al = wasm_i32x4_extend_low_i16x8(a.raw);
  const auto ah = wasm_i32x4_extend_high_i16x8(a.raw);
  const auto bl = wasm_i32x4_extend_low_i16x8(b.raw);
  const auto bh = wasm_i32x4_extend_high_i16x8(b.raw);
  const auto l = wasm_i32x4_mul(al, bl);
  const auto h = wasm_i32x4_mul(ah, bh);
  // TODO(eustas): shift-right + narrow?
  return Vec128<int16_t, N>{
      wasm_i16x8_shuffle(l, h, 1, 3, 5, 7, 9, 11, 13, 15)};
}

template <size_t N>
HWY_API Vec128<int16_t, N> MulFixedPoint15(Vec128<int16_t, N> a,
                                           Vec128<int16_t, N> b) {
  const DFromV<decltype(a)> d;
  const RebindToUnsigned<decltype(d)> du;

  const Vec128<uint16_t, N> lo = BitCast(du, Mul(a, b));
  const Vec128<int16_t, N> hi = MulHigh(a, b);
  // We want (lo + 0x4000) >> 15, but that can overflow, and if it does we must
  // carry that into the result. Instead isolate the top two bits because only
  // they can influence the result.
  const Vec128<uint16_t, N> lo_top2 = ShiftRight<14>(lo);
  // Bits 11: add 2, 10: add 1, 01: add 1, 00: add 0.
  const Vec128<uint16_t, N> rounding = ShiftRight<1>(Add(lo_top2, Set(du, 1)));
  return Add(Add(hi, hi), BitCast(d, rounding));
}

// Multiplies even lanes (0, 2 ..) and returns the double-width result.
template <size_t N>
HWY_API Vec128<int64_t, (N + 1) / 2> MulEven(const Vec128<int32_t, N> a,
                                             const Vec128<int32_t, N> b) {
  // TODO(eustas): replace, when implemented in WASM.
  const auto kEvenMask = wasm_i32x4_make(-1, 0, -1, 0);
  const auto ae = wasm_v128_and(a.raw, kEvenMask);
  const auto be = wasm_v128_and(b.raw, kEvenMask);
  return Vec128<int64_t, (N + 1) / 2>{wasm_i64x2_mul(ae, be)};
}
template <size_t N>
HWY_API Vec128<uint64_t, (N + 1) / 2> MulEven(const Vec128<uint32_t, N> a,
                                              const Vec128<uint32_t, N> b) {
  // TODO(eustas): replace, when implemented in WASM.
  const auto kEvenMask = wasm_i32x4_make(-1, 0, -1, 0);
  const auto ae = wasm_v128_and(a.raw, kEvenMask);
  const auto be = wasm_v128_and(b.raw, kEvenMask);
  return Vec128<uint64_t, (N + 1) / 2>{wasm_i64x2_mul(ae, be)};
}

// ------------------------------ Negate

template <typename T, size_t N, HWY_IF_FLOAT(T)>
HWY_API Vec128<T, N> Neg(const Vec128<T, N> v) {
  return Xor(v, SignBit(DFromV<decltype(v)>()));
}

template <size_t N>
HWY_API Vec128<int8_t, N> Neg(const Vec128<int8_t, N> v) {
  return Vec128<int8_t, N>{wasm_i8x16_neg(v.raw)};
}
template <size_t N>
HWY_API Vec128<int16_t, N> Neg(const Vec128<int16_t, N> v) {
  return Vec128<int16_t, N>{wasm_i16x8_neg(v.raw)};
}
template <size_t N>
HWY_API Vec128<int32_t, N> Neg(const Vec128<int32_t, N> v) {
  return Vec128<int32_t, N>{wasm_i32x4_neg(v.raw)};
}
template <size_t N>
HWY_API Vec128<int64_t, N> Neg(const Vec128<int64_t, N> v) {
  return Vec128<int64_t, N>{wasm_i64x2_neg(v.raw)};
}

// ------------------------------ Floating-point mul / div

template <size_t N>
HWY_API Vec128<float, N> operator*(Vec128<float, N> a, Vec128<float, N> b) {
  return Vec128<float, N>{wasm_f32x4_mul(a.raw, b.raw)};
}

template <size_t N>
HWY_API Vec128<float, N> operator/(const Vec128<float, N> a,
                                   const Vec128<float, N> b) {
  return Vec128<float, N>{wasm_f32x4_div(a.raw, b.raw)};
}

// Approximate reciprocal
template <size_t N>
HWY_API Vec128<float, N> ApproximateReciprocal(const Vec128<float, N> v) {
  const Vec128<float, N> one = Vec128<float, N>{wasm_f32x4_splat(1.0f)};
  return one / v;
}

// Absolute value of difference.
template <size_t N>
HWY_API Vec128<float, N> AbsDiff(const Vec128<float, N> a,
                                 const Vec128<float, N> b) {
  return Abs(a - b);
}

// ------------------------------ Floating-point multiply-add variants

// Returns mul * x + add
template <size_t N>
HWY_API Vec128<float, N> MulAdd(const Vec128<float, N> mul,
                                const Vec128<float, N> x,
                                const Vec128<float, N> add) {
  // TODO(eustas): replace, when implemented in WASM.
  // TODO(eustas): is it wasm_f32x4_qfma?
  return mul * x + add;
}

// Returns add - mul * x
template <size_t N>
HWY_API Vec128<float, N> NegMulAdd(const Vec128<float, N> mul,
                                   const Vec128<float, N> x,
                                   const Vec128<float, N> add) {
  // TODO(eustas): replace, when implemented in WASM.
  return add - mul * x;
}

// Returns mul * x - sub
template <size_t N>
HWY_API Vec128<float, N> MulSub(const Vec128<float, N> mul,
                                const Vec128<float, N> x,
                                const Vec128<float, N> sub) {
  // TODO(eustas): replace, when implemented in WASM.
  // TODO(eustas): is it wasm_f32x4_qfms?
  return mul * x - sub;
}

// Returns -mul * x - sub
template <size_t N>
HWY_API Vec128<float, N> NegMulSub(const Vec128<float, N> mul,
                                   const Vec128<float, N> x,
                                   const Vec128<float, N> sub) {
  // TODO(eustas): replace, when implemented in WASM.
  return Neg(mul) * x - sub;
}

// ------------------------------ Floating-point square root

// Full precision square root
template <size_t N>
HWY_API Vec128<float, N> Sqrt(const Vec128<float, N> v) {
  return Vec128<float, N>{wasm_f32x4_sqrt(v.raw)};
}

// Approximate reciprocal square root
template <size_t N>
HWY_API Vec128<float, N> ApproximateReciprocalSqrt(const Vec128<float, N> v) {
  // TODO(eustas): find cheaper a way to calculate this.
  const Vec128<float, N> one = Vec128<float, N>{wasm_f32x4_splat(1.0f)};
  return one / Sqrt(v);
}

// ------------------------------ Floating-point rounding

// Toward nearest integer, ties to even
template <size_t N>
HWY_API Vec128<float, N> Round(const Vec128<float, N> v) {
  return Vec128<float, N>{wasm_f32x4_nearest(v.raw)};
}

// Toward zero, aka truncate
template <size_t N>
HWY_API Vec128<float, N> Trunc(const Vec128<float, N> v) {
  return Vec128<float, N>{wasm_f32x4_trunc(v.raw)};
}

// Toward +infinity, aka ceiling
template <size_t N>
HWY_API Vec128<float, N> Ceil(const Vec128<float, N> v) {
  return Vec128<float, N>{wasm_f32x4_ceil(v.raw)};
}

// Toward -infinity, aka floor
template <size_t N>
HWY_API Vec128<float, N> Floor(const Vec128<float, N> v) {
  return Vec128<float, N>{wasm_f32x4_floor(v.raw)};
}

// ------------------------------ Floating-point classification
template <typename T, size_t N>
HWY_API Mask128<T, N> IsNaN(const Vec128<T, N> v) {
  return v != v;
}

template <typename T, size_t N, HWY_IF_FLOAT(T)>
HWY_API Mask128<T, N> IsInf(const Vec128<T, N> v) {
  const Simd<T, N, 0> d;
  const RebindToSigned<decltype(d)> di;
  const VFromD<decltype(di)> vi = BitCast(di, v);
  // 'Shift left' to clear the sign bit, check for exponent=max and mantissa=0.
  return RebindMask(d, Eq(Add(vi, vi), Set(di, hwy::MaxExponentTimes2<T>())));
}

// Returns whether normal/subnormal/zero.
template <typename T, size_t N, HWY_IF_FLOAT(T)>
HWY_API Mask128<T, N> IsFinite(const Vec128<T, N> v) {
  const Simd<T, N, 0> d;
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

template <typename TFrom, typename TTo, size_t N>
HWY_API Mask128<TTo, N> RebindMask(Simd<TTo, N, 0> /*tag*/,
                                   Mask128<TFrom, N> m) {
  static_assert(sizeof(TFrom) == sizeof(TTo), "Must have same size");
  return Mask128<TTo, N>{m.raw};
}

template <typename T, size_t N>
HWY_API Mask128<T, N> TestBit(Vec128<T, N> v, Vec128<T, N> bit) {
  static_assert(!hwy::IsFloat<T>(), "Only integer vectors supported");
  return (v & bit) == bit;
}

// ------------------------------ Equality

// Unsigned
template <size_t N>
HWY_API Mask128<uint8_t, N> operator==(const Vec128<uint8_t, N> a,
                                       const Vec128<uint8_t, N> b) {
  return Mask128<uint8_t, N>{wasm_i8x16_eq(a.raw, b.raw)};
}
template <size_t N>
HWY_API Mask128<uint16_t, N> operator==(const Vec128<uint16_t, N> a,
                                        const Vec128<uint16_t, N> b) {
  return Mask128<uint16_t, N>{wasm_i16x8_eq(a.raw, b.raw)};
}
template <size_t N>
HWY_API Mask128<uint32_t, N> operator==(const Vec128<uint32_t, N> a,
                                        const Vec128<uint32_t, N> b) {
  return Mask128<uint32_t, N>{wasm_i32x4_eq(a.raw, b.raw)};
}
template <size_t N>
HWY_API Mask128<uint64_t, N> operator==(const Vec128<uint64_t, N> a,
                                        const Vec128<uint64_t, N> b) {
  return Mask128<uint64_t, N>{wasm_i64x2_eq(a.raw, b.raw)};
}

// Signed
template <size_t N>
HWY_API Mask128<int8_t, N> operator==(const Vec128<int8_t, N> a,
                                      const Vec128<int8_t, N> b) {
  return Mask128<int8_t, N>{wasm_i8x16_eq(a.raw, b.raw)};
}
template <size_t N>
HWY_API Mask128<int16_t, N> operator==(Vec128<int16_t, N> a,
                                       Vec128<int16_t, N> b) {
  return Mask128<int16_t, N>{wasm_i16x8_eq(a.raw, b.raw)};
}
template <size_t N>
HWY_API Mask128<int32_t, N> operator==(const Vec128<int32_t, N> a,
                                       const Vec128<int32_t, N> b) {
  return Mask128<int32_t, N>{wasm_i32x4_eq(a.raw, b.raw)};
}
template <size_t N>
HWY_API Mask128<int64_t, N> operator==(const Vec128<int64_t, N> a,
                                       const Vec128<int64_t, N> b) {
  return Mask128<int64_t, N>{wasm_i64x2_eq(a.raw, b.raw)};
}

// Float
template <size_t N>
HWY_API Mask128<float, N> operator==(const Vec128<float, N> a,
                                     const Vec128<float, N> b) {
  return Mask128<float, N>{wasm_f32x4_eq(a.raw, b.raw)};
}

// ------------------------------ Inequality

// Unsigned
template <size_t N>
HWY_API Mask128<uint8_t, N> operator!=(const Vec128<uint8_t, N> a,
                                       const Vec128<uint8_t, N> b) {
  return Mask128<uint8_t, N>{wasm_i8x16_ne(a.raw, b.raw)};
}
template <size_t N>
HWY_API Mask128<uint16_t, N> operator!=(const Vec128<uint16_t, N> a,
                                        const Vec128<uint16_t, N> b) {
  return Mask128<uint16_t, N>{wasm_i16x8_ne(a.raw, b.raw)};
}
template <size_t N>
HWY_API Mask128<uint32_t, N> operator!=(const Vec128<uint32_t, N> a,
                                        const Vec128<uint32_t, N> b) {
  return Mask128<uint32_t, N>{wasm_i32x4_ne(a.raw, b.raw)};
}
template <size_t N>
HWY_API Mask128<uint64_t, N> operator!=(const Vec128<uint64_t, N> a,
                                        const Vec128<uint64_t, N> b) {
  return Mask128<uint64_t, N>{wasm_i64x2_ne(a.raw, b.raw)};
}

// Signed
template <size_t N>
HWY_API Mask128<int8_t, N> operator!=(const Vec128<int8_t, N> a,
                                      const Vec128<int8_t, N> b) {
  return Mask128<int8_t, N>{wasm_i8x16_ne(a.raw, b.raw)};
}
template <size_t N>
HWY_API Mask128<int16_t, N> operator!=(const Vec128<int16_t, N> a,
                                       const Vec128<int16_t, N> b) {
  return Mask128<int16_t, N>{wasm_i16x8_ne(a.raw, b.raw)};
}
template <size_t N>
HWY_API Mask128<int32_t, N> operator!=(const Vec128<int32_t, N> a,
                                       const Vec128<int32_t, N> b) {
  return Mask128<int32_t, N>{wasm_i32x4_ne(a.raw, b.raw)};
}
template <size_t N>
HWY_API Mask128<int64_t, N> operator!=(const Vec128<int64_t, N> a,
                                       const Vec128<int64_t, N> b) {
  return Mask128<int64_t, N>{wasm_i64x2_ne(a.raw, b.raw)};
}

// Float
template <size_t N>
HWY_API Mask128<float, N> operator!=(const Vec128<float, N> a,
                                     const Vec128<float, N> b) {
  return Mask128<float, N>{wasm_f32x4_ne(a.raw, b.raw)};
}

// ------------------------------ Strict inequality

template <size_t N>
HWY_API Mask128<int8_t, N> operator>(const Vec128<int8_t, N> a,
                                     const Vec128<int8_t, N> b) {
  return Mask128<int8_t, N>{wasm_i8x16_gt(a.raw, b.raw)};
}
template <size_t N>
HWY_API Mask128<int16_t, N> operator>(const Vec128<int16_t, N> a,
                                      const Vec128<int16_t, N> b) {
  return Mask128<int16_t, N>{wasm_i16x8_gt(a.raw, b.raw)};
}
template <size_t N>
HWY_API Mask128<int32_t, N> operator>(const Vec128<int32_t, N> a,
                                      const Vec128<int32_t, N> b) {
  return Mask128<int32_t, N>{wasm_i32x4_gt(a.raw, b.raw)};
}
template <size_t N>
HWY_API Mask128<int64_t, N> operator>(const Vec128<int64_t, N> a,
                                      const Vec128<int64_t, N> b) {
  return Mask128<int64_t, N>{wasm_i64x2_gt(a.raw, b.raw)};
}

template <size_t N>
HWY_API Mask128<uint8_t, N> operator>(const Vec128<uint8_t, N> a,
                                      const Vec128<uint8_t, N> b) {
  return Mask128<uint8_t, N>{wasm_u8x16_gt(a.raw, b.raw)};
}
template <size_t N>
HWY_API Mask128<uint16_t, N> operator>(const Vec128<uint16_t, N> a,
                                       const Vec128<uint16_t, N> b) {
  return Mask128<uint16_t, N>{wasm_u16x8_gt(a.raw, b.raw)};
}
template <size_t N>
HWY_API Mask128<uint32_t, N> operator>(const Vec128<uint32_t, N> a,
                                       const Vec128<uint32_t, N> b) {
  return Mask128<uint32_t, N>{wasm_u32x4_gt(a.raw, b.raw)};
}
template <size_t N>
HWY_API Mask128<uint64_t, N> operator>(const Vec128<uint64_t, N> a,
                                       const Vec128<uint64_t, N> b) {
  const DFromV<decltype(a)> d;
  const Repartition<uint32_t, decltype(d)> d32;
  const auto a32 = BitCast(d32, a);
  const auto b32 = BitCast(d32, b);
  // If the upper halves are not equal, this is the answer.
  const auto m_gt = a32 > b32;

  // Otherwise, the lower half decides.
  const auto m_eq = a32 == b32;
  const auto lo_in_hi = wasm_i32x4_shuffle(m_gt.raw, m_gt.raw, 0, 0, 2, 2);
  const auto lo_gt = And(m_eq, MaskFromVec(VFromD<decltype(d32)>{lo_in_hi}));

  const auto gt = Or(lo_gt, m_gt);
  // Copy result in upper 32 bits to lower 32 bits.
  return Mask128<uint64_t, N>{wasm_i32x4_shuffle(gt.raw, gt.raw, 1, 1, 3, 3)};
}

template <size_t N>
HWY_API Mask128<float, N> operator>(const Vec128<float, N> a,
                                    const Vec128<float, N> b) {
  return Mask128<float, N>{wasm_f32x4_gt(a.raw, b.raw)};
}

template <typename T, size_t N>
HWY_API Mask128<T, N> operator<(const Vec128<T, N> a, const Vec128<T, N> b) {
  return operator>(b, a);
}

// ------------------------------ Weak inequality

// Float <= >=
template <size_t N>
HWY_API Mask128<float, N> operator<=(const Vec128<float, N> a,
                                     const Vec128<float, N> b) {
  return Mask128<float, N>{wasm_f32x4_le(a.raw, b.raw)};
}
template <size_t N>
HWY_API Mask128<float, N> operator>=(const Vec128<float, N> a,
                                     const Vec128<float, N> b) {
  return Mask128<float, N>{wasm_f32x4_ge(a.raw, b.raw)};
}

// ------------------------------ FirstN (Iota, Lt)

template <typename T, size_t N>
HWY_API Mask128<T, N> FirstN(const Simd<T, N, 0> d, size_t num) {
  const RebindToSigned<decltype(d)> di;  // Signed comparisons may be cheaper.
  return RebindMask(d, Iota(di, 0) < Set(di, static_cast<MakeSigned<T>>(num)));
}

// ================================================== LOGICAL

// ------------------------------ Not

template <typename T, size_t N>
HWY_API Vec128<T, N> Not(Vec128<T, N> v) {
  return Vec128<T, N>{wasm_v128_not(v.raw)};
}

// ------------------------------ And

template <typename T, size_t N>
HWY_API Vec128<T, N> And(Vec128<T, N> a, Vec128<T, N> b) {
  return Vec128<T, N>{wasm_v128_and(a.raw, b.raw)};
}

// ------------------------------ AndNot

// Returns ~not_mask & mask.
template <typename T, size_t N>
HWY_API Vec128<T, N> AndNot(Vec128<T, N> not_mask, Vec128<T, N> mask) {
  return Vec128<T, N>{wasm_v128_andnot(mask.raw, not_mask.raw)};
}

// ------------------------------ Or

template <typename T, size_t N>
HWY_API Vec128<T, N> Or(Vec128<T, N> a, Vec128<T, N> b) {
  return Vec128<T, N>{wasm_v128_or(a.raw, b.raw)};
}

// ------------------------------ Xor

template <typename T, size_t N>
HWY_API Vec128<T, N> Xor(Vec128<T, N> a, Vec128<T, N> b) {
  return Vec128<T, N>{wasm_v128_xor(a.raw, b.raw)};
}

// ------------------------------ Or3

template <typename T, size_t N>
HWY_API Vec128<T, N> Or3(Vec128<T, N> o1, Vec128<T, N> o2, Vec128<T, N> o3) {
  return Or(o1, Or(o2, o3));
}

// ------------------------------ OrAnd

template <typename T, size_t N>
HWY_API Vec128<T, N> OrAnd(Vec128<T, N> o, Vec128<T, N> a1, Vec128<T, N> a2) {
  return Or(o, And(a1, a2));
}

// ------------------------------ IfVecThenElse

template <typename T, size_t N>
HWY_API Vec128<T, N> IfVecThenElse(Vec128<T, N> mask, Vec128<T, N> yes,
                                   Vec128<T, N> no) {
  return IfThenElse(MaskFromVec(mask), yes, no);
}

// ------------------------------ Operator overloads (internal-only if float)

template <typename T, size_t N>
HWY_API Vec128<T, N> operator&(const Vec128<T, N> a, const Vec128<T, N> b) {
  return And(a, b);
}

template <typename T, size_t N>
HWY_API Vec128<T, N> operator|(const Vec128<T, N> a, const Vec128<T, N> b) {
  return Or(a, b);
}

template <typename T, size_t N>
HWY_API Vec128<T, N> operator^(const Vec128<T, N> a, const Vec128<T, N> b) {
  return Xor(a, b);
}

// ------------------------------ CopySign

template <typename T, size_t N>
HWY_API Vec128<T, N> CopySign(const Vec128<T, N> magn,
                              const Vec128<T, N> sign) {
  static_assert(IsFloat<T>(), "Only makes sense for floating-point");
  const auto msb = SignBit(DFromV<decltype(magn)>());
  return Or(AndNot(msb, magn), And(msb, sign));
}

template <typename T, size_t N>
HWY_API Vec128<T, N> CopySignToAbs(const Vec128<T, N> abs,
                                   const Vec128<T, N> sign) {
  static_assert(IsFloat<T>(), "Only makes sense for floating-point");
  return Or(abs, And(SignBit(DFromV<decltype(abs)>()), sign));
}

// ------------------------------ BroadcastSignBit (compare)

template <typename T, size_t N, HWY_IF_NOT_LANE_SIZE(T, 1)>
HWY_API Vec128<T, N> BroadcastSignBit(const Vec128<T, N> v) {
  return ShiftRight<sizeof(T) * 8 - 1>(v);
}
template <size_t N>
HWY_API Vec128<int8_t, N> BroadcastSignBit(const Vec128<int8_t, N> v) {
  const DFromV<decltype(v)> d;
  return VecFromMask(d, v < Zero(d));
}

// ------------------------------ Mask

// Mask and Vec are the same (true = FF..FF).
template <typename T, size_t N>
HWY_API Mask128<T, N> MaskFromVec(const Vec128<T, N> v) {
  return Mask128<T, N>{v.raw};
}

template <typename T, size_t N>
HWY_API Vec128<T, N> VecFromMask(Simd<T, N, 0> /* tag */, Mask128<T, N> v) {
  return Vec128<T, N>{v.raw};
}

// mask ? yes : no
template <typename T, size_t N>
HWY_API Vec128<T, N> IfThenElse(Mask128<T, N> mask, Vec128<T, N> yes,
                                Vec128<T, N> no) {
  return Vec128<T, N>{wasm_v128_bitselect(yes.raw, no.raw, mask.raw)};
}

// mask ? yes : 0
template <typename T, size_t N>
HWY_API Vec128<T, N> IfThenElseZero(Mask128<T, N> mask, Vec128<T, N> yes) {
  return yes & VecFromMask(DFromV<decltype(yes)>(), mask);
}

// mask ? 0 : no
template <typename T, size_t N>
HWY_API Vec128<T, N> IfThenZeroElse(Mask128<T, N> mask, Vec128<T, N> no) {
  return AndNot(VecFromMask(DFromV<decltype(no)>(), mask), no);
}

template <typename T, size_t N>
HWY_API Vec128<T, N> IfNegativeThenElse(Vec128<T, N> v, Vec128<T, N> yes,
                                        Vec128<T, N> no) {
  static_assert(IsSigned<T>(), "Only works for signed/float");
  const DFromV<decltype(v)> d;
  const RebindToSigned<decltype(d)> di;

  v = BitCast(d, BroadcastSignBit(BitCast(di, v)));
  return IfThenElse(MaskFromVec(v), yes, no);
}

template <typename T, size_t N, HWY_IF_FLOAT(T)>
HWY_API Vec128<T, N> ZeroIfNegative(Vec128<T, N> v) {
  const DFromV<decltype(v)> d;
  const auto zero = Zero(d);
  return IfThenElse(Mask128<T, N>{(v > zero).raw}, v, zero);
}

// ------------------------------ Mask logical

template <typename T, size_t N>
HWY_API Mask128<T, N> Not(const Mask128<T, N> m) {
  return MaskFromVec(Not(VecFromMask(Simd<T, N, 0>(), m)));
}

template <typename T, size_t N>
HWY_API Mask128<T, N> And(const Mask128<T, N> a, Mask128<T, N> b) {
  const Simd<T, N, 0> d;
  return MaskFromVec(And(VecFromMask(d, a), VecFromMask(d, b)));
}

template <typename T, size_t N>
HWY_API Mask128<T, N> AndNot(const Mask128<T, N> a, Mask128<T, N> b) {
  const Simd<T, N, 0> d;
  return MaskFromVec(AndNot(VecFromMask(d, a), VecFromMask(d, b)));
}

template <typename T, size_t N>
HWY_API Mask128<T, N> Or(const Mask128<T, N> a, Mask128<T, N> b) {
  const Simd<T, N, 0> d;
  return MaskFromVec(Or(VecFromMask(d, a), VecFromMask(d, b)));
}

template <typename T, size_t N>
HWY_API Mask128<T, N> Xor(const Mask128<T, N> a, Mask128<T, N> b) {
  const Simd<T, N, 0> d;
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

template <typename T, size_t N, HWY_IF_LANE_SIZE(T, 2)>
HWY_API Vec128<T, N> operator<<(Vec128<T, N> v, const Vec128<T, N> bits) {
  const DFromV<decltype(v)> d;
  Mask128<T, N> mask;
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

template <typename T, size_t N, HWY_IF_LANE_SIZE(T, 4)>
HWY_API Vec128<T, N> operator<<(Vec128<T, N> v, const Vec128<T, N> bits) {
  const DFromV<decltype(v)> d;
  Mask128<T, N> mask;
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

template <typename T, size_t N, HWY_IF_LANE_SIZE(T, 8)>
HWY_API Vec128<T, N> operator<<(Vec128<T, N> v, const Vec128<T, N> bits) {
  const DFromV<decltype(v)> d;
  alignas(16) T lanes[2];
  alignas(16) T bits_lanes[2];
  Store(v, d, lanes);
  Store(bits, d, bits_lanes);
  lanes[0] <<= bits_lanes[0];
  lanes[1] <<= bits_lanes[1];
  return Load(d, lanes);
}

// ------------------------------ Shr (BroadcastSignBit, IfThenElse)

template <typename T, size_t N, HWY_IF_LANE_SIZE(T, 2)>
HWY_API Vec128<T, N> operator>>(Vec128<T, N> v, const Vec128<T, N> bits) {
  const DFromV<decltype(v)> d;
  Mask128<T, N> mask;
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

template <typename T, size_t N, HWY_IF_LANE_SIZE(T, 4)>
HWY_API Vec128<T, N> operator>>(Vec128<T, N> v, const Vec128<T, N> bits) {
  const DFromV<decltype(v)> d;
  Mask128<T, N> mask;
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
HWY_API Vec128<T> Load(Full128<T> /* tag */, const T* HWY_RESTRICT aligned) {
  return Vec128<T>{wasm_v128_load(aligned)};
}

template <typename T, size_t N>
HWY_API Vec128<T, N> MaskedLoad(Mask128<T, N> m, Simd<T, N, 0> d,
                                const T* HWY_RESTRICT aligned) {
  return IfThenElseZero(m, Load(d, aligned));
}

// Partial load.
template <typename T, size_t N, HWY_IF_LE64(T, N)>
HWY_API Vec128<T, N> Load(Simd<T, N, 0> /* tag */, const T* HWY_RESTRICT p) {
  Vec128<T, N> v;
  CopyBytes<sizeof(T) * N>(p, &v);
  return v;
}

// LoadU == Load.
template <typename T, size_t N>
HWY_API Vec128<T, N> LoadU(Simd<T, N, 0> d, const T* HWY_RESTRICT p) {
  return Load(d, p);
}

// 128-bit SIMD => nothing to duplicate, same as an unaligned load.
template <typename T, size_t N, HWY_IF_LE128(T, N)>
HWY_API Vec128<T, N> LoadDup128(Simd<T, N, 0> d, const T* HWY_RESTRICT p) {
  return Load(d, p);
}

// ------------------------------ Store

template <typename T>
HWY_API void Store(Vec128<T> v, Full128<T> /* tag */, T* HWY_RESTRICT aligned) {
  wasm_v128_store(aligned, v.raw);
}

// Partial store.
template <typename T, size_t N, HWY_IF_LE64(T, N)>
HWY_API void Store(Vec128<T, N> v, Simd<T, N, 0> /* tag */, T* HWY_RESTRICT p) {
  CopyBytes<sizeof(T) * N>(&v, p);
}

HWY_API void Store(const Vec128<float, 1> v, Simd<float, 1, 0> /* tag */,
                   float* HWY_RESTRICT p) {
  *p = wasm_f32x4_extract_lane(v.raw, 0);
}

// StoreU == Store.
template <typename T, size_t N>
HWY_API void StoreU(Vec128<T, N> v, Simd<T, N, 0> d, T* HWY_RESTRICT p) {
  Store(v, d, p);
}

template <typename T, size_t N>
HWY_API void BlendedStore(Vec128<T, N> v, Mask128<T, N> m, Simd<T, N, 0> d,
                          T* HWY_RESTRICT p) {
  StoreU(IfThenElse(m, v, LoadU(d, p)), d, p);
}

// ------------------------------ Non-temporal stores

// Same as aligned stores on non-x86.

template <typename T, size_t N>
HWY_API void Stream(Vec128<T, N> v, Simd<T, N, 0> /* tag */,
                    T* HWY_RESTRICT aligned) {
  wasm_v128_store(aligned, v.raw);
}

// ------------------------------ Scatter (Store)

template <typename T, size_t N, typename Offset, HWY_IF_LE128(T, N)>
HWY_API void ScatterOffset(Vec128<T, N> v, Simd<T, N, 0> d,
                           T* HWY_RESTRICT base,
                           const Vec128<Offset, N> offset) {
  static_assert(sizeof(T) == sizeof(Offset), "Must match for portability");

  alignas(16) T lanes[N];
  Store(v, d, lanes);

  alignas(16) Offset offset_lanes[N];
  Store(offset, Rebind<Offset, decltype(d)>(), offset_lanes);

  uint8_t* base_bytes = reinterpret_cast<uint8_t*>(base);
  for (size_t i = 0; i < N; ++i) {
    CopyBytes<sizeof(T)>(&lanes[i], base_bytes + offset_lanes[i]);
  }
}

template <typename T, size_t N, typename Index, HWY_IF_LE128(T, N)>
HWY_API void ScatterIndex(Vec128<T, N> v, Simd<T, N, 0> d, T* HWY_RESTRICT base,
                          const Vec128<Index, N> index) {
  static_assert(sizeof(T) == sizeof(Index), "Must match for portability");

  alignas(16) T lanes[N];
  Store(v, d, lanes);

  alignas(16) Index index_lanes[N];
  Store(index, Rebind<Index, decltype(d)>(), index_lanes);

  for (size_t i = 0; i < N; ++i) {
    base[index_lanes[i]] = lanes[i];
  }
}

// ------------------------------ Gather (Load/Store)

template <typename T, size_t N, typename Offset>
HWY_API Vec128<T, N> GatherOffset(const Simd<T, N, 0> d,
                                  const T* HWY_RESTRICT base,
                                  const Vec128<Offset, N> offset) {
  static_assert(sizeof(T) == sizeof(Offset), "Must match for portability");

  alignas(16) Offset offset_lanes[N];
  Store(offset, Rebind<Offset, decltype(d)>(), offset_lanes);

  alignas(16) T lanes[N];
  const uint8_t* base_bytes = reinterpret_cast<const uint8_t*>(base);
  for (size_t i = 0; i < N; ++i) {
    CopyBytes<sizeof(T)>(base_bytes + offset_lanes[i], &lanes[i]);
  }
  return Load(d, lanes);
}

template <typename T, size_t N, typename Index>
HWY_API Vec128<T, N> GatherIndex(const Simd<T, N, 0> d,
                                 const T* HWY_RESTRICT base,
                                 const Vec128<Index, N> index) {
  static_assert(sizeof(T) == sizeof(Index), "Must match for portability");

  alignas(16) Index index_lanes[N];
  Store(index, Rebind<Index, decltype(d)>(), index_lanes);

  alignas(16) T lanes[N];
  for (size_t i = 0; i < N; ++i) {
    lanes[i] = base[index_lanes[i]];
  }
  return Load(d, lanes);
}

// ================================================== SWIZZLE

// ------------------------------ ExtractLane

namespace detail {

template <size_t kLane, typename T, size_t N, HWY_IF_LANE_SIZE(T, 1)>
HWY_INLINE T ExtractLane(const Vec128<T, N> v) {
  return static_cast<T>(wasm_i8x16_extract_lane(v.raw, kLane));
}
template <size_t kLane, typename T, size_t N, HWY_IF_LANE_SIZE(T, 2)>
HWY_INLINE T ExtractLane(const Vec128<T, N> v) {
  return static_cast<T>(wasm_i16x8_extract_lane(v.raw, kLane));
}
template <size_t kLane, typename T, size_t N, HWY_IF_LANE_SIZE(T, 4)>
HWY_INLINE T ExtractLane(const Vec128<T, N> v) {
  return static_cast<T>(wasm_i32x4_extract_lane(v.raw, kLane));
}
template <size_t kLane, typename T, size_t N, HWY_IF_LANE_SIZE(T, 8)>
HWY_INLINE T ExtractLane(const Vec128<T, N> v) {
  return static_cast<T>(wasm_i64x2_extract_lane(v.raw, kLane));
}

template <size_t kLane, size_t N>
HWY_INLINE float ExtractLane(const Vec128<float, N> v) {
  return wasm_f32x4_extract_lane(v.raw, kLane);
}

}  // namespace detail

// One overload per vector length just in case *_extract_lane raise compile
// errors if their argument is out of bounds (even if that would never be
// reached at runtime).
template <typename T>
HWY_API T ExtractLane(const Vec128<T, 1> v, size_t i) {
  HWY_DASSERT(i == 0);
  (void)i;
  return GetLane(v);
}

template <typename T>
HWY_API T ExtractLane(const Vec128<T, 2> v, size_t i) {
#if !HWY_IS_DEBUG_BUILD && HWY_COMPILER_GCC  // includes clang
  if (__builtin_constant_p(i)) {
    switch (i) {
      case 0:
        return detail::ExtractLane<0>(v);
      case 1:
        return detail::ExtractLane<1>(v);
    }
  }
#endif
  alignas(16) T lanes[2];
  Store(v, DFromV<decltype(v)>(), lanes);
  return lanes[i];
}

template <typename T>
HWY_API T ExtractLane(const Vec128<T, 4> v, size_t i) {
#if !HWY_IS_DEBUG_BUILD && HWY_COMPILER_GCC  // includes clang
  if (__builtin_constant_p(i)) {
    switch (i) {
      case 0:
        return detail::ExtractLane<0>(v);
      case 1:
        return detail::ExtractLane<1>(v);
      case 2:
        return detail::ExtractLane<2>(v);
      case 3:
        return detail::ExtractLane<3>(v);
    }
  }
#endif
  alignas(16) T lanes[4];
  Store(v, DFromV<decltype(v)>(), lanes);
  return lanes[i];
}

template <typename T>
HWY_API T ExtractLane(const Vec128<T, 8> v, size_t i) {
#if !HWY_IS_DEBUG_BUILD && HWY_COMPILER_GCC  // includes clang
  if (__builtin_constant_p(i)) {
    switch (i) {
      case 0:
        return detail::ExtractLane<0>(v);
      case 1:
        return detail::ExtractLane<1>(v);
      case 2:
        return detail::ExtractLane<2>(v);
      case 3:
        return detail::ExtractLane<3>(v);
      case 4:
        return detail::ExtractLane<4>(v);
      case 5:
        return detail::ExtractLane<5>(v);
      case 6:
        return detail::ExtractLane<6>(v);
      case 7:
        return detail::ExtractLane<7>(v);
    }
  }
#endif
  alignas(16) T lanes[8];
  Store(v, DFromV<decltype(v)>(), lanes);
  return lanes[i];
}

template <typename T>
HWY_API T ExtractLane(const Vec128<T, 16> v, size_t i) {
#if !HWY_IS_DEBUG_BUILD && HWY_COMPILER_GCC  // includes clang
  if (__builtin_constant_p(i)) {
    switch (i) {
      case 0:
        return detail::ExtractLane<0>(v);
      case 1:
        return detail::ExtractLane<1>(v);
      case 2:
        return detail::ExtractLane<2>(v);
      case 3:
        return detail::ExtractLane<3>(v);
      case 4:
        return detail::ExtractLane<4>(v);
      case 5:
        return detail::ExtractLane<5>(v);
      case 6:
        return detail::ExtractLane<6>(v);
      case 7:
        return detail::ExtractLane<7>(v);
      case 8:
        return detail::ExtractLane<8>(v);
      case 9:
        return detail::ExtractLane<9>(v);
      case 10:
        return detail::ExtractLane<10>(v);
      case 11:
        return detail::ExtractLane<11>(v);
      case 12:
        return detail::ExtractLane<12>(v);
      case 13:
        return detail::ExtractLane<13>(v);
      case 14:
        return detail::ExtractLane<14>(v);
      case 15:
        return detail::ExtractLane<15>(v);
    }
  }
#endif
  alignas(16) T lanes[16];
  Store(v, DFromV<decltype(v)>(), lanes);
  return lanes[i];
}

// ------------------------------ GetLane
template <typename T, size_t N>
HWY_API T GetLane(const Vec128<T, N> v) {
  return detail::ExtractLane<0>(v);
}

// ------------------------------ InsertLane

namespace detail {

template <size_t kLane, typename T, size_t N, HWY_IF_LANE_SIZE(T, 1)>
HWY_INLINE Vec128<T, N> InsertLane(const Vec128<T, N> v, T t) {
  static_assert(kLane < N, "Lane index out of bounds");
  return Vec128<T, N>{
      wasm_i8x16_replace_lane(v.raw, kLane, static_cast<int8_t>(t))};
}

template <size_t kLane, typename T, size_t N, HWY_IF_LANE_SIZE(T, 2)>
HWY_INLINE Vec128<T, N> InsertLane(const Vec128<T, N> v, T t) {
  static_assert(kLane < N, "Lane index out of bounds");
  return Vec128<T, N>{
      wasm_i16x8_replace_lane(v.raw, kLane, static_cast<int16_t>(t))};
}

template <size_t kLane, typename T, size_t N, HWY_IF_LANE_SIZE(T, 4)>
HWY_INLINE Vec128<T, N> InsertLane(const Vec128<T, N> v, T t) {
  static_assert(kLane < N, "Lane index out of bounds");
  return Vec128<T, N>{
      wasm_i32x4_replace_lane(v.raw, kLane, static_cast<int32_t>(t))};
}

template <size_t kLane, typename T, size_t N, HWY_IF_LANE_SIZE(T, 8)>
HWY_INLINE Vec128<T, N> InsertLane(const Vec128<T, N> v, T t) {
  static_assert(kLane < N, "Lane index out of bounds");
  return Vec128<T, N>{
      wasm_i64x2_replace_lane(v.raw, kLane, static_cast<int64_t>(t))};
}

template <size_t kLane, size_t N>
HWY_INLINE Vec128<float, N> InsertLane(const Vec128<float, N> v, float t) {
  static_assert(kLane < N, "Lane index out of bounds");
  return Vec128<float, N>{wasm_f32x4_replace_lane(v.raw, kLane, t)};
}

template <size_t kLane, size_t N>
HWY_INLINE Vec128<double, N> InsertLane(const Vec128<double, N> v, double t) {
  static_assert(kLane < 2, "Lane index out of bounds");
  return Vec128<double, N>{wasm_f64x2_replace_lane(v.raw, kLane, t)};
}

}  // namespace detail

// Requires one overload per vector length because InsertLane<3> may be a
// compile error if it calls wasm_f64x2_replace_lane.

template <typename T>
HWY_API Vec128<T, 1> InsertLane(const Vec128<T, 1> v, size_t i, T t) {
  HWY_DASSERT(i == 0);
  (void)i;
  return Set(DFromV<decltype(v)>(), t);
}

template <typename T>
HWY_API Vec128<T, 2> InsertLane(const Vec128<T, 2> v, size_t i, T t) {
#if !HWY_IS_DEBUG_BUILD && HWY_COMPILER_GCC  // includes clang
  if (__builtin_constant_p(i)) {
    switch (i) {
      case 0:
        return detail::InsertLane<0>(v, t);
      case 1:
        return detail::InsertLane<1>(v, t);
    }
  }
#endif
  const DFromV<decltype(v)> d;
  alignas(16) T lanes[2];
  Store(v, d, lanes);
  lanes[i] = t;
  return Load(d, lanes);
}

template <typename T>
HWY_API Vec128<T, 4> InsertLane(const Vec128<T, 4> v, size_t i, T t) {
#if !HWY_IS_DEBUG_BUILD && HWY_COMPILER_GCC  // includes clang
  if (__builtin_constant_p(i)) {
    switch (i) {
      case 0:
        return detail::InsertLane<0>(v, t);
      case 1:
        return detail::InsertLane<1>(v, t);
      case 2:
        return detail::InsertLane<2>(v, t);
      case 3:
        return detail::InsertLane<3>(v, t);
    }
  }
#endif
  const DFromV<decltype(v)> d;
  alignas(16) T lanes[4];
  Store(v, d, lanes);
  lanes[i] = t;
  return Load(d, lanes);
}

template <typename T>
HWY_API Vec128<T, 8> InsertLane(const Vec128<T, 8> v, size_t i, T t) {
#if !HWY_IS_DEBUG_BUILD && HWY_COMPILER_GCC  // includes clang
  if (__builtin_constant_p(i)) {
    switch (i) {
      case 0:
        return detail::InsertLane<0>(v, t);
      case 1:
        return detail::InsertLane<1>(v, t);
      case 2:
        return detail::InsertLane<2>(v, t);
      case 3:
        return detail::InsertLane<3>(v, t);
      case 4:
        return detail::InsertLane<4>(v, t);
      case 5:
        return detail::InsertLane<5>(v, t);
      case 6:
        return detail::InsertLane<6>(v, t);
      case 7:
        return detail::InsertLane<7>(v, t);
    }
  }
#endif
  const DFromV<decltype(v)> d;
  alignas(16) T lanes[8];
  Store(v, d, lanes);
  lanes[i] = t;
  return Load(d, lanes);
}

template <typename T>
HWY_API Vec128<T, 16> InsertLane(const Vec128<T, 16> v, size_t i, T t) {
#if !HWY_IS_DEBUG_BUILD && HWY_COMPILER_GCC  // includes clang
  if (__builtin_constant_p(i)) {
    switch (i) {
      case 0:
        return detail::InsertLane<0>(v, t);
      case 1:
        return detail::InsertLane<1>(v, t);
      case 2:
        return detail::InsertLane<2>(v, t);
      case 3:
        return detail::InsertLane<3>(v, t);
      case 4:
        return detail::InsertLane<4>(v, t);
      case 5:
        return detail::InsertLane<5>(v, t);
      case 6:
        return detail::InsertLane<6>(v, t);
      case 7:
        return detail::InsertLane<7>(v, t);
      case 8:
        return detail::InsertLane<8>(v, t);
      case 9:
        return detail::InsertLane<9>(v, t);
      case 10:
        return detail::InsertLane<10>(v, t);
      case 11:
        return detail::InsertLane<11>(v, t);
      case 12:
        return detail::InsertLane<12>(v, t);
      case 13:
        return detail::InsertLane<13>(v, t);
      case 14:
        return detail::InsertLane<14>(v, t);
      case 15:
        return detail::InsertLane<15>(v, t);
    }
  }
#endif
  const DFromV<decltype(v)> d;
  alignas(16) T lanes[16];
  Store(v, d, lanes);
  lanes[i] = t;
  return Load(d, lanes);
}

// ------------------------------ LowerHalf

template <typename T, size_t N>
HWY_API Vec128<T, N / 2> LowerHalf(Simd<T, N / 2, 0> /* tag */,
                                   Vec128<T, N> v) {
  return Vec128<T, N / 2>{v.raw};
}

template <typename T, size_t N>
HWY_API Vec128<T, N / 2> LowerHalf(Vec128<T, N> v) {
  return LowerHalf(Simd<T, N / 2, 0>(), v);
}

// ------------------------------ ShiftLeftBytes

// 0x01..0F, kBytes = 1 => 0x02..0F00
template <int kBytes, typename T, size_t N>
HWY_API Vec128<T, N> ShiftLeftBytes(Simd<T, N, 0> /* tag */, Vec128<T, N> v) {
  static_assert(0 <= kBytes && kBytes <= 16, "Invalid kBytes");
  const __i8x16 zero = wasm_i8x16_splat(0);
  switch (kBytes) {
    case 0:
      return v;

    case 1:
      return Vec128<T, N>{wasm_i8x16_shuffle(v.raw, zero, 16, 0, 1, 2, 3, 4, 5,
                                             6, 7, 8, 9, 10, 11, 12, 13, 14)};

    case 2:
      return Vec128<T, N>{wasm_i8x16_shuffle(v.raw, zero, 16, 16, 0, 1, 2, 3, 4,
                                             5, 6, 7, 8, 9, 10, 11, 12, 13)};

    case 3:
      return Vec128<T, N>{wasm_i8x16_shuffle(v.raw, zero, 16, 16, 16, 0, 1, 2,
                                             3, 4, 5, 6, 7, 8, 9, 10, 11, 12)};

    case 4:
      return Vec128<T, N>{wasm_i8x16_shuffle(v.raw, zero, 16, 16, 16, 16, 0, 1,
                                             2, 3, 4, 5, 6, 7, 8, 9, 10, 11)};

    case 5:
      return Vec128<T, N>{wasm_i8x16_shuffle(v.raw, zero, 16, 16, 16, 16, 16, 0,
                                             1, 2, 3, 4, 5, 6, 7, 8, 9, 10)};

    case 6:
      return Vec128<T, N>{wasm_i8x16_shuffle(v.raw, zero, 16, 16, 16, 16, 16,
                                             16, 0, 1, 2, 3, 4, 5, 6, 7, 8, 9)};

    case 7:
      return Vec128<T, N>{wasm_i8x16_shuffle(
          v.raw, zero, 16, 16, 16, 16, 16, 16, 16, 0, 1, 2, 3, 4, 5, 6, 7, 8)};

    case 8:
      return Vec128<T, N>{wasm_i8x16_shuffle(
          v.raw, zero, 16, 16, 16, 16, 16, 16, 16, 16, 0, 1, 2, 3, 4, 5, 6, 7)};

    case 9:
      return Vec128<T, N>{wasm_i8x16_shuffle(v.raw, zero, 16, 16, 16, 16, 16,
                                             16, 16, 16, 16, 0, 1, 2, 3, 4, 5,
                                             6)};

    case 10:
      return Vec128<T, N>{wasm_i8x16_shuffle(v.raw, zero, 16, 16, 16, 16, 16,
                                             16, 16, 16, 16, 16, 0, 1, 2, 3, 4,
                                             5)};

    case 11:
      return Vec128<T, N>{wasm_i8x16_shuffle(v.raw, zero, 16, 16, 16, 16, 16,
                                             16, 16, 16, 16, 16, 16, 0, 1, 2, 3,
                                             4)};

    case 12:
      return Vec128<T, N>{wasm_i8x16_shuffle(v.raw, zero, 16, 16, 16, 16, 16,
                                             16, 16, 16, 16, 16, 16, 16, 0, 1,
                                             2, 3)};

    case 13:
      return Vec128<T, N>{wasm_i8x16_shuffle(v.raw, zero, 16, 16, 16, 16, 16,
                                             16, 16, 16, 16, 16, 16, 16, 16, 0,
                                             1, 2)};

    case 14:
      return Vec128<T, N>{wasm_i8x16_shuffle(v.raw, zero, 16, 16, 16, 16, 16,
                                             16, 16, 16, 16, 16, 16, 16, 16, 16,
                                             0, 1)};

    case 15:
      return Vec128<T, N>{wasm_i8x16_shuffle(v.raw, zero, 16, 16, 16, 16, 16,
                                             16, 16, 16, 16, 16, 16, 16, 16, 16,
                                             16, 0)};
  }
  return Vec128<T, N>{zero};
}

template <int kBytes, typename T, size_t N>
HWY_API Vec128<T, N> ShiftLeftBytes(Vec128<T, N> v) {
  return ShiftLeftBytes<kBytes>(Simd<T, N, 0>(), v);
}

// ------------------------------ ShiftLeftLanes

template <int kLanes, typename T, size_t N>
HWY_API Vec128<T, N> ShiftLeftLanes(Simd<T, N, 0> d, const Vec128<T, N> v) {
  const Repartition<uint8_t, decltype(d)> d8;
  return BitCast(d, ShiftLeftBytes<kLanes * sizeof(T)>(BitCast(d8, v)));
}

template <int kLanes, typename T, size_t N>
HWY_API Vec128<T, N> ShiftLeftLanes(const Vec128<T, N> v) {
  return ShiftLeftLanes<kLanes>(DFromV<decltype(v)>(), v);
}

// ------------------------------ ShiftRightBytes
namespace detail {

// Helper function allows zeroing invalid lanes in caller.
template <int kBytes, typename T, size_t N>
HWY_API __i8x16 ShrBytes(const Vec128<T, N> v) {
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
template <int kBytes, typename T, size_t N>
HWY_API Vec128<T, N> ShiftRightBytes(Simd<T, N, 0> /* tag */, Vec128<T, N> v) {
  // For partial vectors, clear upper lanes so we shift in zeros.
  if (N != 16 / sizeof(T)) {
    const Vec128<T> vfull{v.raw};
    v = Vec128<T, N>{IfThenElseZero(FirstN(Full128<T>(), N), vfull).raw};
  }
  return Vec128<T, N>{detail::ShrBytes<kBytes>(v)};
}

// ------------------------------ ShiftRightLanes
template <int kLanes, typename T, size_t N>
HWY_API Vec128<T, N> ShiftRightLanes(Simd<T, N, 0> d, const Vec128<T, N> v) {
  const Repartition<uint8_t, decltype(d)> d8;
  return BitCast(d, ShiftRightBytes<kLanes * sizeof(T)>(d8, BitCast(d8, v)));
}

// ------------------------------ UpperHalf (ShiftRightBytes)

// Full input: copy hi into lo (smaller instruction encoding than shifts).
template <typename T>
HWY_API Vec64<T> UpperHalf(Full64<T> /* tag */, const Vec128<T> v) {
  return Vec64<T>{wasm_i32x4_shuffle(v.raw, v.raw, 2, 3, 2, 3)};
}
HWY_API Vec64<float> UpperHalf(Full64<float> /* tag */, const Vec128<float> v) {
  return Vec64<float>{wasm_i32x4_shuffle(v.raw, v.raw, 2, 3, 2, 3)};
}

// Partial
template <typename T, size_t N, HWY_IF_LE64(T, N)>
HWY_API Vec128<T, (N + 1) / 2> UpperHalf(Half<Simd<T, N, 0>> /* tag */,
                                         Vec128<T, N> v) {
  const DFromV<decltype(v)> d;
  const RebindToUnsigned<decltype(d)> du;
  const auto vu = BitCast(du, v);
  const auto upper = BitCast(d, ShiftRightBytes<N * sizeof(T) / 2>(du, vu));
  return Vec128<T, (N + 1) / 2>{upper.raw};
}

// ------------------------------ CombineShiftRightBytes

template <int kBytes, typename T, class V = Vec128<T>>
HWY_API V CombineShiftRightBytes(Full128<T> /* tag */, V hi, V lo) {
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

template <int kBytes, typename T, size_t N, HWY_IF_LE64(T, N),
          class V = Vec128<T, N>>
HWY_API V CombineShiftRightBytes(Simd<T, N, 0> d, V hi, V lo) {
  constexpr size_t kSize = N * sizeof(T);
  static_assert(0 < kBytes && kBytes < kSize, "kBytes invalid");
  const Repartition<uint8_t, decltype(d)> d8;
  const Full128<uint8_t> d_full8;
  using V8 = VFromD<decltype(d_full8)>;
  const V8 hi8{BitCast(d8, hi).raw};
  // Move into most-significant bytes
  const V8 lo8 = ShiftLeftBytes<16 - kSize>(V8{BitCast(d8, lo).raw});
  const V8 r = CombineShiftRightBytes<16 - kSize + kBytes>(d_full8, hi8, lo8);
  return V{BitCast(Full128<T>(), r).raw};
}

// ------------------------------ Broadcast/splat any lane

template <int kLane, typename T, size_t N, HWY_IF_LANE_SIZE(T, 2)>
HWY_API Vec128<T, N> Broadcast(const Vec128<T, N> v) {
  static_assert(0 <= kLane && kLane < N, "Invalid lane");
  return Vec128<T, N>{wasm_i16x8_shuffle(v.raw, v.raw, kLane, kLane, kLane,
                                         kLane, kLane, kLane, kLane, kLane)};
}

template <int kLane, typename T, size_t N, HWY_IF_LANE_SIZE(T, 4)>
HWY_API Vec128<T, N> Broadcast(const Vec128<T, N> v) {
  static_assert(0 <= kLane && kLane < N, "Invalid lane");
  return Vec128<T, N>{
      wasm_i32x4_shuffle(v.raw, v.raw, kLane, kLane, kLane, kLane)};
}

template <int kLane, typename T, size_t N, HWY_IF_LANE_SIZE(T, 8)>
HWY_API Vec128<T, N> Broadcast(const Vec128<T, N> v) {
  static_assert(0 <= kLane && kLane < N, "Invalid lane");
  return Vec128<T, N>{wasm_i64x2_shuffle(v.raw, v.raw, kLane, kLane)};
}

// ------------------------------ TableLookupBytes

// Returns vector of bytes[from[i]]. "from" is also interpreted as bytes, i.e.
// lane indices in [0, 16).
template <typename T, size_t N, typename TI, size_t NI>
HWY_API Vec128<TI, NI> TableLookupBytes(const Vec128<T, N> bytes,
                                        const Vec128<TI, NI> from) {
// Not yet available in all engines, see
// https://github.com/WebAssembly/simd/blob/bdcc304b2d379f4601c2c44ea9b44ed9484fde7e/proposals/simd/ImplementationStatus.md
// V8 implementation of this had a bug, fixed on 2021-04-03:
// https://chromium-review.googlesource.com/c/v8/v8/+/2822951
#if 0
  return Vec128<TI, NI>{wasm_i8x16_swizzle(bytes.raw, from.raw)};
#else
  alignas(16) uint8_t control[16];
  alignas(16) uint8_t input[16];
  alignas(16) uint8_t output[16];
  wasm_v128_store(control, from.raw);
  wasm_v128_store(input, bytes.raw);
  for (size_t i = 0; i < 16; ++i) {
    output[i] = control[i] < 16 ? input[control[i]] : 0;
  }
  return Vec128<TI, NI>{wasm_v128_load(output)};
#endif
}

template <typename T, size_t N, typename TI, size_t NI>
HWY_API Vec128<TI, NI> TableLookupBytesOr0(const Vec128<T, N> bytes,
                                           const Vec128<TI, NI> from) {
  const Simd<TI, NI, 0> d;
  // Mask size must match vector type, so cast everything to this type.
  Repartition<int8_t, decltype(d)> di8;
  Repartition<int8_t, Simd<T, N, 0>> d_bytes8;
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
template <typename T, size_t N>
HWY_API Vec128<T, N> Shuffle2301(const Vec128<T, N> v) {
  static_assert(sizeof(T) == 4, "Only for 32-bit lanes");
  static_assert(N == 2 || N == 4, "Does not make sense for N=1");
  return Vec128<T, N>{wasm_i32x4_shuffle(v.raw, v.raw, 1, 0, 3, 2)};
}

// These are used by generic_ops-inl to implement LoadInterleaved3.
namespace detail {

template <typename T, size_t N, HWY_IF_LANE_SIZE(T, 1)>
HWY_API Vec128<T, N> Shuffle2301(const Vec128<T, N> a, const Vec128<T, N> b) {
  static_assert(N == 2 || N == 4, "Does not make sense for N=1");
  return Vec128<T, N>{wasm_i8x16_shuffle(a.raw, b.raw, 1, 0, 3 + 16, 2 + 16,
                                         0x7F, 0x7F, 0x7F, 0x7F, 0x7F, 0x7F,
                                         0x7F, 0x7F, 0x7F, 0x7F, 0x7F, 0x7F)};
}
template <typename T, size_t N, HWY_IF_LANE_SIZE(T, 2)>
HWY_API Vec128<T, N> Shuffle2301(const Vec128<T, N> a, const Vec128<T, N> b) {
  static_assert(N == 2 || N == 4, "Does not make sense for N=1");
  return Vec128<T, N>{wasm_i16x8_shuffle(a.raw, b.raw, 1, 0, 3 + 8, 2 + 8,
                                         0x7FFF, 0x7FFF, 0x7FFF, 0x7FFF)};
}
template <typename T, size_t N, HWY_IF_LANE_SIZE(T, 4)>
HWY_API Vec128<T, N> Shuffle2301(const Vec128<T, N> a, const Vec128<T, N> b) {
  static_assert(N == 2 || N == 4, "Does not make sense for N=1");
  return Vec128<T, N>{wasm_i32x4_shuffle(a.raw, b.raw, 1, 0, 3 + 4, 2 + 4)};
}

template <typename T, size_t N, HWY_IF_LANE_SIZE(T, 1)>
HWY_API Vec128<T, N> Shuffle1230(const Vec128<T, N> a, const Vec128<T, N> b) {
  static_assert(N == 2 || N == 4, "Does not make sense for N=1");
  return Vec128<T, N>{wasm_i8x16_shuffle(a.raw, b.raw, 0, 3, 2 + 16, 1 + 16,
                                         0x7F, 0x7F, 0x7F, 0x7F, 0x7F, 0x7F,
                                         0x7F, 0x7F, 0x7F, 0x7F, 0x7F, 0x7F)};
}
template <typename T, size_t N, HWY_IF_LANE_SIZE(T, 2)>
HWY_API Vec128<T, N> Shuffle1230(const Vec128<T, N> a, const Vec128<T, N> b) {
  static_assert(N == 2 || N == 4, "Does not make sense for N=1");
  return Vec128<T, N>{wasm_i16x8_shuffle(a.raw, b.raw, 0, 3, 2 + 8, 1 + 8,
                                         0x7FFF, 0x7FFF, 0x7FFF, 0x7FFF)};
}
template <typename T, size_t N, HWY_IF_LANE_SIZE(T, 4)>
HWY_API Vec128<T, N> Shuffle1230(const Vec128<T, N> a, const Vec128<T, N> b) {
  static_assert(N == 2 || N == 4, "Does not make sense for N=1");
  return Vec128<T, N>{wasm_i32x4_shuffle(a.raw, b.raw, 0, 3, 2 + 4, 1 + 4)};
}

template <typename T, size_t N, HWY_IF_LANE_SIZE(T, 1)>
HWY_API Vec128<T, N> Shuffle3012(const Vec128<T, N> a, const Vec128<T, N> b) {
  static_assert(N == 2 || N == 4, "Does not make sense for N=1");
  return Vec128<T, N>{wasm_i8x16_shuffle(a.raw, b.raw, 2, 1, 0 + 16, 3 + 16,
                                         0x7F, 0x7F, 0x7F, 0x7F, 0x7F, 0x7F,
                                         0x7F, 0x7F, 0x7F, 0x7F, 0x7F, 0x7F)};
}
template <typename T, size_t N, HWY_IF_LANE_SIZE(T, 2)>
HWY_API Vec128<T, N> Shuffle3012(const Vec128<T, N> a, const Vec128<T, N> b) {
  static_assert(N == 2 || N == 4, "Does not make sense for N=1");
  return Vec128<T, N>{wasm_i16x8_shuffle(a.raw, b.raw, 2, 1, 0 + 8, 3 + 8,
                                         0x7FFF, 0x7FFF, 0x7FFF, 0x7FFF)};
}
template <typename T, size_t N, HWY_IF_LANE_SIZE(T, 4)>
HWY_API Vec128<T, N> Shuffle3012(const Vec128<T, N> a, const Vec128<T, N> b) {
  static_assert(N == 2 || N == 4, "Does not make sense for N=1");
  return Vec128<T, N>{wasm_i32x4_shuffle(a.raw, b.raw, 2, 1, 0 + 4, 3 + 4)};
}

}  // namespace detail

// Swap 64-bit halves
template <typename T>
HWY_API Vec128<T> Shuffle01(const Vec128<T> v) {
  static_assert(sizeof(T) == 8, "Only for 64-bit lanes");
  return Vec128<T>{wasm_i64x2_shuffle(v.raw, v.raw, 1, 0)};
}
template <typename T>
HWY_API Vec128<T> Shuffle1032(const Vec128<T> v) {
  static_assert(sizeof(T) == 4, "Only for 32-bit lanes");
  return Vec128<T>{wasm_i64x2_shuffle(v.raw, v.raw, 1, 0)};
}

// Rotate right 32 bits
template <typename T>
HWY_API Vec128<T> Shuffle0321(const Vec128<T> v) {
  static_assert(sizeof(T) == 4, "Only for 32-bit lanes");
  return Vec128<T>{wasm_i32x4_shuffle(v.raw, v.raw, 1, 2, 3, 0)};
}

// Rotate left 32 bits
template <typename T>
HWY_API Vec128<T> Shuffle2103(const Vec128<T> v) {
  static_assert(sizeof(T) == 4, "Only for 32-bit lanes");
  return Vec128<T>{wasm_i32x4_shuffle(v.raw, v.raw, 3, 0, 1, 2)};
}

// Reverse
template <typename T>
HWY_API Vec128<T> Shuffle0123(const Vec128<T> v) {
  static_assert(sizeof(T) == 4, "Only for 32-bit lanes");
  return Vec128<T>{wasm_i32x4_shuffle(v.raw, v.raw, 3, 2, 1, 0)};
}

// ------------------------------ TableLookupLanes

// Returned by SetTableIndices for use by TableLookupLanes.
template <typename T, size_t N>
struct Indices128 {
  __v128_u raw;
};

template <typename T, size_t N, typename TI, HWY_IF_LE128(T, N)>
HWY_API Indices128<T, N> IndicesFromVec(Simd<T, N, 0> d, Vec128<TI, N> vec) {
  static_assert(sizeof(T) == sizeof(TI), "Index size must match lane");
#if HWY_IS_DEBUG_BUILD
  const Rebind<TI, decltype(d)> di;
  HWY_DASSERT(AllFalse(di, Lt(vec, Zero(di))) &&
              AllTrue(di, Lt(vec, Set(di, static_cast<TI>(N)))));
#endif

  const Repartition<uint8_t, decltype(d)> d8;
  using V8 = VFromD<decltype(d8)>;
  const Repartition<uint16_t, decltype(d)> d16;

  // Broadcast each lane index to all bytes of T and shift to bytes
  static_assert(sizeof(T) == 4 || sizeof(T) == 8, "");
  if (sizeof(T) == 4) {
    alignas(16) constexpr uint8_t kBroadcastLaneBytes[16] = {
        0, 0, 0, 0, 4, 4, 4, 4, 8, 8, 8, 8, 12, 12, 12, 12};
    const V8 lane_indices =
        TableLookupBytes(BitCast(d8, vec), Load(d8, kBroadcastLaneBytes));
    const V8 byte_indices =
        BitCast(d8, ShiftLeft<2>(BitCast(d16, lane_indices)));
    alignas(16) constexpr uint8_t kByteOffsets[16] = {0, 1, 2, 3, 0, 1, 2, 3,
                                                      0, 1, 2, 3, 0, 1, 2, 3};
    return Indices128<T, N>{Add(byte_indices, Load(d8, kByteOffsets)).raw};
  } else {
    alignas(16) constexpr uint8_t kBroadcastLaneBytes[16] = {
        0, 0, 0, 0, 0, 0, 0, 0, 8, 8, 8, 8, 8, 8, 8, 8};
    const V8 lane_indices =
        TableLookupBytes(BitCast(d8, vec), Load(d8, kBroadcastLaneBytes));
    const V8 byte_indices =
        BitCast(d8, ShiftLeft<3>(BitCast(d16, lane_indices)));
    alignas(16) constexpr uint8_t kByteOffsets[16] = {0, 1, 2, 3, 4, 5, 6, 7,
                                                      0, 1, 2, 3, 4, 5, 6, 7};
    return Indices128<T, N>{Add(byte_indices, Load(d8, kByteOffsets)).raw};
  }
}

template <typename T, size_t N, typename TI, HWY_IF_LE128(T, N)>
HWY_API Indices128<T, N> SetTableIndices(Simd<T, N, 0> d, const TI* idx) {
  const Rebind<TI, decltype(d)> di;
  return IndicesFromVec(d, LoadU(di, idx));
}

template <typename T, size_t N>
HWY_API Vec128<T, N> TableLookupLanes(Vec128<T, N> v, Indices128<T, N> idx) {
  using TI = MakeSigned<T>;
  const DFromV<decltype(v)> d;
  const Rebind<TI, decltype(d)> di;
  return BitCast(d, TableLookupBytes(BitCast(di, v), Vec128<TI, N>{idx.raw}));
}

// ------------------------------ Reverse (Shuffle0123, Shuffle2301, Shuffle01)

// Single lane: no change
template <typename T>
HWY_API Vec128<T, 1> Reverse(Simd<T, 1, 0> /* tag */, const Vec128<T, 1> v) {
  return v;
}

// Two lanes: shuffle
template <typename T, HWY_IF_LANE_SIZE(T, 4)>
HWY_API Vec128<T, 2> Reverse(Simd<T, 2, 0> /* tag */, const Vec128<T, 2> v) {
  return Vec128<T, 2>{Shuffle2301(Vec128<T>{v.raw}).raw};
}

template <typename T, HWY_IF_LANE_SIZE(T, 8)>
HWY_API Vec128<T> Reverse(Full128<T> /* tag */, const Vec128<T> v) {
  return Shuffle01(v);
}

// Four lanes: shuffle
template <typename T, HWY_IF_LANE_SIZE(T, 4)>
HWY_API Vec128<T> Reverse(Full128<T> /* tag */, const Vec128<T> v) {
  return Shuffle0123(v);
}

// 16-bit
template <typename T, size_t N, HWY_IF_LANE_SIZE(T, 2)>
HWY_API Vec128<T, N> Reverse(Simd<T, N, 0> d, const Vec128<T, N> v) {
  const RepartitionToWide<RebindToUnsigned<decltype(d)>> du32;
  return BitCast(d, RotateRight<16>(Reverse(du32, BitCast(du32, v))));
}

// ------------------------------ Reverse2

template <typename T, size_t N, HWY_IF_LANE_SIZE(T, 2)>
HWY_API Vec128<T, N> Reverse2(Simd<T, N, 0> d, const Vec128<T, N> v) {
  const RepartitionToWide<RebindToUnsigned<decltype(d)>> du32;
  return BitCast(d, RotateRight<16>(BitCast(du32, v)));
}

template <typename T, size_t N, HWY_IF_LANE_SIZE(T, 4)>
HWY_API Vec128<T, N> Reverse2(Simd<T, N, 0> /* tag */, const Vec128<T, N> v) {
  return Shuffle2301(v);
}

template <typename T, size_t N, HWY_IF_LANE_SIZE(T, 8)>
HWY_API Vec128<T, N> Reverse2(Simd<T, N, 0> /* tag */, const Vec128<T, N> v) {
  return Shuffle01(v);
}

// ------------------------------ Reverse4

template <typename T, size_t N, HWY_IF_LANE_SIZE(T, 2)>
HWY_API Vec128<T, N> Reverse4(Simd<T, N, 0> d, const Vec128<T, N> v) {
  return BitCast(d, Vec128<uint16_t, N>{wasm_i16x8_shuffle(v.raw, v.raw, 3, 2,
                                                           1, 0, 7, 6, 5, 4)});
}

template <typename T, size_t N, HWY_IF_LANE_SIZE(T, 4)>
HWY_API Vec128<T, N> Reverse4(Simd<T, N, 0> /* tag */, const Vec128<T, N> v) {
  return Shuffle0123(v);
}

template <typename T, size_t N, HWY_IF_LANE_SIZE(T, 8)>
HWY_API Vec128<T, N> Reverse4(Simd<T, N, 0> /* tag */, const Vec128<T, N>) {
  HWY_ASSERT(0);  // don't have 8 u64 lanes
}

// ------------------------------ Reverse8

template <typename T, size_t N, HWY_IF_LANE_SIZE(T, 2)>
HWY_API Vec128<T, N> Reverse8(Simd<T, N, 0> d, const Vec128<T, N> v) {
  return Reverse(d, v);
}

template <typename T, size_t N, HWY_IF_NOT_LANE_SIZE(T, 2)>
HWY_API Vec128<T, N> Reverse8(Simd<T, N, 0>, const Vec128<T, N>) {
  HWY_ASSERT(0);  // don't have 8 lanes unless 16-bit
}

// ------------------------------ InterleaveLower

template <size_t N>
HWY_API Vec128<uint8_t, N> InterleaveLower(Vec128<uint8_t, N> a,
                                           Vec128<uint8_t, N> b) {
  return Vec128<uint8_t, N>{wasm_i8x16_shuffle(
      a.raw, b.raw, 0, 16, 1, 17, 2, 18, 3, 19, 4, 20, 5, 21, 6, 22, 7, 23)};
}
template <size_t N>
HWY_API Vec128<uint16_t, N> InterleaveLower(Vec128<uint16_t, N> a,
                                            Vec128<uint16_t, N> b) {
  return Vec128<uint16_t, N>{
      wasm_i16x8_shuffle(a.raw, b.raw, 0, 8, 1, 9, 2, 10, 3, 11)};
}
template <size_t N>
HWY_API Vec128<uint32_t, N> InterleaveLower(Vec128<uint32_t, N> a,
                                            Vec128<uint32_t, N> b) {
  return Vec128<uint32_t, N>{wasm_i32x4_shuffle(a.raw, b.raw, 0, 4, 1, 5)};
}
template <size_t N>
HWY_API Vec128<uint64_t, N> InterleaveLower(Vec128<uint64_t, N> a,
                                            Vec128<uint64_t, N> b) {
  return Vec128<uint64_t, N>{wasm_i64x2_shuffle(a.raw, b.raw, 0, 2)};
}

template <size_t N>
HWY_API Vec128<int8_t, N> InterleaveLower(Vec128<int8_t, N> a,
                                          Vec128<int8_t, N> b) {
  return Vec128<int8_t, N>{wasm_i8x16_shuffle(
      a.raw, b.raw, 0, 16, 1, 17, 2, 18, 3, 19, 4, 20, 5, 21, 6, 22, 7, 23)};
}
template <size_t N>
HWY_API Vec128<int16_t, N> InterleaveLower(Vec128<int16_t, N> a,
                                           Vec128<int16_t, N> b) {
  return Vec128<int16_t, N>{
      wasm_i16x8_shuffle(a.raw, b.raw, 0, 8, 1, 9, 2, 10, 3, 11)};
}
template <size_t N>
HWY_API Vec128<int32_t, N> InterleaveLower(Vec128<int32_t, N> a,
                                           Vec128<int32_t, N> b) {
  return Vec128<int32_t, N>{wasm_i32x4_shuffle(a.raw, b.raw, 0, 4, 1, 5)};
}
template <size_t N>
HWY_API Vec128<int64_t, N> InterleaveLower(Vec128<int64_t, N> a,
                                           Vec128<int64_t, N> b) {
  return Vec128<int64_t, N>{wasm_i64x2_shuffle(a.raw, b.raw, 0, 2)};
}

template <size_t N>
HWY_API Vec128<float, N> InterleaveLower(Vec128<float, N> a,
                                         Vec128<float, N> b) {
  return Vec128<float, N>{wasm_i32x4_shuffle(a.raw, b.raw, 0, 4, 1, 5)};
}

template <size_t N>
HWY_API Vec128<double, N> InterleaveLower(Vec128<double, N> a,
                                          Vec128<double, N> b) {
  return Vec128<double, N>{wasm_i64x2_shuffle(a.raw, b.raw, 0, 2)};
}

// Additional overload for the optional tag.
template <class V>
HWY_API V InterleaveLower(DFromV<V> /* tag */, V a, V b) {
  return InterleaveLower(a, b);
}

// ------------------------------ InterleaveUpper (UpperHalf)

// All functions inside detail lack the required D parameter.
namespace detail {

template <size_t N>
HWY_API Vec128<uint8_t, N> InterleaveUpper(Vec128<uint8_t, N> a,
                                           Vec128<uint8_t, N> b) {
  return Vec128<uint8_t, N>{wasm_i8x16_shuffle(a.raw, b.raw, 8, 24, 9, 25, 10,
                                               26, 11, 27, 12, 28, 13, 29, 14,
                                               30, 15, 31)};
}
template <size_t N>
HWY_API Vec128<uint16_t, N> InterleaveUpper(Vec128<uint16_t, N> a,
                                            Vec128<uint16_t, N> b) {
  return Vec128<uint16_t, N>{
      wasm_i16x8_shuffle(a.raw, b.raw, 4, 12, 5, 13, 6, 14, 7, 15)};
}
template <size_t N>
HWY_API Vec128<uint32_t, N> InterleaveUpper(Vec128<uint32_t, N> a,
                                            Vec128<uint32_t, N> b) {
  return Vec128<uint32_t, N>{wasm_i32x4_shuffle(a.raw, b.raw, 2, 6, 3, 7)};
}
template <size_t N>
HWY_API Vec128<uint64_t, N> InterleaveUpper(Vec128<uint64_t, N> a,
                                            Vec128<uint64_t, N> b) {
  return Vec128<uint64_t, N>{wasm_i64x2_shuffle(a.raw, b.raw, 1, 3)};
}

template <size_t N>
HWY_API Vec128<int8_t, N> InterleaveUpper(Vec128<int8_t, N> a,
                                          Vec128<int8_t, N> b) {
  return Vec128<int8_t, N>{wasm_i8x16_shuffle(a.raw, b.raw, 8, 24, 9, 25, 10,
                                              26, 11, 27, 12, 28, 13, 29, 14,
                                              30, 15, 31)};
}
template <size_t N>
HWY_API Vec128<int16_t, N> InterleaveUpper(Vec128<int16_t, N> a,
                                           Vec128<int16_t, N> b) {
  return Vec128<int16_t, N>{
      wasm_i16x8_shuffle(a.raw, b.raw, 4, 12, 5, 13, 6, 14, 7, 15)};
}
template <size_t N>
HWY_API Vec128<int32_t, N> InterleaveUpper(Vec128<int32_t, N> a,
                                           Vec128<int32_t, N> b) {
  return Vec128<int32_t, N>{wasm_i32x4_shuffle(a.raw, b.raw, 2, 6, 3, 7)};
}
template <size_t N>
HWY_API Vec128<int64_t, N> InterleaveUpper(Vec128<int64_t, N> a,
                                           Vec128<int64_t, N> b) {
  return Vec128<int64_t, N>{wasm_i64x2_shuffle(a.raw, b.raw, 1, 3)};
}

template <size_t N>
HWY_API Vec128<float, N> InterleaveUpper(Vec128<float, N> a,
                                         Vec128<float, N> b) {
  return Vec128<float, N>{wasm_i32x4_shuffle(a.raw, b.raw, 2, 6, 3, 7)};
}

template <size_t N>
HWY_API Vec128<double, N> InterleaveUpper(Vec128<double, N> a,
                                          Vec128<double, N> b) {
  return Vec128<double, N>{wasm_i64x2_shuffle(a.raw, b.raw, 1, 3)};
}

}  // namespace detail

// Full
template <typename T, class V = Vec128<T>>
HWY_API V InterleaveUpper(Full128<T> /* tag */, V a, V b) {
  return detail::InterleaveUpper(a, b);
}

// Partial
template <typename T, size_t N, HWY_IF_LE64(T, N), class V = Vec128<T, N>>
HWY_API V InterleaveUpper(Simd<T, N, 0> d, V a, V b) {
  const Half<decltype(d)> d2;
  return InterleaveLower(d, V{UpperHalf(d2, a).raw}, V{UpperHalf(d2, b).raw});
}

// ------------------------------ ZipLower/ZipUpper (InterleaveLower)

// Same as Interleave*, except that the return lanes are double-width integers;
// this is necessary because the single-lane scalar cannot return two values.
template <class V, class DW = RepartitionToWide<DFromV<V>>>
HWY_API VFromD<DW> ZipLower(V a, V b) {
  return BitCast(DW(), InterleaveLower(a, b));
}
template <class V, class D = DFromV<V>, class DW = RepartitionToWide<D>>
HWY_API VFromD<DW> ZipLower(DW dw, V a, V b) {
  return BitCast(dw, InterleaveLower(D(), a, b));
}

template <class V, class D = DFromV<V>, class DW = RepartitionToWide<D>>
HWY_API VFromD<DW> ZipUpper(DW dw, V a, V b) {
  return BitCast(dw, InterleaveUpper(D(), a, b));
}

// ================================================== COMBINE

// ------------------------------ Combine (InterleaveLower)

// N = N/2 + N/2 (upper half undefined)
template <typename T, size_t N>
HWY_API Vec128<T, N> Combine(Simd<T, N, 0> d, Vec128<T, N / 2> hi_half,
                             Vec128<T, N / 2> lo_half) {
  const Half<decltype(d)> d2;
  const RebindToUnsigned<decltype(d2)> du2;
  // Treat half-width input as one lane, and expand to two lanes.
  using VU = Vec128<UnsignedFromSize<N * sizeof(T) / 2>, 2>;
  const VU lo{BitCast(du2, lo_half).raw};
  const VU hi{BitCast(du2, hi_half).raw};
  return BitCast(d, InterleaveLower(lo, hi));
}

// ------------------------------ ZeroExtendVector (Combine, IfThenElseZero)

template <typename T, size_t N>
HWY_API Vec128<T, N> ZeroExtendVector(Simd<T, N, 0> d, Vec128<T, N / 2> lo) {
  return IfThenElseZero(FirstN(d, N / 2), Vec128<T, N>{lo.raw});
}

// ------------------------------ ConcatLowerLower

// hiH,hiL loH,loL |-> hiL,loL (= lower halves)
template <typename T>
HWY_API Vec128<T> ConcatLowerLower(Full128<T> /* tag */, const Vec128<T> hi,
                                   const Vec128<T> lo) {
  return Vec128<T>{wasm_i64x2_shuffle(lo.raw, hi.raw, 0, 2)};
}
template <typename T, size_t N, HWY_IF_LE64(T, N)>
HWY_API Vec128<T, N> ConcatLowerLower(Simd<T, N, 0> d, const Vec128<T, N> hi,
                                      const Vec128<T, N> lo) {
  const Half<decltype(d)> d2;
  return Combine(d, LowerHalf(d2, hi), LowerHalf(d2, lo));
}

// ------------------------------ ConcatUpperUpper

template <typename T>
HWY_API Vec128<T> ConcatUpperUpper(Full128<T> /* tag */, const Vec128<T> hi,
                                   const Vec128<T> lo) {
  return Vec128<T>{wasm_i64x2_shuffle(lo.raw, hi.raw, 1, 3)};
}
template <typename T, size_t N, HWY_IF_LE64(T, N)>
HWY_API Vec128<T, N> ConcatUpperUpper(Simd<T, N, 0> d, const Vec128<T, N> hi,
                                      const Vec128<T, N> lo) {
  const Half<decltype(d)> d2;
  return Combine(d, UpperHalf(d2, hi), UpperHalf(d2, lo));
}

// ------------------------------ ConcatLowerUpper

template <typename T>
HWY_API Vec128<T> ConcatLowerUpper(Full128<T> d, const Vec128<T> hi,
                                   const Vec128<T> lo) {
  return CombineShiftRightBytes<8>(d, hi, lo);
}
template <typename T, size_t N, HWY_IF_LE64(T, N)>
HWY_API Vec128<T, N> ConcatLowerUpper(Simd<T, N, 0> d, const Vec128<T, N> hi,
                                      const Vec128<T, N> lo) {
  const Half<decltype(d)> d2;
  return Combine(d, LowerHalf(d2, hi), UpperHalf(d2, lo));
}

// ------------------------------ ConcatUpperLower
template <typename T, size_t N>
HWY_API Vec128<T, N> ConcatUpperLower(Simd<T, N, 0> d, const Vec128<T, N> hi,
                                      const Vec128<T, N> lo) {
  return IfThenElse(FirstN(d, Lanes(d) / 2), lo, hi);
}

// ------------------------------ ConcatOdd

// 8-bit full
template <typename T, HWY_IF_LANE_SIZE(T, 1)>
HWY_API Vec128<T> ConcatOdd(Full128<T> /* tag */, Vec128<T> hi, Vec128<T> lo) {
  return Vec128<T>{wasm_i8x16_shuffle(lo.raw, hi.raw, 1, 3, 5, 7, 9, 11, 13, 15,
                                      17, 19, 21, 23, 25, 27, 29, 31)};
}

// 8-bit x8
template <typename T, HWY_IF_LANE_SIZE(T, 1)>
HWY_API Vec128<T, 8> ConcatOdd(Simd<T, 8, 0> /* tag */, Vec128<T, 8> hi,
                               Vec128<T, 8> lo) {
  // Don't care about upper half.
  return Vec128<T, 8>{wasm_i8x16_shuffle(lo.raw, hi.raw, 1, 3, 5, 7, 17, 19, 21,
                                         23, 1, 3, 5, 7, 17, 19, 21, 23)};
}

// 8-bit x4
template <typename T, HWY_IF_LANE_SIZE(T, 1)>
HWY_API Vec128<T, 4> ConcatOdd(Simd<T, 4, 0> /* tag */, Vec128<T, 4> hi,
                               Vec128<T, 4> lo) {
  // Don't care about upper 3/4.
  return Vec128<T, 4>{wasm_i8x16_shuffle(lo.raw, hi.raw, 1, 3, 17, 19, 1, 3, 17,
                                         19, 1, 3, 17, 19, 1, 3, 17, 19)};
}

// 16-bit full
template <typename T, HWY_IF_LANE_SIZE(T, 2)>
HWY_API Vec128<T> ConcatOdd(Full128<T> /* tag */, Vec128<T> hi, Vec128<T> lo) {
  return Vec128<T>{
      wasm_i16x8_shuffle(lo.raw, hi.raw, 1, 3, 5, 7, 9, 11, 13, 15)};
}

// 16-bit x4
template <typename T, HWY_IF_LANE_SIZE(T, 2)>
HWY_API Vec128<T, 4> ConcatOdd(Simd<T, 4, 0> /* tag */, Vec128<T, 4> hi,
                               Vec128<T, 4> lo) {
  // Don't care about upper half.
  return Vec128<T, 4>{
      wasm_i16x8_shuffle(lo.raw, hi.raw, 1, 3, 9, 11, 1, 3, 9, 11)};
}

// 32-bit full
template <typename T, HWY_IF_LANE_SIZE(T, 4)>
HWY_API Vec128<T> ConcatOdd(Full128<T> /* tag */, Vec128<T> hi, Vec128<T> lo) {
  return Vec128<T>{wasm_i32x4_shuffle(lo.raw, hi.raw, 1, 3, 5, 7)};
}

// Any T x2
template <typename T>
HWY_API Vec128<T, 2> ConcatOdd(Simd<T, 2, 0> d, Vec128<T, 2> hi,
                               Vec128<T, 2> lo) {
  return InterleaveUpper(d, lo, hi);
}

// ------------------------------ ConcatEven (InterleaveLower)

// 8-bit full
template <typename T, HWY_IF_LANE_SIZE(T, 1)>
HWY_API Vec128<T> ConcatEven(Full128<T> /* tag */, Vec128<T> hi, Vec128<T> lo) {
  return Vec128<T>{wasm_i8x16_shuffle(lo.raw, hi.raw, 0, 2, 4, 6, 8, 10, 12, 14,
                                      16, 18, 20, 22, 24, 26, 28, 30)};
}

// 8-bit x8
template <typename T, HWY_IF_LANE_SIZE(T, 1)>
HWY_API Vec128<T, 8> ConcatEven(Simd<T, 8, 0> /* tag */, Vec128<T, 8> hi,
                                Vec128<T, 8> lo) {
  // Don't care about upper half.
  return Vec128<T, 8>{wasm_i8x16_shuffle(lo.raw, hi.raw, 0, 2, 4, 6, 16, 18, 20,
                                         22, 0, 2, 4, 6, 16, 18, 20, 22)};
}

// 8-bit x4
template <typename T, HWY_IF_LANE_SIZE(T, 1)>
HWY_API Vec128<T, 4> ConcatEven(Simd<T, 4, 0> /* tag */, Vec128<T, 4> hi,
                                Vec128<T, 4> lo) {
  // Don't care about upper 3/4.
  return Vec128<T, 4>{wasm_i8x16_shuffle(lo.raw, hi.raw, 0, 2, 16, 18, 0, 2, 16,
                                         18, 0, 2, 16, 18, 0, 2, 16, 18)};
}

// 16-bit full
template <typename T, HWY_IF_LANE_SIZE(T, 2)>
HWY_API Vec128<T> ConcatEven(Full128<T> /* tag */, Vec128<T> hi, Vec128<T> lo) {
  return Vec128<T>{
      wasm_i16x8_shuffle(lo.raw, hi.raw, 0, 2, 4, 6, 8, 10, 12, 14)};
}

// 16-bit x4
template <typename T, HWY_IF_LANE_SIZE(T, 2)>
HWY_API Vec128<T, 4> ConcatEven(Simd<T, 4, 0> /* tag */, Vec128<T, 4> hi,
                                Vec128<T, 4> lo) {
  // Don't care about upper half.
  return Vec128<T, 4>{
      wasm_i16x8_shuffle(lo.raw, hi.raw, 0, 2, 8, 10, 0, 2, 8, 10)};
}

// 32-bit full
template <typename T, HWY_IF_LANE_SIZE(T, 4)>
HWY_API Vec128<T> ConcatEven(Full128<T> /* tag */, Vec128<T> hi, Vec128<T> lo) {
  return Vec128<T>{wasm_i32x4_shuffle(lo.raw, hi.raw, 0, 2, 4, 6)};
}

// Any T x2
template <typename T>
HWY_API Vec128<T, 2> ConcatEven(Simd<T, 2, 0> d, Vec128<T, 2> hi,
                                Vec128<T, 2> lo) {
  return InterleaveLower(d, lo, hi);
}

// ------------------------------ DupEven (InterleaveLower)

template <typename T, size_t N, HWY_IF_LANE_SIZE(T, 4)>
HWY_API Vec128<T, N> DupEven(Vec128<T, N> v) {
  return Vec128<T, N>{wasm_i32x4_shuffle(v.raw, v.raw, 0, 0, 2, 2)};
}

template <typename T, size_t N, HWY_IF_LANE_SIZE(T, 8)>
HWY_API Vec128<T, N> DupEven(const Vec128<T, N> v) {
  return InterleaveLower(DFromV<decltype(v)>(), v, v);
}

// ------------------------------ DupOdd (InterleaveUpper)

template <typename T, size_t N, HWY_IF_LANE_SIZE(T, 4)>
HWY_API Vec128<T, N> DupOdd(Vec128<T, N> v) {
  return Vec128<T, N>{wasm_i32x4_shuffle(v.raw, v.raw, 1, 1, 3, 3)};
}

template <typename T, size_t N, HWY_IF_LANE_SIZE(T, 8)>
HWY_API Vec128<T, N> DupOdd(const Vec128<T, N> v) {
  return InterleaveUpper(DFromV<decltype(v)>(), v, v);
}

// ------------------------------ OddEven

namespace detail {

template <typename T, size_t N>
HWY_INLINE Vec128<T, N> OddEven(hwy::SizeTag<1> /* tag */, const Vec128<T, N> a,
                                const Vec128<T, N> b) {
  const DFromV<decltype(a)> d;
  const Repartition<uint8_t, decltype(d)> d8;
  alignas(16) constexpr uint8_t mask[16] = {0xFF, 0, 0xFF, 0, 0xFF, 0, 0xFF, 0,
                                            0xFF, 0, 0xFF, 0, 0xFF, 0, 0xFF, 0};
  return IfThenElse(MaskFromVec(BitCast(d, Load(d8, mask))), b, a);
}
template <typename T, size_t N>
HWY_INLINE Vec128<T, N> OddEven(hwy::SizeTag<2> /* tag */, const Vec128<T, N> a,
                                const Vec128<T, N> b) {
  return Vec128<T, N>{
      wasm_i16x8_shuffle(a.raw, b.raw, 8, 1, 10, 3, 12, 5, 14, 7)};
}
template <typename T, size_t N>
HWY_INLINE Vec128<T, N> OddEven(hwy::SizeTag<4> /* tag */, const Vec128<T, N> a,
                                const Vec128<T, N> b) {
  return Vec128<T, N>{wasm_i32x4_shuffle(a.raw, b.raw, 4, 1, 6, 3)};
}
template <typename T, size_t N>
HWY_INLINE Vec128<T, N> OddEven(hwy::SizeTag<8> /* tag */, const Vec128<T, N> a,
                                const Vec128<T, N> b) {
  return Vec128<T, N>{wasm_i64x2_shuffle(a.raw, b.raw, 2, 1)};
}

}  // namespace detail

template <typename T, size_t N>
HWY_API Vec128<T, N> OddEven(const Vec128<T, N> a, const Vec128<T, N> b) {
  return detail::OddEven(hwy::SizeTag<sizeof(T)>(), a, b);
}
template <size_t N>
HWY_API Vec128<float, N> OddEven(const Vec128<float, N> a,
                                 const Vec128<float, N> b) {
  return Vec128<float, N>{wasm_i32x4_shuffle(a.raw, b.raw, 4, 1, 6, 3)};
}

// ------------------------------ OddEvenBlocks
template <typename T, size_t N>
HWY_API Vec128<T, N> OddEvenBlocks(Vec128<T, N> /* odd */, Vec128<T, N> even) {
  return even;
}

// ------------------------------ SwapAdjacentBlocks

template <typename T, size_t N>
HWY_API Vec128<T, N> SwapAdjacentBlocks(Vec128<T, N> v) {
  return v;
}

// ------------------------------ ReverseBlocks

// Single block: no change
template <typename T>
HWY_API Vec128<T> ReverseBlocks(Full128<T> /* tag */, const Vec128<T> v) {
  return v;
}

// ================================================== CONVERT

// ------------------------------ Promotions (part w/ narrow lanes -> full)

// Unsigned: zero-extend.
template <size_t N>
HWY_API Vec128<uint16_t, N> PromoteTo(Simd<uint16_t, N, 0> /* tag */,
                                      const Vec128<uint8_t, N> v) {
  return Vec128<uint16_t, N>{wasm_u16x8_extend_low_u8x16(v.raw)};
}
template <size_t N>
HWY_API Vec128<uint32_t, N> PromoteTo(Simd<uint32_t, N, 0> /* tag */,
                                      const Vec128<uint8_t, N> v) {
  return Vec128<uint32_t, N>{
      wasm_u32x4_extend_low_u16x8(wasm_u16x8_extend_low_u8x16(v.raw))};
}
template <size_t N>
HWY_API Vec128<int16_t, N> PromoteTo(Simd<int16_t, N, 0> /* tag */,
                                     const Vec128<uint8_t, N> v) {
  return Vec128<int16_t, N>{wasm_u16x8_extend_low_u8x16(v.raw)};
}
template <size_t N>
HWY_API Vec128<int32_t, N> PromoteTo(Simd<int32_t, N, 0> /* tag */,
                                     const Vec128<uint8_t, N> v) {
  return Vec128<int32_t, N>{
      wasm_u32x4_extend_low_u16x8(wasm_u16x8_extend_low_u8x16(v.raw))};
}
template <size_t N>
HWY_API Vec128<uint32_t, N> PromoteTo(Simd<uint32_t, N, 0> /* tag */,
                                      const Vec128<uint16_t, N> v) {
  return Vec128<uint32_t, N>{wasm_u32x4_extend_low_u16x8(v.raw)};
}
template <size_t N>
HWY_API Vec128<uint64_t, N> PromoteTo(Simd<uint64_t, N, 0> /* tag */,
                                      const Vec128<uint32_t, N> v) {
  return Vec128<uint64_t, N>{wasm_u64x2_extend_low_u32x4(v.raw)};
}

template <size_t N>
HWY_API Vec128<int32_t, N> PromoteTo(Simd<int32_t, N, 0> /* tag */,
                                     const Vec128<uint16_t, N> v) {
  return Vec128<int32_t, N>{wasm_u32x4_extend_low_u16x8(v.raw)};
}

// Signed: replicate sign bit.
template <size_t N>
HWY_API Vec128<int16_t, N> PromoteTo(Simd<int16_t, N, 0> /* tag */,
                                     const Vec128<int8_t, N> v) {
  return Vec128<int16_t, N>{wasm_i16x8_extend_low_i8x16(v.raw)};
}
template <size_t N>
HWY_API Vec128<int32_t, N> PromoteTo(Simd<int32_t, N, 0> /* tag */,
                                     const Vec128<int8_t, N> v) {
  return Vec128<int32_t, N>{
      wasm_i32x4_extend_low_i16x8(wasm_i16x8_extend_low_i8x16(v.raw))};
}
template <size_t N>
HWY_API Vec128<int32_t, N> PromoteTo(Simd<int32_t, N, 0> /* tag */,
                                     const Vec128<int16_t, N> v) {
  return Vec128<int32_t, N>{wasm_i32x4_extend_low_i16x8(v.raw)};
}
template <size_t N>
HWY_API Vec128<int64_t, N> PromoteTo(Simd<int64_t, N, 0> /* tag */,
                                     const Vec128<int32_t, N> v) {
  return Vec128<int64_t, N>{wasm_i64x2_extend_low_i32x4(v.raw)};
}

template <size_t N>
HWY_API Vec128<double, N> PromoteTo(Simd<double, N, 0> /* tag */,
                                    const Vec128<int32_t, N> v) {
  return Vec128<double, N>{wasm_f64x2_convert_low_i32x4(v.raw)};
}

template <size_t N>
HWY_API Vec128<float, N> PromoteTo(Simd<float, N, 0> df32,
                                   const Vec128<float16_t, N> v) {
  const RebindToSigned<decltype(df32)> di32;
  const RebindToUnsigned<decltype(df32)> du32;
  // Expand to u32 so we can shift.
  const auto bits16 = PromoteTo(du32, Vec128<uint16_t, N>{v.raw});
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

template <size_t N>
HWY_API Vec128<float, N> PromoteTo(Simd<float, N, 0> df32,
                                   const Vec128<bfloat16_t, N> v) {
  const Rebind<uint16_t, decltype(df32)> du16;
  const RebindToSigned<decltype(df32)> di32;
  return BitCast(df32, ShiftLeft<16>(PromoteTo(di32, BitCast(du16, v))));
}

// ------------------------------ Demotions (full -> part w/ narrow lanes)

template <size_t N>
HWY_API Vec128<uint16_t, N> DemoteTo(Simd<uint16_t, N, 0> /* tag */,
                                     const Vec128<int32_t, N> v) {
  return Vec128<uint16_t, N>{wasm_u16x8_narrow_i32x4(v.raw, v.raw)};
}

template <size_t N>
HWY_API Vec128<int16_t, N> DemoteTo(Simd<int16_t, N, 0> /* tag */,
                                    const Vec128<int32_t, N> v) {
  return Vec128<int16_t, N>{wasm_i16x8_narrow_i32x4(v.raw, v.raw)};
}

template <size_t N>
HWY_API Vec128<uint8_t, N> DemoteTo(Simd<uint8_t, N, 0> /* tag */,
                                    const Vec128<int32_t, N> v) {
  const auto intermediate = wasm_i16x8_narrow_i32x4(v.raw, v.raw);
  return Vec128<uint8_t, N>{
      wasm_u8x16_narrow_i16x8(intermediate, intermediate)};
}

template <size_t N>
HWY_API Vec128<uint8_t, N> DemoteTo(Simd<uint8_t, N, 0> /* tag */,
                                    const Vec128<int16_t, N> v) {
  return Vec128<uint8_t, N>{wasm_u8x16_narrow_i16x8(v.raw, v.raw)};
}

template <size_t N>
HWY_API Vec128<int8_t, N> DemoteTo(Simd<int8_t, N, 0> /* tag */,
                                   const Vec128<int32_t, N> v) {
  const auto intermediate = wasm_i16x8_narrow_i32x4(v.raw, v.raw);
  return Vec128<int8_t, N>{wasm_i8x16_narrow_i16x8(intermediate, intermediate)};
}

template <size_t N>
HWY_API Vec128<int8_t, N> DemoteTo(Simd<int8_t, N, 0> /* tag */,
                                   const Vec128<int16_t, N> v) {
  return Vec128<int8_t, N>{wasm_i8x16_narrow_i16x8(v.raw, v.raw)};
}

template <size_t N>
HWY_API Vec128<int32_t, N> DemoteTo(Simd<int32_t, N, 0> /* di */,
                                    const Vec128<double, N> v) {
  return Vec128<int32_t, N>{wasm_i32x4_trunc_sat_f64x2_zero(v.raw)};
}

template <size_t N>
HWY_API Vec128<float16_t, N> DemoteTo(Simd<float16_t, N, 0> df16,
                                      const Vec128<float, N> v) {
  const RebindToUnsigned<decltype(df16)> du16;
  const Rebind<uint32_t, decltype(du16)> du;
  const RebindToSigned<decltype(du)> di;
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
  return Vec128<float16_t, N>{DemoteTo(du16, bits16).raw};
}

template <size_t N>
HWY_API Vec128<bfloat16_t, N> DemoteTo(Simd<bfloat16_t, N, 0> dbf16,
                                       const Vec128<float, N> v) {
  const Rebind<int32_t, decltype(dbf16)> di32;
  const Rebind<uint32_t, decltype(dbf16)> du32;  // for logical shift right
  const Rebind<uint16_t, decltype(dbf16)> du16;
  const auto bits_in_32 = BitCast(di32, ShiftRight<16>(BitCast(du32, v)));
  return BitCast(dbf16, DemoteTo(du16, bits_in_32));
}

template <size_t N>
HWY_API Vec128<bfloat16_t, 2 * N> ReorderDemote2To(
    Simd<bfloat16_t, 2 * N, 0> dbf16, Vec128<float, N> a, Vec128<float, N> b) {
  const RebindToUnsigned<decltype(dbf16)> du16;
  const Repartition<uint32_t, decltype(dbf16)> du32;
  const Vec128<uint32_t, N> b_in_even = ShiftRight<16>(BitCast(du32, b));
  return BitCast(dbf16, OddEven(BitCast(du16, a), BitCast(du16, b_in_even)));
}

// For already range-limited input [0, 255].
template <size_t N>
HWY_API Vec128<uint8_t, N> U8FromU32(const Vec128<uint32_t, N> v) {
  const auto intermediate = wasm_i16x8_narrow_i32x4(v.raw, v.raw);
  return Vec128<uint8_t, N>{
      wasm_u8x16_narrow_i16x8(intermediate, intermediate)};
}

// ------------------------------ Truncations

template <typename From, typename To, HWY_IF_UNSIGNED(From),
          HWY_IF_UNSIGNED(To),
          hwy::EnableIf<(sizeof(To) < sizeof(From))>* = nullptr>
HWY_API Vec128<To, 1> TruncateTo(Simd<To, 1, 0> /* tag */,
                                 const Vec128<From, 1> v) {
  const Repartition<To, DFromV<decltype(v)>> d;
  const auto v1 = BitCast(d, v);
  return Vec128<To, 1>{v1.raw};
}

HWY_API Vec128<uint8_t, 2> TruncateTo(Simd<uint8_t, 2, 0> /* tag */,
                                      const Vec128<uint64_t> v) {
  const Full128<uint8_t> d;
  const auto v1 = BitCast(d, v);
  const auto v2 = ConcatEven(d, v1, v1);
  const auto v4 = ConcatEven(d, v2, v2);
  return LowerHalf(LowerHalf(LowerHalf(ConcatEven(d, v4, v4))));
}

HWY_API Vec128<uint16_t, 2> TruncateTo(Simd<uint16_t, 2, 0> /* tag */,
                                       const Vec128<uint64_t> v) {
  const Full128<uint16_t> d;
  const auto v1 = BitCast(d, v);
  const auto v2 = ConcatEven(d, v1, v1);
  return LowerHalf(LowerHalf(ConcatEven(d, v2, v2)));
}

HWY_API Vec128<uint32_t, 2> TruncateTo(Simd<uint32_t, 2, 0> /* tag */,
                                       const Vec128<uint64_t> v) {
  const Full128<uint32_t> d;
  const auto v1 = BitCast(d, v);
  return LowerHalf(ConcatEven(d, v1, v1));
}

template <size_t N, hwy::EnableIf<N >= 2>* = nullptr>
HWY_API Vec128<uint8_t, N> TruncateTo(Simd<uint8_t, N, 0> /* tag */,
                                      const Vec128<uint32_t, N> v) {
  const Full128<uint8_t> d;
  const auto v1 = Vec128<uint8_t>{v.raw};
  const auto v2 = ConcatEven(d, v1, v1);
  const auto v3 = ConcatEven(d, v2, v2);
  return Vec128<uint8_t, N>{v3.raw};
}

template <size_t N, hwy::EnableIf<N >= 2>* = nullptr>
HWY_API Vec128<uint16_t, N> TruncateTo(Simd<uint16_t, N, 0> /* tag */,
                                       const Vec128<uint32_t, N> v) {
  const Full128<uint16_t> d;
  const auto v1 = Vec128<uint16_t>{v.raw};
  const auto v2 = ConcatEven(d, v1, v1);
  return Vec128<uint16_t, N>{v2.raw};
}

template <size_t N, hwy::EnableIf<N >= 2>* = nullptr>
HWY_API Vec128<uint8_t, N> TruncateTo(Simd<uint8_t, N, 0> /* tag */,
                                      const Vec128<uint16_t, N> v) {
  const Full128<uint8_t> d;
  const auto v1 = Vec128<uint8_t>{v.raw};
  const auto v2 = ConcatEven(d, v1, v1);
  return Vec128<uint8_t, N>{v2.raw};
}

// ------------------------------ Convert i32 <=> f32 (Round)

template <size_t N>
HWY_API Vec128<float, N> ConvertTo(Simd<float, N, 0> /* tag */,
                                   const Vec128<int32_t, N> v) {
  return Vec128<float, N>{wasm_f32x4_convert_i32x4(v.raw)};
}
// Truncates (rounds toward zero).
template <size_t N>
HWY_API Vec128<int32_t, N> ConvertTo(Simd<int32_t, N, 0> /* tag */,
                                     const Vec128<float, N> v) {
  return Vec128<int32_t, N>{wasm_i32x4_trunc_sat_f32x4(v.raw)};
}

template <size_t N>
HWY_API Vec128<int32_t, N> NearestInt(const Vec128<float, N> v) {
  return ConvertTo(Simd<int32_t, N, 0>(), Round(v));
}

// ================================================== MISC

// ------------------------------ SumsOf8 (ShiftRight, Add)
template <size_t N>
HWY_API Vec128<uint64_t, N / 8> SumsOf8(const Vec128<uint8_t, N> v) {
  const DFromV<decltype(v)> du8;
  const RepartitionToWide<decltype(du8)> du16;
  const RepartitionToWide<decltype(du16)> du32;
  const RepartitionToWide<decltype(du32)> du64;
  using VU16 = VFromD<decltype(du16)>;

  const VU16 vFDB97531 = ShiftRight<8>(BitCast(du16, v));
  const VU16 vECA86420 = And(BitCast(du16, v), Set(du16, 0xFF));
  const VU16 sFE_DC_BA_98_76_54_32_10 = Add(vFDB97531, vECA86420);

  const VU16 szz_FE_zz_BA_zz_76_zz_32 =
      BitCast(du16, ShiftRight<16>(BitCast(du32, sFE_DC_BA_98_76_54_32_10)));
  const VU16 sxx_FC_xx_B8_xx_74_xx_30 =
      Add(sFE_DC_BA_98_76_54_32_10, szz_FE_zz_BA_zz_76_zz_32);
  const VU16 szz_zz_xx_FC_zz_zz_xx_74 =
      BitCast(du16, ShiftRight<32>(BitCast(du64, sxx_FC_xx_B8_xx_74_xx_30)));
  const VU16 sxx_xx_xx_F8_xx_xx_xx_70 =
      Add(sxx_FC_xx_B8_xx_74_xx_30, szz_zz_xx_FC_zz_zz_xx_74);
  return And(BitCast(du64, sxx_xx_xx_F8_xx_xx_xx_70), Set(du64, 0xFFFF));
}

// ------------------------------ LoadMaskBits (TestBit)

namespace detail {

template <typename T, size_t N, HWY_IF_LANE_SIZE(T, 1)>
HWY_INLINE Mask128<T, N> LoadMaskBits(Simd<T, N, 0> d, uint64_t bits) {
  const RebindToUnsigned<decltype(d)> du;
  // Easier than Set(), which would require an >8-bit type, which would not
  // compile for T=uint8_t, N=1.
  const Vec128<T, N> vbits{wasm_i32x4_splat(static_cast<int32_t>(bits))};

  // Replicate bytes 8x such that each byte contains the bit that governs it.
  alignas(16) constexpr uint8_t kRep8[16] = {0, 0, 0, 0, 0, 0, 0, 0,
                                             1, 1, 1, 1, 1, 1, 1, 1};
  const auto rep8 = TableLookupBytes(vbits, Load(du, kRep8));

  alignas(16) constexpr uint8_t kBit[16] = {1, 2, 4, 8, 16, 32, 64, 128,
                                            1, 2, 4, 8, 16, 32, 64, 128};
  return RebindMask(d, TestBit(rep8, LoadDup128(du, kBit)));
}

template <typename T, size_t N, HWY_IF_LANE_SIZE(T, 2)>
HWY_INLINE Mask128<T, N> LoadMaskBits(Simd<T, N, 0> d, uint64_t bits) {
  const RebindToUnsigned<decltype(d)> du;
  alignas(16) constexpr uint16_t kBit[8] = {1, 2, 4, 8, 16, 32, 64, 128};
  return RebindMask(
      d, TestBit(Set(du, static_cast<uint16_t>(bits)), Load(du, kBit)));
}

template <typename T, size_t N, HWY_IF_LANE_SIZE(T, 4)>
HWY_INLINE Mask128<T, N> LoadMaskBits(Simd<T, N, 0> d, uint64_t bits) {
  const RebindToUnsigned<decltype(d)> du;
  alignas(16) constexpr uint32_t kBit[8] = {1, 2, 4, 8};
  return RebindMask(
      d, TestBit(Set(du, static_cast<uint32_t>(bits)), Load(du, kBit)));
}

template <typename T, size_t N, HWY_IF_LANE_SIZE(T, 8)>
HWY_INLINE Mask128<T, N> LoadMaskBits(Simd<T, N, 0> d, uint64_t bits) {
  const RebindToUnsigned<decltype(d)> du;
  alignas(16) constexpr uint64_t kBit[8] = {1, 2};
  return RebindMask(d, TestBit(Set(du, bits), Load(du, kBit)));
}

}  // namespace detail

// `p` points to at least 8 readable bytes, not all of which need be valid.
template <typename T, size_t N, HWY_IF_LE128(T, N)>
HWY_API Mask128<T, N> LoadMaskBits(Simd<T, N, 0> d,
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
  alignas(16) uint64_t lanes[2];
  wasm_v128_store(lanes, mask.raw);

  constexpr uint64_t kMagic = 0x103070F1F3F80ULL;
  const uint64_t lo = ((lanes[0] * kMagic) >> 56);
  const uint64_t hi = ((lanes[1] * kMagic) >> 48) & 0xFF00;
  return (hi + lo);
}

// 64-bit
template <typename T>
HWY_INLINE uint64_t BitsFromMask(hwy::SizeTag<1> /*tag*/,
                                 const Mask128<T, 8> mask) {
  constexpr uint64_t kMagic = 0x103070F1F3F80ULL;
  return (static_cast<uint64_t>(wasm_i64x2_extract_lane(mask.raw, 0)) *
          kMagic) >>
         56;
}

// 32-bit or less: need masking
template <typename T, size_t N, HWY_IF_LE32(T, N)>
HWY_INLINE uint64_t BitsFromMask(hwy::SizeTag<1> /*tag*/,
                                 const Mask128<T, N> mask) {
  uint64_t bytes = static_cast<uint64_t>(wasm_i64x2_extract_lane(mask.raw, 0));
  // Clear potentially undefined bytes.
  bytes &= (1ULL << (N * 8)) - 1;
  constexpr uint64_t kMagic = 0x103070F1F3F80ULL;
  return (bytes * kMagic) >> 56;
}

template <typename T, size_t N>
HWY_INLINE uint64_t BitsFromMask(hwy::SizeTag<2> /*tag*/,
                                 const Mask128<T, N> mask) {
  // Remove useless lower half of each u16 while preserving the sign bit.
  const __i16x8 zero = wasm_i16x8_splat(0);
  const Mask128<uint8_t, N> mask8{wasm_i8x16_narrow_i16x8(mask.raw, zero)};
  return BitsFromMask(hwy::SizeTag<1>(), mask8);
}

template <typename T, size_t N>
HWY_INLINE uint64_t BitsFromMask(hwy::SizeTag<4> /*tag*/,
                                 const Mask128<T, N> mask) {
  const __i32x4 mask_i = static_cast<__i32x4>(mask.raw);
  const __i32x4 slice = wasm_i32x4_make(1, 2, 4, 8);
  const __i32x4 sliced_mask = wasm_v128_and(mask_i, slice);
  alignas(16) uint32_t lanes[4];
  wasm_v128_store(lanes, sliced_mask);
  return lanes[0] | lanes[1] | lanes[2] | lanes[3];
}

template <typename T, size_t N>
HWY_INLINE uint64_t BitsFromMask(hwy::SizeTag<8> /*tag*/,
                                 const Mask128<T, N> mask) {
  const __i64x2 mask_i = static_cast<__i64x2>(mask.raw);
  const __i64x2 slice = wasm_i64x2_make(1, 2);
  const __i64x2 sliced_mask = wasm_v128_and(mask_i, slice);
  alignas(16) uint64_t lanes[2];
  wasm_v128_store(lanes, sliced_mask);
  return lanes[0] | lanes[1];
}

// Returns the lowest N bits for the BitsFromMask result.
template <typename T, size_t N>
constexpr uint64_t OnlyActive(uint64_t bits) {
  return ((N * sizeof(T)) == 16) ? bits : bits & ((1ull << N) - 1);
}

// Returns 0xFF for bytes with index >= N, otherwise 0.
template <size_t N>
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

template <typename T, size_t N>
HWY_INLINE uint64_t BitsFromMask(const Mask128<T, N> mask) {
  return OnlyActive<T, N>(BitsFromMask(hwy::SizeTag<sizeof(T)>(), mask));
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
  alignas(16) uint64_t lanes[2];
  wasm_v128_store(lanes, shifted_bits);
  return PopCount(lanes[0] | lanes[1]);
}

template <typename T>
HWY_INLINE size_t CountTrue(hwy::SizeTag<8> /*tag*/, const Mask128<T> m) {
  alignas(16) int64_t lanes[2];
  wasm_v128_store(lanes, m.raw);
  return static_cast<size_t>(-(lanes[0] + lanes[1]));
}

}  // namespace detail

// `p` points to at least 8 writable bytes.
template <typename T, size_t N>
HWY_API size_t StoreMaskBits(const Simd<T, N, 0> /* tag */,
                             const Mask128<T, N> mask, uint8_t* bits) {
  const uint64_t mask_bits = detail::BitsFromMask(mask);
  const size_t kNumBytes = (N + 7) / 8;
  CopyBytes<kNumBytes>(&mask_bits, bits);
  return kNumBytes;
}

template <typename T, size_t N>
HWY_API size_t CountTrue(const Simd<T, N, 0> /* tag */, const Mask128<T> m) {
  return detail::CountTrue(hwy::SizeTag<sizeof(T)>(), m);
}

// Partial vector
template <typename T, size_t N, HWY_IF_LE64(T, N)>
HWY_API size_t CountTrue(const Simd<T, N, 0> d, const Mask128<T, N> m) {
  // Ensure all undefined bytes are 0.
  const Mask128<T, N> mask{detail::BytesAbove<N * sizeof(T)>()};
  return CountTrue(d, Mask128<T>{AndNot(mask, m).raw});
}

// Full vector
template <typename T>
HWY_API bool AllFalse(const Full128<T> d, const Mask128<T> m) {
#if 0
  // Casting followed by wasm_i8x16_any_true results in wasm error:
  // i32.eqz[0] expected type i32, found i8x16.popcnt of type s128
  const auto v8 = BitCast(Full128<int8_t>(), VecFromMask(d, m));
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
template <typename T>
HWY_INLINE bool AllTrue(hwy::SizeTag<8> /*tag*/, const Mask128<T> m) {
  return wasm_i64x2_all_true(m.raw);
}

}  // namespace detail

template <typename T, size_t N>
HWY_API bool AllTrue(const Simd<T, N, 0> /* tag */, const Mask128<T> m) {
  return detail::AllTrue(hwy::SizeTag<sizeof(T)>(), m);
}

// Partial vectors

template <typename T, size_t N, HWY_IF_LE64(T, N)>
HWY_API bool AllFalse(Simd<T, N, 0> /* tag */, const Mask128<T, N> m) {
  // Ensure all undefined bytes are 0.
  const Mask128<T, N> mask{detail::BytesAbove<N * sizeof(T)>()};
  return AllFalse(Full128<T>(), Mask128<T>{AndNot(mask, m).raw});
}

template <typename T, size_t N, HWY_IF_LE64(T, N)>
HWY_API bool AllTrue(const Simd<T, N, 0> /* d */, const Mask128<T, N> m) {
  // Ensure all undefined bytes are FF.
  const Mask128<T, N> mask{detail::BytesAbove<N * sizeof(T)>()};
  return AllTrue(Full128<T>(), Mask128<T>{Or(mask, m).raw});
}

template <typename T, size_t N>
HWY_API intptr_t FindFirstTrue(const Simd<T, N, 0> /* tag */,
                               const Mask128<T, N> mask) {
  const uint64_t bits = detail::BitsFromMask(mask);
  return bits ? static_cast<intptr_t>(Num0BitsBelowLS1Bit_Nonzero64(bits)) : -1;
}

// ------------------------------ Compress

namespace detail {

template <typename T, size_t N, HWY_IF_LANE_SIZE(T, 2)>
HWY_INLINE Vec128<T, N> IdxFromBits(const uint64_t mask_bits) {
  HWY_DASSERT(mask_bits < 256);
  const Simd<T, N, 0> d;
  const Rebind<uint8_t, decltype(d)> d8;
  const Simd<uint16_t, N, 0> du;

  // We need byte indices for TableLookupBytes (one vector's worth for each of
  // 256 combinations of 8 mask bits). Loading them directly requires 4 KiB. We
  // can instead store lane indices and convert to byte indices (2*lane + 0..1),
  // with the doubling baked into the table. Unpacking nibbles is likely more
  // costly than the higher cache footprint from storing bytes.
  alignas(16) constexpr uint8_t table[256 * 8] = {
      // PrintCompress16x8Tables
      0,  2,  4,  6,  8,  10, 12, 14, /**/ 0, 2,  4,  6,  8,  10, 12, 14,  //
      2,  0,  4,  6,  8,  10, 12, 14, /**/ 0, 2,  4,  6,  8,  10, 12, 14,  //
      4,  0,  2,  6,  8,  10, 12, 14, /**/ 0, 4,  2,  6,  8,  10, 12, 14,  //
      2,  4,  0,  6,  8,  10, 12, 14, /**/ 0, 2,  4,  6,  8,  10, 12, 14,  //
      6,  0,  2,  4,  8,  10, 12, 14, /**/ 0, 6,  2,  4,  8,  10, 12, 14,  //
      2,  6,  0,  4,  8,  10, 12, 14, /**/ 0, 2,  6,  4,  8,  10, 12, 14,  //
      4,  6,  0,  2,  8,  10, 12, 14, /**/ 0, 4,  6,  2,  8,  10, 12, 14,  //
      2,  4,  6,  0,  8,  10, 12, 14, /**/ 0, 2,  4,  6,  8,  10, 12, 14,  //
      8,  0,  2,  4,  6,  10, 12, 14, /**/ 0, 8,  2,  4,  6,  10, 12, 14,  //
      2,  8,  0,  4,  6,  10, 12, 14, /**/ 0, 2,  8,  4,  6,  10, 12, 14,  //
      4,  8,  0,  2,  6,  10, 12, 14, /**/ 0, 4,  8,  2,  6,  10, 12, 14,  //
      2,  4,  8,  0,  6,  10, 12, 14, /**/ 0, 2,  4,  8,  6,  10, 12, 14,  //
      6,  8,  0,  2,  4,  10, 12, 14, /**/ 0, 6,  8,  2,  4,  10, 12, 14,  //
      2,  6,  8,  0,  4,  10, 12, 14, /**/ 0, 2,  6,  8,  4,  10, 12, 14,  //
      4,  6,  8,  0,  2,  10, 12, 14, /**/ 0, 4,  6,  8,  2,  10, 12, 14,  //
      2,  4,  6,  8,  0,  10, 12, 14, /**/ 0, 2,  4,  6,  8,  10, 12, 14,  //
      10, 0,  2,  4,  6,  8,  12, 14, /**/ 0, 10, 2,  4,  6,  8,  12, 14,  //
      2,  10, 0,  4,  6,  8,  12, 14, /**/ 0, 2,  10, 4,  6,  8,  12, 14,  //
      4,  10, 0,  2,  6,  8,  12, 14, /**/ 0, 4,  10, 2,  6,  8,  12, 14,  //
      2,  4,  10, 0,  6,  8,  12, 14, /**/ 0, 2,  4,  10, 6,  8,  12, 14,  //
      6,  10, 0,  2,  4,  8,  12, 14, /**/ 0, 6,  10, 2,  4,  8,  12, 14,  //
      2,  6,  10, 0,  4,  8,  12, 14, /**/ 0, 2,  6,  10, 4,  8,  12, 14,  //
      4,  6,  10, 0,  2,  8,  12, 14, /**/ 0, 4,  6,  10, 2,  8,  12, 14,  //
      2,  4,  6,  10, 0,  8,  12, 14, /**/ 0, 2,  4,  6,  10, 8,  12, 14,  //
      8,  10, 0,  2,  4,  6,  12, 14, /**/ 0, 8,  10, 2,  4,  6,  12, 14,  //
      2,  8,  10, 0,  4,  6,  12, 14, /**/ 0, 2,  8,  10, 4,  6,  12, 14,  //
      4,  8,  10, 0,  2,  6,  12, 14, /**/ 0, 4,  8,  10, 2,  6,  12, 14,  //
      2,  4,  8,  10, 0,  6,  12, 14, /**/ 0, 2,  4,  8,  10, 6,  12, 14,  //
      6,  8,  10, 0,  2,  4,  12, 14, /**/ 0, 6,  8,  10, 2,  4,  12, 14,  //
      2,  6,  8,  10, 0,  4,  12, 14, /**/ 0, 2,  6,  8,  10, 4,  12, 14,  //
      4,  6,  8,  10, 0,  2,  12, 14, /**/ 0, 4,  6,  8,  10, 2,  12, 14,  //
      2,  4,  6,  8,  10, 0,  12, 14, /**/ 0, 2,  4,  6,  8,  10, 12, 14,  //
      12, 0,  2,  4,  6,  8,  10, 14, /**/ 0, 12, 2,  4,  6,  8,  10, 14,  //
      2,  12, 0,  4,  6,  8,  10, 14, /**/ 0, 2,  12, 4,  6,  8,  10, 14,  //
      4,  12, 0,  2,  6,  8,  10, 14, /**/ 0, 4,  12, 2,  6,  8,  10, 14,  //
      2,  4,  12, 0,  6,  8,  10, 14, /**/ 0, 2,  4,  12, 6,  8,  10, 14,  //
      6,  12, 0,  2,  4,  8,  10, 14, /**/ 0, 6,  12, 2,  4,  8,  10, 14,  //
      2,  6,  12, 0,  4,  8,  10, 14, /**/ 0, 2,  6,  12, 4,  8,  10, 14,  //
      4,  6,  12, 0,  2,  8,  10, 14, /**/ 0, 4,  6,  12, 2,  8,  10, 14,  //
      2,  4,  6,  12, 0,  8,  10, 14, /**/ 0, 2,  4,  6,  12, 8,  10, 14,  //
      8,  12, 0,  2,  4,  6,  10, 14, /**/ 0, 8,  12, 2,  4,  6,  10, 14,  //
      2,  8,  12, 0,  4,  6,  10, 14, /**/ 0, 2,  8,  12, 4,  6,  10, 14,  //
      4,  8,  12, 0,  2,  6,  10, 14, /**/ 0, 4,  8,  12, 2,  6,  10, 14,  //
      2,  4,  8,  12, 0,  6,  10, 14, /**/ 0, 2,  4,  8,  12, 6,  10, 14,  //
      6,  8,  12, 0,  2,  4,  10, 14, /**/ 0, 6,  8,  12, 2,  4,  10, 14,  //
      2,  6,  8,  12, 0,  4,  10, 14, /**/ 0, 2,  6,  8,  12, 4,  10, 14,  //
      4,  6,  8,  12, 0,  2,  10, 14, /**/ 0, 4,  6,  8,  12, 2,  10, 14,  //
      2,  4,  6,  8,  12, 0,  10, 14, /**/ 0, 2,  4,  6,  8,  12, 10, 14,  //
      10, 12, 0,  2,  4,  6,  8,  14, /**/ 0, 10, 12, 2,  4,  6,  8,  14,  //
      2,  10, 12, 0,  4,  6,  8,  14, /**/ 0, 2,  10, 12, 4,  6,  8,  14,  //
      4,  10, 12, 0,  2,  6,  8,  14, /**/ 0, 4,  10, 12, 2,  6,  8,  14,  //
      2,  4,  10, 12, 0,  6,  8,  14, /**/ 0, 2,  4,  10, 12, 6,  8,  14,  //
      6,  10, 12, 0,  2,  4,  8,  14, /**/ 0, 6,  10, 12, 2,  4,  8,  14,  //
      2,  6,  10, 12, 0,  4,  8,  14, /**/ 0, 2,  6,  10, 12, 4,  8,  14,  //
      4,  6,  10, 12, 0,  2,  8,  14, /**/ 0, 4,  6,  10, 12, 2,  8,  14,  //
      2,  4,  6,  10, 12, 0,  8,  14, /**/ 0, 2,  4,  6,  10, 12, 8,  14,  //
      8,  10, 12, 0,  2,  4,  6,  14, /**/ 0, 8,  10, 12, 2,  4,  6,  14,  //
      2,  8,  10, 12, 0,  4,  6,  14, /**/ 0, 2,  8,  10, 12, 4,  6,  14,  //
      4,  8,  10, 12, 0,  2,  6,  14, /**/ 0, 4,  8,  10, 12, 2,  6,  14,  //
      2,  4,  8,  10, 12, 0,  6,  14, /**/ 0, 2,  4,  8,  10, 12, 6,  14,  //
      6,  8,  10, 12, 0,  2,  4,  14, /**/ 0, 6,  8,  10, 12, 2,  4,  14,  //
      2,  6,  8,  10, 12, 0,  4,  14, /**/ 0, 2,  6,  8,  10, 12, 4,  14,  //
      4,  6,  8,  10, 12, 0,  2,  14, /**/ 0, 4,  6,  8,  10, 12, 2,  14,  //
      2,  4,  6,  8,  10, 12, 0,  14, /**/ 0, 2,  4,  6,  8,  10, 12, 14,  //
      14, 0,  2,  4,  6,  8,  10, 12, /**/ 0, 14, 2,  4,  6,  8,  10, 12,  //
      2,  14, 0,  4,  6,  8,  10, 12, /**/ 0, 2,  14, 4,  6,  8,  10, 12,  //
      4,  14, 0,  2,  6,  8,  10, 12, /**/ 0, 4,  14, 2,  6,  8,  10, 12,  //
      2,  4,  14, 0,  6,  8,  10, 12, /**/ 0, 2,  4,  14, 6,  8,  10, 12,  //
      6,  14, 0,  2,  4,  8,  10, 12, /**/ 0, 6,  14, 2,  4,  8,  10, 12,  //
      2,  6,  14, 0,  4,  8,  10, 12, /**/ 0, 2,  6,  14, 4,  8,  10, 12,  //
      4,  6,  14, 0,  2,  8,  10, 12, /**/ 0, 4,  6,  14, 2,  8,  10, 12,  //
      2,  4,  6,  14, 0,  8,  10, 12, /**/ 0, 2,  4,  6,  14, 8,  10, 12,  //
      8,  14, 0,  2,  4,  6,  10, 12, /**/ 0, 8,  14, 2,  4,  6,  10, 12,  //
      2,  8,  14, 0,  4,  6,  10, 12, /**/ 0, 2,  8,  14, 4,  6,  10, 12,  //
      4,  8,  14, 0,  2,  6,  10, 12, /**/ 0, 4,  8,  14, 2,  6,  10, 12,  //
      2,  4,  8,  14, 0,  6,  10, 12, /**/ 0, 2,  4,  8,  14, 6,  10, 12,  //
      6,  8,  14, 0,  2,  4,  10, 12, /**/ 0, 6,  8,  14, 2,  4,  10, 12,  //
      2,  6,  8,  14, 0,  4,  10, 12, /**/ 0, 2,  6,  8,  14, 4,  10, 12,  //
      4,  6,  8,  14, 0,  2,  10, 12, /**/ 0, 4,  6,  8,  14, 2,  10, 12,  //
      2,  4,  6,  8,  14, 0,  10, 12, /**/ 0, 2,  4,  6,  8,  14, 10, 12,  //
      10, 14, 0,  2,  4,  6,  8,  12, /**/ 0, 10, 14, 2,  4,  6,  8,  12,  //
      2,  10, 14, 0,  4,  6,  8,  12, /**/ 0, 2,  10, 14, 4,  6,  8,  12,  //
      4,  10, 14, 0,  2,  6,  8,  12, /**/ 0, 4,  10, 14, 2,  6,  8,  12,  //
      2,  4,  10, 14, 0,  6,  8,  12, /**/ 0, 2,  4,  10, 14, 6,  8,  12,  //
      6,  10, 14, 0,  2,  4,  8,  12, /**/ 0, 6,  10, 14, 2,  4,  8,  12,  //
      2,  6,  10, 14, 0,  4,  8,  12, /**/ 0, 2,  6,  10, 14, 4,  8,  12,  //
      4,  6,  10, 14, 0,  2,  8,  12, /**/ 0, 4,  6,  10, 14, 2,  8,  12,  //
      2,  4,  6,  10, 14, 0,  8,  12, /**/ 0, 2,  4,  6,  10, 14, 8,  12,  //
      8,  10, 14, 0,  2,  4,  6,  12, /**/ 0, 8,  10, 14, 2,  4,  6,  12,  //
      2,  8,  10, 14, 0,  4,  6,  12, /**/ 0, 2,  8,  10, 14, 4,  6,  12,  //
      4,  8,  10, 14, 0,  2,  6,  12, /**/ 0, 4,  8,  10, 14, 2,  6,  12,  //
      2,  4,  8,  10, 14, 0,  6,  12, /**/ 0, 2,  4,  8,  10, 14, 6,  12,  //
      6,  8,  10, 14, 0,  2,  4,  12, /**/ 0, 6,  8,  10, 14, 2,  4,  12,  //
      2,  6,  8,  10, 14, 0,  4,  12, /**/ 0, 2,  6,  8,  10, 14, 4,  12,  //
      4,  6,  8,  10, 14, 0,  2,  12, /**/ 0, 4,  6,  8,  10, 14, 2,  12,  //
      2,  4,  6,  8,  10, 14, 0,  12, /**/ 0, 2,  4,  6,  8,  10, 14, 12,  //
      12, 14, 0,  2,  4,  6,  8,  10, /**/ 0, 12, 14, 2,  4,  6,  8,  10,  //
      2,  12, 14, 0,  4,  6,  8,  10, /**/ 0, 2,  12, 14, 4,  6,  8,  10,  //
      4,  12, 14, 0,  2,  6,  8,  10, /**/ 0, 4,  12, 14, 2,  6,  8,  10,  //
      2,  4,  12, 14, 0,  6,  8,  10, /**/ 0, 2,  4,  12, 14, 6,  8,  10,  //
      6,  12, 14, 0,  2,  4,  8,  10, /**/ 0, 6,  12, 14, 2,  4,  8,  10,  //
      2,  6,  12, 14, 0,  4,  8,  10, /**/ 0, 2,  6,  12, 14, 4,  8,  10,  //
      4,  6,  12, 14, 0,  2,  8,  10, /**/ 0, 4,  6,  12, 14, 2,  8,  10,  //
      2,  4,  6,  12, 14, 0,  8,  10, /**/ 0, 2,  4,  6,  12, 14, 8,  10,  //
      8,  12, 14, 0,  2,  4,  6,  10, /**/ 0, 8,  12, 14, 2,  4,  6,  10,  //
      2,  8,  12, 14, 0,  4,  6,  10, /**/ 0, 2,  8,  12, 14, 4,  6,  10,  //
      4,  8,  12, 14, 0,  2,  6,  10, /**/ 0, 4,  8,  12, 14, 2,  6,  10,  //
      2,  4,  8,  12, 14, 0,  6,  10, /**/ 0, 2,  4,  8,  12, 14, 6,  10,  //
      6,  8,  12, 14, 0,  2,  4,  10, /**/ 0, 6,  8,  12, 14, 2,  4,  10,  //
      2,  6,  8,  12, 14, 0,  4,  10, /**/ 0, 2,  6,  8,  12, 14, 4,  10,  //
      4,  6,  8,  12, 14, 0,  2,  10, /**/ 0, 4,  6,  8,  12, 14, 2,  10,  //
      2,  4,  6,  8,  12, 14, 0,  10, /**/ 0, 2,  4,  6,  8,  12, 14, 10,  //
      10, 12, 14, 0,  2,  4,  6,  8,  /**/ 0, 10, 12, 14, 2,  4,  6,  8,   //
      2,  10, 12, 14, 0,  4,  6,  8,  /**/ 0, 2,  10, 12, 14, 4,  6,  8,   //
      4,  10, 12, 14, 0,  2,  6,  8,  /**/ 0, 4,  10, 12, 14, 2,  6,  8,   //
      2,  4,  10, 12, 14, 0,  6,  8,  /**/ 0, 2,  4,  10, 12, 14, 6,  8,   //
      6,  10, 12, 14, 0,  2,  4,  8,  /**/ 0, 6,  10, 12, 14, 2,  4,  8,   //
      2,  6,  10, 12, 14, 0,  4,  8,  /**/ 0, 2,  6,  10, 12, 14, 4,  8,   //
      4,  6,  10, 12, 14, 0,  2,  8,  /**/ 0, 4,  6,  10, 12, 14, 2,  8,   //
      2,  4,  6,  10, 12, 14, 0,  8,  /**/ 0, 2,  4,  6,  10, 12, 14, 8,   //
      8,  10, 12, 14, 0,  2,  4,  6,  /**/ 0, 8,  10, 12, 14, 2,  4,  6,   //
      2,  8,  10, 12, 14, 0,  4,  6,  /**/ 0, 2,  8,  10, 12, 14, 4,  6,   //
      4,  8,  10, 12, 14, 0,  2,  6,  /**/ 0, 4,  8,  10, 12, 14, 2,  6,   //
      2,  4,  8,  10, 12, 14, 0,  6,  /**/ 0, 2,  4,  8,  10, 12, 14, 6,   //
      6,  8,  10, 12, 14, 0,  2,  4,  /**/ 0, 6,  8,  10, 12, 14, 2,  4,   //
      2,  6,  8,  10, 12, 14, 0,  4,  /**/ 0, 2,  6,  8,  10, 12, 14, 4,   //
      4,  6,  8,  10, 12, 14, 0,  2,  /**/ 0, 4,  6,  8,  10, 12, 14, 2,   //
      2,  4,  6,  8,  10, 12, 14, 0,  /**/ 0, 2,  4,  6,  8,  10, 12, 14};

  const Vec128<uint8_t, 2 * N> byte_idx{Load(d8, table + mask_bits * 8).raw};
  const Vec128<uint16_t, N> pairs = ZipLower(byte_idx, byte_idx);
  return BitCast(d, pairs + Set(du, 0x0100));
}

template <typename T, size_t N, HWY_IF_LANE_SIZE(T, 2)>
HWY_INLINE Vec128<T, N> IdxFromNotBits(const uint64_t mask_bits) {
  HWY_DASSERT(mask_bits < 256);
  const Simd<T, N, 0> d;
  const Rebind<uint8_t, decltype(d)> d8;
  const Simd<uint16_t, N, 0> du;

  // We need byte indices for TableLookupBytes (one vector's worth for each of
  // 256 combinations of 8 mask bits). Loading them directly requires 4 KiB. We
  // can instead store lane indices and convert to byte indices (2*lane + 0..1),
  // with the doubling baked into the table. Unpacking nibbles is likely more
  // costly than the higher cache footprint from storing bytes.
  alignas(16) constexpr uint8_t table[256 * 8] = {
      // PrintCompressNot16x8Tables
      0, 2,  4,  6,  8,  10, 12, 14, /**/ 2,  4,  6,  8,  10, 12, 14, 0,   //
      0, 4,  6,  8,  10, 12, 14, 2,  /**/ 4,  6,  8,  10, 12, 14, 0,  2,   //
      0, 2,  6,  8,  10, 12, 14, 4,  /**/ 2,  6,  8,  10, 12, 14, 0,  4,   //
      0, 6,  8,  10, 12, 14, 2,  4,  /**/ 6,  8,  10, 12, 14, 0,  2,  4,   //
      0, 2,  4,  8,  10, 12, 14, 6,  /**/ 2,  4,  8,  10, 12, 14, 0,  6,   //
      0, 4,  8,  10, 12, 14, 2,  6,  /**/ 4,  8,  10, 12, 14, 0,  2,  6,   //
      0, 2,  8,  10, 12, 14, 4,  6,  /**/ 2,  8,  10, 12, 14, 0,  4,  6,   //
      0, 8,  10, 12, 14, 2,  4,  6,  /**/ 8,  10, 12, 14, 0,  2,  4,  6,   //
      0, 2,  4,  6,  10, 12, 14, 8,  /**/ 2,  4,  6,  10, 12, 14, 0,  8,   //
      0, 4,  6,  10, 12, 14, 2,  8,  /**/ 4,  6,  10, 12, 14, 0,  2,  8,   //
      0, 2,  6,  10, 12, 14, 4,  8,  /**/ 2,  6,  10, 12, 14, 0,  4,  8,   //
      0, 6,  10, 12, 14, 2,  4,  8,  /**/ 6,  10, 12, 14, 0,  2,  4,  8,   //
      0, 2,  4,  10, 12, 14, 6,  8,  /**/ 2,  4,  10, 12, 14, 0,  6,  8,   //
      0, 4,  10, 12, 14, 2,  6,  8,  /**/ 4,  10, 12, 14, 0,  2,  6,  8,   //
      0, 2,  10, 12, 14, 4,  6,  8,  /**/ 2,  10, 12, 14, 0,  4,  6,  8,   //
      0, 10, 12, 14, 2,  4,  6,  8,  /**/ 10, 12, 14, 0,  2,  4,  6,  8,   //
      0, 2,  4,  6,  8,  12, 14, 10, /**/ 2,  4,  6,  8,  12, 14, 0,  10,  //
      0, 4,  6,  8,  12, 14, 2,  10, /**/ 4,  6,  8,  12, 14, 0,  2,  10,  //
      0, 2,  6,  8,  12, 14, 4,  10, /**/ 2,  6,  8,  12, 14, 0,  4,  10,  //
      0, 6,  8,  12, 14, 2,  4,  10, /**/ 6,  8,  12, 14, 0,  2,  4,  10,  //
      0, 2,  4,  8,  12, 14, 6,  10, /**/ 2,  4,  8,  12, 14, 0,  6,  10,  //
      0, 4,  8,  12, 14, 2,  6,  10, /**/ 4,  8,  12, 14, 0,  2,  6,  10,  //
      0, 2,  8,  12, 14, 4,  6,  10, /**/ 2,  8,  12, 14, 0,  4,  6,  10,  //
      0, 8,  12, 14, 2,  4,  6,  10, /**/ 8,  12, 14, 0,  2,  4,  6,  10,  //
      0, 2,  4,  6,  12, 14, 8,  10, /**/ 2,  4,  6,  12, 14, 0,  8,  10,  //
      0, 4,  6,  12, 14, 2,  8,  10, /**/ 4,  6,  12, 14, 0,  2,  8,  10,  //
      0, 2,  6,  12, 14, 4,  8,  10, /**/ 2,  6,  12, 14, 0,  4,  8,  10,  //
      0, 6,  12, 14, 2,  4,  8,  10, /**/ 6,  12, 14, 0,  2,  4,  8,  10,  //
      0, 2,  4,  12, 14, 6,  8,  10, /**/ 2,  4,  12, 14, 0,  6,  8,  10,  //
      0, 4,  12, 14, 2,  6,  8,  10, /**/ 4,  12, 14, 0,  2,  6,  8,  10,  //
      0, 2,  12, 14, 4,  6,  8,  10, /**/ 2,  12, 14, 0,  4,  6,  8,  10,  //
      0, 12, 14, 2,  4,  6,  8,  10, /**/ 12, 14, 0,  2,  4,  6,  8,  10,  //
      0, 2,  4,  6,  8,  10, 14, 12, /**/ 2,  4,  6,  8,  10, 14, 0,  12,  //
      0, 4,  6,  8,  10, 14, 2,  12, /**/ 4,  6,  8,  10, 14, 0,  2,  12,  //
      0, 2,  6,  8,  10, 14, 4,  12, /**/ 2,  6,  8,  10, 14, 0,  4,  12,  //
      0, 6,  8,  10, 14, 2,  4,  12, /**/ 6,  8,  10, 14, 0,  2,  4,  12,  //
      0, 2,  4,  8,  10, 14, 6,  12, /**/ 2,  4,  8,  10, 14, 0,  6,  12,  //
      0, 4,  8,  10, 14, 2,  6,  12, /**/ 4,  8,  10, 14, 0,  2,  6,  12,  //
      0, 2,  8,  10, 14, 4,  6,  12, /**/ 2,  8,  10, 14, 0,  4,  6,  12,  //
      0, 8,  10, 14, 2,  4,  6,  12, /**/ 8,  10, 14, 0,  2,  4,  6,  12,  //
      0, 2,  4,  6,  10, 14, 8,  12, /**/ 2,  4,  6,  10, 14, 0,  8,  12,  //
      0, 4,  6,  10, 14, 2,  8,  12, /**/ 4,  6,  10, 14, 0,  2,  8,  12,  //
      0, 2,  6,  10, 14, 4,  8,  12, /**/ 2,  6,  10, 14, 0,  4,  8,  12,  //
      0, 6,  10, 14, 2,  4,  8,  12, /**/ 6,  10, 14, 0,  2,  4,  8,  12,  //
      0, 2,  4,  10, 14, 6,  8,  12, /**/ 2,  4,  10, 14, 0,  6,  8,  12,  //
      0, 4,  10, 14, 2,  6,  8,  12, /**/ 4,  10, 14, 0,  2,  6,  8,  12,  //
      0, 2,  10, 14, 4,  6,  8,  12, /**/ 2,  10, 14, 0,  4,  6,  8,  12,  //
      0, 10, 14, 2,  4,  6,  8,  12, /**/ 10, 14, 0,  2,  4,  6,  8,  12,  //
      0, 2,  4,  6,  8,  14, 10, 12, /**/ 2,  4,  6,  8,  14, 0,  10, 12,  //
      0, 4,  6,  8,  14, 2,  10, 12, /**/ 4,  6,  8,  14, 0,  2,  10, 12,  //
      0, 2,  6,  8,  14, 4,  10, 12, /**/ 2,  6,  8,  14, 0,  4,  10, 12,  //
      0, 6,  8,  14, 2,  4,  10, 12, /**/ 6,  8,  14, 0,  2,  4,  10, 12,  //
      0, 2,  4,  8,  14, 6,  10, 12, /**/ 2,  4,  8,  14, 0,  6,  10, 12,  //
      0, 4,  8,  14, 2,  6,  10, 12, /**/ 4,  8,  14, 0,  2,  6,  10, 12,  //
      0, 2,  8,  14, 4,  6,  10, 12, /**/ 2,  8,  14, 0,  4,  6,  10, 12,  //
      0, 8,  14, 2,  4,  6,  10, 12, /**/ 8,  14, 0,  2,  4,  6,  10, 12,  //
      0, 2,  4,  6,  14, 8,  10, 12, /**/ 2,  4,  6,  14, 0,  8,  10, 12,  //
      0, 4,  6,  14, 2,  8,  10, 12, /**/ 4,  6,  14, 0,  2,  8,  10, 12,  //
      0, 2,  6,  14, 4,  8,  10, 12, /**/ 2,  6,  14, 0,  4,  8,  10, 12,  //
      0, 6,  14, 2,  4,  8,  10, 12, /**/ 6,  14, 0,  2,  4,  8,  10, 12,  //
      0, 2,  4,  14, 6,  8,  10, 12, /**/ 2,  4,  14, 0,  6,  8,  10, 12,  //
      0, 4,  14, 2,  6,  8,  10, 12, /**/ 4,  14, 0,  2,  6,  8,  10, 12,  //
      0, 2,  14, 4,  6,  8,  10, 12, /**/ 2,  14, 0,  4,  6,  8,  10, 12,  //
      0, 14, 2,  4,  6,  8,  10, 12, /**/ 14, 0,  2,  4,  6,  8,  10, 12,  //
      0, 2,  4,  6,  8,  10, 12, 14, /**/ 2,  4,  6,  8,  10, 12, 0,  14,  //
      0, 4,  6,  8,  10, 12, 2,  14, /**/ 4,  6,  8,  10, 12, 0,  2,  14,  //
      0, 2,  6,  8,  10, 12, 4,  14, /**/ 2,  6,  8,  10, 12, 0,  4,  14,  //
      0, 6,  8,  10, 12, 2,  4,  14, /**/ 6,  8,  10, 12, 0,  2,  4,  14,  //
      0, 2,  4,  8,  10, 12, 6,  14, /**/ 2,  4,  8,  10, 12, 0,  6,  14,  //
      0, 4,  8,  10, 12, 2,  6,  14, /**/ 4,  8,  10, 12, 0,  2,  6,  14,  //
      0, 2,  8,  10, 12, 4,  6,  14, /**/ 2,  8,  10, 12, 0,  4,  6,  14,  //
      0, 8,  10, 12, 2,  4,  6,  14, /**/ 8,  10, 12, 0,  2,  4,  6,  14,  //
      0, 2,  4,  6,  10, 12, 8,  14, /**/ 2,  4,  6,  10, 12, 0,  8,  14,  //
      0, 4,  6,  10, 12, 2,  8,  14, /**/ 4,  6,  10, 12, 0,  2,  8,  14,  //
      0, 2,  6,  10, 12, 4,  8,  14, /**/ 2,  6,  10, 12, 0,  4,  8,  14,  //
      0, 6,  10, 12, 2,  4,  8,  14, /**/ 6,  10, 12, 0,  2,  4,  8,  14,  //
      0, 2,  4,  10, 12, 6,  8,  14, /**/ 2,  4,  10, 12, 0,  6,  8,  14,  //
      0, 4,  10, 12, 2,  6,  8,  14, /**/ 4,  10, 12, 0,  2,  6,  8,  14,  //
      0, 2,  10, 12, 4,  6,  8,  14, /**/ 2,  10, 12, 0,  4,  6,  8,  14,  //
      0, 10, 12, 2,  4,  6,  8,  14, /**/ 10, 12, 0,  2,  4,  6,  8,  14,  //
      0, 2,  4,  6,  8,  12, 10, 14, /**/ 2,  4,  6,  8,  12, 0,  10, 14,  //
      0, 4,  6,  8,  12, 2,  10, 14, /**/ 4,  6,  8,  12, 0,  2,  10, 14,  //
      0, 2,  6,  8,  12, 4,  10, 14, /**/ 2,  6,  8,  12, 0,  4,  10, 14,  //
      0, 6,  8,  12, 2,  4,  10, 14, /**/ 6,  8,  12, 0,  2,  4,  10, 14,  //
      0, 2,  4,  8,  12, 6,  10, 14, /**/ 2,  4,  8,  12, 0,  6,  10, 14,  //
      0, 4,  8,  12, 2,  6,  10, 14, /**/ 4,  8,  12, 0,  2,  6,  10, 14,  //
      0, 2,  8,  12, 4,  6,  10, 14, /**/ 2,  8,  12, 0,  4,  6,  10, 14,  //
      0, 8,  12, 2,  4,  6,  10, 14, /**/ 8,  12, 0,  2,  4,  6,  10, 14,  //
      0, 2,  4,  6,  12, 8,  10, 14, /**/ 2,  4,  6,  12, 0,  8,  10, 14,  //
      0, 4,  6,  12, 2,  8,  10, 14, /**/ 4,  6,  12, 0,  2,  8,  10, 14,  //
      0, 2,  6,  12, 4,  8,  10, 14, /**/ 2,  6,  12, 0,  4,  8,  10, 14,  //
      0, 6,  12, 2,  4,  8,  10, 14, /**/ 6,  12, 0,  2,  4,  8,  10, 14,  //
      0, 2,  4,  12, 6,  8,  10, 14, /**/ 2,  4,  12, 0,  6,  8,  10, 14,  //
      0, 4,  12, 2,  6,  8,  10, 14, /**/ 4,  12, 0,  2,  6,  8,  10, 14,  //
      0, 2,  12, 4,  6,  8,  10, 14, /**/ 2,  12, 0,  4,  6,  8,  10, 14,  //
      0, 12, 2,  4,  6,  8,  10, 14, /**/ 12, 0,  2,  4,  6,  8,  10, 14,  //
      0, 2,  4,  6,  8,  10, 12, 14, /**/ 2,  4,  6,  8,  10, 0,  12, 14,  //
      0, 4,  6,  8,  10, 2,  12, 14, /**/ 4,  6,  8,  10, 0,  2,  12, 14,  //
      0, 2,  6,  8,  10, 4,  12, 14, /**/ 2,  6,  8,  10, 0,  4,  12, 14,  //
      0, 6,  8,  10, 2,  4,  12, 14, /**/ 6,  8,  10, 0,  2,  4,  12, 14,  //
      0, 2,  4,  8,  10, 6,  12, 14, /**/ 2,  4,  8,  10, 0,  6,  12, 14,  //
      0, 4,  8,  10, 2,  6,  12, 14, /**/ 4,  8,  10, 0,  2,  6,  12, 14,  //
      0, 2,  8,  10, 4,  6,  12, 14, /**/ 2,  8,  10, 0,  4,  6,  12, 14,  //
      0, 8,  10, 2,  4,  6,  12, 14, /**/ 8,  10, 0,  2,  4,  6,  12, 14,  //
      0, 2,  4,  6,  10, 8,  12, 14, /**/ 2,  4,  6,  10, 0,  8,  12, 14,  //
      0, 4,  6,  10, 2,  8,  12, 14, /**/ 4,  6,  10, 0,  2,  8,  12, 14,  //
      0, 2,  6,  10, 4,  8,  12, 14, /**/ 2,  6,  10, 0,  4,  8,  12, 14,  //
      0, 6,  10, 2,  4,  8,  12, 14, /**/ 6,  10, 0,  2,  4,  8,  12, 14,  //
      0, 2,  4,  10, 6,  8,  12, 14, /**/ 2,  4,  10, 0,  6,  8,  12, 14,  //
      0, 4,  10, 2,  6,  8,  12, 14, /**/ 4,  10, 0,  2,  6,  8,  12, 14,  //
      0, 2,  10, 4,  6,  8,  12, 14, /**/ 2,  10, 0,  4,  6,  8,  12, 14,  //
      0, 10, 2,  4,  6,  8,  12, 14, /**/ 10, 0,  2,  4,  6,  8,  12, 14,  //
      0, 2,  4,  6,  8,  10, 12, 14, /**/ 2,  4,  6,  8,  0,  10, 12, 14,  //
      0, 4,  6,  8,  2,  10, 12, 14, /**/ 4,  6,  8,  0,  2,  10, 12, 14,  //
      0, 2,  6,  8,  4,  10, 12, 14, /**/ 2,  6,  8,  0,  4,  10, 12, 14,  //
      0, 6,  8,  2,  4,  10, 12, 14, /**/ 6,  8,  0,  2,  4,  10, 12, 14,  //
      0, 2,  4,  8,  6,  10, 12, 14, /**/ 2,  4,  8,  0,  6,  10, 12, 14,  //
      0, 4,  8,  2,  6,  10, 12, 14, /**/ 4,  8,  0,  2,  6,  10, 12, 14,  //
      0, 2,  8,  4,  6,  10, 12, 14, /**/ 2,  8,  0,  4,  6,  10, 12, 14,  //
      0, 8,  2,  4,  6,  10, 12, 14, /**/ 8,  0,  2,  4,  6,  10, 12, 14,  //
      0, 2,  4,  6,  8,  10, 12, 14, /**/ 2,  4,  6,  0,  8,  10, 12, 14,  //
      0, 4,  6,  2,  8,  10, 12, 14, /**/ 4,  6,  0,  2,  8,  10, 12, 14,  //
      0, 2,  6,  4,  8,  10, 12, 14, /**/ 2,  6,  0,  4,  8,  10, 12, 14,  //
      0, 6,  2,  4,  8,  10, 12, 14, /**/ 6,  0,  2,  4,  8,  10, 12, 14,  //
      0, 2,  4,  6,  8,  10, 12, 14, /**/ 2,  4,  0,  6,  8,  10, 12, 14,  //
      0, 4,  2,  6,  8,  10, 12, 14, /**/ 4,  0,  2,  6,  8,  10, 12, 14,  //
      0, 2,  4,  6,  8,  10, 12, 14, /**/ 2,  0,  4,  6,  8,  10, 12, 14,  //
      0, 2,  4,  6,  8,  10, 12, 14, /**/ 0,  2,  4,  6,  8,  10, 12, 14};

  const Vec128<uint8_t, 2 * N> byte_idx{Load(d8, table + mask_bits * 8).raw};
  const Vec128<uint16_t, N> pairs = ZipLower(byte_idx, byte_idx);
  return BitCast(d, pairs + Set(du, 0x0100));
}

template <typename T, size_t N, HWY_IF_LANE_SIZE(T, 4)>
HWY_INLINE Vec128<T, N> IdxFromBits(const uint64_t mask_bits) {
  HWY_DASSERT(mask_bits < 16);

  // There are only 4 lanes, so we can afford to load the index vector directly.
  alignas(16) constexpr uint8_t u8_indices[16 * 16] = {
      // PrintCompress32x4Tables
      0,  1,  2,  3,  4,  5,  6,  7,  8,  9,  10, 11, 12, 13, 14, 15,  //
      0,  1,  2,  3,  4,  5,  6,  7,  8,  9,  10, 11, 12, 13, 14, 15,  //
      4,  5,  6,  7,  0,  1,  2,  3,  8,  9,  10, 11, 12, 13, 14, 15,  //
      0,  1,  2,  3,  4,  5,  6,  7,  8,  9,  10, 11, 12, 13, 14, 15,  //
      8,  9,  10, 11, 0,  1,  2,  3,  4,  5,  6,  7,  12, 13, 14, 15,  //
      0,  1,  2,  3,  8,  9,  10, 11, 4,  5,  6,  7,  12, 13, 14, 15,  //
      4,  5,  6,  7,  8,  9,  10, 11, 0,  1,  2,  3,  12, 13, 14, 15,  //
      0,  1,  2,  3,  4,  5,  6,  7,  8,  9,  10, 11, 12, 13, 14, 15,  //
      12, 13, 14, 15, 0,  1,  2,  3,  4,  5,  6,  7,  8,  9,  10, 11,  //
      0,  1,  2,  3,  12, 13, 14, 15, 4,  5,  6,  7,  8,  9,  10, 11,  //
      4,  5,  6,  7,  12, 13, 14, 15, 0,  1,  2,  3,  8,  9,  10, 11,  //
      0,  1,  2,  3,  4,  5,  6,  7,  12, 13, 14, 15, 8,  9,  10, 11,  //
      8,  9,  10, 11, 12, 13, 14, 15, 0,  1,  2,  3,  4,  5,  6,  7,   //
      0,  1,  2,  3,  8,  9,  10, 11, 12, 13, 14, 15, 4,  5,  6,  7,   //
      4,  5,  6,  7,  8,  9,  10, 11, 12, 13, 14, 15, 0,  1,  2,  3,   //
      0,  1,  2,  3,  4,  5,  6,  7,  8,  9,  10, 11, 12, 13, 14, 15};
  const Simd<T, N, 0> d;
  const Repartition<uint8_t, decltype(d)> d8;
  return BitCast(d, Load(d8, u8_indices + 16 * mask_bits));
}

template <typename T, size_t N, HWY_IF_LANE_SIZE(T, 4)>
HWY_INLINE Vec128<T, N> IdxFromNotBits(const uint64_t mask_bits) {
  HWY_DASSERT(mask_bits < 16);

  // There are only 4 lanes, so we can afford to load the index vector directly.
  alignas(16) constexpr uint8_t u8_indices[16 * 16] = {
      // PrintCompressNot32x4Tables
      0,  1,  2,  3,  4,  5,  6,  7,  8,  9,  10, 11, 12, 13, 14, 15, 4,  5,
      6,  7,  8,  9,  10, 11, 12, 13, 14, 15, 0,  1,  2,  3,  0,  1,  2,  3,
      8,  9,  10, 11, 12, 13, 14, 15, 4,  5,  6,  7,  8,  9,  10, 11, 12, 13,
      14, 15, 0,  1,  2,  3,  4,  5,  6,  7,  0,  1,  2,  3,  4,  5,  6,  7,
      12, 13, 14, 15, 8,  9,  10, 11, 4,  5,  6,  7,  12, 13, 14, 15, 0,  1,
      2,  3,  8,  9,  10, 11, 0,  1,  2,  3,  12, 13, 14, 15, 4,  5,  6,  7,
      8,  9,  10, 11, 12, 13, 14, 15, 0,  1,  2,  3,  4,  5,  6,  7,  8,  9,
      10, 11, 0,  1,  2,  3,  4,  5,  6,  7,  8,  9,  10, 11, 12, 13, 14, 15,
      4,  5,  6,  7,  8,  9,  10, 11, 0,  1,  2,  3,  12, 13, 14, 15, 0,  1,
      2,  3,  8,  9,  10, 11, 4,  5,  6,  7,  12, 13, 14, 15, 8,  9,  10, 11,
      0,  1,  2,  3,  4,  5,  6,  7,  12, 13, 14, 15, 0,  1,  2,  3,  4,  5,
      6,  7,  8,  9,  10, 11, 12, 13, 14, 15, 4,  5,  6,  7,  0,  1,  2,  3,
      8,  9,  10, 11, 12, 13, 14, 15, 0,  1,  2,  3,  4,  5,  6,  7,  8,  9,
      10, 11, 12, 13, 14, 15, 0,  1,  2,  3,  4,  5,  6,  7,  8,  9,  10, 11,
      12, 13, 14, 15};
  const Simd<T, N, 0> d;
  const Repartition<uint8_t, decltype(d)> d8;
  return BitCast(d, Load(d8, u8_indices + 16 * mask_bits));
}

template <typename T, size_t N, HWY_IF_LANE_SIZE(T, 8)>
HWY_INLINE Vec128<T, N> IdxFromBits(const uint64_t mask_bits) {
  HWY_DASSERT(mask_bits < 4);

  // There are only 2 lanes, so we can afford to load the index vector directly.
  alignas(16) constexpr uint8_t u8_indices[4 * 16] = {
      // PrintCompress64x2Tables
      0, 1, 2,  3,  4,  5,  6,  7,  8, 9, 10, 11, 12, 13, 14, 15,
      0, 1, 2,  3,  4,  5,  6,  7,  8, 9, 10, 11, 12, 13, 14, 15,
      8, 9, 10, 11, 12, 13, 14, 15, 0, 1, 2,  3,  4,  5,  6,  7,
      0, 1, 2,  3,  4,  5,  6,  7,  8, 9, 10, 11, 12, 13, 14, 15};

  const Simd<T, N, 0> d;
  const Repartition<uint8_t, decltype(d)> d8;
  return BitCast(d, Load(d8, u8_indices + 16 * mask_bits));
}

template <typename T, size_t N, HWY_IF_LANE_SIZE(T, 8)>
HWY_INLINE Vec128<T, N> IdxFromNotBits(const uint64_t mask_bits) {
  HWY_DASSERT(mask_bits < 4);

  // There are only 2 lanes, so we can afford to load the index vector directly.
  alignas(16) constexpr uint8_t u8_indices[4 * 16] = {
      // PrintCompressNot64x2Tables
      0, 1, 2,  3,  4,  5,  6,  7,  8, 9, 10, 11, 12, 13, 14, 15,
      8, 9, 10, 11, 12, 13, 14, 15, 0, 1, 2,  3,  4,  5,  6,  7,
      0, 1, 2,  3,  4,  5,  6,  7,  8, 9, 10, 11, 12, 13, 14, 15,
      0, 1, 2,  3,  4,  5,  6,  7,  8, 9, 10, 11, 12, 13, 14, 15};

  const Simd<T, N, 0> d;
  const Repartition<uint8_t, decltype(d)> d8;
  return BitCast(d, Load(d8, u8_indices + 16 * mask_bits));
}

// Helper functions called by both Compress and CompressStore - avoids a
// redundant BitsFromMask in the latter.

template <typename T, size_t N>
HWY_INLINE Vec128<T, N> Compress(Vec128<T, N> v, const uint64_t mask_bits) {
  const auto idx = detail::IdxFromBits<T, N>(mask_bits);
  const DFromV<decltype(v)> d;
  const RebindToSigned<decltype(d)> di;
  return BitCast(d, TableLookupBytes(BitCast(di, v), BitCast(di, idx)));
}

template <typename T, size_t N>
HWY_INLINE Vec128<T, N> CompressNot(Vec128<T, N> v, const uint64_t mask_bits) {
  const auto idx = detail::IdxFromNotBits<T, N>(mask_bits);
  const DFromV<decltype(v)> d;
  const RebindToSigned<decltype(d)> di;
  return BitCast(d, TableLookupBytes(BitCast(di, v), BitCast(di, idx)));
}

}  // namespace detail

template <typename T>
struct CompressIsPartition {
  enum { value = 1 };
};

// Single lane: no-op
template <typename T>
HWY_API Vec128<T, 1> Compress(Vec128<T, 1> v, Mask128<T, 1> /*m*/) {
  return v;
}

// Two lanes: conditional swap
template <typename T, HWY_IF_LANE_SIZE(T, 8)>
HWY_API Vec128<T> Compress(Vec128<T> v, Mask128<T> mask) {
  // If mask[1] = 1 and mask[0] = 0, then swap both halves, else keep.
  const Full128<T> d;
  const Vec128<T> m = VecFromMask(d, mask);
  const Vec128<T> maskL = DupEven(m);
  const Vec128<T> maskH = DupOdd(m);
  const Vec128<T> swap = AndNot(maskL, maskH);
  return IfVecThenElse(swap, Shuffle01(v), v);
}

// General case
template <typename T, size_t N, HWY_IF_NOT_LANE_SIZE(T, 8)>
HWY_API Vec128<T, N> Compress(Vec128<T, N> v, Mask128<T, N> mask) {
  return detail::Compress(v, detail::BitsFromMask(mask));
}

// Single lane: no-op
template <typename T>
HWY_API Vec128<T, 1> CompressNot(Vec128<T, 1> v, Mask128<T, 1> /*m*/) {
  return v;
}

// Two lanes: conditional swap
template <typename T, HWY_IF_LANE_SIZE(T, 8)>
HWY_API Vec128<T> CompressNot(Vec128<T> v, Mask128<T> mask) {
  // If mask[1] = 0 and mask[0] = 1, then swap both halves, else keep.
  const Full128<T> d;
  const Vec128<T> m = VecFromMask(d, mask);
  const Vec128<T> maskL = DupEven(m);
  const Vec128<T> maskH = DupOdd(m);
  const Vec128<T> swap = AndNot(maskH, maskL);
  return IfVecThenElse(swap, Shuffle01(v), v);
}

// General case
template <typename T, size_t N, HWY_IF_NOT_LANE_SIZE(T, 8)>
HWY_API Vec128<T, N> CompressNot(Vec128<T, N> v, Mask128<T, N> mask) {
  // For partial vectors, we cannot pull the Not() into the table because
  // BitsFromMask clears the upper bits.
  if (N < 16 / sizeof(T)) {
    return detail::Compress(v, detail::BitsFromMask(Not(mask)));
  }
  return detail::CompressNot(v, detail::BitsFromMask(mask));
}
// ------------------------------ CompressBlocksNot
HWY_API Vec128<uint64_t> CompressBlocksNot(Vec128<uint64_t> v,
                                           Mask128<uint64_t> /* m */) {
  return v;
}

// ------------------------------ CompressBits

template <typename T, size_t N>
HWY_API Vec128<T, N> CompressBits(Vec128<T, N> v,
                                  const uint8_t* HWY_RESTRICT bits) {
  uint64_t mask_bits = 0;
  constexpr size_t kNumBytes = (N + 7) / 8;
  CopyBytes<kNumBytes>(bits, &mask_bits);
  if (N < 8) {
    mask_bits &= (1ull << N) - 1;
  }

  return detail::Compress(v, mask_bits);
}

// ------------------------------ CompressStore
template <typename T, size_t N>
HWY_API size_t CompressStore(Vec128<T, N> v, const Mask128<T, N> mask,
                             Simd<T, N, 0> d, T* HWY_RESTRICT unaligned) {
  const uint64_t mask_bits = detail::BitsFromMask(mask);
  const auto c = detail::Compress(v, mask_bits);
  StoreU(c, d, unaligned);
  return PopCount(mask_bits);
}

// ------------------------------ CompressBlendedStore
template <typename T, size_t N>
HWY_API size_t CompressBlendedStore(Vec128<T, N> v, Mask128<T, N> m,
                                    Simd<T, N, 0> d,
                                    T* HWY_RESTRICT unaligned) {
  const RebindToUnsigned<decltype(d)> du;  // so we can support fp16/bf16
  using TU = TFromD<decltype(du)>;
  const uint64_t mask_bits = detail::BitsFromMask(m);
  const size_t count = PopCount(mask_bits);
  const Vec128<TU, N> compressed = detail::Compress(BitCast(du, v), mask_bits);
  const Mask128<T, N> store_mask = RebindMask(d, FirstN(du, count));
  BlendedStore(BitCast(d, compressed), store_mask, d, unaligned);
  return count;
}

// ------------------------------ CompressBitsStore

template <typename T, size_t N>
HWY_API size_t CompressBitsStore(Vec128<T, N> v,
                                 const uint8_t* HWY_RESTRICT bits,
                                 Simd<T, N, 0> d, T* HWY_RESTRICT unaligned) {
  uint64_t mask_bits = 0;
  constexpr size_t kNumBytes = (N + 7) / 8;
  CopyBytes<kNumBytes>(bits, &mask_bits);
  if (N < 8) {
    mask_bits &= (1ull << N) - 1;
  }

  const auto c = detail::Compress(v, mask_bits);
  StoreU(c, d, unaligned);
  return PopCount(mask_bits);
}

// ------------------------------ StoreInterleaved2/3/4

// HWY_NATIVE_LOAD_STORE_INTERLEAVED not set, hence defined in
// generic_ops-inl.h.

// ------------------------------ MulEven/Odd (Load)

HWY_INLINE Vec128<uint64_t> MulEven(const Vec128<uint64_t> a,
                                    const Vec128<uint64_t> b) {
  alignas(16) uint64_t mul[2];
  mul[0] =
      Mul128(static_cast<uint64_t>(wasm_i64x2_extract_lane(a.raw, 0)),
             static_cast<uint64_t>(wasm_i64x2_extract_lane(b.raw, 0)), &mul[1]);
  return Load(Full128<uint64_t>(), mul);
}

HWY_INLINE Vec128<uint64_t> MulOdd(const Vec128<uint64_t> a,
                                   const Vec128<uint64_t> b) {
  alignas(16) uint64_t mul[2];
  mul[0] =
      Mul128(static_cast<uint64_t>(wasm_i64x2_extract_lane(a.raw, 1)),
             static_cast<uint64_t>(wasm_i64x2_extract_lane(b.raw, 1)), &mul[1]);
  return Load(Full128<uint64_t>(), mul);
}

// ------------------------------ ReorderWidenMulAccumulate (MulAdd, ZipLower)

template <size_t N>
HWY_API Vec128<float, N> ReorderWidenMulAccumulate(Simd<float, N, 0> df32,
                                                   Vec128<bfloat16_t, 2 * N> a,
                                                   Vec128<bfloat16_t, 2 * N> b,
                                                   const Vec128<float, N> sum0,
                                                   Vec128<float, N>& sum1) {
  const Repartition<uint16_t, decltype(df32)> du16;
  const RebindToUnsigned<decltype(df32)> du32;
  const Vec128<uint16_t, 2 * N> zero = Zero(du16);
  const Vec128<uint32_t, N> a0 = ZipLower(du32, zero, BitCast(du16, a));
  const Vec128<uint32_t, N> a1 = ZipUpper(du32, zero, BitCast(du16, a));
  const Vec128<uint32_t, N> b0 = ZipLower(du32, zero, BitCast(du16, b));
  const Vec128<uint32_t, N> b1 = ZipUpper(du32, zero, BitCast(du16, b));
  sum1 = MulAdd(BitCast(df32, a1), BitCast(df32, b1), sum1);
  return MulAdd(BitCast(df32, a0), BitCast(df32, b0), sum0);
}

// ------------------------------ Reductions

namespace detail {

// N=1 for any T: no-op
template <typename T>
HWY_INLINE Vec128<T, 1> SumOfLanes(hwy::SizeTag<sizeof(T)> /* tag */,
                                   const Vec128<T, 1> v) {
  return v;
}
template <typename T>
HWY_INLINE Vec128<T, 1> MinOfLanes(hwy::SizeTag<sizeof(T)> /* tag */,
                                   const Vec128<T, 1> v) {
  return v;
}
template <typename T>
HWY_INLINE Vec128<T, 1> MaxOfLanes(hwy::SizeTag<sizeof(T)> /* tag */,
                                   const Vec128<T, 1> v) {
  return v;
}

// u32/i32/f32:

// N=2
template <typename T>
HWY_INLINE Vec128<T, 2> SumOfLanes(hwy::SizeTag<4> /* tag */,
                                   const Vec128<T, 2> v10) {
  return v10 + Vec128<T, 2>{Shuffle2301(Vec128<T>{v10.raw}).raw};
}
template <typename T>
HWY_INLINE Vec128<T, 2> MinOfLanes(hwy::SizeTag<4> /* tag */,
                                   const Vec128<T, 2> v10) {
  return Min(v10, Vec128<T, 2>{Shuffle2301(Vec128<T>{v10.raw}).raw});
}
template <typename T>
HWY_INLINE Vec128<T, 2> MaxOfLanes(hwy::SizeTag<4> /* tag */,
                                   const Vec128<T, 2> v10) {
  return Max(v10, Vec128<T, 2>{Shuffle2301(Vec128<T>{v10.raw}).raw});
}

// N=4 (full)
template <typename T>
HWY_INLINE Vec128<T> SumOfLanes(hwy::SizeTag<4> /* tag */,
                                const Vec128<T> v3210) {
  const Vec128<T> v1032 = Shuffle1032(v3210);
  const Vec128<T> v31_20_31_20 = v3210 + v1032;
  const Vec128<T> v20_31_20_31 = Shuffle0321(v31_20_31_20);
  return v20_31_20_31 + v31_20_31_20;
}
template <typename T>
HWY_INLINE Vec128<T> MinOfLanes(hwy::SizeTag<4> /* tag */,
                                const Vec128<T> v3210) {
  const Vec128<T> v1032 = Shuffle1032(v3210);
  const Vec128<T> v31_20_31_20 = Min(v3210, v1032);
  const Vec128<T> v20_31_20_31 = Shuffle0321(v31_20_31_20);
  return Min(v20_31_20_31, v31_20_31_20);
}
template <typename T>
HWY_INLINE Vec128<T> MaxOfLanes(hwy::SizeTag<4> /* tag */,
                                const Vec128<T> v3210) {
  const Vec128<T> v1032 = Shuffle1032(v3210);
  const Vec128<T> v31_20_31_20 = Max(v3210, v1032);
  const Vec128<T> v20_31_20_31 = Shuffle0321(v31_20_31_20);
  return Max(v20_31_20_31, v31_20_31_20);
}

// u64/i64/f64:

// N=2 (full)
template <typename T>
HWY_INLINE Vec128<T> SumOfLanes(hwy::SizeTag<8> /* tag */,
                                const Vec128<T> v10) {
  const Vec128<T> v01 = Shuffle01(v10);
  return v10 + v01;
}
template <typename T>
HWY_INLINE Vec128<T> MinOfLanes(hwy::SizeTag<8> /* tag */,
                                const Vec128<T> v10) {
  const Vec128<T> v01 = Shuffle01(v10);
  return Min(v10, v01);
}
template <typename T>
HWY_INLINE Vec128<T> MaxOfLanes(hwy::SizeTag<8> /* tag */,
                                const Vec128<T> v10) {
  const Vec128<T> v01 = Shuffle01(v10);
  return Max(v10, v01);
}

// u16/i16
template <typename T, size_t N, HWY_IF_LANE_SIZE(T, 2), HWY_IF_GE32(T, N)>
HWY_API Vec128<T, N> MinOfLanes(hwy::SizeTag<2> /* tag */, Vec128<T, N> v) {
  const DFromV<decltype(v)> d;
  const Repartition<int32_t, decltype(d)> d32;
  const auto even = And(BitCast(d32, v), Set(d32, 0xFFFF));
  const auto odd = ShiftRight<16>(BitCast(d32, v));
  const auto min = MinOfLanes(d32, Min(even, odd));
  // Also broadcast into odd lanes.
  return BitCast(d, Or(min, ShiftLeft<16>(min)));
}
template <typename T, size_t N, HWY_IF_LANE_SIZE(T, 2), HWY_IF_GE32(T, N)>
HWY_API Vec128<T, N> MaxOfLanes(hwy::SizeTag<2> /* tag */, Vec128<T, N> v) {
  const DFromV<decltype(v)> d;
  const Repartition<int32_t, decltype(d)> d32;
  const auto even = And(BitCast(d32, v), Set(d32, 0xFFFF));
  const auto odd = ShiftRight<16>(BitCast(d32, v));
  const auto min = MaxOfLanes(d32, Max(even, odd));
  // Also broadcast into odd lanes.
  return BitCast(d, Or(min, ShiftLeft<16>(min)));
}

}  // namespace detail

// Supported for u/i/f 32/64. Returns the same value in each lane.
template <typename T, size_t N>
HWY_API Vec128<T, N> SumOfLanes(Simd<T, N, 0> /* tag */, const Vec128<T, N> v) {
  return detail::SumOfLanes(hwy::SizeTag<sizeof(T)>(), v);
}
template <typename T, size_t N>
HWY_API Vec128<T, N> MinOfLanes(Simd<T, N, 0> /* tag */, const Vec128<T, N> v) {
  return detail::MinOfLanes(hwy::SizeTag<sizeof(T)>(), v);
}
template <typename T, size_t N>
HWY_API Vec128<T, N> MaxOfLanes(Simd<T, N, 0> /* tag */, const Vec128<T, N> v) {
  return detail::MaxOfLanes(hwy::SizeTag<sizeof(T)>(), v);
}

// ------------------------------ Lt128

template <typename T, size_t N, HWY_IF_LE128(T, N)>
HWY_INLINE Mask128<T, N> Lt128(Simd<T, N, 0> d, Vec128<T, N> a,
                               Vec128<T, N> b) {
  static_assert(!IsSigned<T>() && sizeof(T) == 8, "Use u64");
  // Truth table of Eq and Lt for Hi and Lo u64.
  // (removed lines with (=H && cH) or (=L && cL) - cannot both be true)
  // =H =L cH cL  | out = cH | (=H & cL)
  //  0  0  0  0  |  0
  //  0  0  0  1  |  0
  //  0  0  1  0  |  1
  //  0  0  1  1  |  1
  //  0  1  0  0  |  0
  //  0  1  0  1  |  0
  //  0  1  1  0  |  1
  //  1  0  0  0  |  0
  //  1  0  0  1  |  1
  //  1  1  0  0  |  0
  const Mask128<T, N> eqHL = Eq(a, b);
  const Vec128<T, N> ltHL = VecFromMask(d, Lt(a, b));
  // We need to bring cL to the upper lane/bit corresponding to cH. Comparing
  // the result of InterleaveUpper/Lower requires 9 ops, whereas shifting the
  // comparison result leftwards requires only 4. IfThenElse compiles to the
  // same code as OrAnd().
  const Vec128<T, N> ltLx = DupEven(ltHL);
  const Vec128<T, N> outHx = IfThenElse(eqHL, ltLx, ltHL);
  return MaskFromVec(DupOdd(outHx));
}

template <typename T, size_t N, HWY_IF_LE128(T, N)>
HWY_INLINE Mask128<T, N> Lt128Upper(Simd<T, N, 0> d, Vec128<T, N> a,
                                    Vec128<T, N> b) {
  const Vec128<T, N> ltHL = VecFromMask(d, Lt(a, b));
  return MaskFromVec(InterleaveUpper(d, ltHL, ltHL));
}

// ------------------------------ Eq128

template <typename T, size_t N, HWY_IF_LE128(T, N)>
HWY_INLINE Mask128<T, N> Eq128(Simd<T, N, 0> d, Vec128<T, N> a,
                               Vec128<T, N> b) {
  static_assert(!IsSigned<T>() && sizeof(T) == 8, "Use u64");
  const Vec128<T, N> eqHL = VecFromMask(d, Eq(a, b));
  return MaskFromVec(And(Reverse2(d, eqHL), eqHL));
}

template <typename T, size_t N, HWY_IF_LE128(T, N)>
HWY_INLINE Mask128<T, N> Eq128Upper(Simd<T, N, 0> d, Vec128<T, N> a,
                                    Vec128<T, N> b) {
  const Vec128<T, N> eqHL = VecFromMask(d, Eq(a, b));
  return MaskFromVec(InterleaveUpper(d, eqHL, eqHL));
}

// ------------------------------ Min128, Max128 (Lt128)

// Without a native OddEven, it seems infeasible to go faster than Lt128.
template <class D>
HWY_INLINE VFromD<D> Min128(D d, const VFromD<D> a, const VFromD<D> b) {
  return IfThenElse(Lt128(d, a, b), a, b);
}

template <class D>
HWY_INLINE VFromD<D> Max128(D d, const VFromD<D> a, const VFromD<D> b) {
  return IfThenElse(Lt128(d, b, a), a, b);
}

template <class D>
HWY_INLINE VFromD<D> Min128Upper(D d, const VFromD<D> a, const VFromD<D> b) {
  return IfThenElse(Lt128Upper(d, a, b), a, b);
}

template <class D>
HWY_INLINE VFromD<D> Max128Upper(D d, const VFromD<D> a, const VFromD<D> b) {
  return IfThenElse(Lt128Upper(d, b, a), a, b);
}

// ================================================== Operator wrapper

template <class V>
HWY_API V Add(V a, V b) {
  return a + b;
}
template <class V>
HWY_API V Sub(V a, V b) {
  return a - b;
}

template <class V>
HWY_API V Mul(V a, V b) {
  return a * b;
}
template <class V>
HWY_API V Div(V a, V b) {
  return a / b;
}

template <class V>
V Shl(V a, V b) {
  return a << b;
}
template <class V>
V Shr(V a, V b) {
  return a >> b;
}

template <class V>
HWY_API auto Eq(V a, V b) -> decltype(a == b) {
  return a == b;
}
template <class V>
HWY_API auto Ne(V a, V b) -> decltype(a == b) {
  return a != b;
}
template <class V>
HWY_API auto Lt(V a, V b) -> decltype(a == b) {
  return a < b;
}

template <class V>
HWY_API auto Gt(V a, V b) -> decltype(a == b) {
  return a > b;
}
template <class V>
HWY_API auto Ge(V a, V b) -> decltype(a == b) {
  return a >= b;
}

template <class V>
HWY_API auto Le(V a, V b) -> decltype(a == b) {
  return a <= b;
}

// NOLINTNEXTLINE(google-readability-namespace-comments)
}  // namespace HWY_NAMESPACE
}  // namespace hwy
HWY_AFTER_NAMESPACE();
