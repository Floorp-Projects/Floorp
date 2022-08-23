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

// Single-element vectors and operations.
// External include guard in highway.h - see comment there.

#include <stddef.h>
#include <stdint.h>

#include "hwy/base.h"
#include "hwy/ops/shared-inl.h"

HWY_BEFORE_NAMESPACE();
namespace hwy {
namespace HWY_NAMESPACE {

template <typename T>
using Full128 = Simd<T, 16 / sizeof(T), 0>;

// (Wrapper class required for overloading comparison operators.)
template <typename T, size_t N = 16 / sizeof(T)>
struct Vec128 {
  HWY_INLINE Vec128() = default;
  Vec128(const Vec128&) = default;
  Vec128& operator=(const Vec128&) = default;

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

  // Behave like wasm128 (vectors can always hold 128 bits). generic_ops-inl.h
  // relies on this for LoadInterleaved*. CAVEAT: this method of padding
  // prevents using range for, especially in SumOfLanes, where it would be
  // incorrect. Moving padding to another field would require handling the case
  // where N = 16 / sizeof(T) (i.e. there is no padding), which is also awkward.
  T raw[16 / sizeof(T)] = {};
};

// 0 or FF..FF, same size as Vec128.
template <typename T, size_t N = 16 / sizeof(T)>
struct Mask128 {
  using Raw = hwy::MakeUnsigned<T>;
  static HWY_INLINE Raw FromBool(bool b) {
    return b ? static_cast<Raw>(~Raw{0}) : 0;
  }

