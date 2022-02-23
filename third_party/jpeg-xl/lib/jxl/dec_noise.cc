// Copyright (c) the JPEG XL Project Authors. All rights reserved.
//
// Use of this source code is governed by a BSD-style
// license that can be found in the LICENSE file.

#include "lib/jxl/dec_noise.h"

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

#include <algorithm>
#include <numeric>
#include <utility>

#undef HWY_TARGET_INCLUDE
#define HWY_TARGET_INCLUDE "lib/jxl/dec_noise.cc"
#include <hwy/foreach_target.h>
#include <hwy/highway.h>

#include "lib/jxl/base/compiler_specific.h"
#include "lib/jxl/chroma_from_luma.h"
#include "lib/jxl/image_ops.h"
#include "lib/jxl/opsin_params.h"
#include "lib/jxl/sanitizers.h"
#include "lib/jxl/xorshift128plus-inl.h"
HWY_BEFORE_NAMESPACE();
namespace jxl {
namespace HWY_NAMESPACE {

// These templates are not found via ADL.
using hwy::HWY_NAMESPACE::ShiftRight;
using hwy::HWY_NAMESPACE::Vec;

using D = HWY_CAPPED(float, kBlockDim);
using DI = hwy::HWY_NAMESPACE::Rebind<int, D>;
using DI8 = hwy::HWY_NAMESPACE::Repartition<uint8_t, D>;

// Converts one vector's worth of random bits to floats in [1, 2).
// NOTE: as the convolution kernel sums to 0, it doesn't matter if inputs are in
// [0, 1) or in [1, 2).
void BitsToFloat(const uint32_t* JXL_RESTRICT random_bits,
                 float* JXL_RESTRICT floats) {
  const HWY_FULL(float) df;
  const HWY_FULL(uint32_t) du;

  const auto bits = Load(du, random_bits);
  // 1.0 + 23 random mantissa bits = [1, 2)
  const auto rand12 = BitCast(df, ShiftRight<9>(bits) | Set(du, 0x3F800000));
  Store(rand12, df, floats);
}

void RandomImage(Xorshift128Plus* rng, const Rect& rect,
                 ImageF* JXL_RESTRICT noise) {
  const size_t xsize = rect.xsize();
  const size_t ysize = rect.ysize();

  // May exceed the vector size, hence we have two loops over x below.
  constexpr size_t kFloatsPerBatch =
      Xorshift128Plus::N * sizeof(uint64_t) / sizeof(float);
  HWY_ALIGN uint64_t batch[Xorshift128Plus::N];

  const HWY_FULL(float) df;
  const size_t N = Lanes(df);

  for (size_t y = 0; y < ysize; ++y) {
    float* JXL_RESTRICT row = rect.Row(noise, y);

    size_t x = 0;
    // Only entire batches (avoids exceeding the image padding).
    for (; x + kFloatsPerBatch <= xsize; x += kFloatsPerBatch) {
      rng->Fill(batch);
      for (size_t i = 0; i < kFloatsPerBatch; i += Lanes(df)) {
        BitsToFloat(reinterpret_cast<const uint32_t*>(batch) + i, row + x + i);
      }
    }

    // Any remaining pixels, rounded up to vectors (safe due to padding).
    rng->Fill(batch);
    size_t batch_pos = 0;  // < kFloatsPerBatch
    for (; x < xsize; x += N) {
      BitsToFloat(reinterpret_cast<const uint32_t*>(batch) + batch_pos,
                  row + x);
      batch_pos += N;
    }
  }
}

// [0, max_value]
template <class D, class V>
static HWY_INLINE V Clamp0ToMax(D d, const V x, const V max_value) {
  const auto clamped = Min(x, max_value);
  return ZeroIfNegative(clamped);
}

// x is in [0+delta, 1+delta], delta ~= 0.06
template <class StrengthEval>
typename StrengthEval::V NoiseStrength(const StrengthEval& eval,
                                       const typename StrengthEval::V x) {
  return Clamp0ToMax(D(), eval(x), Set(D(), 1.0f));
}

// TODO(veluca): SIMD-fy.
class StrengthEvalLut {
 public:
  using V = Vec<D>;

  explicit StrengthEvalLut(const NoiseParams& noise_params)
#if HWY_TARGET == HWY_SCALAR
      : noise_params_(noise_params)
#endif
  {
#if HWY_TARGET != HWY_SCALAR
    uint32_t lut[8];
    memcpy(lut, noise_params.lut, sizeof(lut));
    for (size_t i = 0; i < 8; i++) {
      low16_lut[2 * i] = (lut[i] >> 0) & 0xFF;
      low16_lut[2 * i + 1] = (lut[i] >> 8) & 0xFF;
      high16_lut[2 * i] = (lut[i] >> 16) & 0xFF;
      high16_lut[2 * i + 1] = (lut[i] >> 24) & 0xFF;
    }
#endif
  }

