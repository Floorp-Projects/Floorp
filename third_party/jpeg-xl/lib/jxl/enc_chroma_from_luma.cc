// Copyright (c) the JPEG XL Project Authors. All rights reserved.
//
// Use of this source code is governed by a BSD-style
// license that can be found in the LICENSE file.

#include "lib/jxl/enc_chroma_from_luma.h"

#include <float.h>
#include <stdlib.h>

#include <algorithm>
#include <array>
#include <cmath>

#undef HWY_TARGET_INCLUDE
#define HWY_TARGET_INCLUDE "lib/jxl/enc_chroma_from_luma.cc"
#include <hwy/aligned_allocator.h>
#include <hwy/foreach_target.h>
#include <hwy/highway.h>

#include "lib/jxl/aux_out.h"
#include "lib/jxl/base/bits.h"
#include "lib/jxl/base/padded_bytes.h"
#include "lib/jxl/base/profiler.h"
#include "lib/jxl/base/span.h"
#include "lib/jxl/base/status.h"
#include "lib/jxl/common.h"
#include "lib/jxl/dec_transforms-inl.h"
#include "lib/jxl/enc_transforms-inl.h"
#include "lib/jxl/entropy_coder.h"
#include "lib/jxl/image_ops.h"
#include "lib/jxl/modular/encoding/encoding.h"
#include "lib/jxl/quantizer.h"
HWY_BEFORE_NAMESPACE();
namespace jxl {
namespace HWY_NAMESPACE {

static HWY_FULL(float) df;

struct CFLFunction {
  static constexpr float kCoeff = 1.f / 3;
  static constexpr float kThres = 100.0f;
  static constexpr float kInvColorFactor = 1.0f / kDefaultColorFactor;
  CFLFunction(const float* values_m, const float* values_s, size_t num,
              float base, float distance_mul)
      : values_m(values_m),
        values_s(values_s),
        num(num),
        base(base),
        distance_mul(distance_mul) {}

  // Returns f'(x), where f is 1/3 * sum ((|color residual| + 1)^2-1) +
  // distance_mul * x^2 * num.
  float Compute(float x, float eps, float* fpeps, float* fmeps) const {
    float first_derivative = 2 * distance_mul * num * x;
    float first_derivative_peps = 2 * distance_mul * num * (x + eps);
    float first_derivative_meps = 2 * distance_mul * num * (x - eps);

    const auto inv_color_factor = Set(df, kInvColorFactor);
    const auto thres = Set(df, kThres);
    const auto coeffx2 = Set(df, kCoeff * 2.0f);
    const auto one = Set(df, 1.0f);
    const auto zero = Set(df, 0.0f);
    const auto base_v = Set(df, base);
    const auto x_v = Set(df, x);
    const auto xpe_v = Set(df, x + eps);
    const auto xme_v = Set(df, x - eps);
    auto fd_v = Zero(df);
    auto fdpe_v = Zero(df);
    auto fdme_v = Zero(df);
    JXL_ASSERT(num % Lanes(df) == 0);

    for (size_t i = 0; i < num; i += Lanes(df)) {
      // color residual = ax + b
      const auto a = inv_color_factor * Load(df, values_m + i);
      const auto b = base_v * Load(df, values_m + i) - Load(df, values_s + i);
      const auto v = a * x_v + b;
      const auto vpe = a * xpe_v + b;
      const auto vme = a * xme_v + b;
      const auto av = Abs(v);
      const auto avpe = Abs(vpe);
      const auto avme = Abs(vme);
      auto d = coeffx2 * (av + one) * a;
      auto dpe = coeffx2 * (avpe + one) * a;
      auto dme = coeffx2 * (avme + one) * a;
      d = IfThenElse(v < zero, zero - d, d);
      dpe = IfThenElse(vpe < zero, zero - dpe, dpe);
      dme = IfThenElse(vme < zero, zero - dme, dme);
      fd_v += IfThenElse(av >= thres, zero, d);
      fdpe_v += IfThenElse(av >= thres, zero, dpe);
      fdme_v += IfThenElse(av >= thres, zero, dme);
    }

    *fpeps = first_derivative_peps + GetLane(SumOfLanes(fdpe_v));
    *fmeps = first_derivative_meps + GetLane(SumOfLanes(fdme_v));
    return first_derivative + GetLane(SumOfLanes(fd_v));
  }