  // Must match the size of Vec128.
  Raw bits[16 / sizeof(T)] = {};
};

namespace detail {

// Deduce Simd<T, N, 0> from Vec128<T, N>
struct Deduce128 {
  template <typename T, size_t N>
  Simd<T, N, 0> operator()(Vec128<T, N>) const {
    return Simd<T, N, 0>();
  }
};

}  // namespace detail

template <class V>
using DFromV = decltype(detail::Deduce128()(V()));

template <class V>
using TFromV = TFromD<DFromV<V>>;

// ------------------------------ BitCast

template <typename T, size_t N, typename FromT, size_t FromN>
HWY_API Vec128<T, N> BitCast(Simd<T, N, 0> /* tag */, Vec128<FromT, FromN> v) {
  Vec128<T, N> to;
  static_assert(sizeof(T) * N == sizeof(FromT) * FromN,
                "Casting does not change size");
  CopyBytes<sizeof(T) * N>(v.raw, to.raw);
  return to;
}

// ------------------------------ Set

template <typename T, size_t N>
HWY_API Vec128<T, N> Zero(Simd<T, N, 0> /* tag */) {
  Vec128<T, N> v;
  ZeroBytes<sizeof(T) * N>(v.raw);
  return v;
}

template <class D>
using VFromD = decltype(Zero(D()));

template <typename T, size_t N, typename T2>
HWY_API Vec128<T, N> Set(Simd<T, N, 0> /* tag */, const T2 t) {
  Vec128<T, N> v;
  for (size_t i = 0; i < N; ++i) {
    v.raw[i] = static_cast<T>(t);
  }
  return v;
}

template <typename T, size_t N>
HWY_API Vec128<T, N> Undefined(Simd<T, N, 0> d) {
  return Zero(d);
}

namespace detail {

template <typename T>
HWY_INLINE constexpr T IncrementWithWraparound(hwy::FloatTag /*tag*/, T t) {
  return t + T{1};
}

template <typename T>
HWY_INLINE constexpr T IncrementWithWraparound(hwy::NonFloatTag /*tag*/, T t) {
  using TU = MakeUnsigned<T>;
  return static_cast<T>(static_cast<TU>(static_cast<TU>(t) + TU{1}) &
                        hwy::LimitsMax<TU>());
}

}  // namespace detail

template <typename T, size_t N, typename T2>
HWY_API Vec128<T, N> Iota(const Simd<T, N, 0> /* tag */, T2 first) {
  Vec128<T, N> v;
  T counter = static_cast<T>(first);
  for (size_t i = 0; i < N; ++i) {
    v.raw[i] = counter;
    counter = detail::IncrementWithWraparound(hwy::IsFloatTag<T>(), counter);
  }
  return v;
}

// ================================================== LOGICAL

// ------------------------------ Not
template <typename T, size_t N>
HWY_API Vec128<T, N> Not(const Vec128<T, N> v) {
  const Simd<T, N, 0> d;
  const RebindToUnsigned<decltype(d)> du;
  using TU = TFromD<decltype(du)>;
  VFromD<decltype(du)> vu = BitCast(du, v);
  for (size_t i = 0; i < N; ++i) {
    vu.raw[i] = static_cast<TU>(~vu.raw[i]);
  }
  return BitCast(d, vu);
}

// ------------------------------ And
template <typename T, size_t N>
HWY_API Vec128<T, N> And(const Vec128<T, N> a, const Vec128<T, N> b) {
  const Simd<T, N, 0> d;
  const RebindToUnsigned<decltype(d)> du;
  auto au = BitCast(du, a);
  auto bu = BitCast(du, b);
  for (size_t i = 0; i < N; ++i) {
    au.raw[i] &= bu.raw[i];
  }
  return BitCast(d, au);
}
template <typename T, size_t N>
HWY_API Vec128<T, N> operator&(const Vec128<T, N> a, const Vec128<T, N> b) {
  return And(a, b);
}

// ------------------------------ AndNot
template <typename T, size_t N>
HWY_API Vec128<T, N> AndNot(const Vec128<T, N> a, const Vec128<T, N> b) {
  return And(Not(a), b);
}

// ------------------------------ Or
template <typename T, size_t N>
HWY_API Vec128<T, N> Or(const Vec128<T, N> a, const Vec128<T, N> b) {
  const Simd<T, N, 0> d;
  const RebindToUnsigned<decltype(d)> du;
  auto au = BitCast(du, a);
  auto bu = BitCast(du, b);
  for (size_t i = 0; i < N; ++i) {
    au.raw[i] |= bu.raw[i];
  }
  return BitCast(d, au);
}
template <typename T, size_t N>
HWY_API Vec128<T, N> operator|(const Vec128<T, N> a, const Vec128<T, N> b) {
  return Or(a, b);
}

// ------------------------------ Xor
template <typename T, size_t N>
HWY_API Vec128<T, N> Xor(const Vec128<T, N> a, const Vec128<T, N> b) {
  const Simd<T, N, 0> d;
  const RebindToUnsigned<decltype(d)> du;
  auto au = BitCast(du, a);
  auto bu = BitCast(du, b);
  for (size_t i = 0; i < N; ++i) {
    au.raw[i] ^= bu.raw[i];
  }
  return BitCast(d, au);
}
template <typename T, size_t N>
HWY_API Vec128<T, N> operator^(const Vec128<T, N> a, const Vec128<T, N> b) {
  return Xor(a, b);
}

// ------------------------------ Or3

template <typename T, size_t N>
HWY_API Vec128<T, N> Or3(Vec128<T, N> o1, Vec128<T, N> o2, Vec128<T, N> o3) {
  return Or(o1, Or(o2, o3));
}

// ------------------------------ OrAnd
template <typename T, size_t N>
HWY_API Vec128<T, N> OrAnd(const Vec128<T, N> o, const Vec128<T, N> a1,
                           const Vec128<T, N> a2) {
  return Or(o, And(a1, a2));
}

// ------------------------------ IfVecThenElse
template <typename T, size_t N>
HWY_API Vec128<T, N> IfVecThenElse(Vec128<T, N> mask, Vec128<T, N> yes,
                                   Vec128<T, N> no) {
  return Or(And(mask, yes), AndNot(mask, no));
}

// ------------------------------ CopySign
template <typename T, size_t N>
HWY_API Vec128<T, N> CopySign(const Vec128<T, N> magn,
                              const Vec128<T, N> sign) {
  static_assert(IsFloat<T>(), "Only makes sense for floating-point");
  const auto msb = SignBit(Simd<T, N, 0>());
  return Or(AndNot(msb, magn), And(msb, sign));
}

template <typename T, size_t N>
HWY_API Vec128<T, N> CopySignToAbs(const Vec128<T, N> abs,
                                   const Vec128<T, N> sign) {
  static_assert(IsFloat<T>(), "Only makes sense for floating-point");
  return Or(abs, And(SignBit(Simd<T, N, 0>()), sign));
}

// ------------------------------ BroadcastSignBit
template <typename T, size_t N>
HWY_API Vec128<T, N> BroadcastSignBit(Vec128<T, N> v) {
  // This is used inside ShiftRight, so we cannot implement in terms of it.
  for (size_t i = 0; i < N; ++i) {
    v.raw[i] = v.raw[i] < 0 ? T(-1) : T(0);
  }
  return v;
}

// ------------------------------ Mask

template <typename TFrom, typename TTo, size_t N>
HWY_API Mask128<TTo, N> RebindMask(Simd<TTo, N, 0> /*tag*/,
                                   Mask128<TFrom, N> mask) {
  Mask128<TTo, N> to;
  static_assert(sizeof(TTo) * N == sizeof(TFrom) * N, "Must have same size");
  CopyBytes<sizeof(TTo) * N>(mask.bits, to.bits);
  return to;
}

// v must be 0 or FF..FF.
template <typename T, size_t N>
HWY_API Mask128<T, N> MaskFromVec(const Vec128<T, N> v) {
  Mask128<T, N> mask;
  static_assert(sizeof(v) == sizeof(mask), "Must have same size");
  CopyBytes<sizeof(T) * N>(v.raw, mask.bits);
  return mask;
}

template <typename T, size_t N>
Vec128<T, N> VecFromMask(const Mask128<T, N> mask) {
  Vec128<T, N> v;
  CopyBytes<sizeof(T) * N>(mask.bits, v.raw);
  return v;
}

template <typename T, size_t N>
Vec128<T, N> VecFromMask(Simd<T, N, 0> /* tag */, const Mask128<T, N> mask) {
  return VecFromMask(mask);
}

template <typename T, size_t N>
HWY_API Mask128<T, N> FirstN(Simd<T, N, 0> /*tag*/, size_t n) {
  Mask128<T, N> m;
  for (size_t i = 0; i < N; ++i) {
    m.bits[i] = Mask128<T, N>::FromBool(i < n);
  }
  return m;
}

// Returns mask ? yes : no.
template <typename T, size_t N>
HWY_API Vec128<T, N> IfThenElse(const Mask128<T, N> mask,
                                const Vec128<T, N> yes, const Vec128<T, N> no) {
  return IfVecThenElse(VecFromMask(mask), yes, no);
}

template <typename T, size_t N>
HWY_API Vec128<T, N> IfThenElseZero(const Mask128<T, N> mask,
                                    const Vec128<T, N> yes) {
  return IfVecThenElse(VecFromMask(mask), yes, Zero(Simd<T, N, 0>()));
}

template <typename T, size_t N>
HWY_API Vec128<T, N> IfThenZeroElse(const Mask128<T, N> mask,
                                    const Vec128<T, N> no) {
  return IfVecThenElse(VecFromMask(mask), Zero(Simd<T, N, 0>()), no);
}

template <typename T, size_t N>
HWY_API Vec128<T, N> IfNegativeThenElse(Vec128<T, N> v, Vec128<T, N> yes,
                                        Vec128<T, N> no) {
  for (size_t i = 0; i < N; ++i) {
    v.raw[i] = v.raw[i] < 0 ? yes.raw[i] : no.raw[i];
  }
  return v;
}

template <typename T, size_t N>
HWY_API Vec128<T, N> ZeroIfNegative(const Vec128<T, N> v) {
  return IfNegativeThenElse(v, Zero(Simd<T, N, 0>()), v);
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

// ================================================== SHIFTS

// ------------------------------ ShiftLeft/ShiftRight (BroadcastSignBit)

template <int kBits, typename T, size_t N>
HWY_API Vec128<T, N> ShiftLeft(Vec128<T, N> v) {
  static_assert(0 <= kBits && kBits < sizeof(T) * 8, "Invalid shift");
  for (size_t i = 0; i < N; ++i) {
    const auto shifted = static_cast<hwy::MakeUnsigned<T>>(v.raw[i]) << kBits;
    v.raw[i] = static_cast<T>(shifted);
  }
  return v;
}

template <int kBits, typename T, size_t N>
HWY_API Vec128<T, N> ShiftRight(Vec128<T, N> v) {
  static_assert(0 <= kBits && kBits < sizeof(T) * 8, "Invalid shift");
#if __cplusplus >= 202002L
  // Signed right shift is now guaranteed to be arithmetic (rounding toward
  // negative infinity, i.e. shifting in the sign bit).
  for (size_t i = 0; i < N; ++i) {
    v.raw[i] = static_cast<T>(v.raw[i] >> kBits);
  }
#else
  if (IsSigned<T>()) {
    // Emulate arithmetic shift using only logical (unsigned) shifts, because
    // signed shifts are still implementation-defined.
    using TU = hwy::MakeUnsigned<T>;
    for (size_t i = 0; i < N; ++i) {
      const TU shifted = static_cast<TU>(static_cast<TU>(v.raw[i]) >> kBits);
      const TU sign = v.raw[i] < 0 ? static_cast<TU>(~TU{0}) : 0;
      const size_t sign_shift =
          static_cast<size_t>(static_cast<int>(sizeof(TU)) * 8 - 1 - kBits);
      const TU upper = static_cast<TU>(sign << sign_shift);
      v.raw[i] = static_cast<T>(shifted | upper);
    }
  } else {  // T is unsigned
    for (size_t i = 0; i < N; ++i) {
      v.raw[i] = static_cast<T>(v.raw[i] >> kBits);
    }
  }
#endif
  return v;
}

// ------------------------------ RotateRight (ShiftRight)

namespace detail {

// For partial specialization: kBits == 0 results in an invalid shift count
template <int kBits>
struct RotateRight {
  template <typename T, size_t N>
  HWY_INLINE Vec128<T, N> operator()(const Vec128<T, N> v) const {
    return Or(ShiftRight<kBits>(v), ShiftLeft<sizeof(T) * 8 - kBits>(v));
  }
};

template <>
struct RotateRight<0> {
  template <typename T, size_t N>
  HWY_INLINE Vec128<T, N> operator()(const Vec128<T, N> v) const {
    return v;
  }
};

}  // namespace detail

template <int kBits, typename T, size_t N>
HWY_API Vec128<T, N> RotateRight(const Vec128<T, N> v) {
  static_assert(0 <= kBits && kBits < sizeof(T) * 8, "Invalid shift");
  return detail::RotateRight<kBits>()(v);
}

// ------------------------------ ShiftLeftSame

template <typename T, size_t N>
HWY_API Vec128<T, N> ShiftLeftSame(Vec128<T, N> v, int bits) {
  for (size_t i = 0; i < N; ++i) {
    const auto shifted = static_cast<hwy::MakeUnsigned<T>>(v.raw[i]) << bits;
    v.raw[i] = static_cast<T>(shifted);
  }
  return v;
}

template <typename T, size_t N>
HWY_API Vec128<T, N> ShiftRightSame(Vec128<T, N> v, int bits) {
#if __cplusplus >= 202002L
  // Signed right shift is now guaranteed to be arithmetic (rounding toward
  // negative infinity, i.e. shifting in the sign bit).
  for (size_t i = 0; i < N; ++i) {
    v.raw[i] = static_cast<T>(v.raw[i] >> bits);
  }
#else
  if (IsSigned<T>()) {
    // Emulate arithmetic shift using only logical (unsigned) shifts, because
    // signed shifts are still implementation-defined.
    using TU = hwy::MakeUnsigned<T>;
    for (size_t i = 0; i < N; ++i) {
      const TU shifted = static_cast<TU>(static_cast<TU>(v.raw[i]) >> bits);
      const TU sign = v.raw[i] < 0 ? static_cast<TU>(~TU{0}) : 0;
      const size_t sign_shift =
          static_cast<size_t>(static_cast<int>(sizeof(TU)) * 8 - 1 - bits);
      const TU upper = static_cast<TU>(sign << sign_shift);
      v.raw[i] = static_cast<T>(shifted | upper);
    }
  } else {
    for (size_t i = 0; i < N; ++i) {
      v.raw[i] = static_cast<T>(v.raw[i] >> bits);  // unsigned, logical shift
    }
  }
#endif
  return v;
}

// ------------------------------ Shl

template <typename T, size_t N>
HWY_API Vec128<T, N> operator<<(Vec128<T, N> v, const Vec128<T, N> bits) {
  for (size_t i = 0; i < N; ++i) {
    const auto shifted = static_cast<hwy::MakeUnsigned<T>>(v.raw[i])
                         << bits.raw[i];
    v.raw[i] = static_cast<T>(shifted);
  }
  return v;
}

template <typename T, size_t N>
HWY_API Vec128<T, N> operator>>(Vec128<T, N> v, const Vec128<T, N> bits) {
#if __cplusplus >= 202002L
  // Signed right shift is now guaranteed to be arithmetic (rounding toward
  // negative infinity, i.e. shifting in the sign bit).
  for (size_t i = 0; i < N; ++i) {
    v.raw[i] = static_cast<T>(v.raw[i] >> bits.raw[i]);
  }
#else
  if (IsSigned<T>()) {
    // Emulate arithmetic shift using only logical (unsigned) shifts, because
    // signed shifts are still implementation-defined.
    using TU = hwy::MakeUnsigned<T>;
    for (size_t i = 0; i < N; ++i) {
      const TU shifted =
          static_cast<TU>(static_cast<TU>(v.raw[i]) >> bits.raw[i]);
      const TU sign = v.raw[i] < 0 ? static_cast<TU>(~TU{0}) : 0;
      const size_t sign_shift = static_cast<size_t>(
          static_cast<int>(sizeof(TU)) * 8 - 1 - bits.raw[i]);
      const TU upper = static_cast<TU>(sign << sign_shift);
      v.raw[i] = static_cast<T>(shifted | upper);
    }
  } else {  // T is unsigned
    for (size_t i = 0; i < N; ++i) {
      v.raw[i] = static_cast<T>(v.raw[i] >> bits.raw[i]);
    }
  }
#endif
  return v;
}

// ================================================== ARITHMETIC

// Tag dispatch instead of SFINAE for MSVC 2017 compatibility
namespace detail {

template <typename T, size_t N>
HWY_INLINE Vec128<T, N> Add(hwy::NonFloatTag /*tag*/, Vec128<T, N> a,
                            Vec128<T, N> b) {
  for (size_t i = 0; i < N; ++i) {
    const uint64_t a64 = static_cast<uint64_t>(a.raw[i]);
    const uint64_t b64 = static_cast<uint64_t>(b.raw[i]);
    a.raw[i] = static_cast<T>((a64 + b64) & static_cast<uint64_t>(~T(0)));
  }
  return a;
}
template <typename T, size_t N>
HWY_INLINE Vec128<T, N> Sub(hwy::NonFloatTag /*tag*/, Vec128<T, N> a,
                            Vec128<T, N> b) {
  for (size_t i = 0; i < N; ++i) {
    const uint64_t a64 = static_cast<uint64_t>(a.raw[i]);
    const uint64_t b64 = static_cast<uint64_t>(b.raw[i]);
    a.raw[i] = static_cast<T>((a64 - b64) & static_cast<uint64_t>(~T(0)));
  }
  return a;
}

template <typename T, size_t N>
HWY_INLINE Vec128<T, N> Add(hwy::FloatTag /*tag*/, Vec128<T, N> a,
                            const Vec128<T, N> b) {
  for (size_t i = 0; i < N; ++i) {
    a.raw[i] += b.raw[i];
  }
  return a;
}

template <typename T, size_t N>
HWY_INLINE Vec128<T, N> Sub(hwy::FloatTag /*tag*/, Vec128<T, N> a,
                            const Vec128<T, N> b) {
  for (size_t i = 0; i < N; ++i) {
    a.raw[i] -= b.raw[i];
  }
  return a;
}

}  // namespace detail

template <typename T, size_t N>
HWY_API Vec128<T, N> operator-(Vec128<T, N> a, const Vec128<T, N> b) {
  return detail::Sub(hwy::IsFloatTag<T>(), a, b);
}
template <typename T, size_t N>
HWY_API Vec128<T, N> operator+(Vec128<T, N> a, const Vec128<T, N> b) {
  return detail::Add(hwy::IsFloatTag<T>(), a, b);
}

// ------------------------------ SumsOf8

template <size_t N>
HWY_API Vec128<uint64_t, (N + 7) / 8> SumsOf8(const Vec128<uint8_t, N> v) {
  Vec128<uint64_t, (N + 7) / 8> sums;
  for (size_t i = 0; i < N; ++i) {
    sums.raw[i / 8] += v.raw[i];
  }
  return sums;
}

// ------------------------------ SaturatedAdd
template <typename T, size_t N>
HWY_API Vec128<T, N> SaturatedAdd(Vec128<T, N> a, const Vec128<T, N> b) {
  for (size_t i = 0; i < N; ++i) {
    a.raw[i] = static_cast<T>(
        HWY_MIN(HWY_MAX(hwy::LowestValue<T>(), a.raw[i] + b.raw[i]),
                hwy::HighestValue<T>()));
  }
  return a;
}

// ------------------------------ SaturatedSub
template <typename T, size_t N>
HWY_API Vec128<T, N> SaturatedSub(Vec128<T, N> a, const Vec128<T, N> b) {
  for (size_t i = 0; i < N; ++i) {
    a.raw[i] = static_cast<T>(
        HWY_MIN(HWY_MAX(hwy::LowestValue<T>(), a.raw[i] - b.raw[i]),
                hwy::HighestValue<T>()));
  }
  return a;
}

// ------------------------------ AverageRound
template <typename T, size_t N>
HWY_API Vec128<T, N> AverageRound(Vec128<T, N> a, const Vec128<T, N> b) {
  static_assert(!IsSigned<T>(), "Only for unsigned");
  for (size_t i = 0; i < N; ++i) {
    a.raw[i] = static_cast<T>((a.raw[i] + b.raw[i] + 1) / 2);
  }
  return a;
}

// ------------------------------ Abs

// Tag dispatch instead of SFINAE for MSVC 2017 compatibility
namespace detail {

template <typename T, size_t N>
HWY_INLINE Vec128<T, N> Abs(SignedTag /*tag*/, Vec128<T, N> a) {
  for (size_t i = 0; i < N; ++i) {
    const T s = a.raw[i];
    const T min = hwy::LimitsMin<T>();
    a.raw[i] = static_cast<T>((s >= 0 || s == min) ? a.raw[i] : -s);
  }
  return a;
}

template <typename T, size_t N>
HWY_INLINE Vec128<T, N> Abs(hwy::FloatTag /*tag*/, Vec128<T, N> v) {
  for (size_t i = 0; i < N; ++i) {
    v.raw[i] = std::abs(v.raw[i]);
  }
  return v;
}

}  // namespace detail

template <typename T, size_t N>
HWY_API Vec128<T, N> Abs(Vec128<T, N> a) {
  return detail::Abs(hwy::TypeTag<T>(), a);
}

// ------------------------------ Min/Max

// Tag dispatch instead of SFINAE for MSVC 2017 compatibility
namespace detail {

template <typename T, size_t N>
HWY_INLINE Vec128<T, N> Min(hwy::NonFloatTag /*tag*/, Vec128<T, N> a,
                            const Vec128<T, N> b) {
  for (size_t i = 0; i < N; ++i) {
    a.raw[i] = HWY_MIN(a.raw[i], b.raw[i]);
  }
  return a;
}
template <typename T, size_t N>
HWY_INLINE Vec128<T, N> Max(hwy::NonFloatTag /*tag*/, Vec128<T, N> a,
                            const Vec128<T, N> b) {
  for (size_t i = 0; i < N; ++i) {
    a.raw[i] = HWY_MAX(a.raw[i], b.raw[i]);
  }
  return a;
}

template <typename T, size_t N>
HWY_INLINE Vec128<T, N> Min(hwy::FloatTag /*tag*/, Vec128<T, N> a,
                            const Vec128<T, N> b) {
  for (size_t i = 0; i < N; ++i) {
    if (std::isnan(a.raw[i])) {
      a.raw[i] = b.raw[i];
    } else if (std::isnan(b.raw[i])) {
      // no change
    } else {
      a.raw[i] = HWY_MIN(a.raw[i], b.raw[i]);
    }
  }
  return a;
}
template <typename T, size_t N>
HWY_INLINE Vec128<T, N> Max(hwy::FloatTag /*tag*/, Vec128<T, N> a,
                            const Vec128<T, N> b) {
  for (size_t i = 0; i < N; ++i) {
    if (std::isnan(a.raw[i])) {
      a.raw[i] = b.raw[i];
    } else if (std::isnan(b.raw[i])) {
      // no change
    } else {
      a.raw[i] = HWY_MAX(a.raw[i], b.raw[i]);
    }
  }
  return a;
}

}  // namespace detail

template <typename T, size_t N>
HWY_API Vec128<T, N> Min(Vec128<T, N> a, const Vec128<T, N> b) {
  return detail::Min(hwy::IsFloatTag<T>(), a, b);
}

template <typename T, size_t N>
HWY_API Vec128<T, N> Max(Vec128<T, N> a, const Vec128<T, N> b) {
  return detail::Max(hwy::IsFloatTag<T>(), a, b);
}

// ------------------------------ Neg

// Tag dispatch instead of SFINAE for MSVC 2017 compatibility
namespace detail {

template <typename T, size_t N>
HWY_API Vec128<T, N> Neg(hwy::NonFloatTag /*tag*/, Vec128<T, N> v) {
  return Zero(Simd<T, N, 0>()) - v;
}

template <typename T, size_t N>
HWY_API Vec128<T, N> Neg(hwy::FloatTag /*tag*/, Vec128<T, N> v) {
  return Xor(v, SignBit(Simd<T, N, 0>()));
}

}  // namespace detail

template <typename T, size_t N>
HWY_API Vec128<T, N> Neg(Vec128<T, N> v) {
  return detail::Neg(hwy::IsFloatTag<T>(), v);
}

// ------------------------------ Mul/Div

// Tag dispatch instead of SFINAE for MSVC 2017 compatibility
namespace detail {

template <typename T, size_t N>
HWY_INLINE Vec128<T, N> Mul(hwy::FloatTag /*tag*/, Vec128<T, N> a,
                            const Vec128<T, N> b) {
  for (size_t i = 0; i < N; ++i) {
    a.raw[i] *= b.raw[i];
  }
  return a;
}

template <typename T, size_t N>
HWY_INLINE Vec128<T, N> Mul(SignedTag /*tag*/, Vec128<T, N> a,
                            const Vec128<T, N> b) {
  for (size_t i = 0; i < N; ++i) {
    a.raw[i] = static_cast<T>(static_cast<int64_t>(a.raw[i]) * b.raw[i]);
  }
  return a;
}

template <typename T, size_t N>
HWY_INLINE Vec128<T, N> Mul(UnsignedTag /*tag*/, Vec128<T, N> a,
                            const Vec128<T, N> b) {
  for (size_t i = 0; i < N; ++i) {
    a.raw[i] = static_cast<T>(static_cast<uint64_t>(a.raw[i]) * b.raw[i]);
  }
  return a;
}

}  // namespace detail

template <typename T, size_t N>
HWY_API Vec128<T, N> operator*(Vec128<T, N> a, const Vec128<T, N> b) {
  return detail::Mul(hwy::TypeTag<T>(), a, b);
}

template <typename T, size_t N>
HWY_API Vec128<T, N> operator/(Vec128<T, N> a, const Vec128<T, N> b) {
  for (size_t i = 0; i < N; ++i) {
    a.raw[i] /= b.raw[i];
  }
  return a;
}

// Returns the upper 16 bits of a * b in each lane.
template <size_t N>
HWY_API Vec128<int16_t, N> MulHigh(Vec128<int16_t, N> a,
                                   const Vec128<int16_t, N> b) {
  for (size_t i = 0; i < N; ++i) {
    a.raw[i] = static_cast<int16_t>((int32_t{a.raw[i]} * b.raw[i]) >> 16);
  }
  return a;
}
template <size_t N>
HWY_API Vec128<uint16_t, N> MulHigh(Vec128<uint16_t, N> a,
                                    const Vec128<uint16_t, N> b) {
  for (size_t i = 0; i < N; ++i) {
    // Cast to uint32_t first to prevent overflow. Otherwise the result of
    // uint16_t * uint16_t is in "int" which may overflow. In practice the
    // result is the same but this way it is also defined.
    a.raw[i] = static_cast<uint16_t>(
        (static_cast<uint32_t>(a.raw[i]) * static_cast<uint32_t>(b.raw[i])) >>
        16);
  }
  return a;
}

template <size_t N>
HWY_API Vec128<int16_t, N> MulFixedPoint15(Vec128<int16_t, N> a,
                                           Vec128<int16_t, N> b) {
  for (size_t i = 0; i < N; ++i) {
    a.raw[i] = static_cast<int16_t>((2 * a.raw[i] * b.raw[i] + 32768) >> 16);
  }
  return a;
}

// Multiplies even lanes (0, 2 ..) and returns the double-wide result.
template <size_t N>
HWY_API Vec128<int64_t, (N + 1) / 2> MulEven(const Vec128<int32_t, N> a,
                                             const Vec128<int32_t, N> b) {
  Vec128<int64_t, (N + 1) / 2> mul;
  for (size_t i = 0; i < N; i += 2) {
    const int64_t a64 = a.raw[i];
    mul.raw[i / 2] = a64 * b.raw[i];
  }
  return mul;
}
template <size_t N>
HWY_API Vec128<uint64_t, (N + 1) / 2> MulEven(Vec128<uint32_t, N> a,
                                              const Vec128<uint32_t, N> b) {
  Vec128<uint64_t, (N + 1) / 2> mul;
  for (size_t i = 0; i < N; i += 2) {
    const uint64_t a64 = a.raw[i];
    mul.raw[i / 2] = a64 * b.raw[i];
  }
  return mul;
}

template <size_t N>
HWY_API Vec128<int64_t, (N + 1) / 2> MulOdd(const Vec128<int32_t, N> a,
                                            const Vec128<int32_t, N> b) {
  Vec128<int64_t, (N + 1) / 2> mul;
  for (size_t i = 0; i < N; i += 2) {
    const int64_t a64 = a.raw[i + 1];
    mul.raw[i / 2] = a64 * b.raw[i + 1];
  }
  return mul;
}
template <size_t N>
HWY_API Vec128<uint64_t, (N + 1) / 2> MulOdd(Vec128<uint32_t, N> a,
                                             const Vec128<uint32_t, N> b) {
  Vec128<uint64_t, (N + 1) / 2> mul;
  for (size_t i = 0; i < N; i += 2) {
    const uint64_t a64 = a.raw[i + 1];
    mul.raw[i / 2] = a64 * b.raw[i + 1];
  }
  return mul;
}

template <size_t N>
HWY_API Vec128<float, N> ApproximateReciprocal(Vec128<float, N> v) {
  for (size_t i = 0; i < N; ++i) {
    // Zero inputs are allowed, but callers are responsible for replacing the
    // return value with something else (typically using IfThenElse). This check
    // avoids a ubsan error. The result is arbitrary.
    v.raw[i] = (std::abs(v.raw[i]) == 0.0f) ? 0.0f : 1.0f / v.raw[i];
  }
  return v;
}

template <size_t N>
HWY_API Vec128<float, N> AbsDiff(Vec128<float, N> a, const Vec128<float, N> b) {
  return Abs(a - b);
}

// ------------------------------ Floating-point multiply-add variants

template <typename T, size_t N>
HWY_API Vec128<T, N> MulAdd(Vec128<T, N> mul, const Vec128<T, N> x,
                            const Vec128<T, N> add) {
  return mul * x + add;
}

template <typename T, size_t N>
HWY_API Vec128<T, N> NegMulAdd(Vec128<T, N> mul, const Vec128<T, N> x,
                               const Vec128<T, N> add) {
  return add - mul * x;
}

template <typename T, size_t N>
HWY_API Vec128<T, N> MulSub(Vec128<T, N> mul, const Vec128<T, N> x,
                            const Vec128<T, N> sub) {
  return mul * x - sub;
}

template <typename T, size_t N>
HWY_API Vec128<T, N> NegMulSub(Vec128<T, N> mul, const Vec128<T, N> x,
                               const Vec128<T, N> sub) {
  return Neg(mul) * x - sub;
}

// ------------------------------ Floating-point square root

template <size_t N>
HWY_API Vec128<float, N> ApproximateReciprocalSqrt(Vec128<float, N> v) {
  for (size_t i = 0; i < N; ++i) {
    const float half = v.raw[i] * 0.5f;
    uint32_t bits;
    CopyBytes<4>(&v.raw[i], &bits);
    // Initial guess based on log2(f)
    bits = 0x5F3759DF - (bits >> 1);
    CopyBytes<4>(&bits, &v.raw[i]);
    // One Newton-Raphson iteration
    v.raw[i] = v.raw[i] * (1.5f - (half * v.raw[i] * v.raw[i]));
  }
  return v;
}

template <typename T, size_t N>
HWY_API Vec128<T, N> Sqrt(Vec128<T, N> v) {
  for (size_t i = 0; i < N; ++i) {
    v.raw[i] = std::sqrt(v.raw[i]);
  }
  return v;
}

// ------------------------------ Floating-point rounding

template <typename T, size_t N>
HWY_API Vec128<T, N> Round(Vec128<T, N> v) {
  using TI = MakeSigned<T>;
  const Vec128<T, N> a = Abs(v);
  for (size_t i = 0; i < N; ++i) {
    if (!(a.raw[i] < MantissaEnd<T>())) {  // Huge or NaN
      continue;
    }
    const T bias = v.raw[i] < T(0.0) ? T(-0.5) : T(0.5);
    const TI rounded = static_cast<TI>(v.raw[i] + bias);
    if (rounded == 0) {
      v.raw[i] = v.raw[i] < 0 ? T{-0} : T{0};
      continue;
    }
    const T rounded_f = static_cast<T>(rounded);
    // Round to even
    if ((rounded & 1) && std::abs(rounded_f - v.raw[i]) == T(0.5)) {
      v.raw[i] = static_cast<T>(rounded - (v.raw[i] < T(0) ? -1 : 1));
      continue;
    }
    v.raw[i] = rounded_f;
  }
  return v;
}

// Round-to-nearest even.
template <size_t N>
HWY_API Vec128<int32_t, N> NearestInt(const Vec128<float, N> v) {
  using T = float;
  using TI = int32_t;

  const Vec128<float, N> abs = Abs(v);
  Vec128<int32_t, N> ret;
  for (size_t i = 0; i < N; ++i) {
    const bool signbit = std::signbit(v.raw[i]);

    if (!(abs.raw[i] < MantissaEnd<T>())) {  // Huge or NaN
      // Check if too large to cast or NaN
      if (!(abs.raw[i] <= static_cast<T>(LimitsMax<TI>()))) {
        ret.raw[i] = signbit ? LimitsMin<TI>() : LimitsMax<TI>();
        continue;
      }
      ret.raw[i] = static_cast<TI>(v.raw[i]);
      continue;
    }
    const T bias = v.raw[i] < T(0.0) ? T(-0.5) : T(0.5);
    const TI rounded = static_cast<TI>(v.raw[i] + bias);
    if (rounded == 0) {
      ret.raw[i] = 0;
      continue;
    }
    const T rounded_f = static_cast<T>(rounded);
    // Round to even
    if ((rounded & 1) && std::abs(rounded_f - v.raw[i]) == T(0.5)) {
      ret.raw[i] = rounded - (signbit ? -1 : 1);
      continue;
    }
    ret.raw[i] = rounded;
  }
  return ret;
}

template <typename T, size_t N>
HWY_API Vec128<T, N> Trunc(Vec128<T, N> v) {
  using TI = MakeSigned<T>;
  const Vec128<T, N> abs = Abs(v);
  for (size_t i = 0; i < N; ++i) {
    if (!(abs.raw[i] <= MantissaEnd<T>())) {  // Huge or NaN
      continue;
    }
    const TI truncated = static_cast<TI>(v.raw[i]);
    if (truncated == 0) {
      v.raw[i] = v.raw[i] < 0 ? -T{0} : T{0};
      continue;
    }
    v.raw[i] = static_cast<T>(truncated);
  }
  return v;
}

// Toward +infinity, aka ceiling
template <typename Float, size_t N>
Vec128<Float, N> Ceil(Vec128<Float, N> v) {
  constexpr int kMantissaBits = MantissaBits<Float>();
  using Bits = MakeUnsigned<Float>;
  const Bits kExponentMask = MaxExponentField<Float>();
  const Bits kMantissaMask = MantissaMask<Float>();
  const Bits kBias = kExponentMask / 2;

  for (size_t i = 0; i < N; ++i) {
    const bool positive = v.raw[i] > Float(0.0);

    Bits bits;
    CopyBytes<sizeof(Bits)>(&v.raw[i], &bits);

    const int exponent =
        static_cast<int>(((bits >> kMantissaBits) & kExponentMask) - kBias);
    // Already an integer.
    if (exponent >= kMantissaBits) continue;
    // |v| <= 1 => 0 or 1.
    if (exponent < 0) {
      v.raw[i] = positive ? Float{1} : Float{-0.0};
      continue;
    }

    const Bits mantissa_mask = kMantissaMask >> exponent;
    // Already an integer
    if ((bits & mantissa_mask) == 0) continue;

    // Clear fractional bits and round up
    if (positive) bits += (kMantissaMask + 1) >> exponent;
    bits &= ~mantissa_mask;

    CopyBytes<sizeof(Bits)>(&bits, &v.raw[i]);
  }
  return v;
}

// Toward -infinity, aka floor
template <typename Float, size_t N>
Vec128<Float, N> Floor(Vec128<Float, N> v) {
  constexpr int kMantissaBits = MantissaBits<Float>();
  using Bits = MakeUnsigned<Float>;
  const Bits kExponentMask = MaxExponentField<Float>();
  const Bits kMantissaMask = MantissaMask<Float>();
  const Bits kBias = kExponentMask / 2;

  for (size_t i = 0; i < N; ++i) {
    const bool negative = v.raw[i] < Float(0.0);

    Bits bits;
    CopyBytes<sizeof(Bits)>(&v.raw[i], &bits);

    const int exponent =
        static_cast<int>(((bits >> kMantissaBits) & kExponentMask) - kBias);
    // Already an integer.
    if (exponent >= kMantissaBits) continue;
    // |v| <= 1 => -1 or 0.
    if (exponent < 0) {
      v.raw[i] = negative ? Float(-1.0) : Float(0.0);
      continue;
    }

    const Bits mantissa_mask = kMantissaMask >> exponent;
    // Already an integer
    if ((bits & mantissa_mask) == 0) continue;

    // Clear fractional bits and round down
    if (negative) bits += (kMantissaMask + 1) >> exponent;
    bits &= ~mantissa_mask;

    CopyBytes<sizeof(Bits)>(&bits, &v.raw[i]);
  }
  return v;
}

// ------------------------------ Floating-point classification

template <typename T, size_t N>
HWY_API Mask128<T, N> IsNaN(const Vec128<T, N> v) {
  Mask128<T, N> ret;
  for (size_t i = 0; i < N; ++i) {
    // std::isnan returns false for 0x7F..FF in clang AVX3 builds, so DIY.
    MakeUnsigned<T> bits;
    memcpy(&bits, &v.raw[i], sizeof(T));
    bits += bits;
    bits >>= 1;  // clear sign bit
    // NaN if all exponent bits are set and the mantissa is not zero.
    ret.bits[i] = Mask128<T, N>::FromBool(bits > ExponentMask<T>());
  }
  return ret;
}

template <typename T, size_t N>
HWY_API Mask128<T, N> IsInf(const Vec128<T, N> v) {
  static_assert(IsFloat<T>(), "Only for float");
  const Simd<T, N, 0> d;
  const RebindToSigned<decltype(d)> di;
  const VFromD<decltype(di)> vi = BitCast(di, v);
  // 'Shift left' to clear the sign bit, check for exponent=max and mantissa=0.
  return RebindMask(d, Eq(Add(vi, vi), Set(di, hwy::MaxExponentTimes2<T>())));
}

// Returns whether normal/subnormal/zero.
template <typename T, size_t N>
HWY_API Mask128<T, N> IsFinite(const Vec128<T, N> v) {
  static_assert(IsFloat<T>(), "Only for float");
  const Simd<T, N, 0> d;
  const RebindToUnsigned<decltype(d)> du;
  const RebindToSigned<decltype(d)> di;  // cheaper than unsigned comparison
  using VI = VFromD<decltype(di)>;
  using VU = VFromD<decltype(du)>;
  const VU vu = BitCast(du, v);
  // 'Shift left' to clear the sign bit, then right so we can compare with the
  // max exponent (cannot compare with MaxExponentTimes2 directly because it is
  // negative and non-negative floats would be greater).
  const VI exp =
      BitCast(di, ShiftRight<hwy::MantissaBits<T>() + 1>(Add(vu, vu)));
  return RebindMask(d, Lt(exp, Set(di, hwy::MaxExponentField<T>())));
}

// ================================================== COMPARE

template <typename T, size_t N>
HWY_API Mask128<T, N> operator==(const Vec128<T, N> a, const Vec128<T, N> b) {
  Mask128<T, N> m;
  for (size_t i = 0; i < N; ++i) {
    m.bits[i] = Mask128<T, N>::FromBool(a.raw[i] == b.raw[i]);
  }
  return m;
}

template <typename T, size_t N>
HWY_API Mask128<T, N> operator!=(const Vec128<T, N> a, const Vec128<T, N> b) {
  Mask128<T, N> m;
  for (size_t i = 0; i < N; ++i) {
    m.bits[i] = Mask128<T, N>::FromBool(a.raw[i] != b.raw[i]);
  }
  return m;
}

template <typename T, size_t N>
HWY_API Mask128<T, N> TestBit(const Vec128<T, N> v, const Vec128<T, N> bit) {
  static_assert(!hwy::IsFloat<T>(), "Only integer vectors supported");
  return (v & bit) == bit;
}

template <typename T, size_t N>
HWY_API Mask128<T, N> operator<(const Vec128<T, N> a, const Vec128<T, N> b) {
  Mask128<T, N> m;
  for (size_t i = 0; i < N; ++i) {
    m.bits[i] = Mask128<T, N>::FromBool(a.raw[i] < b.raw[i]);
  }
  return m;
}
template <typename T, size_t N>
HWY_API Mask128<T, N> operator>(const Vec128<T, N> a, const Vec128<T, N> b) {
  Mask128<T, N> m;
  for (size_t i = 0; i < N; ++i) {
    m.bits[i] = Mask128<T, N>::FromBool(a.raw[i] > b.raw[i]);
  }
  return m;
}

template <typename T, size_t N>
HWY_API Mask128<T, N> operator<=(const Vec128<T, N> a, const Vec128<T, N> b) {
  Mask128<T, N> m;
  for (size_t i = 0; i < N; ++i) {
    m.bits[i] = Mask128<T, N>::FromBool(a.raw[i] <= b.raw[i]);
  }
  return m;
}
template <typename T, size_t N>
HWY_API Mask128<T, N> operator>=(const Vec128<T, N> a, const Vec128<T, N> b) {
  Mask128<T, N> m;
  for (size_t i = 0; i < N; ++i) {
    m.bits[i] = Mask128<T, N>::FromBool(a.raw[i] >= b.raw[i]);
  }
  return m;
}

// ------------------------------ Lt128

// Only makes sense for full vectors of u64.
HWY_API Mask128<uint64_t> Lt128(Simd<uint64_t, 2, 0> /* tag */,
                                Vec128<uint64_t> a, const Vec128<uint64_t> b) {
  const bool lt =
      (a.raw[1] < b.raw[1]) || (a.raw[1] == b.raw[1] && a.raw[0] < b.raw[0]);
  Mask128<uint64_t> ret;
  ret.bits[0] = ret.bits[1] = Mask128<uint64_t>::FromBool(lt);
  return ret;
}

HWY_API Mask128<uint64_t> Lt128Upper(Simd<uint64_t, 2, 0> /* tag */,
                                     Vec128<uint64_t> a,
                                     const Vec128<uint64_t> b) {
  const bool lt = a.raw[1] < b.raw[1];
  Mask128<uint64_t> ret;
  ret.bits[0] = ret.bits[1] = Mask128<uint64_t>::FromBool(lt);
  return ret;
}

// ------------------------------ Eq128

// Only makes sense for full vectors of u64.
HWY_API Mask128<uint64_t> Eq128(Simd<uint64_t, 2, 0> /* tag */,
                                Vec128<uint64_t> a, const Vec128<uint64_t> b) {
  const bool eq = a.raw[1] == b.raw[1] && a.raw[0] == b.raw[0];
  Mask128<uint64_t> ret;
  ret.bits[0] = ret.bits[1] = Mask128<uint64_t>::FromBool(eq);
  return ret;
}

HWY_API Mask128<uint64_t> Eq128Upper(Simd<uint64_t, 2, 0> /* tag */,
                                     Vec128<uint64_t> a,
                                     const Vec128<uint64_t> b) {
  const bool eq = a.raw[1] == b.raw[1];
  Mask128<uint64_t> ret;
  ret.bits[0] = ret.bits[1] = Mask128<uint64_t>::FromBool(eq);
  return ret;
}

// ------------------------------ Min128, Max128 (Lt128)

template <class D, class V = VFromD<D>>
HWY_API V Min128(D d, const V a, const V b) {
  return IfThenElse(Lt128(d, a, b), a, b);
}

template <class D, class V = VFromD<D>>
HWY_API V Max128(D d, const V a, const V b) {
  return IfThenElse(Lt128(d, b, a), a, b);
}

template <class D, class V = VFromD<D>>
HWY_API V Min128Upper(D d, const V a, const V b) {
  return IfThenElse(Lt128Upper(d, a, b), a, b);
}

template <class D, class V = VFromD<D>>
HWY_API V Max128Upper(D d, const V a, const V b) {
  return IfThenElse(Lt128Upper(d, b, a), a, b);
}

// ================================================== MEMORY

// ------------------------------ Load

template <typename T, size_t N>
HWY_API Vec128<T, N> Load(Simd<T, N, 0> /* tag */,
                          const T* HWY_RESTRICT aligned) {
  Vec128<T, N> v;
  CopyBytes<sizeof(T) * N>(aligned, v.raw);
  return v;
}

template <typename T, size_t N>
HWY_API Vec128<T, N> MaskedLoad(Mask128<T, N> m, Simd<T, N, 0> d,
                                const T* HWY_RESTRICT aligned) {
  return IfThenElseZero(m, Load(d, aligned));
}

template <typename T, size_t N>
HWY_API Vec128<T, N> LoadU(Simd<T, N, 0> d, const T* HWY_RESTRICT p) {
  return Load(d, p);
}

// In some use cases, "load single lane" is sufficient; otherwise avoid this.
template <typename T, size_t N>
HWY_API Vec128<T, N> LoadDup128(Simd<T, N, 0> d,
                                const T* HWY_RESTRICT aligned) {
  return Load(d, aligned);
}

// ------------------------------ Store

template <typename T, size_t N>
HWY_API void Store(const Vec128<T, N> v, Simd<T, N, 0> /* tag */,
                   T* HWY_RESTRICT aligned) {
  CopyBytes<sizeof(T) * N>(v.raw, aligned);
}

template <typename T, size_t N>
HWY_API void StoreU(const Vec128<T, N> v, Simd<T, N, 0> d, T* HWY_RESTRICT p) {
  Store(v, d, p);
}

template <typename T, size_t N>
HWY_API void BlendedStore(const Vec128<T, N> v, Mask128<T, N> m,
                          Simd<T, N, 0> /* tag */, T* HWY_RESTRICT p) {
  for (size_t i = 0; i < N; ++i) {
    if (m.bits[i]) p[i] = v.raw[i];
  }
}

// ------------------------------ LoadInterleaved2/3/4

// Per-target flag to prevent generic_ops-inl.h from defining LoadInterleaved2.
// We implement those here because scalar code is likely faster than emulation
// via shuffles.
#ifdef HWY_NATIVE_LOAD_STORE_INTERLEAVED
#undef HWY_NATIVE_LOAD_STORE_INTERLEAVED
#else
#define HWY_NATIVE_LOAD_STORE_INTERLEAVED
#endif

template <typename T, size_t N>
HWY_API void LoadInterleaved2(Simd<T, N, 0> d, const T* HWY_RESTRICT unaligned,
                              Vec128<T, N>& v0, Vec128<T, N>& v1) {
  alignas(16) T buf0[N];
  alignas(16) T buf1[N];
  for (size_t i = 0; i < N; ++i) {
    buf0[i] = *unaligned++;
    buf1[i] = *unaligned++;
  }
  v0 = Load(d, buf0);
  v1 = Load(d, buf1);
}

template <typename T, size_t N>
HWY_API void LoadInterleaved3(Simd<T, N, 0> d, const T* HWY_RESTRICT unaligned,
                              Vec128<T, N>& v0, Vec128<T, N>& v1,
                              Vec128<T, N>& v2) {
  alignas(16) T buf0[N];
  alignas(16) T buf1[N];
  alignas(16) T buf2[N];
  for (size_t i = 0; i < N; ++i) {
    buf0[i] = *unaligned++;
    buf1[i] = *unaligned++;
    buf2[i] = *unaligned++;
  }
  v0 = Load(d, buf0);
  v1 = Load(d, buf1);
  v2 = Load(d, buf2);
}

template <typename T, size_t N>
HWY_API void LoadInterleaved4(Simd<T, N, 0> d, const T* HWY_RESTRICT unaligned,
                              Vec128<T, N>& v0, Vec128<T, N>& v1,
                              Vec128<T, N>& v2, Vec128<T, N>& v3) {
  alignas(16) T buf0[N];
  alignas(16) T buf1[N];
  alignas(16) T buf2[N];
  alignas(16) T buf3[N];
  for (size_t i = 0; i < N; ++i) {
    buf0[i] = *unaligned++;
    buf1[i] = *unaligned++;
    buf2[i] = *unaligned++;
    buf3[i] = *unaligned++;
  }
  v0 = Load(d, buf0);
  v1 = Load(d, buf1);
  v2 = Load(d, buf2);
  v3 = Load(d, buf3);
}

// ------------------------------ StoreInterleaved2/3/4

template <typename T, size_t N>
HWY_API void StoreInterleaved2(const Vec128<T, N> v0, const Vec128<T, N> v1,
                               Simd<T, N, 0> /* tag */,
                               T* HWY_RESTRICT unaligned) {
  for (size_t i = 0; i < N; ++i) {
    *unaligned++ = v0.raw[i];
    *unaligned++ = v1.raw[i];
  }
}

template <typename T, size_t N>
HWY_API void StoreInterleaved3(const Vec128<T, N> v0, const Vec128<T, N> v1,
                               const Vec128<T, N> v2, Simd<T, N, 0> /* tag */,
                               T* HWY_RESTRICT unaligned) {
  for (size_t i = 0; i < N; ++i) {
    *unaligned++ = v0.raw[i];
    *unaligned++ = v1.raw[i];
    *unaligned++ = v2.raw[i];
  }
}

template <typename T, size_t N>
HWY_API void StoreInterleaved4(const Vec128<T, N> v0, const Vec128<T, N> v1,
                               const Vec128<T, N> v2, const Vec128<T, N> v3,
                               Simd<T, N, 0> /* tag */,
                               T* HWY_RESTRICT unaligned) {
  for (size_t i = 0; i < N; ++i) {
    *unaligned++ = v0.raw[i];
    *unaligned++ = v1.raw[i];
    *unaligned++ = v2.raw[i];
    *unaligned++ = v3.raw[i];
  }
}

// ------------------------------ Stream

template <typename T, size_t N>
HWY_API void Stream(const Vec128<T, N> v, Simd<T, N, 0> d,
                    T* HWY_RESTRICT aligned) {
  Store(v, d, aligned);
}

// ------------------------------ Scatter

template <typename T, size_t N, typename Offset>
HWY_API void ScatterOffset(Vec128<T, N> v, Simd<T, N, 0> /* tag */, T* base,
                           const Vec128<Offset, N> offset) {
  static_assert(sizeof(T) == sizeof(Offset), "Must match for portability");
  for (size_t i = 0; i < N; ++i) {
    uint8_t* const base8 = reinterpret_cast<uint8_t*>(base) + offset.raw[i];
    CopyBytes<sizeof(T)>(&v.raw[i], base8);
  }
}

template <typename T, size_t N, typename Index>
HWY_API void ScatterIndex(Vec128<T, N> v, Simd<T, N, 0> /* tag */,
                          T* HWY_RESTRICT base, const Vec128<Index, N> index) {
  static_assert(sizeof(T) == sizeof(Index), "Must match for portability");
  for (size_t i = 0; i < N; ++i) {
    base[index.raw[i]] = v.raw[i];
  }
}

// ------------------------------ Gather

template <typename T, size_t N, typename Offset>
HWY_API Vec128<T, N> GatherOffset(Simd<T, N, 0> /* tag */, const T* base,
                                  const Vec128<Offset, N> offset) {
  static_assert(sizeof(T) == sizeof(Offset), "Must match for portability");
  Vec128<T, N> v;
  for (size_t i = 0; i < N; ++i) {
    const uint8_t* base8 =
        reinterpret_cast<const uint8_t*>(base) + offset.raw[i];
    CopyBytes<sizeof(T)>(base8, &v.raw[i]);
  }
  return v;
}

template <typename T, size_t N, typename Index>
HWY_API Vec128<T, N> GatherIndex(Simd<T, N, 0> /* tag */,
                                 const T* HWY_RESTRICT base,
                                 const Vec128<Index, N> index) {
  static_assert(sizeof(T) == sizeof(Index), "Must match for portability");
  Vec128<T, N> v;
  for (size_t i = 0; i < N; ++i) {
    v.raw[i] = base[index.raw[i]];
  }
  return v;
}

// ================================================== CONVERT

// ConvertTo and DemoteTo with floating-point input and integer output truncate
// (rounding toward zero).

template <typename FromT, typename ToT, size_t N>
HWY_API Vec128<ToT, N> PromoteTo(Simd<ToT, N, 0> /* tag */,
                                 Vec128<FromT, N> from) {
  static_assert(sizeof(ToT) > sizeof(FromT), "Not promoting");
  Vec128<ToT, N> ret;
  for (size_t i = 0; i < N; ++i) {
    // For bits Y > X, floatX->floatY and intX->intY are always representable.
    ret.raw[i] = static_cast<ToT>(from.raw[i]);
  }
  return ret;
}

// MSVC 19.10 cannot deduce the argument type if HWY_IF_FLOAT(FromT) is here,
// so we overload for FromT=double and ToT={float,int32_t}.
template <size_t N>
HWY_API Vec128<float, N> DemoteTo(Simd<float, N, 0> /* tag */,
                                  Vec128<double, N> from) {
  Vec128<float, N> ret;
  for (size_t i = 0; i < N; ++i) {
    // Prevent ubsan errors when converting float to narrower integer/float
    if (std::isinf(from.raw[i]) ||
        std::fabs(from.raw[i]) > static_cast<double>(HighestValue<float>())) {
      ret.raw[i] = std::signbit(from.raw[i]) ? LowestValue<float>()
                                             : HighestValue<float>();
      continue;
    }
    ret.raw[i] = static_cast<float>(from.raw[i]);
  }
  return ret;
}
template <size_t N>
HWY_API Vec128<int32_t, N> DemoteTo(Simd<int32_t, N, 0> /* tag */,
                                    Vec128<double, N> from) {
  Vec128<int32_t, N> ret;
  for (size_t i = 0; i < N; ++i) {
    // Prevent ubsan errors when converting int32_t to narrower integer/int32_t
    if (std::isinf(from.raw[i]) ||
        std::fabs(from.raw[i]) > static_cast<double>(HighestValue<int32_t>())) {
      ret.raw[i] = std::signbit(from.raw[i]) ? LowestValue<int32_t>()
                                             : HighestValue<int32_t>();
      continue;
    }
    ret.raw[i] = static_cast<int32_t>(from.raw[i]);
  }
  return ret;
}

template <typename FromT, typename ToT, size_t N>
HWY_API Vec128<ToT, N> DemoteTo(Simd<ToT, N, 0> /* tag */,
                                Vec128<FromT, N> from) {
  static_assert(!IsFloat<FromT>(), "FromT=double are handled above");
  static_assert(sizeof(ToT) < sizeof(FromT), "Not demoting");

  Vec128<ToT, N> ret;
  for (size_t i = 0; i < N; ++i) {
    // Int to int: choose closest value in ToT to `from` (avoids UB)
    from.raw[i] =
        HWY_MIN(HWY_MAX(LimitsMin<ToT>(), from.raw[i]), LimitsMax<ToT>());
    ret.raw[i] = static_cast<ToT>(from.raw[i]);
  }
  return ret;
}

template <size_t N>
HWY_API Vec128<bfloat16_t, 2 * N> ReorderDemote2To(
    Simd<bfloat16_t, 2 * N, 0> dbf16, Vec128<float, N> a, Vec128<float, N> b) {
  const Repartition<uint32_t, decltype(dbf16)> du32;
  const Vec128<uint32_t, N> b_in_lower = ShiftRight<16>(BitCast(du32, b));
  // Avoid OddEven - we want the upper half of `a` even on big-endian systems.
  const Vec128<uint32_t, N> a_mask = Set(du32, 0xFFFF0000);
  return BitCast(dbf16, IfVecThenElse(a_mask, BitCast(du32, a), b_in_lower));
}

namespace detail {

HWY_INLINE void StoreU16ToF16(const uint16_t val,
                              hwy::float16_t* HWY_RESTRICT to) {
  CopyBytes<2>(&val, to);
}

HWY_INLINE uint16_t U16FromF16(const hwy::float16_t* HWY_RESTRICT from) {
  uint16_t bits16;
  CopyBytes<2>(from, &bits16);
  return bits16;
}

}  // namespace detail

template <size_t N>
HWY_API Vec128<float, N> PromoteTo(Simd<float, N, 0> /* tag */,
                                   const Vec128<float16_t, N> v) {
  Vec128<float, N> ret;
  for (size_t i = 0; i < N; ++i) {
    const uint16_t bits16 = detail::U16FromF16(&v.raw[i]);
    const uint32_t sign = static_cast<uint32_t>(bits16 >> 15);
    const uint32_t biased_exp = (bits16 >> 10) & 0x1F;
    const uint32_t mantissa = bits16 & 0x3FF;

    // Subnormal or zero
    if (biased_exp == 0) {
      const float subnormal =
          (1.0f / 16384) * (static_cast<float>(mantissa) * (1.0f / 1024));
      ret.raw[i] = sign ? -subnormal : subnormal;
      continue;
    }

    // Normalized: convert the representation directly (faster than
    // ldexp/tables).
    const uint32_t biased_exp32 = biased_exp + (127 - 15);
    const uint32_t mantissa32 = mantissa << (23 - 10);
    const uint32_t bits32 = (sign << 31) | (biased_exp32 << 23) | mantissa32;
    CopyBytes<4>(&bits32, &ret.raw[i]);
  }
  return ret;
}

template <size_t N>
HWY_API Vec128<float, N> PromoteTo(Simd<float, N, 0> /* tag */,
                                   const Vec128<bfloat16_t, N> v) {
  Vec128<float, N> ret;
  for (size_t i = 0; i < N; ++i) {
    ret.raw[i] = F32FromBF16(v.raw[i]);
  }
  return ret;
}

template <size_t N>
HWY_API Vec128<float16_t, N> DemoteTo(Simd<float16_t, N, 0> /* tag */,
                                      const Vec128<float, N> v) {
  Vec128<float16_t, N> ret;
  for (size_t i = 0; i < N; ++i) {
    uint32_t bits32;
    CopyBytes<4>(&v.raw[i], &bits32);
    const uint32_t sign = bits32 >> 31;
    const uint32_t biased_exp32 = (bits32 >> 23) & 0xFF;
    const uint32_t mantissa32 = bits32 & 0x7FFFFF;

    const int32_t exp = HWY_MIN(static_cast<int32_t>(biased_exp32) - 127, 15);

    // Tiny or zero => zero.
    if (exp < -24) {
      ZeroBytes<sizeof(uint16_t)>(&ret.raw[i]);
      continue;
    }

    uint32_t biased_exp16, mantissa16;

    // exp = [-24, -15] => subnormal
    if (exp < -14) {
      biased_exp16 = 0;
      const uint32_t sub_exp = static_cast<uint32_t>(-14 - exp);
      HWY_DASSERT(1 <= sub_exp && sub_exp < 11);
      mantissa16 = static_cast<uint32_t>((1u << (10 - sub_exp)) +
                                         (mantissa32 >> (13 + sub_exp)));
    } else {
      // exp = [-14, 15]
      biased_exp16 = static_cast<uint32_t>(exp + 15);
      HWY_DASSERT(1 <= biased_exp16 && biased_exp16 < 31);
      mantissa16 = mantissa32 >> 13;
    }

    HWY_DASSERT(mantissa16 < 1024);
    const uint32_t bits16 = (sign << 15) | (biased_exp16 << 10) | mantissa16;
    HWY_DASSERT(bits16 < 0x10000);
    const uint16_t narrowed = static_cast<uint16_t>(bits16);  // big-endian safe
    detail::StoreU16ToF16(narrowed, &ret.raw[i]);
  }
  return ret;
}

template <size_t N>
HWY_API Vec128<bfloat16_t, N> DemoteTo(Simd<bfloat16_t, N, 0> /* tag */,
                                       const Vec128<float, N> v) {
  Vec128<bfloat16_t, N> ret;
  for (size_t i = 0; i < N; ++i) {
    ret.raw[i] = BF16FromF32(v.raw[i]);
  }
  return ret;
}

// Tag dispatch instead of SFINAE for MSVC 2017 compatibility
namespace detail {

template <typename FromT, typename ToT, size_t N>
HWY_API Vec128<ToT, N> ConvertTo(hwy::FloatTag /*tag*/,
                                 Simd<ToT, N, 0> /* tag */,
                                 Vec128<FromT, N> from) {
  static_assert(sizeof(ToT) == sizeof(FromT), "Should have same size");
  Vec128<ToT, N> ret;
  for (size_t i = 0; i < N; ++i) {
    // float## -> int##: return closest representable value. We cannot exactly
    // represent LimitsMax<ToT> in FromT, so use double.
    const double f = static_cast<double>(from.raw[i]);
    if (std::isinf(from.raw[i]) ||
        std::fabs(f) > static_cast<double>(LimitsMax<ToT>())) {
      ret.raw[i] =
          std::signbit(from.raw[i]) ? LimitsMin<ToT>() : LimitsMax<ToT>();
      continue;
    }
    ret.raw[i] = static_cast<ToT>(from.raw[i]);
  }
  return ret;
}

template <typename FromT, typename ToT, size_t N>
HWY_API Vec128<ToT, N> ConvertTo(hwy::NonFloatTag /*tag*/,
                                 Simd<ToT, N, 0> /* tag */,
                                 Vec128<FromT, N> from) {
  static_assert(sizeof(ToT) == sizeof(FromT), "Should have same size");
  Vec128<ToT, N> ret;
  for (size_t i = 0; i < N; ++i) {
    // int## -> float##: no check needed
    ret.raw[i] = static_cast<ToT>(from.raw[i]);
  }
  return ret;
}

}  // namespace detail

template <typename FromT, typename ToT, size_t N>
HWY_API Vec128<ToT, N> ConvertTo(Simd<ToT, N, 0> d, Vec128<FromT, N> from) {
  return detail::ConvertTo(hwy::IsFloatTag<FromT>(), d, from);
}

template <size_t N>
HWY_API Vec128<uint8_t, N> U8FromU32(const Vec128<uint32_t, N> v) {
  return DemoteTo(Simd<uint8_t, N, 0>(), v);
}

// ------------------------------ Truncations

template <size_t N>
HWY_API Vec128<uint8_t, N> TruncateTo(Simd<uint8_t, N, 0> /* tag */,
                                      const Vec128<uint64_t, N> v) {
  Vec128<uint8_t, N> ret;
  for (size_t i = 0; i < N; ++i) {
    ret.raw[i] = static_cast<uint8_t>(v.raw[i] & 0xFF);
  }
  return ret;
}

template <size_t N>
HWY_API Vec128<uint16_t, N> TruncateTo(Simd<uint16_t, N, 0> /* tag */,
                                       const Vec128<uint64_t, N> v) {
  Vec128<uint16_t, N> ret;
  for (size_t i = 0; i < N; ++i) {
    ret.raw[i] = static_cast<uint16_t>(v.raw[i] & 0xFFFF);
  }
  return ret;
}

template <size_t N>
HWY_API Vec128<uint32_t, N> TruncateTo(Simd<uint32_t, N, 0> /* tag */,
                                       const Vec128<uint64_t, N> v) {
  Vec128<uint32_t, N> ret;
  for (size_t i = 0; i < N; ++i) {
    ret.raw[i] = static_cast<uint32_t>(v.raw[i] & 0xFFFFFFFFu);
  }
  return ret;
}

template <size_t N>
HWY_API Vec128<uint8_t, N> TruncateTo(Simd<uint8_t, N, 0> /* tag */,
                                      const Vec128<uint32_t, N> v) {
  Vec128<uint8_t, N> ret;
  for (size_t i = 0; i < N; ++i) {
    ret.raw[i] = static_cast<uint8_t>(v.raw[i] & 0xFF);
  }
  return ret;
}

template <size_t N>
HWY_API Vec128<uint16_t, N> TruncateTo(Simd<uint16_t, N, 0> /* tag */,
                                       const Vec128<uint32_t, N> v) {
  Vec128<uint16_t, N> ret;
  for (size_t i = 0; i < N; ++i) {
    ret.raw[i] = static_cast<uint16_t>(v.raw[i] & 0xFFFF);
  }
  return ret;
}

template <size_t N>
HWY_API Vec128<uint8_t, N> TruncateTo(Simd<uint8_t, N, 0> /* tag */,
                                      const Vec128<uint16_t, N> v) {
  Vec128<uint8_t, N> ret;
  for (size_t i = 0; i < N; ++i) {
    ret.raw[i] = static_cast<uint8_t>(v.raw[i] & 0xFF);
  }
  return ret;
}

// ================================================== COMBINE

template <typename T, size_t N>
HWY_API Vec128<T, N / 2> LowerHalf(Vec128<T, N> v) {
  Vec128<T, N / 2> ret;
  CopyBytes<N / 2 * sizeof(T)>(v.raw, ret.raw);
  return ret;
}

template <typename T, size_t N>
HWY_API Vec128<T, N / 2> LowerHalf(Simd<T, N / 2, 0> /* tag */,
                                   Vec128<T, N> v) {
  return LowerHalf(v);
}

template <typename T, size_t N>
HWY_API Vec128<T, N / 2> UpperHalf(Simd<T, N / 2, 0> /* tag */,
                                   Vec128<T, N> v) {
  Vec128<T, N / 2> ret;
  CopyBytes<N / 2 * sizeof(T)>(&v.raw[N / 2], ret.raw);
  return ret;
}

template <typename T, size_t N>
HWY_API Vec128<T, N> ZeroExtendVector(Simd<T, N, 0> /* tag */,
                                      Vec128<T, N / 2> v) {
  Vec128<T, N> ret;
  CopyBytes<N / 2 * sizeof(T)>(v.raw, ret.raw);
  return ret;
}

template <typename T, size_t N>
HWY_API Vec128<T, N> Combine(Simd<T, N, 0> /* tag */, Vec128<T, N / 2> hi_half,
                             Vec128<T, N / 2> lo_half) {
  Vec128<T, N> ret;
  CopyBytes<N / 2 * sizeof(T)>(lo_half.raw, &ret.raw[0]);
  CopyBytes<N / 2 * sizeof(T)>(hi_half.raw, &ret.raw[N / 2]);
  return ret;
}

template <typename T, size_t N>
HWY_API Vec128<T, N> ConcatLowerLower(Simd<T, N, 0> /* tag */, Vec128<T, N> hi,
                                      Vec128<T, N> lo) {
  Vec128<T, N> ret;
  CopyBytes<N / 2 * sizeof(T)>(lo.raw, &ret.raw[0]);
  CopyBytes<N / 2 * sizeof(T)>(hi.raw, &ret.raw[N / 2]);
  return ret;
}

template <typename T, size_t N>
HWY_API Vec128<T, N> ConcatUpperUpper(Simd<T, N, 0> /* tag */, Vec128<T, N> hi,
                                      Vec128<T, N> lo) {
  Vec128<T, N> ret;
  CopyBytes<N / 2 * sizeof(T)>(&lo.raw[N / 2], &ret.raw[0]);
  CopyBytes<N / 2 * sizeof(T)>(&hi.raw[N / 2], &ret.raw[N / 2]);
  return ret;
}

template <typename T, size_t N>
HWY_API Vec128<T, N> ConcatLowerUpper(Simd<T, N, 0> /* tag */,
                                      const Vec128<T, N> hi,
                                      const Vec128<T, N> lo) {
  Vec128<T, N> ret;
  CopyBytes<N / 2 * sizeof(T)>(&lo.raw[N / 2], &ret.raw[0]);
  CopyBytes<N / 2 * sizeof(T)>(hi.raw, &ret.raw[N / 2]);
  return ret;
}

template <typename T, size_t N>
HWY_API Vec128<T, N> ConcatUpperLower(Simd<T, N, 0> /* tag */, Vec128<T, N> hi,
                                      Vec128<T, N> lo) {
  Vec128<T, N> ret;
  CopyBytes<N / 2 * sizeof(T)>(lo.raw, &ret.raw[0]);
  CopyBytes<N / 2 * sizeof(T)>(&hi.raw[N / 2], &ret.raw[N / 2]);
  return ret;
}

template <typename T, size_t N>
HWY_API Vec128<T, N> ConcatEven(Simd<T, N, 0> /* tag */, Vec128<T, N> hi,
                                Vec128<T, N> lo) {
  Vec128<T, N> ret;
  for (size_t i = 0; i < N / 2; ++i) {
    ret.raw[i] = lo.raw[2 * i];
  }
  for (size_t i = 0; i < N / 2; ++i) {
    ret.raw[N / 2 + i] = hi.raw[2 * i];
  }
  return ret;
}

template <typename T, size_t N>
HWY_API Vec128<T, N> ConcatOdd(Simd<T, N, 0> /* tag */, Vec128<T, N> hi,
                               Vec128<T, N> lo) {
  Vec128<T, N> ret;
  for (size_t i = 0; i < N / 2; ++i) {
    ret.raw[i] = lo.raw[2 * i + 1];
  }
  for (size_t i = 0; i < N / 2; ++i) {
    ret.raw[N / 2 + i] = hi.raw[2 * i + 1];
  }
  return ret;
}

// ------------------------------ CombineShiftRightBytes

template <int kBytes, typename T, size_t N, class V = Vec128<T, N>>
HWY_API V CombineShiftRightBytes(Simd<T, N, 0> /* tag */, V hi, V lo) {
  V ret;
  const uint8_t* HWY_RESTRICT lo8 =
      reinterpret_cast<const uint8_t * HWY_RESTRICT>(lo.raw);
  uint8_t* HWY_RESTRICT ret8 =
      reinterpret_cast<uint8_t * HWY_RESTRICT>(ret.raw);
  CopyBytes<sizeof(T) * N - kBytes>(lo8 + kBytes, ret8);
  CopyBytes<kBytes>(hi.raw, ret8 + sizeof(T) * N - kBytes);
  return ret;
}

// ------------------------------ ShiftLeftBytes

template <int kBytes, typename T, size_t N>
HWY_API Vec128<T, N> ShiftLeftBytes(Simd<T, N, 0> /* tag */, Vec128<T, N> v) {
  static_assert(0 <= kBytes && kBytes <= 16, "Invalid kBytes");
  Vec128<T, N> ret;
  uint8_t* HWY_RESTRICT ret8 =
      reinterpret_cast<uint8_t * HWY_RESTRICT>(ret.raw);
  ZeroBytes<kBytes>(ret8);
  CopyBytes<sizeof(T) * N - kBytes>(v.raw, ret8 + kBytes);
  return ret;
}

template <int kBytes, typename T, size_t N>
HWY_API Vec128<T, N> ShiftLeftBytes(const Vec128<T, N> v) {
  return ShiftLeftBytes<kBytes>(DFromV<decltype(v)>(), v);
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
template <int kBytes, typename T, size_t N>
HWY_API Vec128<T, N> ShiftRightBytes(Simd<T, N, 0> /* tag */, Vec128<T, N> v) {
  static_assert(0 <= kBytes && kBytes <= 16, "Invalid kBytes");
  Vec128<T, N> ret;
  const uint8_t* HWY_RESTRICT v8 =
      reinterpret_cast<const uint8_t * HWY_RESTRICT>(v.raw);
  uint8_t* HWY_RESTRICT ret8 =
      reinterpret_cast<uint8_t * HWY_RESTRICT>(ret.raw);
  CopyBytes<sizeof(T) * N - kBytes>(v8 + kBytes, ret8);
  ZeroBytes<kBytes>(ret8 + sizeof(T) * N - kBytes);
  return ret;
}

// ------------------------------ ShiftRightLanes
template <int kLanes, typename T, size_t N>
HWY_API Vec128<T, N> ShiftRightLanes(Simd<T, N, 0> d, const Vec128<T, N> v) {
  const Repartition<uint8_t, decltype(d)> d8;
  return BitCast(d, ShiftRightBytes<kLanes * sizeof(T)>(d8, BitCast(d8, v)));
}

// ================================================== SWIZZLE

template <typename T, size_t N>
HWY_API T GetLane(const Vec128<T, N> v) {
  return v.raw[0];
}

template <typename T, size_t N>
HWY_API Vec128<T, N> InsertLane(Vec128<T, N> v, size_t i, T t) {
  v.raw[i] = t;
  return v;
}

template <typename T, size_t N>
HWY_API T ExtractLane(const Vec128<T, N> v, size_t i) {
  return v.raw[i];
}

template <typename T, size_t N>
HWY_API Vec128<T, N> DupEven(Vec128<T, N> v) {
  for (size_t i = 0; i < N; i += 2) {
    v.raw[i + 1] = v.raw[i];
  }
  return v;
}

template <typename T, size_t N>
HWY_API Vec128<T, N> DupOdd(Vec128<T, N> v) {
  for (size_t i = 0; i < N; i += 2) {
    v.raw[i] = v.raw[i + 1];
  }
  return v;
}

template <typename T, size_t N>
HWY_API Vec128<T, N> OddEven(Vec128<T, N> odd, Vec128<T, N> even) {
  for (size_t i = 0; i < N; i += 2) {
    odd.raw[i] = even.raw[i];
  }
  return odd;
}

template <typename T, size_t N>
HWY_API Vec128<T, N> OddEvenBlocks(Vec128<T, N> /* odd */, Vec128<T, N> even) {
  return even;
}

// ------------------------------ SwapAdjacentBlocks

template <typename T, size_t N>
HWY_API Vec128<T, N> SwapAdjacentBlocks(Vec128<T, N> v) {
  return v;
}

// ------------------------------ TableLookupLanes

// Returned by SetTableIndices for use by TableLookupLanes.
template <typename T, size_t N>
struct Indices128 {
  MakeSigned<T> raw[N];
};

template <typename T, size_t N, typename TI>
HWY_API Indices128<T, N> IndicesFromVec(Simd<T, N, 0>, Vec128<TI, N> vec) {
  static_assert(sizeof(T) == sizeof(TI), "Index size must match lane size");
  Indices128<T, N> ret;
  CopyBytes<N * sizeof(T)>(vec.raw, ret.raw);
  return ret;
}

template <typename T, size_t N, typename TI>
HWY_API Indices128<T, N> SetTableIndices(Simd<T, N, 0> d, const TI* idx) {
  return IndicesFromVec(d, LoadU(Simd<TI, N, 0>(), idx));
}

template <typename T, size_t N>
HWY_API Vec128<T, N> TableLookupLanes(const Vec128<T, N> v,
                                      const Indices128<T, N> idx) {
  Vec128<T, N> ret;
  for (size_t i = 0; i < N; ++i) {
    ret.raw[i] = v.raw[idx.raw[i]];
  }
  return ret;
}

// ------------------------------ ReverseBlocks

// Single block: no change
template <typename T, size_t N>
HWY_API Vec128<T, N> ReverseBlocks(Simd<T, N, 0> /* tag */,
                                   const Vec128<T, N> v) {
  return v;
}

// ------------------------------ Reverse

template <typename T, size_t N>
HWY_API Vec128<T, N> Reverse(Simd<T, N, 0> /* tag */, const Vec128<T, N> v) {
  Vec128<T, N> ret;
  for (size_t i = 0; i < N; ++i) {
    ret.raw[i] = v.raw[N - 1 - i];
  }
  return ret;
}

template <typename T, size_t N>
HWY_API Vec128<T, N> Reverse2(Simd<T, N, 0> /* tag */, const Vec128<T, N> v) {
  Vec128<T, N> ret;
  for (size_t i = 0; i < N; i += 2) {
    ret.raw[i + 0] = v.raw[i + 1];
    ret.raw[i + 1] = v.raw[i + 0];
  }
  return ret;
}

template <typename T, size_t N>
HWY_API Vec128<T, N> Reverse4(Simd<T, N, 0> /* tag */, const Vec128<T, N> v) {
  Vec128<T, N> ret;
  for (size_t i = 0; i < N; i += 4) {
    ret.raw[i + 0] = v.raw[i + 3];
    ret.raw[i + 1] = v.raw[i + 2];
    ret.raw[i + 2] = v.raw[i + 1];
    ret.raw[i + 3] = v.raw[i + 0];
  }
  return ret;
}

template <typename T, size_t N>
HWY_API Vec128<T, N> Reverse8(Simd<T, N, 0> /* tag */, const Vec128<T, N> v) {
  Vec128<T, N> ret;
  for (size_t i = 0; i < N; i += 8) {
    ret.raw[i + 0] = v.raw[i + 7];
    ret.raw[i + 1] = v.raw[i + 6];
    ret.raw[i + 2] = v.raw[i + 5];
    ret.raw[i + 3] = v.raw[i + 4];
    ret.raw[i + 4] = v.raw[i + 3];
    ret.raw[i + 5] = v.raw[i + 2];
    ret.raw[i + 6] = v.raw[i + 1];
    ret.raw[i + 7] = v.raw[i + 0];
  }
  return ret;
}

// ================================================== BLOCKWISE

// ------------------------------ Shuffle*

// Swap 32-bit halves in 64-bit halves.
template <typename T, size_t N>
HWY_API Vec128<T, N> Shuffle2301(const Vec128<T, N> v) {
  static_assert(sizeof(T) == 4, "Only for 32-bit");
  static_assert(N == 2 || N == 4, "Does not make sense for N=1");
  return Reverse2(DFromV<decltype(v)>(), v);
}

// Swap 64-bit halves
template <typename T>
HWY_API Vec128<T> Shuffle1032(const Vec128<T> v) {
  static_assert(sizeof(T) == 4, "Only for 32-bit");
  Vec128<T> ret;
  ret.raw[3] = v.raw[1];
  ret.raw[2] = v.raw[0];
  ret.raw[1] = v.raw[3];
  ret.raw[0] = v.raw[2];
  return ret;
}
template <typename T>
HWY_API Vec128<T> Shuffle01(const Vec128<T> v) {
  static_assert(sizeof(T) == 8, "Only for 64-bit");
  return Reverse2(DFromV<decltype(v)>(), v);
}

// Rotate right 32 bits
template <typename T>
HWY_API Vec128<T> Shuffle0321(const Vec128<T> v) {
  Vec128<T> ret;
  ret.raw[3] = v.raw[0];
  ret.raw[2] = v.raw[3];
  ret.raw[1] = v.raw[2];
  ret.raw[0] = v.raw[1];
  return ret;
}

// Rotate left 32 bits
template <typename T>
HWY_API Vec128<T> Shuffle2103(const Vec128<T> v) {
  Vec128<T> ret;
  ret.raw[3] = v.raw[2];
  ret.raw[2] = v.raw[1];
  ret.raw[1] = v.raw[0];
  ret.raw[0] = v.raw[3];
  return ret;
}

template <typename T>
HWY_API Vec128<T> Shuffle0123(const Vec128<T> v) {
  return Reverse4(DFromV<decltype(v)>(), v);
}

// ------------------------------ Broadcast/splat any lane

template <int kLane, typename T, size_t N>
HWY_API Vec128<T, N> Broadcast(Vec128<T, N> v) {
  for (size_t i = 0; i < N; ++i) {
    v.raw[i] = v.raw[kLane];
  }
  return v;
}

// ------------------------------ TableLookupBytes, TableLookupBytesOr0

template <typename T, size_t N, typename TI, size_t NI>
HWY_API Vec128<TI, NI> TableLookupBytes(const Vec128<T, N> v,
                                        const Vec128<TI, NI> indices) {
  const uint8_t* HWY_RESTRICT v_bytes =
      reinterpret_cast<const uint8_t * HWY_RESTRICT>(v.raw);
  const uint8_t* HWY_RESTRICT idx_bytes =
      reinterpret_cast<const uint8_t*>(indices.raw);
  Vec128<TI, NI> ret;
  uint8_t* HWY_RESTRICT ret_bytes =
      reinterpret_cast<uint8_t * HWY_RESTRICT>(ret.raw);
  for (size_t i = 0; i < NI * sizeof(TI); ++i) {
    const size_t idx = idx_bytes[i];
    // Avoid out of bounds reads.
    ret_bytes[i] = idx < sizeof(T) * N ? v_bytes[idx] : 0;
  }
  return ret;
}

template <typename T, size_t N, typename TI, size_t NI>
HWY_API Vec128<TI, NI> TableLookupBytesOr0(const Vec128<T, N> v,
                                           const Vec128<TI, NI> indices) {
  // Same as TableLookupBytes, which already returns 0 if out of bounds.
  return TableLookupBytes(v, indices);
}

// ------------------------------ InterleaveLower/InterleaveUpper

template <typename T, size_t N>
HWY_API Vec128<T, N> InterleaveLower(const Vec128<T, N> a,
                                     const Vec128<T, N> b) {
  Vec128<T, N> ret;
  for (size_t i = 0; i < N / 2; ++i) {
    ret.raw[2 * i + 0] = a.raw[i];
    ret.raw[2 * i + 1] = b.raw[i];
  }
  return ret;
}

// Additional overload for the optional tag (also for 256/512).
template <class V>
HWY_API V InterleaveLower(DFromV<V> /* tag */, V a, V b) {
  return InterleaveLower(a, b);
}

template <typename T, size_t N>
HWY_API Vec128<T, N> InterleaveUpper(Simd<T, N, 0> /* tag */,
                                     const Vec128<T, N> a,
                                     const Vec128<T, N> b) {
  Vec128<T, N> ret;
  for (size_t i = 0; i < N / 2; ++i) {
    ret.raw[2 * i + 0] = a.raw[N / 2 + i];
    ret.raw[2 * i + 1] = b.raw[N / 2 + i];
  }
  return ret;
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

// ================================================== MASK

template <typename T, size_t N>
HWY_API bool AllFalse(Simd<T, N, 0> /* tag */, const Mask128<T, N> mask) {
  typename Mask128<T, N>::Raw or_sum = 0;
  for (size_t i = 0; i < N; ++i) {
    or_sum |= mask.bits[i];
  }
  return or_sum == 0;
}

template <typename T, size_t N>
HWY_API bool AllTrue(Simd<T, N, 0> /* tag */, const Mask128<T, N> mask) {
  using Bits = typename Mask128<T, N>::Raw;
  constexpr Bits kAll = static_cast<Bits>(~Bits{0});
  Bits and_sum = kAll;
  for (size_t i = 0; i < N; ++i) {
    and_sum &= mask.bits[i];
  }
  return and_sum == kAll;
}

// `p` points to at least 8 readable bytes, not all of which need be valid.
template <typename T, size_t N>
HWY_API Mask128<T, N> LoadMaskBits(Simd<T, N, 0> /* tag */,
                                   const uint8_t* HWY_RESTRICT bits) {
  Mask128<T, N> m;
  for (size_t i = 0; i < N; ++i) {
    const size_t bit = size_t{1} << (i & 7);
    const size_t idx_byte = i >> 3;
    m.bits[i] = Mask128<T, N>::FromBool((bits[idx_byte] & bit) != 0);
  }
  return m;
}

// `p` points to at least 8 writable bytes.
template <typename T, size_t N>
HWY_API size_t StoreMaskBits(Simd<T, N, 0> /* tag */, const Mask128<T, N> mask,
                             uint8_t* bits) {
  bits[0] = 0;
  if (N > 8) bits[1] = 0;  // N <= 16, so max two bytes
  for (size_t i = 0; i < N; ++i) {
    const size_t bit = size_t{1} << (i & 7);
    const size_t idx_byte = i >> 3;
    if (mask.bits[i]) {
      bits[idx_byte] = static_cast<uint8_t>(bits[idx_byte] | bit);
    }
  }
  return N > 8 ? 2 : 1;
}

template <typename T, size_t N>
HWY_API size_t CountTrue(Simd<T, N, 0> /* tag */, const Mask128<T, N> mask) {
  size_t count = 0;
  for (size_t i = 0; i < N; ++i) {
    count += mask.bits[i] != 0;
  }
  return count;
}

template <typename T, size_t N>
HWY_API intptr_t FindFirstTrue(Simd<T, N, 0> /* tag */,
                               const Mask128<T, N> mask) {
  for (size_t i = 0; i < N; ++i) {
    if (mask.bits[i] != 0) return static_cast<intptr_t>(i);
  }
  return intptr_t{-1};
}

// ------------------------------ Compress

template <typename T>
struct CompressIsPartition {
  enum { value = 1 };
};

template <typename T, size_t N>
HWY_API Vec128<T, N> Compress(Vec128<T, N> v, const Mask128<T, N> mask) {
  size_t count = 0;
  Vec128<T, N> ret;
  for (size_t i = 0; i < N; ++i) {
    if (mask.bits[i]) {
      ret.raw[count++] = v.raw[i];
    }
  }
  for (size_t i = 0; i < N; ++i) {
    if (!mask.bits[i]) {
      ret.raw[count++] = v.raw[i];
    }
  }
  HWY_DASSERT(count == N);
  return ret;
}

// ------------------------------ CompressNot
template <typename T, size_t N>
HWY_API Vec128<T, N> CompressNot(Vec128<T, N> v, const Mask128<T, N> mask) {
  size_t count = 0;
  Vec128<T, N> ret;
  for (size_t i = 0; i < N; ++i) {
    if (!mask.bits[i]) {
      ret.raw[count++] = v.raw[i];
    }
  }
  for (size_t i = 0; i < N; ++i) {
    if (mask.bits[i]) {
      ret.raw[count++] = v.raw[i];
    }
  }
  HWY_DASSERT(count == N);
  return ret;
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
  return Compress(v, LoadMaskBits(Simd<T, N, 0>(), bits));
}

// ------------------------------ CompressStore
template <typename T, size_t N>
HWY_API size_t CompressStore(Vec128<T, N> v, const Mask128<T, N> mask,
                             Simd<T, N, 0> /* tag */,
                             T* HWY_RESTRICT unaligned) {
  size_t count = 0;
  for (size_t i = 0; i < N; ++i) {
    if (mask.bits[i]) {
      unaligned[count++] = v.raw[i];
    }
  }
  return count;
}

// ------------------------------ CompressBlendedStore
template <typename T, size_t N>
HWY_API size_t CompressBlendedStore(Vec128<T, N> v, const Mask128<T, N> mask,
                                    Simd<T, N, 0> d,
                                    T* HWY_RESTRICT unaligned) {
  return CompressStore(v, mask, d, unaligned);
}

// ------------------------------ CompressBitsStore
template <typename T, size_t N>
HWY_API size_t CompressBitsStore(Vec128<T, N> v,
                                 const uint8_t* HWY_RESTRICT bits,
                                 Simd<T, N, 0> d, T* HWY_RESTRICT unaligned) {
  const Mask128<T, N> mask = LoadMaskBits(d, bits);
  StoreU(Compress(v, mask), d, unaligned);
  return CountTrue(d, mask);
}

// ------------------------------ ReorderWidenMulAccumulate (MulAdd, ZipLower)
template <size_t N>
HWY_API Vec128<float, N> ReorderWidenMulAccumulate(Simd<float, N, 0> df32,
                                                   Vec128<bfloat16_t, 2 * N> a,
                                                   Vec128<bfloat16_t, 2 * N> b,
                                                   const Vec128<float, N> sum0,
                                                   Vec128<float, N>& sum1) {
  const Rebind<bfloat16_t, decltype(df32)> dbf16;
  // Avoid ZipLower/Upper so this also works on big-endian systems.
  const Vec128<float, N> a0 = PromoteTo(df32, LowerHalf(dbf16, a));
  const Vec128<float, N> a1 = PromoteTo(df32, UpperHalf(dbf16, a));
  const Vec128<float, N> b0 = PromoteTo(df32, LowerHalf(dbf16, b));
  const Vec128<float, N> b1 = PromoteTo(df32, UpperHalf(dbf16, b));
  sum1 = MulAdd(BitCast(df32, a1), BitCast(df32, b1), sum1);
  return MulAdd(BitCast(df32, a0), BitCast(df32, b0), sum0);
}

// ================================================== REDUCTIONS

template <typename T, size_t N>
HWY_API Vec128<T, N> SumOfLanes(Simd<T, N, 0> d, const Vec128<T, N> v) {
  T sum = T{0};
  for (size_t i = 0; i < N; ++i) {
    sum += v.raw[i];
  }
  return Set(d, sum);
}
template <typename T, size_t N>
HWY_API Vec128<T, N> MinOfLanes(Simd<T, N, 0> d, const Vec128<T, N> v) {
  T min = HighestValue<T>();
  for (size_t i = 0; i < N; ++i) {
    min = HWY_MIN(min, v.raw[i]);
  }
  return Set(d, min);
}
template <typename T, size_t N>
HWY_API Vec128<T, N> MaxOfLanes(Simd<T, N, 0> d, const Vec128<T, N> v) {
  T max = LowestValue<T>();
  for (size_t i = 0; i < N; ++i) {
    max = HWY_MAX(max, v.raw[i]);
  }
  return Set(d, max);
}

// ================================================== OPS WITH DEPENDENCIES

// ------------------------------ MulEven/Odd 64x64 (UpperHalf)

HWY_INLINE Vec128<uint64_t> MulEven(const Vec128<uint64_t> a,
                                    const Vec128<uint64_t> b) {
  alignas(16) uint64_t mul[2];
  mul[0] = Mul128(GetLane(a), GetLane(b), &mul[1]);
  return Load(Full128<uint64_t>(), mul);
}

HWY_INLINE Vec128<uint64_t> MulOdd(const Vec128<uint64_t> a,
                                   const Vec128<uint64_t> b) {
  alignas(16) uint64_t mul[2];
  const Half<Full128<uint64_t>> d2;
  mul[0] =
      Mul128(GetLane(UpperHalf(d2, a)), GetLane(UpperHalf(d2, b)), &mul[1]);
  return Load(Full128<uint64_t>(), mul);
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
