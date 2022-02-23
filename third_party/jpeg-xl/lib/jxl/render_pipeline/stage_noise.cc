// Copyright (c) the JPEG XL Project Authors. All rights reserved.
//
// Use of this source code is governed by a BSD-style
// license that can be found in the LICENSE file.

#include "lib/jxl/render_pipeline/stage_noise.h"

#undef HWY_TARGET_INCLUDE
#define HWY_TARGET_INCLUDE "lib/jxl/render_pipeline/stage_noise.cc"
#include <hwy/foreach_target.h>
#include <hwy/highway.h>

#include "lib/jxl/fast_math-inl.h"
#include "lib/jxl/transfer_functions-inl.h"

HWY_BEFORE_NAMESPACE();
namespace jxl {
namespace HWY_NAMESPACE {

// These templates are not found via ADL.
using hwy::HWY_NAMESPACE::ShiftRight;
using hwy::HWY_NAMESPACE::Vec;

using D = HWY_CAPPED(float, kBlockDim);
using DI = hwy::HWY_NAMESPACE::Rebind<int, D>;
using DI8 = hwy::HWY_NAMESPACE::Repartition<uint8_t, D>;

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

class AddNoiseStage : public RenderPipelineStage {
 public:
  AddNoiseStage(const NoiseParams& noise_params,
                const ColorCorrelationMap& cmap, size_t first_c)
      : RenderPipelineStage(RenderPipelineStage::Settings::Symmetric(
            /*shift=*/0, /*border=*/2)),
        noise_params_(noise_params),
        cmap_(cmap),
        first_c_(first_c) {}

  void ProcessRow(const RowInfo& input_rows, const RowInfo& output_rows,
                  size_t xextra, size_t xsize, size_t xpos, size_t ypos,
                  float* JXL_RESTRICT temp) const final {
    PROFILER_ZONE("Noise apply");

    if (!noise_params_.HasAny()) return;
    const StrengthEvalLut noise_model(noise_params_);
    D d;
    const auto half = Set(d, 0.5f);

    // With the prior subtract-random Laplacian approximation, rnd_* ranges were
    // about [-1.5, 1.6]; Laplacian3 about doubles this to [-3.6, 3.6], so the
    // normalizer is half of what it was before (0.5).
    const auto norm_const = Set(d, 0.22f);

    float ytox = cmap_.YtoXRatio(0);
    float ytob = cmap_.YtoBRatio(0);

    const size_t xsize_v = RoundUpTo(xsize, Lanes(d));

    float* JXL_RESTRICT row_x = GetInputRow(input_rows, 0, 0);
    float* JXL_RESTRICT row_y = GetInputRow(input_rows, 1, 0);
    float* JXL_RESTRICT row_b = GetInputRow(input_rows, 2, 0);
    const float* JXL_RESTRICT row_rnd_r =
        GetInputRow(input_rows, first_c_ + 0, 0);
    const float* JXL_RESTRICT row_rnd_g =
        GetInputRow(input_rows, first_c_ + 1, 0);
    const float* JXL_RESTRICT row_rnd_c =
        GetInputRow(input_rows, first_c_ + 2, 0);
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

  RenderPipelineChannelMode GetChannelMode(size_t c) const final {
    return c >= first_c_ ? RenderPipelineChannelMode::kInput
           : c < 3       ? RenderPipelineChannelMode::kInPlace
                         : RenderPipelineChannelMode::kIgnored;
  }

 private:
  const NoiseParams& noise_params_;
  const ColorCorrelationMap& cmap_;
  size_t first_c_;
};

std::unique_ptr<RenderPipelineStage> GetAddNoiseStage(
    const NoiseParams& noise_params, const ColorCorrelationMap& cmap,
    size_t noise_c_start) {
  return jxl::make_unique<AddNoiseStage>(noise_params, cmap, noise_c_start);
}

class ConvolveNoiseStage : public RenderPipelineStage {
 public:
  explicit ConvolveNoiseStage(size_t first_c)
      : RenderPipelineStage(RenderPipelineStage::Settings::Symmetric(
            /*shift=*/0, /*border=*/2)),
        first_c_(first_c) {}

  void ProcessRow(const RowInfo& input_rows, const RowInfo& output_rows,
                  size_t xextra, size_t xsize, size_t xpos, size_t ypos,
                  float* JXL_RESTRICT temp) const final {
    PROFILER_ZONE("Noise convolve");

    const HWY_FULL(float) d;
    for (size_t c = first_c_; c < first_c_ + 3; c++) {
      float* JXL_RESTRICT rows[5];
      for (size_t i = 0; i < 5; i++) {
        rows[i] = GetInputRow(input_rows, c, i - 2);
      }
      float* JXL_RESTRICT row_out = GetOutputRow(output_rows, c, 0);
      for (int64_t x = -RoundUpTo(xextra, Lanes(d));
           x < (int64_t)(xsize + xextra); x += Lanes(d)) {
        const auto p00 = Load(d, rows[2] + x);
        auto others = Zero(d);
        for (ssize_t i = -2; i <= 2; i++) {
          others += LoadU(d, rows[0] + x + i);
          others += LoadU(d, rows[1] + x + i);
          others += LoadU(d, rows[3] + x + i);
          others += LoadU(d, rows[4] + x + i);
        }
        others += LoadU(d, rows[2] + x - 2);
        others += LoadU(d, rows[2] + x - 1);
        others += LoadU(d, rows[2] + x + 1);
        others += LoadU(d, rows[2] + x + 2);
        // 4 * (1 - box kernel)
        auto pixels = MulAdd(others, Set(d, 0.16), p00 * Set(d, -3.84));
        Store(pixels, d, row_out + x);
      }
    }
  }

  RenderPipelineChannelMode GetChannelMode(size_t c) const final {
    return c >= first_c_ ? RenderPipelineChannelMode::kInOut
                         : RenderPipelineChannelMode::kIgnored;
  }

 private:
  size_t first_c_;
};

std::unique_ptr<RenderPipelineStage> GetConvolveNoiseStage(
    size_t noise_c_start) {
  return jxl::make_unique<ConvolveNoiseStage>(noise_c_start);
}

// NOLINTNEXTLINE(google-readability-namespace-comments)
}  // namespace HWY_NAMESPACE
}  // namespace jxl
HWY_AFTER_NAMESPACE();

#if HWY_ONCE
namespace jxl {

HWY_EXPORT(GetAddNoiseStage);
HWY_EXPORT(GetConvolveNoiseStage);

std::unique_ptr<RenderPipelineStage> GetAddNoiseStage(
    const NoiseParams& noise_params, const ColorCorrelationMap& cmap,
    size_t noise_c_start) {
  return HWY_DYNAMIC_DISPATCH(GetAddNoiseStage)(noise_params, cmap,
                                                noise_c_start);
}

std::unique_ptr<RenderPipelineStage> GetConvolveNoiseStage(
    size_t noise_c_start) {
  return HWY_DYNAMIC_DISPATCH(GetConvolveNoiseStage)(noise_c_start);
}

}  // namespace jxl
#endif