  const float* JXL_RESTRICT values_m;
  const float* JXL_RESTRICT values_s;
  size_t num;
  float base;
  float distance_mul;
};

int32_t FindBestMultiplier(const float* values_m, const float* values_s,
                           size_t num, float base, float distance_mul,
                           bool fast) {
  if (num == 0) {
    return 0;
  }
  float x;
  if (fast) {
    static constexpr float kInvColorFactor = 1.0f / kDefaultColorFactor;
    auto ca = Zero(df);
    auto cb = Zero(df);
    const auto inv_color_factor = Set(df, kInvColorFactor);
    const auto base_v = Set(df, base);
    for (size_t i = 0; i < num; i += Lanes(df)) {
      // color residual = ax + b
      const auto a = inv_color_factor * Load(df, values_m + i);
      const auto b = base_v * Load(df, values_m + i) - Load(df, values_s + i);
      ca = MulAdd(a, a, ca);
      cb = MulAdd(a, b, cb);
    }
    // + distance_mul * x^2 * num
    x = -GetLane(SumOfLanes(cb)) /
        (GetLane(SumOfLanes(ca)) + num * distance_mul * 0.5f);
  } else {
    constexpr float eps = 1;
    constexpr float kClamp = 20.0f;
    CFLFunction fn(values_m, values_s, num, base, distance_mul);
    x = 0;
    // Up to 20 Newton iterations, with approximate derivatives.
    // Derivatives are approximate due to the high amount of noise in the exact
    // derivatives.
    for (size_t i = 0; i < 20; i++) {
      float dfpeps, dfmeps;
      float df = fn.Compute(x, eps, &dfpeps, &dfmeps);
      float ddf = (dfpeps - dfmeps) / (2 * eps);
      float step = df / ddf;
      x -= std::min(kClamp, std::max(-kClamp, step));
      if (std::abs(step) < 3e-3) break;
    }
  }
  return std::max(-128.0f, std::min(127.0f, roundf(x)));
}

void InitDCStorage(size_t num_blocks, ImageF* dc_values) {
  // First row: Y channel
  // Second row: X channel
  // Third row: Y channel
  // Fourth row: B channel
  *dc_values = ImageF(RoundUpTo(num_blocks, Lanes(df)), 4);

  JXL_ASSERT(dc_values->xsize() != 0);
  // Zero-fill the last lanes
  for (size_t y = 0; y < 4; y++) {
    for (size_t x = dc_values->xsize() - Lanes(df); x < dc_values->xsize();
         x++) {
      dc_values->Row(y)[x] = 0;
    }
  }
}

void ComputeDC(const ImageF& dc_values, bool fast, int* dc_x, int* dc_b) {
  constexpr float kDistanceMultiplierDC = 1e-5f;
  const float* JXL_RESTRICT dc_values_yx = dc_values.Row(0);
  const float* JXL_RESTRICT dc_values_x = dc_values.Row(1);
  const float* JXL_RESTRICT dc_values_yb = dc_values.Row(2);
  const float* JXL_RESTRICT dc_values_b = dc_values.Row(3);
  *dc_x = FindBestMultiplier(dc_values_yx, dc_values_x, dc_values.xsize(), 0.0f,
                             kDistanceMultiplierDC, fast);
  *dc_b = FindBestMultiplier(dc_values_yb, dc_values_b, dc_values.xsize(),
                             kYToBRatio, kDistanceMultiplierDC, fast);
}

void ComputeTile(const Image3F& opsin, const DequantMatrices& dequant,
                 const AcStrategyImage* ac_strategy, const Quantizer* quantizer,
                 const Rect& r, bool fast, bool use_dct8, ImageSB* map_x,
                 ImageSB* map_b, ImageF* dc_values, float* mem) {
  static_assert(kEncTileDimInBlocks == kColorTileDimInBlocks,
                "Invalid color tile dim");
  size_t xsize_blocks = opsin.xsize() / kBlockDim;
  constexpr float kDistanceMultiplierAC = 1e-3f;

  const size_t y0 = r.y0();
  const size_t x0 = r.x0();
  const size_t x1 = r.x0() + r.xsize();
  const size_t y1 = r.y0() + r.ysize();

  int ty = y0 / kColorTileDimInBlocks;
  int tx = x0 / kColorTileDimInBlocks;

  int8_t* JXL_RESTRICT row_out_x = map_x->Row(ty);
  int8_t* JXL_RESTRICT row_out_b = map_b->Row(ty);

  float* JXL_RESTRICT dc_values_yx = dc_values->Row(0);
  float* JXL_RESTRICT dc_values_x = dc_values->Row(1);
  float* JXL_RESTRICT dc_values_yb = dc_values->Row(2);
  float* JXL_RESTRICT dc_values_b = dc_values->Row(3);

  // All are aligned.
  float* HWY_RESTRICT block_y = mem;
  float* HWY_RESTRICT block_x = block_y + AcStrategy::kMaxCoeffArea;
  float* HWY_RESTRICT block_b = block_x + AcStrategy::kMaxCoeffArea;
  float* HWY_RESTRICT coeffs_yx = block_b + AcStrategy::kMaxCoeffArea;
  float* HWY_RESTRICT coeffs_x = coeffs_yx + kColorTileDim * kColorTileDim;
  float* HWY_RESTRICT coeffs_yb = coeffs_x + kColorTileDim * kColorTileDim;
  float* HWY_RESTRICT coeffs_b = coeffs_yb + kColorTileDim * kColorTileDim;
  float* HWY_RESTRICT scratch_space = coeffs_b + kColorTileDim * kColorTileDim;
  JXL_DASSERT(scratch_space + 2 * AcStrategy::kMaxCoeffArea ==
              block_y + CfLHeuristics::kItemsPerThread);

  // Small (~256 bytes each)
  HWY_ALIGN_MAX float
      dc_y[AcStrategy::kMaxCoeffBlocks * AcStrategy::kMaxCoeffBlocks] = {};
  HWY_ALIGN_MAX float
      dc_x[AcStrategy::kMaxCoeffBlocks * AcStrategy::kMaxCoeffBlocks] = {};
  HWY_ALIGN_MAX float
      dc_b[AcStrategy::kMaxCoeffBlocks * AcStrategy::kMaxCoeffBlocks] = {};
  size_t num_ac = 0;

  for (size_t y = y0; y < y1; ++y) {
    const float* JXL_RESTRICT row_y = opsin.ConstPlaneRow(1, y * kBlockDim);
    const float* JXL_RESTRICT row_x = opsin.ConstPlaneRow(0, y * kBlockDim);
    const float* JXL_RESTRICT row_b = opsin.ConstPlaneRow(2, y * kBlockDim);
    size_t stride = opsin.PixelsPerRow();

    for (size_t x = x0; x < x1; x++) {
      AcStrategy acs = use_dct8
                           ? AcStrategy::FromRawStrategy(AcStrategy::Type::DCT)
                           : ac_strategy->ConstRow(y)[x];
      if (!acs.IsFirstBlock()) continue;
      size_t xs = acs.covered_blocks_x();
      TransformFromPixels(acs.Strategy(), row_y + x * kBlockDim, stride,
                          block_y, scratch_space);
      DCFromLowestFrequencies(acs.Strategy(), block_y, dc_y, xs);
      TransformFromPixels(acs.Strategy(), row_x + x * kBlockDim, stride,
                          block_x, scratch_space);
      DCFromLowestFrequencies(acs.Strategy(), block_x, dc_x, xs);
      TransformFromPixels(acs.Strategy(), row_b + x * kBlockDim, stride,
                          block_b, scratch_space);
      DCFromLowestFrequencies(acs.Strategy(), block_b, dc_b, xs);
      const float* const JXL_RESTRICT qm_x =
          dequant.InvMatrix(acs.Strategy(), 0);
      const float* const JXL_RESTRICT qm_b =
          dequant.InvMatrix(acs.Strategy(), 2);
      // Why does a constant seem to work better than
      // raw_quant_field->Row(y)[x] ?
      float q = use_dct8 ? 1 : quantizer->Scale() * 400.0f;
      float q_dc_x = use_dct8 ? 1 : 1.0f / quantizer->GetInvDcStep(0);
      float q_dc_b = use_dct8 ? 1 : 1.0f / quantizer->GetInvDcStep(2);

      // Copy DCs in dc_values.
      for (size_t iy = 0; iy < acs.covered_blocks_y(); iy++) {
        for (size_t ix = 0; ix < xs; ix++) {
          dc_values_yx[(iy + y) * xsize_blocks + ix + x] =
              dc_y[iy * xs + ix] * q_dc_x;
          dc_values_x[(iy + y) * xsize_blocks + ix + x] =
              dc_x[iy * xs + ix] * q_dc_x;
          dc_values_yb[(iy + y) * xsize_blocks + ix + x] =
              dc_y[iy * xs + ix] * q_dc_b;
          dc_values_b[(iy + y) * xsize_blocks + ix + x] =
              dc_b[iy * xs + ix] * q_dc_b;
        }
      }

      // Do not use this block for computing AC CfL.
      if (acs.covered_blocks_x() + x0 > x1 ||
          acs.covered_blocks_y() + y0 > y1) {
        continue;
      }

      // Copy AC coefficients in the local block. The order in which
      // coefficients get stored does not matter.
      size_t cx = acs.covered_blocks_x();
      size_t cy = acs.covered_blocks_y();
      CoefficientLayout(&cy, &cx);
      // Zero out LFs. This introduces terms in the optimization loop that
      // don't affect the result, as they are all 0, but allow for simpler
      // SIMDfication.
      for (size_t iy = 0; iy < cy; iy++) {
        for (size_t ix = 0; ix < cx; ix++) {
          block_y[cx * kBlockDim * iy + ix] = 0;
          block_x[cx * kBlockDim * iy + ix] = 0;
          block_b[cx * kBlockDim * iy + ix] = 0;
        }
      }
      const auto qv = Set(df, q);
      for (size_t i = 0; i < cx * cy * 64; i += Lanes(df)) {
        const auto b_y = Load(df, block_y + i);
        const auto b_x = Load(df, block_x + i);
        const auto b_b = Load(df, block_b + i);
        const auto qqm_x = qv * Load(df, qm_x + i);
        const auto qqm_b = qv * Load(df, qm_b + i);
        Store(b_y * qqm_x, df, coeffs_yx + num_ac);
        Store(b_x * qqm_x, df, coeffs_x + num_ac);
        Store(b_y * qqm_b, df, coeffs_yb + num_ac);
        Store(b_b * qqm_b, df, coeffs_b + num_ac);
        num_ac += Lanes(df);
      }
    }
  }
  JXL_CHECK(num_ac % Lanes(df) == 0);
  row_out_x[tx] = FindBestMultiplier(coeffs_yx, coeffs_x, num_ac, 0.0f,
                                     kDistanceMultiplierAC, fast);
  row_out_b[tx] = FindBestMultiplier(coeffs_yb, coeffs_b, num_ac, kYToBRatio,
                                     kDistanceMultiplierAC, fast);
}

// NOLINTNEXTLINE(google-readability-namespace-comments)
}  // namespace HWY_NAMESPACE
}  // namespace jxl
HWY_AFTER_NAMESPACE();