  V operator()(const V vx) const {
    constexpr size_t kScale = NoiseParams::kNumNoisePoints - 2;
    auto scaled_vx = Max(Zero(D()), vx * Set(D(), kScale));
    auto floor_x = Floor(scaled_vx);
    auto frac_x = scaled_vx - floor_x;
    floor_x = IfThenElse(scaled_vx >= Set(D(), kScale), Set(D(), kScale - 1),
                         floor_x);
    frac_x = IfThenElse(scaled_vx >= Set(D(), kScale), Set(D(), 1), frac_x);
    auto floor_x_int = ConvertTo(DI(), floor_x);
#if HWY_TARGET == HWY_SCALAR
    auto low = Set(D(), noise_params_.lut[floor_x_int.raw]);
    auto hi = Set(D(), noise_params_.lut[floor_x_int.raw + 1]);
#else
    // Set each lane's bytes to {0, 0, 2x+1, 2x}.
    auto floorx_indices_low =
        floor_x_int * Set(DI(), 0x0202) + Set(DI(), 0x0100);
    // Set each lane's bytes to {2x+1, 2x, 0, 0}.
    auto floorx_indices_hi =
        floor_x_int * Set(DI(), 0x02020000) + Set(DI(), 0x01000000);
    // load LUT
    auto low16 = BitCast(DI(), LoadDup128(DI8(), low16_lut));
    auto lowm = Set(DI(), 0xFFFF);
    auto hi16 = BitCast(DI(), LoadDup128(DI8(), high16_lut));
    auto him = Set(DI(), 0xFFFF0000);
    // low = noise_params.lut[floor_x]
    auto low =
        BitCast(D(), (TableLookupBytes(low16, floorx_indices_low) & lowm) |
                         (TableLookupBytes(hi16, floorx_indices_hi) & him));
    // hi = noise_params.lut[floor_x+1]
    floorx_indices_low += Set(DI(), 0x0202);
    floorx_indices_hi += Set(DI(), 0x02020000);
    auto hi =
        BitCast(D(), (TableLookupBytes(low16, floorx_indices_low) & lowm) |
                         (TableLookupBytes(hi16, floorx_indices_hi) & him));
#endif
    return MulAdd(hi - low, frac_x, low);
  }

