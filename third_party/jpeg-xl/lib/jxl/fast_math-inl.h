// Copyright (c) the JPEG XL Project
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

// Fast SIMD math ops (log2, encoder only, cos, erf for splines)

#if defined(LIB_JXL_FAST_MATH_INL_H_) == defined(HWY_TARGET_TOGGLE)
#ifdef LIB_JXL_FAST_MATH_INL_H_
#undef LIB_JXL_FAST_MATH_INL_H_
#else
#define LIB_JXL_FAST_MATH_INL_H_
#endif

#include <hwy/highway.h>

#include "lib/jxl/common.h"
#include "lib/jxl/rational_polynomial-inl.h"
HWY_BEFORE_NAMESPACE();
namespace jxl {
namespace HWY_NAMESPACE {

// These templates are not found via ADL.
using hwy::HWY_NAMESPACE::Rebind;
using hwy::HWY_NAMESPACE::ShiftLeft;
using hwy::HWY_NAMESPACE::ShiftRight;

// Computes base-2 logarithm like std::log2. Undefined if negative / NaN.
// L1 error ~3.9E-6
template <class DF, class V>
V FastLog2f(const DF df, V x) {
  // 2,2 rational polynomial approximation of std::log1p(x) / std::log(2).
  HWY_ALIGN const float p[4 * (2 + 1)] = {HWY_REP4(-1.8503833400518310E-06f),
                                          HWY_REP4(1.4287160470083755E+00f),
                                          HWY_REP4(7.4245873327820566E-01f)};
  HWY_ALIGN const float q[4 * (2 + 1)] = {HWY_REP4(9.9032814277590719E-01f),
                                          HWY_REP4(1.0096718572241148E+00f),
                                          HWY_REP4(1.7409343003366853E-01f)};

  const Rebind<int32_t, DF> di;
  const auto x_bits = BitCast(di, x);

  // Range reduction to [-1/3, 1/3] - 3 integer, 2 float ops
  const auto exp_bits = x_bits - Set(di, 0x3f2aaaab);  // = 2/3
  // Shifted exponent = log2; also used to clear mantissa.
  const auto exp_shifted = ShiftRight<23>(exp_bits);
  const auto mantissa = BitCast(df, x_bits - ShiftLeft<23>(exp_shifted));
  const auto exp_val = ConvertTo(df, exp_shifted);
  return EvalRationalPolynomial(df, mantissa - Set(df, 1.0f), p, q) + exp_val;
}

// max relative error ~3e-7
template <class DF, class V>
V FastPow2f(const DF df, V x) {
  const Rebind<int32_t, DF> di;
  auto floorx = Floor(x);
  auto exp = BitCast(df, ShiftLeft<23>(ConvertTo(di, floorx) + Set(di, 127)));
  auto frac = x - floorx;
  auto num = frac + Set(df, 1.01749063e+01);
  num = MulAdd(num, frac, Set(df, 4.88687798e+01));
  num = MulAdd(num, frac, Set(df, 9.85506591e+01));
  num = num * exp;
  auto den = MulAdd(frac, Set(df, 2.10242958e-01), Set(df, -2.22328856e-02));
  den = MulAdd(den, frac, Set(df, -1.94414990e+01));
  den = MulAdd(den, frac, Set(df, 9.85506633e+01));
  return num / den;
}

// max relative error ~3e-5
template <class DF, class V>
V FastPowf(const DF df, V base, V exponent) {
  return FastPow2f(df, FastLog2f(df, base) * exponent);
}

// Computes cosine like std::cos.
// L1 error 7e-5.
template <class DF, class V>
V FastCosf(const DF df, V x) {
  // Step 1: range reduction to [0, 2pi)
  const auto pi2 = Set(df, kPi * 2.0f);
  const auto pi2_inv = Set(df, 0.5f / kPi);
  const auto npi2 = Floor(x * pi2_inv) * pi2;
  const auto xmodpi2 = x - npi2;
  // Step 2: range reduction to [0, pi]
  const auto x_pi = Min(xmodpi2, pi2 - xmodpi2);
  // Step 3: range reduction to [0, pi/2]
  const auto above_pihalf = x_pi >= Set(df, kPi / 2.0f);
  const auto x_pihalf = IfThenElse(above_pihalf, Set(df, kPi) - x_pi, x_pi);
  // Step 4: Taylor-like approximation, scaled by 2**0.75 to make angle
  // duplication steps faster, on x/4.
  const auto xs = x_pihalf * Set(df, 0.25f);
  const auto x2 = xs * xs;
  const auto x4 = x2 * x2;
  const auto cosx_prescaling =
      MulAdd(x4, Set(df, 0.06960438),
             MulAdd(x2, Set(df, -0.84087373), Set(df, 1.68179268)));
  // Step 5: angle duplication.
  const auto cosx_scale1 =
      MulAdd(cosx_prescaling, cosx_prescaling, Set(df, -1.414213562));
  const auto cosx_scale2 = MulAdd(cosx_scale1, cosx_scale1, Set(df, -1));
  // Step 6: change sign if needed.
  const Rebind<uint32_t, DF> du;
  auto signbit = ShiftLeft<31>(BitCast(du, VecFromMask(above_pihalf)));
  return BitCast(df, signbit ^ BitCast(du, cosx_scale2));
}

// Computes the error function like std::erf.
// L1 error 7e-4.
template <class DF, class V>
V FastErff(const DF df, V x) {
  // Formula from
  // https://en.wikipedia.org/wiki/Error_function#Numerical_approximations
  // but constants have been recomputed.
  const auto xle0 = x <= Zero(df);
  const auto absx = Abs(x);
  // Compute 1 - 1 / ((((x * a + b) * x + c) * x + d) * x + 1)**4
  const auto denom1 =
      MulAdd(absx, Set(df, 7.77394369e-02), Set(df, 2.05260015e-04));
  const auto denom2 = MulAdd(denom1, absx, Set(df, 2.32120216e-01));
  const auto denom3 = MulAdd(denom2, absx, Set(df, 2.77820801e-01));
  const auto denom4 = MulAdd(denom3, absx, Set(df, 1.0f));
  const auto denom5 = denom4 * denom4;
  const auto inv_denom5 = Set(df, 1.0f) / denom5;
  const auto result = NegMulAdd(inv_denom5, inv_denom5, Set(df, 1.0f));
  // Change sign if needed.
  const Rebind<uint32_t, DF> du;
  auto signbit = ShiftLeft<31>(BitCast(du, VecFromMask(xle0)));
  return BitCast(df, signbit ^ BitCast(du, result));
}

inline float FastLog2f(float f) {
  HWY_CAPPED(float, 1) D;
  return GetLane(FastLog2f(D, Set(D, f)));
}

inline float FastPow2f(float f) {
  HWY_CAPPED(float, 1) D;
  return GetLane(FastPow2f(D, Set(D, f)));
}

inline float FastPowf(float b, float e) {
  HWY_CAPPED(float, 1) D;
  return GetLane(FastPowf(D, Set(D, b), Set(D, e)));
}

inline float FastCosf(float f) {
  HWY_CAPPED(float, 1) D;
  return GetLane(FastCosf(D, Set(D, f)));
}

inline float FastErff(float f) {
  HWY_CAPPED(float, 1) D;
  return GetLane(FastErff(D, Set(D, f)));
}

// NOLINTNEXTLINE(google-readability-namespace-comments)
}  // namespace HWY_NAMESPACE
}  // namespace jxl
HWY_AFTER_NAMESPACE();

#endif  // LIB_JXL_FAST_MATH_INL_H_

#if HWY_ONCE

namespace jxl {
inline float FastLog2f(float f) { return HWY_STATIC_DISPATCH(FastLog2f)(f); }
inline float FastPow2f(float f) { return HWY_STATIC_DISPATCH(FastPow2f)(f); }
inline float FastPowf(float b, float e) {
  return HWY_STATIC_DISPATCH(FastPowf)(b, e);
}
inline float FastCosf(float f) { return HWY_STATIC_DISPATCH(FastCosf)(f); }
inline float FastErff(float f) { return HWY_STATIC_DISPATCH(FastErff)(f); }
}  // namespace jxl

#endif  // HWY_ONCE