#if HWY_ONCE
namespace jxl {

HWY_EXPORT(InitDCStorage);
HWY_EXPORT(ComputeDC);
HWY_EXPORT(ComputeTile);

void CfLHeuristics::Init(const Image3F& opsin) {
  size_t xsize_blocks = opsin.xsize() / kBlockDim;
  size_t ysize_blocks = opsin.ysize() / kBlockDim;
  HWY_DYNAMIC_DISPATCH(InitDCStorage)
  (xsize_blocks * ysize_blocks, &dc_values);
}

void CfLHeuristics::ComputeTile(const Rect& r, const Image3F& opsin,
                                const DequantMatrices& dequant,
                                const AcStrategyImage* ac_strategy,
                                const Quantizer* quantizer, bool fast,
                                size_t thread, ColorCorrelationMap* cmap) {
  bool use_dct8 = ac_strategy == nullptr;
  HWY_DYNAMIC_DISPATCH(ComputeTile)
  (opsin, dequant, ac_strategy, quantizer, r, fast, use_dct8, &cmap->ytox_map,
   &cmap->ytob_map, &dc_values, mem.get() + thread * kItemsPerThread);
}

void CfLHeuristics::ComputeDC(bool fast, ColorCorrelationMap* cmap) {
  int32_t ytob_dc = 0;
  int32_t ytox_dc = 0;
  HWY_DYNAMIC_DISPATCH(ComputeDC)(dc_values, fast, &ytox_dc, &ytob_dc);
  cmap->SetYToBDC(ytob_dc);
  cmap->SetYToXDC(ytox_dc);
}

void ColorCorrelationMapEncodeDC(ColorCorrelationMap* map, BitWriter* writer,
                                 size_t layer, AuxOut* aux_out) {
  float color_factor = map->GetColorFactor();
  float base_correlation_x = map->GetBaseCorrelationX();
  float base_correlation_b = map->GetBaseCorrelationB();
  int32_t ytox_dc = map->GetYToXDC();
  int32_t ytob_dc = map->GetYToBDC();

  BitWriter::Allotment allotment(writer, 1 + 2 * kBitsPerByte + 12 + 32);
  if (ytox_dc == 0 && ytob_dc == 0 && color_factor == kDefaultColorFactor &&
      base_correlation_x == 0.0f && base_correlation_b == kYToBRatio) {
    writer->Write(1, 1);
    ReclaimAndCharge(writer, &allotment, layer, aux_out);
    return;
  }
  writer->Write(1, 0);
  JXL_CHECK(U32Coder::Write(kColorFactorDist, color_factor, writer));
  JXL_CHECK(F16Coder::Write(base_correlation_x, writer));
  JXL_CHECK(F16Coder::Write(base_correlation_b, writer));
  writer->Write(kBitsPerByte, ytox_dc - std::numeric_limits<int8_t>::min());
  writer->Write(kBitsPerByte, ytob_dc - std::numeric_limits<int8_t>::min());
  ReclaimAndCharge(writer, &allotment, layer, aux_out);
}

}  // namespace jxl
#endif  // HWY_ONCE
