// Copyright (c) the JPEG XL Project Authors. All rights reserved.
//
// Use of this source code is governed by a BSD-style
// license that can be found in the LICENSE file.

#include "lib/jpegli/adaptive_quantization.h"

#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>

#include <algorithm>
#include <cmath>
#include <string>
#include <vector>

#undef HWY_TARGET_INCLUDE
#define HWY_TARGET_INCLUDE "lib/jpegli/adaptive_quantization.cc"
#include <hwy/foreach_target.h>
#include <hwy/highway.h>

#include "lib/jpegli/encode_internal.h"
#include "lib/jxl/base/compiler_specific.h"
#include "lib/jxl/base/data_parallel.h"
#include "lib/jxl/base/status.h"
#include "lib/jxl/image.h"
#include "lib/jxl/image_ops.h"
HWY_BEFORE_NAMESPACE();
namespace jpegli {
namespace HWY_NAMESPACE {
namespace {

// These templates are not found via ADL.
using hwy::HWY_NAMESPACE::AbsDiff;
using hwy::HWY_NAMESPACE::Add;
using hwy::HWY_NAMESPACE::And;
using hwy::HWY_NAMESPACE::Div;
using hwy::HWY_NAMESPACE::Floor;
using hwy::HWY_NAMESPACE::GetLane;
using hwy::HWY_NAMESPACE::Max;
using hwy::HWY_NAMESPACE::Min;
using hwy::HWY_NAMESPACE::Mul;
using hwy::HWY_NAMESPACE::MulAdd;
using hwy::HWY_NAMESPACE::NegMulAdd;
using hwy::HWY_NAMESPACE::Rebind;
using hwy::HWY_NAMESPACE::ShiftLeft;
using hwy::HWY_NAMESPACE::ShiftRight;
using hwy::HWY_NAMESPACE::Sqrt;
using hwy::HWY_NAMESPACE::Sub;
using hwy::HWY_NAMESPACE::ZeroIfNegative;

static constexpr size_t kEncTileDim = 64;
;
static constexpr size_t kEncTileDimInBlocks = kEncTileDim / jxl::kBlockDim;

// Primary template: default to actual division.
template <typename T, class V>
struct FastDivision {
  HWY_INLINE V operator()(const V n, const V d) const { return n / d; }
};
// Partial specialization for float vectors.
template <class V>
struct FastDivision<float, V> {
  // One Newton-Raphson iteration.
  static HWY_INLINE V ReciprocalNR(const V x) {
    const auto rcp = ApproximateReciprocal(x);
    const auto sum = Add(rcp, rcp);
    const auto x_rcp = Mul(x, rcp);
    return NegMulAdd(x_rcp, rcp, sum);
  }