 private:
#if HWY_TARGET != HWY_SCALAR
  // noise_params.lut transformed into two 16-bit lookup tables.
  HWY_ALIGN uint8_t high16_lut[16];
  HWY_ALIGN uint8_t low16_lut[16];
#else
  const NoiseParams& noise_params_;
#endif
};

template <class D>
void AddNoiseToRGB(const D d, const Vec<D> rnd_noise_r,
                   const Vec<D> rnd_noise_g, const Vec<D> rnd_noise_cor,
                   const Vec<D> noise_strength_g, const Vec<D> noise_strength_r,
                   float ytox, float ytob, float* JXL_RESTRICT out_x,
                   float* JXL_RESTRICT out_y, float* JXL_RESTRICT out_b) {
  const auto kRGCorr = Set(d, 0.9921875f);   // 127/128
  const auto kRGNCorr = Set(d, 0.0078125f);  // 1/128

  const auto red_noise = kRGNCorr * rnd_noise_r * noise_strength_r +
                         kRGCorr * rnd_noise_cor * noise_strength_r;
  const auto green_noise = kRGNCorr * rnd_noise_g * noise_strength_g +
                           kRGCorr * rnd_noise_cor * noise_strength_g;

  auto vx = Load(d, out_x);
  auto vy = Load(d, out_y);
  auto vb = Load(d, out_b);

  vx += red_noise - green_noise + Set(d, ytox) * (red_noise + green_noise);
  vy += red_noise + green_noise;
  vb += Set(d, ytob) * (red_noise + green_noise);

  Store(vx, d, out_x);
  Store(vy, d, out_y);
  Store(vb, d, out_b);
}

void AddNoise(const NoiseParams& noise_params, const Rect& noise_rect,
              const Image3F& noise, const Rect& opsin_rect,
              const ColorCorrelationMap& cmap, Image3F* opsin) {
  if (!noise_params.HasAny()) return;
  const StrengthEvalLut noise_model(noise_params);
  D d;
  const auto half = Set(d, 0.5f);

  const size_t xsize = opsin_rect.xsize();
  const size_t ysize = opsin_rect.ysize();

  // With the prior subtract-random Laplacian approximation, rnd_* ranges were
  // about [-1.5, 1.6]; Laplacian3 about doubles this to [-3.6, 3.6], so the
  // normalizer is half of what it was before (0.5).
  const auto norm_const = Set(d, 0.22f);

  float ytox = cmap.YtoXRatio(0);
  float ytob = cmap.YtoBRatio(0);

  const size_t xsize_v = RoundUpTo(xsize, Lanes(d));

  for (size_t y = 0; y < ysize; ++y) {
    float* JXL_RESTRICT row_x = opsin_rect.PlaneRow(opsin, 0, y);
    float* JXL_RESTRICT row_y = opsin_rect.PlaneRow(opsin, 1, y);
    float* JXL_RESTRICT row_b = opsin_rect.PlaneRow(opsin, 2, y);
    const float* JXL_RESTRICT row_rnd_r = noise_rect.ConstPlaneRow(noise, 0, y);
    const float* JXL_RESTRICT row_rnd_g = noise_rect.ConstPlaneRow(noise, 1, y);
    const float* JXL_RESTRICT row_rnd_c = noise_rect.ConstPlaneRow(noise, 2, y);
    // Needed by the calls to Floor() in StrengthEvalLut. Only arithmetic and
    // shuffles are otherwise done on the data, so this is safe.
    msan::UnpoisonMemory(row_x + xsize, (xsize_v - xsize) * sizeof(float));
    msan::UnpoisonMemory(row_y + xsize, (xsize_v - xsize) * sizeof(float));
    for (size_t x = 0; x < xsize; x += Lanes(d)) {
      const auto vx = Load(d, row_x + x);
      const auto vy = Load(d, row_y + x);
      const auto in_g = vy - vx;
      const auto in_r = vy + vx;
      const auto noise_strength_g = NoiseStrength(noise_model, in_g * half);
      const auto noise_strength_r = NoiseStrength(noise_model, in_r * half);
      const auto addit_rnd_noise_red = Load(d, row_rnd_r + x) * norm_const;
      const auto addit_rnd_noise_green = Load(d, row_rnd_g + x) * norm_const;
      const auto addit_rnd_noise_correlated =
          Load(d, row_rnd_c + x) * norm_const;
      AddNoiseToRGB(D(), addit_rnd_noise_red, addit_rnd_noise_green,
                    addit_rnd_noise_correlated, noise_strength_g,
                    noise_strength_r, ytox, ytob, row_x + x, row_y + x,
                    row_b + x);
    }
    msan::PoisonMemory(row_x + xsize, (xsize_v - xsize) * sizeof(float));
    msan::PoisonMemory(row_y + xsize, (xsize_v - xsize) * sizeof(float));
    msan::PoisonMemory(row_b + xsize, (xsize_v - xsize) * sizeof(float));
  }
}

void RandomImage3(size_t seed, const Rect& rect, Image3F* JXL_RESTRICT noise) {
  HWY_ALIGN Xorshift128Plus rng(seed);
  RandomImage(&rng, rect, &noise->Plane(0));
  RandomImage(&rng, rect, &noise->Plane(1));
  RandomImage(&rng, rect, &noise->Plane(2));
}

// NOLINTNEXTLINE(google-readability-namespace-comments)
}  // namespace HWY_NAMESPACE
}  // namespace jxl
HWY_AFTER_NAMESPACE();

#if HWY_ONCE
namespace jxl {

HWY_EXPORT(AddNoise);
void AddNoise(const NoiseParams& noise_params, const Rect& noise_rect,
              const Image3F& noise, const Rect& opsin_rect,
              const ColorCorrelationMap& cmap, Image3F* opsin) {
  return HWY_DYNAMIC_DISPATCH(AddNoise)(noise_params, noise_rect, noise,
                                        opsin_rect, cmap, opsin);
}

HWY_EXPORT(RandomImage3);
void RandomImage3(size_t seed, const Rect& rect, Image3F* JXL_RESTRICT noise) {
  return HWY_DYNAMIC_DISPATCH(RandomImage3)(seed, rect, noise);
}

void DecodeFloatParam(float precision, float* val, BitReader* br) {
  const int absval_quant = br->ReadFixedBits<10>();
  *val = absval_quant / precision;
}

Status DecodeNoise(BitReader* br, NoiseParams* noise_params) {
  for (float& i : noise_params->lut) {
    DecodeFloatParam(kNoisePrecision, &i, br);
  }
  return true;
}

}  // namespace jxl
#endif  // HWY_ONCE
