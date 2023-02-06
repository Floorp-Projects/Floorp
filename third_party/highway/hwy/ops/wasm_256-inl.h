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

// For half-width vectors. Already includes base.h and shared-inl.h.
#include "hwy/ops/wasm_128-inl.h"

HWY_BEFORE_NAMESPACE();
namespace hwy {
namespace HWY_NAMESPACE {

template <typename T>
class Vec256 {
 public:
  using PrivateT = T;                                  // only for DFromV
  static constexpr size_t kPrivateN = 32 / sizeof(T);  // only for DFromV

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
}

// ------------------------------ Zero

template <typename T>
HWY_API Vec256<T> Zero(Full256<T> d) {
  const Half<decltype(d)> dh;
  Vec256<T> ret;
  ret.v0 = ret.v1 = Zero(dh);
  return ret;
}

template <class D>
using VFromD = decltype(Zero(D()));

// ------------------------------ Set

// Returns a vector/part with all lanes set to "t".
template <typename T, typename T2>
HWY_API Vec256<T> Set(Full256<T> d, const T2 t) {
  const Half<decltype(d)> dh;
  Vec256<T> ret;
  ret.v0 = ret.v1 = Set(dh, static_cast<T>(t));
  return ret;
}

template <typename T>
HWY_API Vec256<T> Undefined(Full256<T> d) {
  const Half<decltype(d)> dh;
  Vec256<T> ret;
  ret.v0 = ret.v1 = Undefined(dh);
  return ret;
}

template <typename T, typename T2>
Vec256<T> Iota(const Full256<T> d, const T2 first) {
  const Half<decltype(d)> dh;
  Vec256<T> ret;
  ret.v0 = Iota(dh, first);
  ret.v1 = Iota(dh, first + static_cast<T2>(Lanes(dh)));
  return ret;
}

// ================================================== ARITHMETIC

template <typename T>
HWY_API Vec256<T> operator+(Vec256<T> a, const Vec256<T> b) {
  a.v0 += b.v0;
  a.v1 += b.v1;
  return a;
}

template <typename T>
HWY_API Vec256<T> operator-(Vec256<T> a, const Vec256<T> b) {
  a.v0 -= b.v0;
  a.v1 -= b.v1;
  return a;
}

// ------------------------------ SumsOf8
HWY_API Vec256<uint64_t> SumsOf8(const Vec256<uint8_t> v) {
  Vec256<uint64_t> ret;
  ret.v0 = SumsOf8(v.v0);
  ret.v1 = SumsOf8(v.v1);
  return ret;
}

template <typename T>
HWY_API Vec256<T> SaturatedAdd(Vec256<T> a, const Vec256<T> b) {
  a.v0 = SaturatedAdd(a.v0, b.v0);
  a.v1 = SaturatedAdd(a.v1, b.v1);
  return a;
}

template <typename T>
HWY_API Vec256<T> SaturatedSub(Vec256<T> a, const Vec256<T> b) {
  a.v0 = SaturatedSub(a.v0, b.v0);
  a.v1 = SaturatedSub(a.v1, b.v1);
  return a;
}

template <typename T>
HWY_API Vec256<T> AverageRound(Vec256<T> a, const Vec256<T> b) {
  a.v0 = AverageRound(a.v0, b.v0);
  a.v1 = AverageRound(a.v1, b.v1);
  return a;
}

template <typename T>
HWY_API Vec256<T> Abs(Vec256<T> v) {
  v.v0 = Abs(v.v0);
  v.v1 = Abs(v.v1);
  return v;
}

// ------------------------------ Shift lanes by constant #bits

template <int kBits, typename T>
HWY_API Vec256<T> ShiftLeft(Vec256<T> v) {
  v.v0 = ShiftLeft<kBits>(v.v0);
  v.v1 = ShiftLeft<kBits>(v.v1);
  return v;
}

template <int kBits, typename T>
HWY_API Vec256<T> ShiftRight(Vec256<T> v) {
  v.v0 = ShiftRight<kBits>(v.v0);
  v.v1 = ShiftRight<kBits>(v.v1);
  return v;
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

template <typename T>
HWY_API Vec256<T> ShiftLeftSame(Vec256<T> v, const int bits) {
  v.v0 = ShiftLeftSame(v.v0, bits);
  v.v1 = ShiftLeftSame(v.v1, bits);
  return v;
}

template <typename T>
HWY_API Vec256<T> ShiftRightSame(Vec256<T> v, const int bits) {
  v.v0 = ShiftRightSame(v.v0, bits);
  v.v1 = ShiftRightSame(v.v1, bits);
  return v;
}

// ------------------------------ Min, Max
template <typename T>
HWY_API Vec256<T> Min(Vec256<T> a, const Vec256<T> b) {
  a.v0 = Min(a.v0, b.v0);
  a.v1 = Min(a.v1, b.v1);
  return a;
}

template <typename T>
HWY_API Vec256<T> Max(Vec256<T> a, const Vec256<T> b) {
  a.v0 = Max(a.v0, b.v0);
  a.v1 = Max(a.v1, b.v1);
  return a;
}
// ------------------------------ Integer multiplication

template <typename T>
HWY_API Vec256<T> operator*(Vec256<T> a, const Vec256<T> b) {
  a.v0 *= b.v0;
  a.v1 *= b.v1;
  return a;
}

template <typename T>
HWY_API Vec256<T> MulHigh(Vec256<T> a, const Vec256<T> b) {
  a.v0 = MulHigh(a.v0, b.v0);
  a.v1 = MulHigh(a.v1, b.v1);
  return a;
}

template <typename T>
HWY_API Vec256<T> MulFixedPoint15(Vec256<T> a, const Vec256<T> b) {
  a.v0 = MulFixedPoint15(a.v0, b.v0);
  a.v1 = MulFixedPoint15(a.v1, b.v1);
  return a;
}

// Cannot use MakeWide because that returns uint128_t for uint64_t, but we want
// uint64_t.
HWY_API Vec256<uint64_t> MulEven(Vec256<uint32_t> a, const Vec256<uint32_t> b) {
  Vec256<uint64_t> ret;
  ret.v0 = MulEven(a.v0, b.v0);
  ret.v1 = MulEven(a.v1, b.v1);
  return ret;
}
HWY_API Vec256<int64_t> MulEven(Vec256<int32_t> a, const Vec256<int32_t> b) {
  Vec256<int64_t> ret;
  ret.v0 = MulEven(a.v0, b.v0);
  ret.v1 = MulEven(a.v1, b.v1);
  return ret;
}

HWY_API Vec256<uint64_t> MulEven(Vec256<uint64_t> a, const Vec256<uint64_t> b) {
  Vec256<uint64_t> ret;
  ret.v0 = MulEven(a.v0, b.v0);
  ret.v1 = MulEven(a.v1, b.v1);
  return ret;
}
HWY_API Vec256<uint64_t> MulOdd(Vec256<uint64_t> a, const Vec256<uint64_t> b) {
  Vec256<uint64_t> ret;
  ret.v0 = MulOdd(a.v0, b.v0);
  ret.v1 = MulOdd(a.v1, b.v1);
  return ret;
}

// ------------------------------ Negate
template <typename T>
HWY_API Vec256<T> Neg(Vec256<T> v) {
  v.v0 = Neg(v.v0);
  v.v1 = Neg(v.v1);
  return v;
}

// ------------------------------ Floating-point division
template <typename T>
HWY_API Vec256<T> operator/(Vec256<T> a, const Vec256<T> b) {
  a.v0 /= b.v0;
  a.v1 /= b.v1;
  return a;
}

// Approximate reciprocal
HWY_API Vec256<float> ApproximateReciprocal(const Vec256<float> v) {
  const Vec256<float> one = Set(Full256<float>(), 1.0f);
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

template <typename T>
HWY_API Vec256<T> Sqrt(Vec256<T> v) {
  v.v0 = Sqrt(v.v0);
  v.v1 = Sqrt(v.v1);
  return v;
}

// Approximate reciprocal square root
HWY_API Vec256<float> ApproximateReciprocalSqrt(const Vec256<float> v) {
  // TODO(eustas): find cheaper a way to calculate this.
  const Vec256<float> one = Set(Full256<float>(), 1.0f);
  return one / Sqrt(v);
}

// ------------------------------ Floating-point rounding

// Toward nearest integer, ties to even
HWY_API Vec256<float> Round(Vec256<float> v) {
  v.v0 = Round(v.v0);
  v.v1 = Round(v.v1);
  return v;
}

// Toward zero, aka truncate
HWY_API Vec256<float> Trunc(Vec256<float> v) {
  v.v0 = Trunc(v.v0);
  v.v1 = Trunc(v.v1);
  return v;
}

// Toward +infinity, aka ceiling
HWY_API Vec256<float> Ceil(Vec256<float> v) {
  v.v0 = Ceil(v.v0);
  v.v1 = Ceil(v.v1);
  return v;
}

// Toward -infinity, aka floor
HWY_API Vec256<float> Floor(Vec256<float> v) {
  v.v0 = Floor(v.v0);
  v.v1 = Floor(v.v1);
  return v;
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
  return Mask256<TTo>{Mask128<TTo>{m.m0.raw}, Mask128<TTo>{m.m1.raw}};
}

template <typename T>
HWY_API Mask256<T> TestBit(Vec256<T> v, Vec256<T> bit) {
  static_assert(!hwy::IsFloat<T>(), "Only integer vectors supported");
  return (v & bit) == bit;
}

template <typename T>
HWY_API Mask256<T> operator==(Vec256<T> a, const Vec256<T> b) {
  Mask256<T> m;
  m.m0 = operator==(a.v0, b.v0);
  m.m1 = operator==(a.v1, b.v1);
  return m;
}

template <typename T>
HWY_API Mask256<T> operator!=(Vec256<T> a, const Vec256<T> b) {
  Mask256<T> m;
  m.m0 = operator!=(a.v0, b.v0);
  m.m1 = operator!=(a.v1, b.v1);
  return m;
}

template <typename T>
HWY_API Mask256<T> operator<(Vec256<T> a, const Vec256<T> b) {
  Mask256<T> m;
  m.m0 = operator<(a.v0, b.v0);
  m.m1 = operator<(a.v1, b.v1);
  return m;
}

template <typename T>
HWY_API Mask256<T> operator>(Vec256<T> a, const Vec256<T> b) {
  Mask256<T> m;
  m.m0 = operator>(a.v0, b.v0);
  m.m1 = operator>(a.v1, b.v1);
  return m;
}

template <typename T>
HWY_API Mask256<T> operator<=(Vec256<T> a, const Vec256<T> b) {
  Mask256<T> m;
  m.m0 = operator<=(a.v0, b.v0);
  m.m1 = operator<=(a.v1, b.v1);
  return m;
}

template <typename T>
HWY_API Mask256<T> operator>=(Vec256<T> a, const Vec256<T> b) {
  Mask256<T> m;
  m.m0 = operator>=(a.v0, b.v0);
  m.m1 = operator>=(a.v1, b.v1);
  return m;
}

// ------------------------------ FirstN (Iota, Lt)

template <typename T>
HWY_API Mask256<T> FirstN(const Full256<T> d, size_t num) {
  const RebindToSigned<decltype(d)> di;  // Signed comparisons may be cheaper.
  return RebindMask(d, Iota(di, 0) < Set(di, static_cast<MakeSigned<T>>(num)));
}

// ================================================== LOGICAL

template <typename T>
HWY_API Vec256<T> Not(Vec256<T> v) {
  v.v0 = Not(v.v0);
  v.v1 = Not(v.v1);
  return v;
}

template <typename T>
HWY_API Vec256<T> And(Vec256<T> a, Vec256<T> b) {
  a.v0 = And(a.v0, b.v0);
  a.v1 = And(a.v1, b.v1);
  return a;
}

template <typename T>
HWY_API Vec256<T> AndNot(Vec256<T> not_mask, Vec256<T> mask) {
  not_mask.v0 = AndNot(not_mask.v0, mask.v0);
  not_mask.v1 = AndNot(not_mask.v1, mask.v1);
  return not_mask;
}

template <typename T>
HWY_API Vec256<T> Or(Vec256<T> a, Vec256<T> b) {
  a.v0 = Or(a.v0, b.v0);
  a.v1 = Or(a.v1, b.v1);
  return a;
}

template <typename T>
HWY_API Vec256<T> Xor(Vec256<T> a, Vec256<T> b) {
  a.v0 = Xor(a.v0, b.v0);
  a.v1 = Xor(a.v1, b.v1);
  return a;
}

template <typename T>
HWY_API Vec256<T> Or3(Vec256<T> o1, Vec256<T> o2, Vec256<T> o3) {
  return Or(o1, Or(o2, o3));
}

template <typename T>
HWY_API Vec256<T> OrAnd(Vec256<T> o, Vec256<T> a1, Vec256<T> a2) {
  return Or(o, And(a1, a2));
}

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

// ------------------------------ Mask

// Mask and Vec are the same (true = FF..FF).
template <typename T>
HWY_API Mask256<T> MaskFromVec(const Vec256<T> v) {
  Mask256<T> m;
  m.m0 = MaskFromVec(v.v0);
  m.m1 = MaskFromVec(v.v1);
  return m;
}

template <typename T>
HWY_API Vec256<T> VecFromMask(Full256<T> d, Mask256<T> m) {
  const Half<decltype(d)> dh;
  Vec256<T> v;
  v.v0 = VecFromMask(dh, m.m0);
  v.v1 = VecFromMask(dh, m.m1);
  return v;
}

// mask ? yes : no
template <typename T>
HWY_API Vec256<T> IfThenElse(Mask256<T> mask, Vec256<T> yes, Vec256<T> no) {
  yes.v0 = IfThenElse(mask.m0, yes.v0, no.v0);
  yes.v1 = IfThenElse(mask.m1, yes.v1, no.v1);
  return yes;
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
HWY_API Vec256<T> IfNegativeThenElse(Vec256<T> v, Vec256<T> yes, Vec256<T> no) {
  v.v0 = IfNegativeThenElse(v.v0, yes.v0, no.v0);
  v.v1 = IfNegativeThenElse(v.v1, yes.v1, no.v1);
  return v;
}

template <typename T, HWY_IF_FLOAT(T)>
HWY_API Vec256<T> ZeroIfNegative(Vec256<T> v) {
  return IfThenZeroElse(v < Zero(Full256<T>()), v);
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

template <typename T>
HWY_API Mask256<T> ExclusiveNeither(const Mask256<T> a, Mask256<T> b) {
  const Full256<T> d;
  return MaskFromVec(AndNot(VecFromMask(d, a), Not(VecFromMask(d, b))));
}

// ------------------------------ Shl (BroadcastSignBit, IfThenElse)
template <typename T>
HWY_API Vec256<T> operator<<(Vec256<T> v, const Vec256<T> bits) {
  v.v0 = operator<<(v.v0, bits.v0);
  v.v1 = operator<<(v.v1, bits.v1);
  return v;
}

// ------------------------------ Shr (BroadcastSignBit, IfThenElse)
template <typename T>
HWY_API Vec256<T> operator>>(Vec256<T> v, const Vec256<T> bits) {
  v.v0 = operator>>(v.v0, bits.v0);
  v.v1 = operator>>(v.v1, bits.v1);
  return v;
}

// ------------------------------ BroadcastSignBit (compare, VecFromMask)

template <typename T, HWY_IF_NOT_LANE_SIZE(T, 1)>
HWY_API Vec256<T> BroadcastSignBit(const Vec256<T> v) {
  return ShiftRight<sizeof(T) * 8 - 1>(v);
}
HWY_API Vec256<int8_t> BroadcastSignBit(const Vec256<int8_t> v) {
  const Full256<int8_t> d;
  return VecFromMask(d, v < Zero(d));
}

// ================================================== MEMORY

// ------------------------------ Load

template <typename T>
HWY_API Vec256<T> Load(Full256<T> d, const T* HWY_RESTRICT aligned) {
  const Half<decltype(d)> dh;
  Vec256<T> ret;
  ret.v0 = Load(dh, aligned);
  ret.v1 = Load(dh, aligned + Lanes(dh));
  return ret;
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

template <typename T>
HWY_API Vec256<T> LoadDup128(Full256<T> d, const T* HWY_RESTRICT p) {
  const Half<decltype(d)> dh;
  Vec256<T> ret;
  ret.v0 = ret.v1 = Load(dh, p);
  return ret;
}

// ------------------------------ Store

template <typename T>
HWY_API void Store(Vec256<T> v, Full256<T> d, T* HWY_RESTRICT aligned) {
  const Half<decltype(d)> dh;
  Store(v.v0, dh, aligned);
  Store(v.v1, dh, aligned + Lanes(dh));
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

// ------------------------------ Stream
template <typename T>
HWY_API void Stream(Vec256<T> v, Full256<T> d, T* HWY_RESTRICT aligned) {
  // Same as aligned stores.
  Store(v, d, aligned);
}

// ------------------------------ Scatter (Store)

template <typename T, typename Offset>
HWY_API void ScatterOffset(Vec256<T> v, Full256<T> d, T* HWY_RESTRICT base,
                           const Vec256<Offset> offset) {
  constexpr size_t N = 32 / sizeof(T);
  static_assert(sizeof(T) == sizeof(Offset), "Must match for portability");

  alignas(32) T lanes[N];
  Store(v, d, lanes);

  alignas(32) Offset offset_lanes[N];
  Store(offset, Full256<Offset>(), offset_lanes);

  uint8_t* base_bytes = reinterpret_cast<uint8_t*>(base);
  for (size_t i = 0; i < N; ++i) {
    CopyBytes<sizeof(T)>(&lanes[i], base_bytes + offset_lanes[i]);
  }
}

template <typename T, typename Index>
HWY_API void ScatterIndex(Vec256<T> v, Full256<T> d, T* HWY_RESTRICT base,
                          const Vec256<Index> index) {
  constexpr size_t N = 32 / sizeof(T);
  static_assert(sizeof(T) == sizeof(Index), "Must match for portability");

  alignas(32) T lanes[N];
  Store(v, d, lanes);

  alignas(32) Index index_lanes[N];
  Store(index, Full256<Index>(), index_lanes);

  for (size_t i = 0; i < N; ++i) {
    base[index_lanes[i]] = lanes[i];
  }
}

// ------------------------------ Gather (Load/Store)

template <typename T, typename Offset>
HWY_API Vec256<T> GatherOffset(const Full256<T> d, const T* HWY_RESTRICT base,
                               const Vec256<Offset> offset) {
  constexpr size_t N = 32 / sizeof(T);
  static_assert(sizeof(T) == sizeof(Offset), "Must match for portability");

  alignas(32) Offset offset_lanes[N];
  Store(offset, Full256<Offset>(), offset_lanes);

  alignas(32) T lanes[N];
  const uint8_t* base_bytes = reinterpret_cast<const uint8_t*>(base);
  for (size_t i = 0; i < N; ++i) {
    CopyBytes<sizeof(T)>(base_bytes + offset_lanes[i], &lanes[i]);
  }
  return Load(d, lanes);
}

template <typename T, typename Index>
HWY_API Vec256<T> GatherIndex(const Full256<T> d, const T* HWY_RESTRICT base,
                              const Vec256<Index> index) {
  constexpr size_t N = 32 / sizeof(T);
  static_assert(sizeof(T) == sizeof(Index), "Must match for portability");

  alignas(32) Index index_lanes[N];
  Store(index, Full256<Index>(), index_lanes);

  alignas(32) T lanes[N];
  for (size_t i = 0; i < N; ++i) {
    lanes[i] = base[index_lanes[i]];
  }
  return Load(d, lanes);
}

// ================================================== SWIZZLE

// ------------------------------ ExtractLane
template <typename T>
HWY_API T ExtractLane(const Vec256<T> v, size_t i) {
  alignas(32) T lanes[32 / sizeof(T)];
  Store(v, Full256<T>(), lanes);
  return lanes[i];
}

// ------------------------------ InsertLane
template <typename T>
HWY_API Vec256<T> InsertLane(const Vec256<T> v, size_t i, T t) {
  Full256<T> d;
  alignas(32) T lanes[32 / sizeof(T)];
  Store(v, d, lanes);
  lanes[i] = t;
  return Load(d, lanes);
}

// ------------------------------ LowerHalf

template <typename T>
HWY_API Vec128<T> LowerHalf(Full128<T> /* tag */, Vec256<T> v) {
  return v.v0;
}

template <typename T>
HWY_API Vec128<T> LowerHalf(Vec256<T> v) {
  return v.v0;
}

// ------------------------------ GetLane (LowerHalf)
template <typename T>
HWY_API T GetLane(const Vec256<T> v) {
  return GetLane(LowerHalf(v));
}

// ------------------------------ ShiftLeftBytes

template <int kBytes, typename T>
HWY_API Vec256<T> ShiftLeftBytes(Full256<T> d, Vec256<T> v) {
  const Half<decltype(d)> dh;
  v.v0 = ShiftLeftBytes<kBytes>(dh, v.v0);
  v.v1 = ShiftLeftBytes<kBytes>(dh, v.v1);
  return v;
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
template <int kBytes, typename T>
HWY_API Vec256<T> ShiftRightBytes(Full256<T> d, Vec256<T> v) {
  const Half<decltype(d)> dh;
  v.v0 = ShiftRightBytes<kBytes>(dh, v.v0);
  v.v1 = ShiftRightBytes<kBytes>(dh, v.v1);
  return v;
}

// ------------------------------ ShiftRightLanes
template <int kLanes, typename T>
HWY_API Vec256<T> ShiftRightLanes(Full256<T> d, const Vec256<T> v) {
  const Repartition<uint8_t, decltype(d)> d8;
  return BitCast(d, ShiftRightBytes<kLanes * sizeof(T)>(d8, BitCast(d8, v)));
}

// ------------------------------ UpperHalf (ShiftRightBytes)

template <typename T>
HWY_API Vec128<T> UpperHalf(Full128<T> /* tag */, const Vec256<T> v) {
  return v.v1;
}

// ------------------------------ CombineShiftRightBytes

template <int kBytes, typename T, class V = Vec256<T>>
HWY_API V CombineShiftRightBytes(Full256<T> d, V hi, V lo) {
  const Half<decltype(d)> dh;
  hi.v0 = CombineShiftRightBytes<kBytes>(dh, hi.v0, lo.v0);
  hi.v1 = CombineShiftRightBytes<kBytes>(dh, hi.v1, lo.v1);
  return hi;
}

// ------------------------------ Broadcast/splat any lane

template <int kLane, typename T>
HWY_API Vec256<T> Broadcast(const Vec256<T> v) {
  Vec256<T> ret;
  ret.v0 = Broadcast<kLane>(v.v0);
  ret.v1 = Broadcast<kLane>(v.v1);
  return ret;
}

// ------------------------------ TableLookupBytes

// Both full
template <typename T, typename TI>
HWY_API Vec256<TI> TableLookupBytes(const Vec256<T> bytes, Vec256<TI> from) {
  from.v0 = TableLookupBytes(bytes.v0, from.v0);
  from.v1 = TableLookupBytes(bytes.v1, from.v1);
  return from;
}

// Partial index vector
template <typename T, typename TI, size_t NI>
HWY_API Vec128<TI, NI> TableLookupBytes(const Vec256<T> bytes,
                                        const Vec128<TI, NI> from) {
  // First expand to full 128, then 256.
  const auto from_256 = ZeroExtendVector(Full256<TI>(), Vec128<TI>{from.raw});
  const auto tbl_full = TableLookupBytes(bytes, from_256);
  // Shrink to 128, then partial.
  return Vec128<TI, NI>{LowerHalf(Full128<TI>(), tbl_full).raw};
}

// Partial table vector
template <typename T, size_t N, typename TI>
HWY_API Vec256<TI> TableLookupBytes(const Vec128<T, N> bytes,
                                    const Vec256<TI> from) {
  // First expand to full 128, then 256.
  const auto bytes_256 = ZeroExtendVector(Full256<T>(), Vec128<T>{bytes.raw});
  return TableLookupBytes(bytes_256, from);
}

// Partial both are handled by wasm_128.

template <class V, class VI>
HWY_API VI TableLookupBytesOr0(const V bytes, VI from) {
  // wasm out-of-bounds policy already zeros, so TableLookupBytes is fine.
  return TableLookupBytes(bytes, from);
}

// ------------------------------ Hard-coded shuffles

template <typename T>
HWY_API Vec256<T> Shuffle01(Vec256<T> v) {
  v.v0 = Shuffle01(v.v0);
  v.v1 = Shuffle01(v.v1);
  return v;
}

template <typename T>
HWY_API Vec256<T> Shuffle2301(Vec256<T> v) {
  v.v0 = Shuffle2301(v.v0);
  v.v1 = Shuffle2301(v.v1);
  return v;
}

template <typename T>
HWY_API Vec256<T> Shuffle1032(Vec256<T> v) {
  v.v0 = Shuffle1032(v.v0);
  v.v1 = Shuffle1032(v.v1);
  return v;
}

template <typename T>
HWY_API Vec256<T> Shuffle0321(Vec256<T> v) {
  v.v0 = Shuffle0321(v.v0);
  v.v1 = Shuffle0321(v.v1);
  return v;
}

template <typename T>
HWY_API Vec256<T> Shuffle2103(Vec256<T> v) {
  v.v0 = Shuffle2103(v.v0);
  v.v1 = Shuffle2103(v.v1);
  return v;
}

template <typename T>
HWY_API Vec256<T> Shuffle0123(Vec256<T> v) {
  v.v0 = Shuffle0123(v.v0);
  v.v1 = Shuffle0123(v.v1);
  return v;
}

// Used by generic_ops-inl.h
namespace detail {

template <typename T, HWY_IF_LANE_SIZE(T, 4)>
HWY_API Vec256<T> Shuffle2301(Vec256<T> a, const Vec256<T> b) {
  a.v0 = Shuffle2301(a.v0, b.v0);
  a.v1 = Shuffle2301(a.v1, b.v1);
  return a;
}
template <typename T, HWY_IF_LANE_SIZE(T, 4)>
HWY_API Vec256<T> Shuffle1230(Vec256<T> a, const Vec256<T> b) {
  a.v0 = Shuffle1230(a.v0, b.v0);
  a.v1 = Shuffle1230(a.v1, b.v1);
  return a;
}
template <typename T, HWY_IF_LANE_SIZE(T, 4)>
HWY_API Vec256<T> Shuffle3012(Vec256<T> a, const Vec256<T> b) {
  a.v0 = Shuffle3012(a.v0, b.v0);
  a.v1 = Shuffle3012(a.v1, b.v1);
  return a;
}

}  // namespace detail

// ------------------------------ TableLookupLanes

// Returned by SetTableIndices for use by TableLookupLanes.
template <typename T>
struct Indices256 {
  __v128_u i0;
  __v128_u i1;
};

template <typename T, typename TI>
HWY_API Indices256<T> IndicesFromVec(Full256<T> /* tag */, Vec256<TI> vec) {
  static_assert(sizeof(T) == sizeof(TI), "Index size must match lane");
  Indices256<T> ret;
  ret.i0 = vec.v0.raw;
  ret.i1 = vec.v1.raw;
  return ret;
}

template <typename T, typename TI>
HWY_API Indices256<T> SetTableIndices(Full256<T> d, const TI* idx) {
  const Rebind<TI, decltype(d)> di;
  return IndicesFromVec(d, LoadU(di, idx));
}

template <typename T>
HWY_API Vec256<T> TableLookupLanes(const Vec256<T> v, Indices256<T> idx) {
  using TU = MakeUnsigned<T>;
  const Full128<T> dh;
  const Full128<TU> duh;
  constexpr size_t kLanesPerHalf = 16 / sizeof(TU);

  const Vec128<TU> vi0{idx.i0};
  const Vec128<TU> vi1{idx.i1};
  const Vec128<TU> mask = Set(duh, static_cast<TU>(kLanesPerHalf - 1));
  const Vec128<TU> vmod0 = vi0 & mask;
  const Vec128<TU> vmod1 = vi1 & mask;
  // If ANDing did not change the index, it is for the lower half.
  const Mask128<T> is_lo0 = RebindMask(dh, vi0 == vmod0);
  const Mask128<T> is_lo1 = RebindMask(dh, vi1 == vmod1);
  const Indices128<T> mod0 = IndicesFromVec(dh, vmod0);
  const Indices128<T> mod1 = IndicesFromVec(dh, vmod1);

  Vec256<T> ret;
  ret.v0 = IfThenElse(is_lo0, TableLookupLanes(v.v0, mod0),
                      TableLookupLanes(v.v1, mod0));
  ret.v1 = IfThenElse(is_lo1, TableLookupLanes(v.v0, mod1),
                      TableLookupLanes(v.v1, mod1));
  return ret;
}

template <typename T>
HWY_API Vec256<T> TableLookupLanesOr0(Vec256<T> v, Indices256<T> idx) {
  // The out of bounds behavior will already zero lanes.
  return TableLookupLanesOr0(v, idx);
}

// ------------------------------ Reverse
template <typename T>
HWY_API Vec256<T> Reverse(Full256<T> d, const Vec256<T> v) {
  const Half<decltype(d)> dh;
  Vec256<T> ret;
  ret.v1 = Reverse(dh, v.v0);  // note reversed v1 member order
  ret.v0 = Reverse(dh, v.v1);
  return ret;
}

// ------------------------------ Reverse2
template <typename T>
HWY_API Vec256<T> Reverse2(Full256<T> d, Vec256<T> v) {
  const Half<decltype(d)> dh;
  v.v0 = Reverse2(dh, v.v0);
  v.v1 = Reverse2(dh, v.v1);
  return v;
}

// ------------------------------ Reverse4

// Each block has only 2 lanes, so swap blocks and their lanes.
template <typename T, HWY_IF_LANE_SIZE(T, 8)>
HWY_API Vec256<T> Reverse4(Full256<T> d, const Vec256<T> v) {
  const Half<decltype(d)> dh;
  Vec256<T> ret;
  ret.v0 = Reverse2(dh, v.v1);  // swapped
  ret.v1 = Reverse2(dh, v.v0);
  return ret;
}

template <typename T, HWY_IF_NOT_LANE_SIZE(T, 8)>
HWY_API Vec256<T> Reverse4(Full256<T> d, Vec256<T> v) {
  const Half<decltype(d)> dh;
  v.v0 = Reverse4(dh, v.v0);
  v.v1 = Reverse4(dh, v.v1);
  return v;
}

// ------------------------------ Reverse8

template <typename T, HWY_IF_LANE_SIZE(T, 8)>
HWY_API Vec256<T> Reverse8(Full256<T> /* tag */, Vec256<T> /* v */) {
  HWY_ASSERT(0);  // don't have 8 u64 lanes
}

// Each block has only 4 lanes, so swap blocks and their lanes.
template <typename T, HWY_IF_LANE_SIZE(T, 4)>
HWY_API Vec256<T> Reverse8(Full256<T> d, const Vec256<T> v) {
  const Half<decltype(d)> dh;
  Vec256<T> ret;
  ret.v0 = Reverse4(dh, v.v1);  // swapped
  ret.v1 = Reverse4(dh, v.v0);
  return ret;
}

template <typename T, HWY_IF_LANE_SIZE_LT(T, 4)>
HWY_API Vec256<T> Reverse8(Full256<T> d, Vec256<T> v) {
  const Half<decltype(d)> dh;
  v.v0 = Reverse8(dh, v.v0);
  v.v1 = Reverse8(dh, v.v1);
  return v;
}

// ------------------------------ InterleaveLower

template <typename T>
HWY_API Vec256<T> InterleaveLower(Vec256<T> a, Vec256<T> b) {
  a.v0 = InterleaveLower(a.v0, b.v0);
  a.v1 = InterleaveLower(a.v1, b.v1);
  return a;
}

// wasm_128 already defines a template with D, V, V args.

// ------------------------------ InterleaveUpper (UpperHalf)

template <typename T, class V = Vec256<T>>
HWY_API V InterleaveUpper(Full256<T> d, V a, V b) {
  const Half<decltype(d)> dh;
  a.v0 = InterleaveUpper(dh, a.v0, b.v0);
  a.v1 = InterleaveUpper(dh, a.v1, b.v1);
  return a;
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
template <typename T>
HWY_API Vec256<T> Combine(Full256<T> /* d */, Vec128<T> hi, Vec128<T> lo) {
  Vec256<T> ret;
  ret.v1 = hi;
  ret.v0 = lo;
  return ret;
}

// ------------------------------ ZeroExtendVector (Combine)
template <typename T>
HWY_API Vec256<T> ZeroExtendVector(Full256<T> d, Vec128<T> lo) {
  const Half<decltype(d)> dh;
  return Combine(d, Zero(dh), lo);
}

// ------------------------------ ConcatLowerLower
template <typename T>
HWY_API Vec256<T> ConcatLowerLower(Full256<T> /* tag */, const Vec256<T> hi,
                                   const Vec256<T> lo) {
  Vec256<T> ret;
  ret.v1 = hi.v0;
  ret.v0 = lo.v0;
  return ret;
}

// ------------------------------ ConcatUpperUpper
template <typename T>
HWY_API Vec256<T> ConcatUpperUpper(Full256<T> /* tag */, const Vec256<T> hi,
                                   const Vec256<T> lo) {
  Vec256<T> ret;
  ret.v1 = hi.v1;
  ret.v0 = lo.v1;
  return ret;
}

// ------------------------------ ConcatLowerUpper
template <typename T>
HWY_API Vec256<T> ConcatLowerUpper(Full256<T> /* tag */, const Vec256<T> hi,
                                   const Vec256<T> lo) {
  Vec256<T> ret;
  ret.v1 = hi.v0;
  ret.v0 = lo.v1;
  return ret;
}

// ------------------------------ ConcatUpperLower
template <typename T>
HWY_API Vec256<T> ConcatUpperLower(Full256<T> /* tag */, const Vec256<T> hi,
                                   const Vec256<T> lo) {
  Vec256<T> ret;
  ret.v1 = hi.v1;
  ret.v0 = lo.v0;
  return ret;
}

// ------------------------------ ConcatOdd
template <typename T>
HWY_API Vec256<T> ConcatOdd(Full256<T> d, const Vec256<T> hi,
                            const Vec256<T> lo) {
  const Half<decltype(d)> dh;
  Vec256<T> ret;
  ret.v0 = ConcatOdd(dh, lo.v1, lo.v0);
  ret.v1 = ConcatOdd(dh, hi.v1, hi.v0);
  return ret;
}

// ------------------------------ ConcatEven
template <typename T>
HWY_API Vec256<T> ConcatEven(Full256<T> d, const Vec256<T> hi,
                             const Vec256<T> lo) {
  const Half<decltype(d)> dh;
  Vec256<T> ret;
  ret.v0 = ConcatEven(dh, lo.v1, lo.v0);
  ret.v1 = ConcatEven(dh, hi.v1, hi.v0);
  return ret;
}

// ------------------------------ DupEven
template <typename T>
HWY_API Vec256<T> DupEven(Vec256<T> v) {
  v.v0 = DupEven(v.v0);
  v.v1 = DupEven(v.v1);
  return v;
}

// ------------------------------ DupOdd
template <typename T>
HWY_API Vec256<T> DupOdd(Vec256<T> v) {
  v.v0 = DupOdd(v.v0);
  v.v1 = DupOdd(v.v1);
  return v;
}

// ------------------------------ OddEven
template <typename T>
HWY_API Vec256<T> OddEven(Vec256<T> a, const Vec256<T> b) {
  a.v0 = OddEven(a.v0, b.v0);
  a.v1 = OddEven(a.v1, b.v1);
  return a;
}

// ------------------------------ OddEvenBlocks
template <typename T>
HWY_API Vec256<T> OddEvenBlocks(Vec256<T> odd, Vec256<T> even) {
  odd.v0 = even.v0;
  return odd;
}

// ------------------------------ SwapAdjacentBlocks
template <typename T>
HWY_API Vec256<T> SwapAdjacentBlocks(Vec256<T> v) {
  Vec256<T> ret;
  ret.v0 = v.v1;  // swapped order
  ret.v1 = v.v0;
  return ret;
}

// ------------------------------ ReverseBlocks
template <typename T>
HWY_API Vec256<T> ReverseBlocks(Full256<T> /* tag */, const Vec256<T> v) {
  return SwapAdjacentBlocks(v);  // 2 blocks, so Swap = Reverse
}

// ================================================== CONVERT

// ------------------------------ Promotions (part w/ narrow lanes -> full)

namespace detail {

// Unsigned: zero-extend.
HWY_API Vec128<uint16_t> PromoteUpperTo(Full128<uint16_t> /* tag */,
                                        const Vec128<uint8_t> v) {
  return Vec128<uint16_t>{wasm_u16x8_extend_high_u8x16(v.raw)};
}
HWY_API Vec128<uint32_t> PromoteUpperTo(Full128<uint32_t> /* tag */,
                                        const Vec128<uint8_t> v) {
  return Vec128<uint32_t>{
      wasm_u32x4_extend_high_u16x8(wasm_u16x8_extend_high_u8x16(v.raw))};
}
HWY_API Vec128<int16_t> PromoteUpperTo(Full128<int16_t> /* tag */,
                                       const Vec128<uint8_t> v) {
  return Vec128<int16_t>{wasm_u16x8_extend_high_u8x16(v.raw)};
}
HWY_API Vec128<int32_t> PromoteUpperTo(Full128<int32_t> /* tag */,
                                       const Vec128<uint8_t> v) {
  return Vec128<int32_t>{
      wasm_u32x4_extend_high_u16x8(wasm_u16x8_extend_high_u8x16(v.raw))};
}
HWY_API Vec128<uint32_t> PromoteUpperTo(Full128<uint32_t> /* tag */,
                                        const Vec128<uint16_t> v) {
  return Vec128<uint32_t>{wasm_u32x4_extend_high_u16x8(v.raw)};
}
HWY_API Vec128<uint64_t> PromoteUpperTo(Full128<uint64_t> /* tag */,
                                        const Vec128<uint32_t> v) {
  return Vec128<uint64_t>{wasm_u64x2_extend_high_u32x4(v.raw)};
}
HWY_API Vec128<int32_t> PromoteUpperTo(Full128<int32_t> /* tag */,
                                       const Vec128<uint16_t> v) {
  return Vec128<int32_t>{wasm_u32x4_extend_high_u16x8(v.raw)};
}

// Signed: replicate sign bit.
HWY_API Vec128<int16_t> PromoteUpperTo(Full128<int16_t> /* tag */,
                                       const Vec128<int8_t> v) {
  return Vec128<int16_t>{wasm_i16x8_extend_high_i8x16(v.raw)};
}
HWY_API Vec128<int32_t> PromoteUpperTo(Full128<int32_t> /* tag */,
                                       const Vec128<int8_t> v) {
  return Vec128<int32_t>{
      wasm_i32x4_extend_high_i16x8(wasm_i16x8_extend_high_i8x16(v.raw))};
}
HWY_API Vec128<int32_t> PromoteUpperTo(Full128<int32_t> /* tag */,
                                       const Vec128<int16_t> v) {
  return Vec128<int32_t>{wasm_i32x4_extend_high_i16x8(v.raw)};
}
HWY_API Vec128<int64_t> PromoteUpperTo(Full128<int64_t> /* tag */,
                                       const Vec128<int32_t> v) {
  return Vec128<int64_t>{wasm_i64x2_extend_high_i32x4(v.raw)};
}

HWY_API Vec128<double> PromoteUpperTo(Full128<double> dd,
                                      const Vec128<int32_t> v) {
  // There is no wasm_f64x2_convert_high_i32x4.
  const Full64<int32_t> di32h;
  return PromoteTo(dd, UpperHalf(di32h, v));
}

HWY_API Vec128<float> PromoteUpperTo(Full128<float> df32,
                                     const Vec128<float16_t> v) {
  const RebindToSigned<decltype(df32)> di32;
  const RebindToUnsigned<decltype(df32)> du32;
  // Expand to u32 so we can shift.
  const auto bits16 = PromoteUpperTo(du32, Vec128<uint16_t>{v.raw});
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

HWY_API Vec128<float> PromoteUpperTo(Full128<float> df32,
                                     const Vec128<bfloat16_t> v) {
  const Full128<uint16_t> du16;
  const RebindToSigned<decltype(df32)> di32;
  return BitCast(df32, ShiftLeft<16>(PromoteUpperTo(di32, BitCast(du16, v))));
}

}  // namespace detail

template <typename T, typename TN>
HWY_API Vec256<T> PromoteTo(Full256<T> d, const Vec128<TN> v) {
  const Half<decltype(d)> dh;
  Vec256<T> ret;
  ret.v0 = PromoteTo(dh, LowerHalf(v));
  ret.v1 = detail::PromoteUpperTo(dh, v);
  return ret;
}

// This is the only 4x promotion from 8 to 32-bit.
template <typename TW, typename TN>
HWY_API Vec256<TW> PromoteTo(Full256<TW> d, const Vec64<TN> v) {
  const Half<decltype(d)> dh;
  const Rebind<MakeWide<TN>, decltype(d)> d2;  // 16-bit lanes
  const auto v16 = PromoteTo(d2, v);
  Vec256<TW> ret;
  ret.v0 = PromoteTo(dh, LowerHalf(v16));
  ret.v1 = detail::PromoteUpperTo(dh, v16);
  return ret;
}

// ------------------------------ DemoteTo

HWY_API Vec128<uint16_t> DemoteTo(Full128<uint16_t> /* tag */,
                                  const Vec256<int32_t> v) {
  return Vec128<uint16_t>{wasm_u16x8_narrow_i32x4(v.v0.raw, v.v1.raw)};
}

HWY_API Vec128<int16_t> DemoteTo(Full128<int16_t> /* tag */,
                                 const Vec256<int32_t> v) {
  return Vec128<int16_t>{wasm_i16x8_narrow_i32x4(v.v0.raw, v.v1.raw)};
}

HWY_API Vec64<uint8_t> DemoteTo(Full64<uint8_t> /* tag */,
                                const Vec256<int32_t> v) {
  const auto intermediate = wasm_i16x8_narrow_i32x4(v.v0.raw, v.v1.raw);
  return Vec64<uint8_t>{wasm_u8x16_narrow_i16x8(intermediate, intermediate)};
}

HWY_API Vec128<uint8_t> DemoteTo(Full128<uint8_t> /* tag */,
                                 const Vec256<int16_t> v) {
  return Vec128<uint8_t>{wasm_u8x16_narrow_i16x8(v.v0.raw, v.v1.raw)};
}

HWY_API Vec64<int8_t> DemoteTo(Full64<int8_t> /* tag */,
                               const Vec256<int32_t> v) {
  const auto intermediate = wasm_i16x8_narrow_i32x4(v.v0.raw, v.v1.raw);
  return Vec64<int8_t>{wasm_i8x16_narrow_i16x8(intermediate, intermediate)};
}

HWY_API Vec128<int8_t> DemoteTo(Full128<int8_t> /* tag */,
                                const Vec256<int16_t> v) {
  return Vec128<int8_t>{wasm_i8x16_narrow_i16x8(v.v0.raw, v.v1.raw)};
}

HWY_API Vec128<int32_t> DemoteTo(Full128<int32_t> di, const Vec256<double> v) {
  const Vec64<int32_t> lo{wasm_i32x4_trunc_sat_f64x2_zero(v.v0.raw)};
  const Vec64<int32_t> hi{wasm_i32x4_trunc_sat_f64x2_zero(v.v1.raw)};
  return Combine(di, hi, lo);
}

HWY_API Vec128<float16_t> DemoteTo(Full128<float16_t> d16,
                                   const Vec256<float> v) {
  const Half<decltype(d16)> d16h;
  const Vec64<float16_t> lo = DemoteTo(d16h, v.v0);
  const Vec64<float16_t> hi = DemoteTo(d16h, v.v1);
  return Combine(d16, hi, lo);
}

HWY_API Vec128<bfloat16_t> DemoteTo(Full128<bfloat16_t> dbf16,
                                    const Vec256<float> v) {
  const Half<decltype(dbf16)> dbf16h;
  const Vec64<bfloat16_t> lo = DemoteTo(dbf16h, v.v0);
  const Vec64<bfloat16_t> hi = DemoteTo(dbf16h, v.v1);
  return Combine(dbf16, hi, lo);
}

// For already range-limited input [0, 255].
HWY_API Vec64<uint8_t> U8FromU32(const Vec256<uint32_t> v) {
  const Full64<uint8_t> du8;
  const Full256<int32_t> di32;  // no unsigned DemoteTo
  return DemoteTo(du8, BitCast(di32, v));
}

// ------------------------------ Truncations

HWY_API Vec32<uint8_t> TruncateTo(Full32<uint8_t> /* tag */,
                                  const Vec256<uint64_t> v) {
  return Vec32<uint8_t>{wasm_i8x16_shuffle(v.v0.raw, v.v1.raw, 0, 8, 16, 24, 0,
                                           8, 16, 24, 0, 8, 16, 24, 0, 8, 16,
                                           24)};
}

HWY_API Vec64<uint16_t> TruncateTo(Full64<uint16_t> /* tag */,
                                   const Vec256<uint64_t> v) {
  return Vec64<uint16_t>{wasm_i8x16_shuffle(v.v0.raw, v.v1.raw, 0, 1, 8, 9, 16,
                                            17, 24, 25, 0, 1, 8, 9, 16, 17, 24,
                                            25)};
}

HWY_API Vec128<uint32_t> TruncateTo(Full128<uint32_t> /* tag */,
                                    const Vec256<uint64_t> v) {
  return Vec128<uint32_t>{wasm_i8x16_shuffle(v.v0.raw, v.v1.raw, 0, 1, 2, 3, 8,
                                             9, 10, 11, 16, 17, 18, 19, 24, 25,
                                             26, 27)};
}

HWY_API Vec64<uint8_t> TruncateTo(Full64<uint8_t> /* tag */,
                                  const Vec256<uint32_t> v) {
  return Vec64<uint8_t>{wasm_i8x16_shuffle(v.v0.raw, v.v1.raw, 0, 4, 8, 12, 16,
                                           20, 24, 28, 0, 4, 8, 12, 16, 20, 24,
                                           28)};
}

HWY_API Vec128<uint16_t> TruncateTo(Full128<uint16_t> /* tag */,
                                    const Vec256<uint32_t> v) {
  return Vec128<uint16_t>{wasm_i8x16_shuffle(v.v0.raw, v.v1.raw, 0, 1, 4, 5, 8,
                                             9, 12, 13, 16, 17, 20, 21, 24, 25,
                                             28, 29)};
}

HWY_API Vec128<uint8_t> TruncateTo(Full128<uint8_t> /* tag */,
                                   const Vec256<uint16_t> v) {
  return Vec128<uint8_t>{wasm_i8x16_shuffle(v.v0.raw, v.v1.raw, 0, 2, 4, 6, 8,
                                            10, 12, 14, 16, 18, 20, 22, 24, 26,
                                            28, 30)};
}

// ------------------------------ ReorderDemote2To
HWY_API Vec256<bfloat16_t> ReorderDemote2To(Full256<bfloat16_t> dbf16,
                                            Vec256<float> a, Vec256<float> b) {
  const RebindToUnsigned<decltype(dbf16)> du16;
  return BitCast(dbf16, ConcatOdd(du16, BitCast(du16, b), BitCast(du16, a)));
}

HWY_API Vec256<int16_t> ReorderDemote2To(Full256<int16_t> d16,
                                         Vec256<int32_t> a, Vec256<int32_t> b) {
  const Half<decltype(d16)> d16h;
  Vec256<int16_t> demoted;
  demoted.v0 = DemoteTo(d16h, a);
  demoted.v1 = DemoteTo(d16h, b);
  return demoted;
}

// ------------------------------ Convert i32 <=> f32 (Round)

template <typename TTo, typename TFrom>
HWY_API Vec256<TTo> ConvertTo(Full256<TTo> d, const Vec256<TFrom> v) {
  const Half<decltype(d)> dh;
  Vec256<TTo> ret;
  ret.v0 = ConvertTo(dh, v.v0);
  ret.v1 = ConvertTo(dh, v.v1);
  return ret;
}

HWY_API Vec256<int32_t> NearestInt(const Vec256<float> v) {
  return ConvertTo(Full256<int32_t>(), Round(v));
}

// ================================================== MISC

// ------------------------------ LoadMaskBits (TestBit)

// `p` points to at least 8 readable bytes, not all of which need be valid.
template <typename T, HWY_IF_LANE_SIZE_GE(T, 4)>
HWY_API Mask256<T> LoadMaskBits(Full256<T> d,
                                const uint8_t* HWY_RESTRICT bits) {
  const Half<decltype(d)> dh;
  Mask256<T> ret;
  ret.m0 = LoadMaskBits(dh, bits);
  // If size=4, one 128-bit vector has 4 mask bits; otherwise 2 for size=8.
  // Both halves fit in one byte's worth of mask bits.
  constexpr size_t kBitsPerHalf = 16 / sizeof(T);
  const uint8_t bits_upper[8] = {static_cast<uint8_t>(bits[0] >> kBitsPerHalf)};
  ret.m1 = LoadMaskBits(dh, bits_upper);
  return ret;
}

template <typename T, HWY_IF_LANE_SIZE_LT(T, 4)>
HWY_API Mask256<T> LoadMaskBits(Full256<T> d,
                                const uint8_t* HWY_RESTRICT bits) {
  const Half<decltype(d)> dh;
  Mask256<T> ret;
  ret.m0 = LoadMaskBits(dh, bits);
  constexpr size_t kLanesPerHalf = 16 / sizeof(T);
  constexpr size_t kBytesPerHalf = kLanesPerHalf / 8;
  static_assert(kBytesPerHalf != 0, "Lane size <= 16 bits => at least 8 lanes");
  ret.m1 = LoadMaskBits(dh, bits + kBytesPerHalf);
  return ret;
}

// ------------------------------ Mask

// `p` points to at least 8 writable bytes.
template <typename T, HWY_IF_LANE_SIZE_GE(T, 4)>
HWY_API size_t StoreMaskBits(const Full256<T> d, const Mask256<T> mask,
                             uint8_t* bits) {
  const Half<decltype(d)> dh;
  StoreMaskBits(dh, mask.m0, bits);
  const uint8_t lo = bits[0];
  StoreMaskBits(dh, mask.m1, bits);
  // If size=4, one 128-bit vector has 4 mask bits; otherwise 2 for size=8.
  // Both halves fit in one byte's worth of mask bits.
  constexpr size_t kBitsPerHalf = 16 / sizeof(T);
  bits[0] = static_cast<uint8_t>(lo | (bits[0] << kBitsPerHalf));
  return (kBitsPerHalf * 2 + 7) / 8;
}

template <typename T, HWY_IF_LANE_SIZE_LT(T, 4)>
HWY_API size_t StoreMaskBits(const Full256<T> d, const Mask256<T> mask,
                             uint8_t* bits) {
  const Half<decltype(d)> dh;
  constexpr size_t kLanesPerHalf = 16 / sizeof(T);
  constexpr size_t kBytesPerHalf = kLanesPerHalf / 8;
  static_assert(kBytesPerHalf != 0, "Lane size <= 16 bits => at least 8 lanes");
  StoreMaskBits(dh, mask.m0, bits);
  StoreMaskBits(dh, mask.m1, bits + kBytesPerHalf);
  return kBytesPerHalf * 2;
}

template <typename T>
HWY_API size_t CountTrue(const Full256<T> d, const Mask256<T> m) {
  const Half<decltype(d)> dh;
  return CountTrue(dh, m.m0) + CountTrue(dh, m.m1);
}

template <typename T>
HWY_API bool AllFalse(const Full256<T> d, const Mask256<T> m) {
  const Half<decltype(d)> dh;
  return AllFalse(dh, m.m0) && AllFalse(dh, m.m1);
}

template <typename T>
HWY_API bool AllTrue(const Full256<T> d, const Mask256<T> m) {
  const Half<decltype(d)> dh;
  return AllTrue(dh, m.m0) && AllTrue(dh, m.m1);
}

template <typename T>
HWY_API size_t FindKnownFirstTrue(const Full256<T> d, const Mask256<T> mask) {
  const Half<decltype(d)> dh;
  const intptr_t lo = FindFirstTrue(dh, mask.m0);  // not known
  constexpr size_t kLanesPerHalf = 16 / sizeof(T);
  return lo >= 0 ? static_cast<size_t>(lo)
                 : kLanesPerHalf + FindKnownFirstTrue(dh, mask.m1);
}

template <typename T>
HWY_API intptr_t FindFirstTrue(const Full256<T> d, const Mask256<T> mask) {
  const Half<decltype(d)> dh;
  const intptr_t lo = FindFirstTrue(dh, mask.m0);
  const intptr_t hi = FindFirstTrue(dh, mask.m1);
  if (lo < 0 && hi < 0) return lo;
  constexpr int kLanesPerHalf = 16 / sizeof(T);
  return lo >= 0 ? lo : hi + kLanesPerHalf;
}

// ------------------------------ CompressStore
template <typename T>
HWY_API size_t CompressStore(const Vec256<T> v, const Mask256<T> mask,
                             Full256<T> d, T* HWY_RESTRICT unaligned) {
  const Half<decltype(d)> dh;
  const size_t count = CompressStore(v.v0, mask.m0, dh, unaligned);
  const size_t count2 = CompressStore(v.v1, mask.m1, dh, unaligned + count);
  return count + count2;
}

// ------------------------------ CompressBlendedStore
template <typename T>
HWY_API size_t CompressBlendedStore(const Vec256<T> v, const Mask256<T> m,
                                    Full256<T> d, T* HWY_RESTRICT unaligned) {
  const Half<decltype(d)> dh;
  const size_t count = CompressBlendedStore(v.v0, m.m0, dh, unaligned);
  const size_t count2 = CompressBlendedStore(v.v1, m.m1, dh, unaligned + count);
  return count + count2;
}

// ------------------------------ CompressBitsStore

template <typename T>
HWY_API size_t CompressBitsStore(const Vec256<T> v,
                                 const uint8_t* HWY_RESTRICT bits, Full256<T> d,
                                 T* HWY_RESTRICT unaligned) {
  const Mask256<T> m = LoadMaskBits(d, bits);
  return CompressStore(v, m, d, unaligned);
}

// ------------------------------ Compress

template <typename T>
HWY_API Vec256<T> Compress(const Vec256<T> v, const Mask256<T> mask) {
  const Full256<T> d;
  alignas(32) T lanes[32 / sizeof(T)] = {};
  (void)CompressStore(v, mask, d, lanes);
  return Load(d, lanes);
}

// ------------------------------ CompressNot
template <typename T>
HWY_API Vec256<T> CompressNot(Vec256<T> v, const Mask256<T> mask) {
  return Compress(v, Not(mask));
}

// ------------------------------ CompressBlocksNot
HWY_API Vec256<uint64_t> CompressBlocksNot(Vec256<uint64_t> v,
                                           Mask256<uint64_t> mask) {
  const Full128<uint64_t> dh;
  // Because the non-selected (mask=1) blocks are undefined, we can return the
  // input unless mask = 01, in which case we must bring down the upper block.
  return AllTrue(dh, AndNot(mask.m1, mask.m0)) ? SwapAdjacentBlocks(v) : v;
}

// ------------------------------ CompressBits

template <typename T>
HWY_API Vec256<T> CompressBits(Vec256<T> v, const uint8_t* HWY_RESTRICT bits) {
  const Mask256<T> m = LoadMaskBits(Full256<T>(), bits);
  return Compress(v, m);
}

// ------------------------------ LoadInterleaved3/4

// Implemented in generic_ops, we just overload LoadTransposedBlocks3/4.

namespace detail {

// Input:
// 1 0 (<- first block of unaligned)
// 3 2
// 5 4
// Output:
// 3 0
// 4 1
// 5 2
template <typename T>
HWY_API void LoadTransposedBlocks3(Full256<T> d,
                                   const T* HWY_RESTRICT unaligned,
                                   Vec256<T>& A, Vec256<T>& B, Vec256<T>& C) {
  constexpr size_t N = 32 / sizeof(T);
  const Vec256<T> v10 = LoadU(d, unaligned + 0 * N);  // 1 0
  const Vec256<T> v32 = LoadU(d, unaligned + 1 * N);
  const Vec256<T> v54 = LoadU(d, unaligned + 2 * N);

  A = ConcatUpperLower(d, v32, v10);
  B = ConcatLowerUpper(d, v54, v10);
  C = ConcatUpperLower(d, v54, v32);
}

// Input (128-bit blocks):
// 1 0 (first block of unaligned)
// 3 2
// 5 4
// 7 6
// Output:
// 4 0 (LSB of A)
// 5 1
// 6 2
// 7 3
template <typename T>
HWY_API void LoadTransposedBlocks4(Full256<T> d,
                                   const T* HWY_RESTRICT unaligned,
                                   Vec256<T>& A, Vec256<T>& B, Vec256<T>& C,
                                   Vec256<T>& D) {
  constexpr size_t N = 32 / sizeof(T);
  const Vec256<T> v10 = LoadU(d, unaligned + 0 * N);
  const Vec256<T> v32 = LoadU(d, unaligned + 1 * N);
  const Vec256<T> v54 = LoadU(d, unaligned + 2 * N);
  const Vec256<T> v76 = LoadU(d, unaligned + 3 * N);

  A = ConcatLowerLower(d, v54, v10);
  B = ConcatUpperUpper(d, v54, v10);
  C = ConcatLowerLower(d, v76, v32);
  D = ConcatUpperUpper(d, v76, v32);
}

}  // namespace detail

// ------------------------------ StoreInterleaved2/3/4 (ConcatUpperLower)

// Implemented in generic_ops, we just overload StoreTransposedBlocks2/3/4.

namespace detail {

// Input (128-bit blocks):
// 2 0 (LSB of i)
// 3 1
// Output:
// 1 0
// 3 2
template <typename T>
HWY_API void StoreTransposedBlocks2(const Vec256<T> i, const Vec256<T> j,
                                    const Full256<T> d,
                                    T* HWY_RESTRICT unaligned) {
  constexpr size_t N = 32 / sizeof(T);
  const auto out0 = ConcatLowerLower(d, j, i);
  const auto out1 = ConcatUpperUpper(d, j, i);
  StoreU(out0, d, unaligned + 0 * N);
  StoreU(out1, d, unaligned + 1 * N);
}

// Input (128-bit blocks):
// 3 0 (LSB of i)
// 4 1
// 5 2
// Output:
// 1 0
// 3 2
// 5 4
template <typename T>
HWY_API void StoreTransposedBlocks3(const Vec256<T> i, const Vec256<T> j,
                                    const Vec256<T> k, Full256<T> d,
                                    T* HWY_RESTRICT unaligned) {
  constexpr size_t N = 32 / sizeof(T);
  const auto out0 = ConcatLowerLower(d, j, i);
  const auto out1 = ConcatUpperLower(d, i, k);
  const auto out2 = ConcatUpperUpper(d, k, j);
  StoreU(out0, d, unaligned + 0 * N);
  StoreU(out1, d, unaligned + 1 * N);
  StoreU(out2, d, unaligned + 2 * N);
}

// Input (128-bit blocks):
// 4 0 (LSB of i)
// 5 1
// 6 2
// 7 3
// Output:
// 1 0
// 3 2
// 5 4
// 7 6
template <typename T>
HWY_API void StoreTransposedBlocks4(const Vec256<T> i, const Vec256<T> j,
                                    const Vec256<T> k, const Vec256<T> l,
                                    Full256<T> d, T* HWY_RESTRICT unaligned) {
  constexpr size_t N = 32 / sizeof(T);
  // Write lower halves, then upper.
  const auto out0 = ConcatLowerLower(d, j, i);
  const auto out1 = ConcatLowerLower(d, l, k);
  StoreU(out0, d, unaligned + 0 * N);
  StoreU(out1, d, unaligned + 1 * N);
  const auto out2 = ConcatUpperUpper(d, j, i);
  const auto out3 = ConcatUpperUpper(d, l, k);
  StoreU(out2, d, unaligned + 2 * N);
  StoreU(out3, d, unaligned + 3 * N);
}

}  // namespace detail

// ------------------------------ ReorderWidenMulAccumulate
template <typename TN, typename TW>
HWY_API Vec256<TW> ReorderWidenMulAccumulate(Full256<TW> d, Vec256<TN> a,
                                             Vec256<TN> b, Vec256<TW> sum0,
                                             Vec256<TW>& sum1) {
  const Half<decltype(d)> dh;
  sum0.v0 = ReorderWidenMulAccumulate(dh, a.v0, b.v0, sum0.v0, sum1.v0);
  sum0.v1 = ReorderWidenMulAccumulate(dh, a.v1, b.v1, sum0.v1, sum1.v1);
  return sum0;
}

// ------------------------------ Reductions

template <typename T>
HWY_API Vec256<T> SumOfLanes(Full256<T> d, const Vec256<T> v) {
  const Half<decltype(d)> dh;
  const Vec128<T> lo = SumOfLanes(dh, Add(v.v0, v.v1));
  return Combine(d, lo, lo);
}

template <typename T>
HWY_API Vec256<T> MinOfLanes(Full256<T> d, const Vec256<T> v) {
  const Half<decltype(d)> dh;
  const Vec128<T> lo = MinOfLanes(dh, Min(v.v0, v.v1));
  return Combine(d, lo, lo);
}

template <typename T>
HWY_API Vec256<T> MaxOfLanes(Full256<T> d, const Vec256<T> v) {
  const Half<decltype(d)> dh;
  const Vec128<T> lo = MaxOfLanes(dh, Max(v.v0, v.v1));
  return Combine(d, lo, lo);
}

// ------------------------------ Lt128

template <typename T>
HWY_INLINE Mask256<T> Lt128(Full256<T> d, Vec256<T> a, Vec256<T> b) {
  const Half<decltype(d)> dh;
  Mask256<T> ret;
  ret.m0 = Lt128(dh, a.v0, b.v0);
  ret.m1 = Lt128(dh, a.v1, b.v1);
  return ret;
}

template <typename T>
HWY_INLINE Mask256<T> Lt128Upper(Full256<T> d, Vec256<T> a, Vec256<T> b) {
  const Half<decltype(d)> dh;
  Mask256<T> ret;
  ret.m0 = Lt128Upper(dh, a.v0, b.v0);
  ret.m1 = Lt128Upper(dh, a.v1, b.v1);
  return ret;
}

template <typename T>
HWY_INLINE Mask256<T> Eq128(Full256<T> d, Vec256<T> a, Vec256<T> b) {
  const Half<decltype(d)> dh;
  Mask256<T> ret;
  ret.m0 = Eq128(dh, a.v0, b.v0);
  ret.m1 = Eq128(dh, a.v1, b.v1);
  return ret;
}

template <typename T>
HWY_INLINE Mask256<T> Eq128Upper(Full256<T> d, Vec256<T> a, Vec256<T> b) {
  const Half<decltype(d)> dh;
  Mask256<T> ret;
  ret.m0 = Eq128Upper(dh, a.v0, b.v0);
  ret.m1 = Eq128Upper(dh, a.v1, b.v1);
  return ret;
}

template <typename T>
HWY_INLINE Mask256<T> Ne128(Full256<T> d, Vec256<T> a, Vec256<T> b) {
  const Half<decltype(d)> dh;
  Mask256<T> ret;
  ret.m0 = Ne128(dh, a.v0, b.v0);
  ret.m1 = Ne128(dh, a.v1, b.v1);
  return ret;
}

template <typename T>
HWY_INLINE Mask256<T> Ne128Upper(Full256<T> d, Vec256<T> a, Vec256<T> b) {
  const Half<decltype(d)> dh;
  Mask256<T> ret;
  ret.m0 = Ne128Upper(dh, a.v0, b.v0);
  ret.m1 = Ne128Upper(dh, a.v1, b.v1);
  return ret;
}

template <typename T>
HWY_INLINE Vec256<T> Min128(Full256<T> d, Vec256<T> a, Vec256<T> b) {
  const Half<decltype(d)> dh;
  Vec256<T> ret;
  ret.v0 = Min128(dh, a.v0, b.v0);
  ret.v1 = Min128(dh, a.v1, b.v1);
  return ret;
}

template <typename T>
HWY_INLINE Vec256<T> Max128(Full256<T> d, Vec256<T> a, Vec256<T> b) {
  const Half<decltype(d)> dh;
  Vec256<T> ret;
  ret.v0 = Max128(dh, a.v0, b.v0);
  ret.v1 = Max128(dh, a.v1, b.v1);
  return ret;
}

template <typename T>
HWY_INLINE Vec256<T> Min128Upper(Full256<T> d, Vec256<T> a, Vec256<T> b) {
  const Half<decltype(d)> dh;
  Vec256<T> ret;
  ret.v0 = Min128Upper(dh, a.v0, b.v0);
  ret.v1 = Min128Upper(dh, a.v1, b.v1);
  return ret;
}

template <typename T>
HWY_INLINE Vec256<T> Max128Upper(Full256<T> d, Vec256<T> a, Vec256<T> b) {
  const Half<decltype(d)> dh;
  Vec256<T> ret;
  ret.v0 = Max128Upper(dh, a.v0, b.v0);
  ret.v1 = Max128Upper(dh, a.v1, b.v1);
  return ret;
}

// NOLINTNEXTLINE(google-readability-namespace-comments)
}  // namespace HWY_NAMESPACE
}  // namespace hwy
HWY_AFTER_NAMESPACE();