  V operator()(const V n, const V d) const {
#if 1  // Faster on SKX
    return Div(n, d);
#else
    return n * ReciprocalNR(d);
#endif
  }
};

// Approximates smooth functions via rational polynomials (i.e. dividing two
// polynomials). Evaluates polynomials via Horner's scheme, which is faster than
// Clenshaw recurrence for Chebyshev polynomials. LoadDup128 allows us to
// specify constants (replicated 4x) independently of the lane count.
template <size_t NP, size_t NQ, class D, class V, typename T>
HWY_INLINE HWY_MAYBE_UNUSED V EvalRationalPolynomial(const D d, const V x,
                                                     const T (&p)[NP],
                                                     const T (&q)[NQ]) {
  constexpr size_t kDegP = NP / 4 - 1;
  constexpr size_t kDegQ = NQ / 4 - 1;
  auto yp = LoadDup128(d, &p[kDegP * 4]);
  auto yq = LoadDup128(d, &q[kDegQ * 4]);
  // We use pointer arithmetic to refer to &p[(kDegP - n) * 4] to avoid a
  // compiler warning that the index is out of bounds since we are already
  // checking that it is not out of bounds with (kDegP >= n) and the access
  // will be optimized away. Similarly with q and kDegQ.
  HWY_FENCE;
  if (kDegP >= 1) yp = MulAdd(yp, x, LoadDup128(d, p + ((kDegP - 1) * 4)));
  if (kDegQ >= 1) yq = MulAdd(yq, x, LoadDup128(d, q + ((kDegQ - 1) * 4)));
  HWY_FENCE;
  if (kDegP >= 2) yp = MulAdd(yp, x, LoadDup128(d, p + ((kDegP - 2) * 4)));
  if (kDegQ >= 2) yq = MulAdd(yq, x, LoadDup128(d, q + ((kDegQ - 2) * 4)));
  HWY_FENCE;
  if (kDegP >= 3) yp = MulAdd(yp, x, LoadDup128(d, p + ((kDegP - 3) * 4)));
  if (kDegQ >= 3) yq = MulAdd(yq, x, LoadDup128(d, q + ((kDegQ - 3) * 4)));
  HWY_FENCE;
  if (kDegP >= 4) yp = MulAdd(yp, x, LoadDup128(d, p + ((kDegP - 4) * 4)));
  if (kDegQ >= 4) yq = MulAdd(yq, x, LoadDup128(d, q + ((kDegQ - 4) * 4)));
  HWY_FENCE;
  if (kDegP >= 5) yp = MulAdd(yp, x, LoadDup128(d, p + ((kDegP - 5) * 4)));
  if (kDegQ >= 5) yq = MulAdd(yq, x, LoadDup128(d, q + ((kDegQ - 5) * 4)));
  HWY_FENCE;
  if (kDegP >= 6) yp = MulAdd(yp, x, LoadDup128(d, p + ((kDegP - 6) * 4)));
  if (kDegQ >= 6) yq = MulAdd(yq, x, LoadDup128(d, q + ((kDegQ - 6) * 4)));
  HWY_FENCE;
  if (kDegP >= 7) yp = MulAdd(yp, x, LoadDup128(d, p + ((kDegP - 7) * 4)));
  if (kDegQ >= 7) yq = MulAdd(yq, x, LoadDup128(d, q + ((kDegQ - 7) * 4)));

  return FastDivision<T, V>()(yp, yq);
}

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
  const auto exp_bits = Sub(x_bits, Set(di, 0x3f2aaaab));  // = 2/3
  // Shifted exponent = log2; also used to clear mantissa.
  const auto exp_shifted = ShiftRight<23>(exp_bits);
  const auto mantissa = BitCast(df, Sub(x_bits, ShiftLeft<23>(exp_shifted)));
  const auto exp_val = ConvertTo(df, exp_shifted);
  return Add(EvalRationalPolynomial(df, Sub(mantissa, Set(df, 1.0f)), p, q),
             exp_val);
}

// max relative error ~3e-7
template <class DF, class V>
V FastPow2f(const DF df, V x) {
  const Rebind<int32_t, DF> di;
  auto floorx = Floor(x);
  auto exp =
      BitCast(df, ShiftLeft<23>(Add(ConvertTo(di, floorx), Set(di, 127))));
  auto frac = Sub(x, floorx);
  auto num = Add(frac, Set(df, 1.01749063e+01));
  num = MulAdd(num, frac, Set(df, 4.88687798e+01));
  num = MulAdd(num, frac, Set(df, 9.85506591e+01));
  num = Mul(num, exp);
  auto den = MulAdd(frac, Set(df, 2.10242958e-01), Set(df, -2.22328856e-02));
  den = MulAdd(den, frac, Set(df, -1.94414990e+01));
  den = MulAdd(den, frac, Set(df, 9.85506633e+01));
  return Div(num, den);
}

inline float FastPow2f(float f) {
  HWY_CAPPED(float, 1) D;
  return GetLane(FastPow2f(D, Set(D, f)));
}

// The following functions modulate an exponent (out_val) and return the updated
// value. Their descriptor is limited to 8 lanes for 8x8 blocks.

template <class D, class V>
V ComputeMask(const D d, const V out_val) {
  const auto kBase = Set(d, -0.74174993f);
  const auto kMul4 = Set(d, 3.2353257320940401f);
  const auto kMul2 = Set(d, 12.906028311180409f);
  const auto kOffset2 = Set(d, 305.04035728311436f);
  const auto kMul3 = Set(d, 5.0220313103171232f);
  const auto kOffset3 = Set(d, 2.1925739705298404f);
  const auto kOffset4 = Mul(Set(d, 0.25f), kOffset3);
  const auto kMul0 = Set(d, 0.74760422233706747f);
  const auto k1 = Set(d, 1.0f);

  // Avoid division by zero.
  const auto v1 = Max(Mul(out_val, kMul0), Set(d, 1e-3f));
  const auto v2 = Div(k1, Add(v1, kOffset2));
  const auto v3 = Div(k1, MulAdd(v1, v1, kOffset3));
  const auto v4 = Div(k1, MulAdd(v1, v1, kOffset4));
  // TODO(jyrki):
  // A log or two here could make sense. In butteraugli we have effectively
  // log(log(x + C)) for this kind of use, as a single log is used in
  // saturating visual masking and here the modulation values are exponential,
  // another log would counter that.
  return Add(kBase, MulAdd(kMul4, v4, MulAdd(kMul2, v2, Mul(kMul3, v3))));
}

// mul and mul2 represent a scaling difference between jxl and butteraugli.
static const float kSGmul = 226.0480446705883f;
static const float kSGmul2 = 1.0f / 73.377132366608819f;
static const float kLog2 = 0.693147181f;
// Includes correction factor for std::log -> log2.
static const float kSGRetMul = kSGmul2 * 18.6580932135f * kLog2;
static const float kSGVOffset = 7.14672470003f;

template <bool invert, typename D, typename V>
V RatioOfDerivativesOfCubicRootToSimpleGamma(const D d, V v) {
  // The opsin space in jxl is the cubic root of photons, i.e., v * v * v
  // is related to the number of photons.
  //
  // SimpleGamma(v * v * v) is the psychovisual space in butteraugli.
  // This ratio allows quantization to move from jxl's opsin space to
  // butteraugli's log-gamma space.
  float kEpsilon = 1e-2;
  v = ZeroIfNegative(v);
  const auto kNumMul = Set(d, kSGRetMul * 3 * kSGmul);
  const auto kVOffset = Set(d, kSGVOffset * kLog2 + kEpsilon);
  const auto kDenMul = Set(d, kLog2 * kSGmul);

  const auto v2 = Mul(v, v);

  const auto num = MulAdd(kNumMul, v2, Set(d, kEpsilon));
  const auto den = MulAdd(Mul(kDenMul, v), v2, kVOffset);
  return invert ? Div(num, den) : Div(den, num);
}

template <bool invert = false>
static float RatioOfDerivativesOfCubicRootToSimpleGamma(float v) {
  using DScalar = HWY_CAPPED(float, 1);
  auto vscalar = Load(DScalar(), &v);
  return GetLane(
      RatioOfDerivativesOfCubicRootToSimpleGamma<invert>(DScalar(), vscalar));
}

// TODO(veluca): this function computes an approximation of the derivative of
// SimpleGamma with (f(x+eps)-f(x))/eps. Consider two-sided approximation or
// exact derivatives. For reference, SimpleGamma was:
/*
template <typename D, typename V>
V SimpleGamma(const D d, V v) {
  // A simple HDR compatible gamma function.
  const auto mul = Set(d, kSGmul);
  const auto kRetMul = Set(d, kSGRetMul);
  const auto kRetAdd = Set(d, kSGmul2 * -20.2789020414f);
  const auto kVOffset = Set(d, kSGVOffset);

  v *= mul;

  // This should happen rarely, but may lead to a NaN, which is rather
  // undesirable. Since negative photons don't exist we solve the NaNs by
  // clamping here.
  // TODO(veluca): with FastLog2f, this no longer leads to NaNs.
  v = ZeroIfNegative(v);
  return kRetMul * FastLog2f(d, v + kVOffset) + kRetAdd;
}
*/

template <class D, class V>
V GammaModulation(const D d, const size_t x, const size_t y,
                  const jxl::ImageF& xyb_y, const V out_val) {
  const float kBias = 0.16f;
  auto overall_ratio = Zero(d);
  auto bias = Set(d, kBias);
  for (size_t dy = 0; dy < 8; ++dy) {
    const float* const JXL_RESTRICT row_in_y = xyb_y.Row(y + dy);
    for (size_t dx = 0; dx < 8; dx += Lanes(d)) {
      const auto iny = Add(Load(d, row_in_y + x + dx), bias);
      const auto ratio_g =
          RatioOfDerivativesOfCubicRootToSimpleGamma</*invert=*/true>(d, iny);
      overall_ratio = Add(overall_ratio, ratio_g);
    }
  }
  overall_ratio = Mul(SumOfLanes(d, overall_ratio), Set(d, 1.0f / 64));
  // ideally -1.0, but likely optimal correction adds some entropy, so slightly
  // less than that.
  // ln(2) constant folded in because we want std::log but have FastLog2f.
  const auto kGam = Set(d, -0.15526878023684174f * 0.693147180559945f);
  return MulAdd(kGam, FastLog2f(d, overall_ratio), out_val);
}

// Change precision in 8x8 blocks that have high frequency content.
template <class D, class V>
V HfModulation(const D d, const size_t x, const size_t y,
               const jxl::ImageF& xyb, const V out_val) {
  // Zero out the invalid differences for the rightmost value per row.
  const Rebind<uint32_t, D> du;
  HWY_ALIGN constexpr uint32_t kMaskRight[jxl::kBlockDim] = {~0u, ~0u, ~0u, ~0u,
                                                             ~0u, ~0u, ~0u, 0};

  auto sum = Zero(d);  // sum of absolute differences with right and below

  for (size_t dy = 0; dy < 8; ++dy) {
    const float* JXL_RESTRICT row_in = xyb.Row(y + dy) + x;
    const float* JXL_RESTRICT row_in_next =
        dy == 7 ? row_in : xyb.Row(y + dy + 1) + x;

    // In SCALAR, there is no guarantee of having extra row padding.
    // Hence, we need to ensure we don't access pixels outside the row itself.
    // In SIMD modes, however, rows are padded, so it's safe to access one
    // garbage value after the row. The vector then gets masked with kMaskRight
    // to remove the influence of that value.
#if HWY_TARGET != HWY_SCALAR
    for (size_t dx = 0; dx < 8; dx += Lanes(d)) {
#else
    for (size_t dx = 0; dx < 7; dx += Lanes(d)) {
#endif
      const auto p = Load(d, row_in + dx);
      const auto pr = LoadU(d, row_in + dx + 1);
      const auto mask = BitCast(d, Load(du, kMaskRight + dx));
      sum = Add(sum, And(mask, AbsDiff(p, pr)));

      const auto pd = Load(d, row_in_next + dx);
      sum = Add(sum, AbsDiff(p, pd));
    }
#if HWY_TARGET == HWY_SCALAR
    const auto p = Load(d, row_in + 7);
    const auto pd = Load(d, row_in_next + 7);
    sum = Add(sum, AbsDiff(p, pd));
#endif
  }

  sum = SumOfLanes(d, sum);
  return MulAdd(sum, Set(d, -2.0052193233688884f / 112), out_val);
}

void PerBlockModulations(const float butteraugli_target,
                         const jxl::ImageF& xyb_y, const float scale,
                         const jxl::Rect& rect, jxl::ImageF* out) {
  JXL_ASSERT(DivCeil(xyb_y.xsize(), jxl::kBlockDim) == out->xsize());
  JXL_ASSERT(DivCeil(xyb_y.ysize(), jxl::kBlockDim) == out->ysize());

  float base_level = 0.48f * scale;
  float kDampenRampStart = 2.0f;
  float kDampenRampEnd = 14.0f;
  float dampen = 1.0f;
  if (butteraugli_target >= kDampenRampStart) {
    dampen = 1.0f - ((butteraugli_target - kDampenRampStart) /
                     (kDampenRampEnd - kDampenRampStart));
    if (dampen < 0) {
      dampen = 0;
    }
  }
  const float mul = scale * dampen;
  const float add = (1.0f - dampen) * base_level;
  for (size_t iy = rect.y0(); iy < rect.y0() + rect.ysize(); iy++) {
    const size_t y = iy * 8;
    float* const JXL_RESTRICT row_out = out->Row(iy);
    const HWY_CAPPED(float, jxl::kBlockDim) df;
    for (size_t ix = rect.x0(); ix < rect.x0() + rect.xsize(); ix++) {
      size_t x = ix * 8;
      auto out_val = Set(df, row_out[ix]);
      out_val = ComputeMask(df, out_val);
      out_val = HfModulation(df, x, y, xyb_y, out_val);
      out_val = GammaModulation(df, x, y, xyb_y, out_val);
      // We want multiplicative quantization field, so everything
      // until this point has been modulating the exponent.
      row_out[ix] = FastPow2f(GetLane(out_val) * 1.442695041f) * mul + add;
    }
  }
}

template <typename D, typename V>
V MaskingSqrt(const D d, V v) {
  static const float kLogOffset = 28;
  static const float kMul = 211.50759899638012f;
  const auto mul_v = Set(d, kMul * 1e8);
  const auto offset_v = Set(d, kLogOffset);
  return Mul(Set(d, 0.25f), Sqrt(MulAdd(v, Sqrt(mul_v), offset_v)));
}

float MaskingSqrt(const float v) {
  using DScalar = HWY_CAPPED(float, 1);
  auto vscalar = Load(DScalar(), &v);
  return GetLane(MaskingSqrt(DScalar(), vscalar));
}

void StoreMin4(const float v, float& min0, float& min1, float& min2,
               float& min3) {
  if (v < min3) {
    if (v < min0) {
      min3 = min2;
      min2 = min1;
      min1 = min0;
      min0 = v;
    } else if (v < min1) {
      min3 = min2;
      min2 = min1;
      min1 = v;
    } else if (v < min2) {
      min3 = min2;
      min2 = v;
    } else {
      min3 = v;
    }
  }
}

// Look for smooth areas near the area of degradation.
// If the areas are generally smooth, don't do masking.
// Output is downsampled 2x.
void FuzzyErosion(const jxl::Rect& from_rect, const jxl::ImageF& from,
                  const jxl::Rect& to_rect, jxl::ImageF* to) {
  const size_t xsize = from.xsize();
  const size_t ysize = from.ysize();
  constexpr int kStep = 1;
  static_assert(kStep == 1, "Step must be 1");
  JXL_ASSERT(to_rect.xsize() * 2 == from_rect.xsize());
  JXL_ASSERT(to_rect.ysize() * 2 == from_rect.ysize());
  for (size_t fy = 0; fy < from_rect.ysize(); ++fy) {
    size_t y = fy + from_rect.y0();
    size_t ym1 = y >= kStep ? y - kStep : y;
    size_t yp1 = y + kStep < ysize ? y + kStep : y;
    const float* rowt = from.Row(ym1);
    const float* row = from.Row(y);
    const float* rowb = from.Row(yp1);
    float* row_out = to_rect.Row(to, fy / 2);
    for (size_t fx = 0; fx < from_rect.xsize(); ++fx) {
      size_t x = fx + from_rect.x0();
      size_t xm1 = x >= kStep ? x - kStep : x;
      size_t xp1 = x + kStep < xsize ? x + kStep : x;
      float min0 = row[x];
      float min1 = row[xm1];
      float min2 = row[xp1];
      float min3 = rowt[xm1];
      // Sort the first four values.
      if (min0 > min1) std::swap(min0, min1);
      if (min0 > min2) std::swap(min0, min2);
      if (min0 > min3) std::swap(min0, min3);
      if (min1 > min2) std::swap(min1, min2);
      if (min1 > min3) std::swap(min1, min3);
      if (min2 > min3) std::swap(min2, min3);
      // The remaining five values of a 3x3 neighbourhood.
      StoreMin4(rowt[x], min0, min1, min2, min3);
      StoreMin4(rowt[xp1], min0, min1, min2, min3);
      StoreMin4(rowb[xm1], min0, min1, min2, min3);
      StoreMin4(rowb[x], min0, min1, min2, min3);
      StoreMin4(rowb[xp1], min0, min1, min2, min3);
      static const float kMul0 = 0.125f;
      static const float kMul1 = 0.075f;
      static const float kMul2 = 0.06f;
      static const float kMul3 = 0.05f;
      float v = kMul0 * min0 + kMul1 * min1 + kMul2 * min2 + kMul3 * min3;
      if (fx % 2 == 0 && fy % 2 == 0) {
        row_out[fx / 2] = v;
      } else {
        row_out[fx / 2] += v;
      }
    }
  }
}

struct AdaptiveQuantizationImpl {
  void Init(const jxl::ImageF& xyb_y) {
    JXL_DASSERT(xyb_y.xsize() % jxl::kBlockDim == 0);
    JXL_DASSERT(xyb_y.ysize() % jxl::kBlockDim == 0);
    const size_t xsize = xyb_y.xsize();
    const size_t ysize = xyb_y.ysize();
    aq_map = jxl::ImageF(xsize / jxl::kBlockDim, ysize / jxl::kBlockDim);
  }
  void PrepareBuffers(size_t num_threads) {
    diff_buffer = jxl::ImageF(kEncTileDim + 8, num_threads);
    for (size_t i = pre_erosion.size(); i < num_threads; i++) {
      pre_erosion.emplace_back(kEncTileDimInBlocks * 2 + 2,
                               kEncTileDimInBlocks * 2 + 2);
    }
  }

  void ComputeTile(float butteraugli_target, float scale,
                   const jxl::ImageF& xyb_y, const jxl::Rect& rect,
                   const int thread) {
    const size_t xsize = xyb_y.xsize();
    const size_t ysize = xyb_y.ysize();

    // The XYB gamma is 3.0 to be able to decode faster with two muls.
    // Butteraugli's gamma is matching the gamma of human eye, around 2.6.
    // We approximate the gamma difference by adding one cubic root into
    // the adaptive quantization. This gives us a total gamma of 2.6666
    // for quantization uses.
    const float match_gamma_offset = 0.019;

    const HWY_FULL(float) df;

    size_t y_start = rect.y0() * 8;
    size_t y_end = y_start + rect.ysize() * 8;

    size_t x0 = rect.x0() * 8;
    size_t x1 = x0 + rect.xsize() * 8;
    if (x0 != 0) x0 -= 4;
    if (x1 != xyb_y.xsize()) x1 += 4;
    if (y_start != 0) y_start -= 4;
    if (y_end != xyb_y.ysize()) y_end += 4;
    pre_erosion[thread].ShrinkTo((x1 - x0) / 4, (y_end - y_start) / 4);

    static const float limit = 0.2f;
    // Computes image (padded to multiple of 8x8) of local pixel differences.
    // Subsample both directions by 4.
    for (size_t y = y_start; y < y_end; ++y) {
      size_t y2 = y + 1 < ysize ? y + 1 : y;
      size_t y1 = y > 0 ? y - 1 : y;

      const float* row_in = xyb_y.Row(y);
      const float* row_in1 = xyb_y.Row(y1);
      const float* row_in2 = xyb_y.Row(y2);
      float* JXL_RESTRICT row_out = diff_buffer.Row(thread);

      auto scalar_pixel = [&](size_t x) {
        const size_t x2 = x + 1 < xsize ? x + 1 : x;
        const size_t x1 = x > 0 ? x - 1 : x;
        const float base =
            0.25f * (row_in2[x] + row_in1[x] + row_in[x1] + row_in[x2]);
        const float gammac = RatioOfDerivativesOfCubicRootToSimpleGamma(
            row_in[x] + match_gamma_offset);
        float diff = gammac * (row_in[x] - base);
        diff *= diff;
        if (diff >= limit) {
          diff = limit;
        }
        diff = MaskingSqrt(diff);
        if ((y % 4) != 0) {
          row_out[x - x0] += diff;
        } else {
          row_out[x - x0] = diff;
        }
      };

      size_t x = x0;
      // First pixel of the row.
      if (x0 == 0) {
        scalar_pixel(x0);
        ++x;
      }
      // SIMD
      const auto match_gamma_offset_v = Set(df, match_gamma_offset);
      const auto quarter = Set(df, 0.25f);
      for (; x + 1 + Lanes(df) < x1; x += Lanes(df)) {
        const auto in = LoadU(df, row_in + x);
        const auto in_r = LoadU(df, row_in + x + 1);
        const auto in_l = LoadU(df, row_in + x - 1);
        const auto in_t = LoadU(df, row_in2 + x);
        const auto in_b = LoadU(df, row_in1 + x);
        auto base = Mul(quarter, Add(Add(in_r, in_l), Add(in_t, in_b)));
        auto gammacv =
            RatioOfDerivativesOfCubicRootToSimpleGamma</*invert=*/false>(
                df, Add(in, match_gamma_offset_v));
        auto diff = Mul(gammacv, Sub(in, base));
        diff = Mul(diff, diff);
        diff = Min(diff, Set(df, limit));
        diff = MaskingSqrt(df, diff);
        if ((y & 3) != 0) {
          diff = Add(diff, LoadU(df, row_out + x - x0));
        }
        StoreU(diff, df, row_out + x - x0);
      }
      // Scalar
      for (; x < x1; ++x) {
        scalar_pixel(x);
      }
      if (y % 4 == 3) {
        float* row_dout = pre_erosion[thread].Row((y - y_start) / 4);
        for (size_t x = 0; x < (x1 - x0) / 4; x++) {
          row_dout[x] = (row_out[x * 4] + row_out[x * 4 + 1] +
                         row_out[x * 4 + 2] + row_out[x * 4 + 3]) *
                        0.25f;
        }
      }
    }
    jxl::Rect from_rect(x0 % 8 == 0 ? 0 : 1, y_start % 8 == 0 ? 0 : 1,
                        rect.xsize() * 2, rect.ysize() * 2);
    FuzzyErosion(from_rect, pre_erosion[thread], rect, &aq_map);
    PerBlockModulations(butteraugli_target, xyb_y, scale, rect, &aq_map);
  }
  std::vector<jxl::ImageF> pre_erosion;
  jxl::ImageF aq_map;
  jxl::ImageF diff_buffer;
};

jxl::ImageF AdaptiveQuantizationMap(const float butteraugli_target,
                                    const jxl::ImageF& xyb_y, float scale,
                                    jxl::ThreadPool* pool) {
  AdaptiveQuantizationImpl impl;
  impl.Init(xyb_y);
  const size_t xsize_blocks = xyb_y.xsize() / jxl::kBlockDim;
  const size_t ysize_blocks = xyb_y.ysize() / jxl::kBlockDim;
  JXL_CHECK(RunOnPool(
      pool, 0,
      DivCeil(xsize_blocks, kEncTileDimInBlocks) *
          DivCeil(ysize_blocks, kEncTileDimInBlocks),
      [&](const size_t num_threads) {
        impl.PrepareBuffers(num_threads);
        return true;
      },
      [&](const uint32_t tid, const size_t thread) {
        size_t n_enc_tiles = DivCeil(xsize_blocks, kEncTileDimInBlocks);
        size_t tx = tid % n_enc_tiles;
        size_t ty = tid / n_enc_tiles;
        size_t by0 = ty * kEncTileDimInBlocks;
        size_t by1 = std::min((ty + 1) * kEncTileDimInBlocks, ysize_blocks);
        size_t bx0 = tx * kEncTileDimInBlocks;
        size_t bx1 = std::min((tx + 1) * kEncTileDimInBlocks, xsize_blocks);
        jxl::Rect r(bx0, by0, bx1 - bx0, by1 - by0);
        impl.ComputeTile(butteraugli_target, scale, xyb_y, r, thread);
      },
      "AQ DiffPrecompute"));

  return std::move(impl).aq_map;
}

}  // namespace

// NOLINTNEXTLINE(google-readability-namespace-comments)
}  // namespace HWY_NAMESPACE
}  // namespace jpegli
HWY_AFTER_NAMESPACE();

#if HWY_ONCE
namespace jpegli {
HWY_EXPORT(AdaptiveQuantizationMap);

namespace {

constexpr float kDcQuantPow = 0.66f;
static const float kDcQuant = 1.1f;
static const float kAcQuant = 0.841f;

}  // namespace

float InitialQuantDC(float butteraugli_target) {
  const float kDcMul = 1.5;  // Butteraugli target where non-linearity kicks in.
  const float butteraugli_target_dc = std::max<float>(
      0.5f * butteraugli_target,
      std::min<float>(butteraugli_target,
                      kDcMul * std::pow((1.0f / kDcMul) * butteraugli_target,
                                        kDcQuantPow)));
  // We want the maximum DC value to be at most 2**15 * kInvDCQuant / quant_dc.
  // The maximum DC value might not be in the kXybRange because of inverse
  // gaborish, so we add some slack to the maximum theoretical quant obtained
  // this way (64).
  return std::min(kDcQuant / butteraugli_target_dc, 50.f);
}

jxl::ImageF InitialQuantField(const float butteraugli_target,
                              const jxl::ImageF& opsin_y, jxl::ThreadPool* pool,
                              float rescale) {
  const float quant_ac = kAcQuant / butteraugli_target;
  return HWY_DYNAMIC_DISPATCH(AdaptiveQuantizationMap)(
      butteraugli_target, opsin_y, quant_ac * rescale, pool);
}

void ComputeAdaptiveQuantField(j_compress_ptr cinfo) {
  jpeg_comp_master* m = cinfo->master;
  m->quant_field.Allocate(m->ysize_blocks, m->xsize_blocks);
  if (m->use_adaptive_quantization) {
    int y_channel = cinfo->jpeg_color_space == JCS_RGB && m->xyb_mode ? 1 : 0;
    jxl::ImageF input(m->xsize_blocks * DCTSIZE, m->ysize_blocks * DCTSIZE);
    for (size_t y = 0; y < input.ysize(); ++y) {
      memcpy(input.Row(y), m->input_buffer[y_channel].Row(y),
             input.xsize() * sizeof(float));
    }
    jxl::ImageF qf =
        jpegli::InitialQuantField(m->distance, input, nullptr, m->distance);
    float qfmin;
    ImageMinMax(qf, &qfmin, &m->quant_field_max);
    for (size_t y = 0; y < m->ysize_blocks; ++y) {
      m->quant_field.CopyRow(y, qf.Row(y), m->xsize_blocks);
    }
  } else {
    constexpr float kDefaultQuantFieldMax = 0.575f;
    m->quant_field_max = kDefaultQuantFieldMax;
    for (size_t y = 0; y < m->ysize_blocks; ++y) {
      m->quant_field.FillRow(y, m->quant_field_max, m->xsize_blocks);
    }
  }
}

}  // namespace jpegli
#endif  // HWY_ONCE
