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

#include <stddef.h>
#include <stdint.h>

#include <algorithm>
#include <numeric>
#include <string>

#include "lib/jxl/convolve.h"
#include "lib/jxl/enc_ac_strategy.h"
#include "lib/jxl/enc_adaptive_quantization.h"
#include "lib/jxl/enc_ar_control_field.h"
#include "lib/jxl/enc_cache.h"
#include "lib/jxl/enc_heuristics.h"
#include "lib/jxl/enc_noise.h"
#include "lib/jxl/gaborish.h"
#include "lib/jxl/gauss_blur.h"

#undef HWY_TARGET_INCLUDE
#define HWY_TARGET_INCLUDE "lib/jxl/enc_fast_heuristics.cc"
#include <hwy/foreach_target.h>
#include <hwy/highway.h>

HWY_BEFORE_NAMESPACE();
namespace jxl {
namespace HWY_NAMESPACE {
namespace {
using DF4 = HWY_CAPPED(float, 4);
DF4 df4;
HWY_FULL(float) df;

Status Heuristics(PassesEncoderState* enc_state,
                  ModularFrameEncoder* modular_frame_encoder,
                  const ImageBundle* linear, Image3F* opsin, ThreadPool* pool,
                  AuxOut* aux_out) {
  PROFILER_ZONE("JxlLossyFrameHeuristics uninstrumented");
  CompressParams& cparams = enc_state->cparams;
  PassesSharedState& shared = enc_state->shared;
  const FrameDimensions& frame_dim = enc_state->shared.frame_dim;
  JXL_CHECK(cparams.butteraugli_distance > 0);

  // TODO(veluca): make this tiled.
  if (shared.frame_header.loop_filter.gab) {
    GaborishInverse(opsin, 0.9908511000000001f, pool);
  }
  // Compute image of high frequencies by removing a blurred version.
  // TODO(veluca): certainly can be made faster, and use less memory...
  constexpr size_t pad = 16;
  Image3F padded = PadImageMirror(*opsin, pad, pad);
  // Make the image (X, Y, B-Y)
  // TODO(veluca): SubtractFrom is not parallel *and* not SIMD-fied.
  SubtractFrom(padded.Plane(1), &padded.Plane(2));
  // Ensure that OOB access for CfL does nothing. Not necessary if doing things
  // properly...
  Image3F hf(padded.xsize() + 64, padded.ysize());
  ZeroFillImage(&hf);
  hf.ShrinkTo(padded.xsize(), padded.ysize());
  ImageF temp(padded.xsize(), padded.ysize());
  // TODO(veluca): consider some faster blurring method.
  auto g = CreateRecursiveGaussian(11.415258091746161);
  for (size_t c = 0; c < 3; c++) {
    FastGaussian(g, padded.Plane(c), pool, &temp, &hf.Plane(c));
    SubtractFrom(padded.Plane(c), &hf.Plane(c));
  }
  // TODO(veluca): DC CfL?
  size_t xcolortiles = DivCeil(frame_dim.xsize_blocks, kColorTileDimInBlocks);
  size_t ycolortiles = DivCeil(frame_dim.ysize_blocks, kColorTileDimInBlocks);
  RunOnPool(
      pool, 0, xcolortiles * ycolortiles, ThreadPool::SkipInit(),
      [&](size_t tile_id, size_t _) {
        size_t tx = tile_id % xcolortiles;
        size_t ty = tile_id / xcolortiles;
        size_t x0 = tx * kColorTileDim;
        size_t x1 = std::min(x0 + kColorTileDim, hf.xsize());
        size_t y0 = ty * kColorTileDim;
        size_t y1 = std::min(y0 + kColorTileDim, hf.ysize());
        for (size_t c : {0, 2}) {
          static constexpr float kInvColorFactor = 1.0f / kDefaultColorFactor;
          auto ca = Zero(df);
          auto cb = Zero(df);
          const auto inv_color_factor = Set(df, kInvColorFactor);
          for (size_t y = y0; y < y1; y++) {
            const float* row_m = hf.PlaneRow(1, y);
            const float* row_s = hf.PlaneRow(c, y);
            for (size_t x = x0; x < x1; x += Lanes(df)) {
              // color residual = ax + b
              const auto a = inv_color_factor * Load(df, row_m + x);
              const auto b = Zero(df) - Load(df, row_s + x);
              ca = MulAdd(a, a, ca);
              cb = MulAdd(a, b, cb);
            }
          }
          float best =
              -GetLane(SumOfLanes(cb)) / (GetLane(SumOfLanes(ca)) + 1e-9f);
          int8_t& res = (c == 0 ? shared.cmap.ytox_map : shared.cmap.ytob_map)
                            .Row(ty)[tx];
          res = std::max(-128.0f, std::min(127.0f, roundf(best)));
        }
      },
      "CfL");
  Image3F pooled(frame_dim.xsize_padded / 4, frame_dim.ysize_padded / 4);
  Image3F summed(frame_dim.xsize_padded / 4, frame_dim.ysize_padded / 4);
  RunOnPool(
      pool, 0, frame_dim.ysize_padded / 4, ThreadPool::SkipInit(),
      [&](size_t y, size_t _) {
        for (size_t c = 0; c < 3; c++) {
          float* JXL_RESTRICT row_out = pooled.PlaneRow(c, y);
          float* JXL_RESTRICT row_out_avg = summed.PlaneRow(c, y);
          const float* JXL_RESTRICT row_in[4];
          for (size_t iy = 0; iy < 4; iy++) {
            row_in[iy] = hf.PlaneRow(c, 4 * y + pad + iy);
          }
          for (size_t x = 0; x < frame_dim.xsize_padded / 4; x++) {
            auto max = Zero(df4);
            auto sum = Zero(df4);
            for (size_t iy = 0; iy < 4; iy++) {
              for (size_t ix = 0; ix < 4; ix += Lanes(df4)) {
                const auto nn = Abs(Load(df4, row_in[iy] + x * 4 + ix + pad));
                sum += nn;
                max = IfThenElse(max > nn, max, nn);
              }
            }
            row_out_avg[x] = GetLane(SumOfLanes(sum));
            row_out[x] = GetLane(MaxOfLanes(max));
          }
        }
      },
      "MaxPool");
  // TODO(veluca): better handling of the border
  // TODO(veluca): consider some faster blurring method.
  // TODO(veluca): parallelize.
  // Remove noise from the resulting image.
  auto g2 = CreateRecursiveGaussian(2.0849544429861884);
  constexpr size_t pad2 = 16;
  Image3F summed_pad = PadImageMirror(summed, pad2, pad2);
  ImageF tmp_out(summed_pad.xsize(), summed_pad.ysize());
  ImageF tmp2(summed_pad.xsize(), summed_pad.ysize());
  Image3F pooled_pad = PadImageMirror(pooled, pad2, pad2);
  for (size_t c = 0; c < 3; c++) {
    FastGaussian(g2, summed_pad.Plane(c), pool, &tmp2, &tmp_out);
    const auto unblurred_multiplier = Set(df, 0.5f);
    for (size_t y = 0; y < summed.ysize(); y++) {
      float* row = summed.PlaneRow(c, y);
      const float* row_blur = tmp_out.Row(y + pad2);
      for (size_t x = 0; x < summed.xsize(); x += Lanes(df)) {
        const auto b = Load(df, row_blur + x + pad2);
        const auto o = Load(df, row + x) * unblurred_multiplier;
        const auto m = IfThenElse(b > o, b, o);
        Store(m, df, row + x);
      }
    }
  }
  for (size_t c = 0; c < 3; c++) {
    FastGaussian(g2, pooled_pad.Plane(c), pool, &tmp2, &tmp_out);
    const auto unblurred_multiplier = Set(df, 0.5f);
    for (size_t y = 0; y < pooled.ysize(); y++) {
      float* row = pooled.PlaneRow(c, y);
      const float* row_blur = tmp_out.Row(y + pad2);
      for (size_t x = 0; x < pooled.xsize(); x += Lanes(df)) {
        const auto b = Load(df, row_blur + x + pad2);
        const auto o = Load(df, row + x) * unblurred_multiplier;
        const auto m = IfThenElse(b > o, b, o);
        Store(m, df, row + x);
      }
    }
  }
  const static float kChannelMul[3] = {
      7.9644294909680253f,
      0.5700000183257159f,
      0.20267448837597055f,
  };
  ImageF pooledhf44(pooled.xsize(), pooled.ysize());
  for (size_t y = 0; y < pooled.ysize(); y++) {
    const float* row_in_x = pooled.ConstPlaneRow(0, y);
    const float* row_in_y = pooled.ConstPlaneRow(1, y);
    const float* row_in_b = pooled.ConstPlaneRow(2, y);
    float* row_out = pooledhf44.Row(y);
    for (size_t x = 0; x < pooled.xsize(); x += Lanes(df)) {
      auto v = Set(df, kChannelMul[0]) * Load(df, row_in_x + x);
      v = MulAdd(Set(df, kChannelMul[1]), Load(df, row_in_y + x), v);
      v = MulAdd(Set(df, kChannelMul[2]), Load(df, row_in_b + x), v);
      Store(v, df, row_out + x);
    }
  }
  ImageF summedhf44(summed.xsize(), summed.ysize());
  for (size_t y = 0; y < summed.ysize(); y++) {
    const float* row_in_x = summed.ConstPlaneRow(0, y);
    const float* row_in_y = summed.ConstPlaneRow(1, y);
    const float* row_in_b = summed.ConstPlaneRow(2, y);
    float* row_out = summedhf44.Row(y);
    for (size_t x = 0; x < summed.xsize(); x += Lanes(df)) {
      auto v = Set(df, kChannelMul[0]) * Load(df, row_in_x + x);
      v = MulAdd(Set(df, kChannelMul[1]), Load(df, row_in_y + x), v);
      v = MulAdd(Set(df, kChannelMul[2]), Load(df, row_in_b + x), v);
      Store(v, df, row_out + x);
    }
  }
  aux_out->DumpPlaneNormalized("pooledhf44", pooledhf44);
  aux_out->DumpPlaneNormalized("summedhf44", summedhf44);

  static const float kDcQuantMul = 0.88170190420916206;
  static const float kAcQuantMul = 2.5165738934721524;

  float dc_quant = kDcQuantMul * InitialQuantDC(cparams.butteraugli_distance);
  float ac_quant_base = kAcQuantMul / cparams.butteraugli_distance;
  ImageF quant_field(frame_dim.xsize_blocks, frame_dim.ysize_blocks);

  static_assert(kColorTileDim == 64, "Fix the code below");
  auto mmacs = [&](size_t bx, size_t by, AcStrategy acs, float& min,
                   float& max) {
    min = 1e10;
    max = 0;
    for (size_t y = 2 * by; y < 2 * (by + acs.covered_blocks_y()); y++) {
      const float* row = summedhf44.Row(y);
      for (size_t x = 2 * bx; x < 2 * (bx + acs.covered_blocks_x()); x++) {
        min = std::min(min, row[x]);
        max = std::max(max, row[x]);
      }
    }
  };
  // Multipliers for allowed range of summedhf44.
  std::pair<AcStrategy::Type, float> candidates[] = {
    // The order is such that, in case of ties, 8x8 is favoured over 4x4 which
    // is favoured over 2x2. Similarly, we prefer square transforms over
    // same-area rectangular ones.
    {AcStrategy::Type::DCT2X2, 1.5f},
    {AcStrategy::Type::DCT4X4, 1.4f},
    {AcStrategy::Type::DCT4X8, 1.2f},
    {AcStrategy::Type::DCT8X4, 1.2f},
    {AcStrategy::Type::AFV0,
     1.15f},  // doesn't really work with these heuristics
    {AcStrategy::Type::AFV1, 1.15f},
    {AcStrategy::Type::AFV2, 1.15f},
    {AcStrategy::Type::AFV3, 1.15f},
    {AcStrategy::Type::DCT, 1.0f},
    {AcStrategy::Type::DCT16X8, 0.8f},
    {AcStrategy::Type::DCT8X16, 0.8f},
    {AcStrategy::Type::DCT16X16, 0.2f},
    {AcStrategy::Type::DCT16X32, 0.2f},
    {AcStrategy::Type::DCT32X16, 0.2f},
    {AcStrategy::Type::DCT32X32, 0.2f},
    {AcStrategy::Type::DCT32X64, 0.1f},
    {AcStrategy::Type::DCT64X32, 0.1f},
    {AcStrategy::Type::DCT64X64, 0.04f},

#if 0
      {AcStrategy::Type::DCT2X2, 1e+10},  {AcStrategy::Type::DCT4X4, 2.0f},
      {AcStrategy::Type::DCT, 1.0f},      {AcStrategy::Type::DCT16X8, 1.0f},
      {AcStrategy::Type::DCT8X16, 1.0f},  {AcStrategy::Type::DCT32X8, 1.0f},
      {AcStrategy::Type::DCT8X32, 1.0f},  {AcStrategy::Type::DCT32X16, 1.0f},
      {AcStrategy::Type::DCT16X32, 1.0f}, {AcStrategy::Type::DCT64X32, 1.0f},
      {AcStrategy::Type::DCT32X64, 1.0f}, {AcStrategy::Type::DCT16X16, 1.0f},
      {AcStrategy::Type::DCT32X32, 1.0f}, {AcStrategy::Type::DCT64X64, 1.0f},
#endif
    // TODO(veluca): figure out if we want 4x8 and/or AVF.
  };
  float max_range = 1e-8f + 0.5f * std::pow(cparams.butteraugli_distance, 0.5f);
  // Change quant field and sharpness amounts based on (pooled|summed)hf44, and
  // compute block sizes.
  // TODO(veluca): maybe this could be done per group: it would allow choosing
  // floating blocks better.
  RunOnPool(
      pool, 0, xcolortiles * ycolortiles, ThreadPool::SkipInit(),
      [&](size_t tile_id, size_t _) {
        size_t tx = tile_id % xcolortiles;
        size_t ty = tile_id / xcolortiles;
        size_t x0 = tx * kColorTileDim / kBlockDim;
        size_t x1 = std::min(x0 + kColorTileDimInBlocks, quant_field.xsize());
        size_t y0 = ty * kColorTileDim / kBlockDim;
        size_t y1 = std::min(y0 + kColorTileDimInBlocks, quant_field.ysize());
        size_t qf_stride = quant_field.PixelsPerRow();
        size_t epf_stride = shared.epf_sharpness.PixelsPerRow();
        bool chosen_mask[64] = {};
        for (size_t y = y0; y < y1; y++) {
          uint8_t* epf_row = shared.epf_sharpness.Row(y);
          float* qf_row = quant_field.Row(y);
          for (size_t x = x0; x < x1; x++) {
            if (chosen_mask[(y - y0) * 8 + (x - x0)]) continue;
            // Default to DCT8 just in case something funny happens in the loop
            // below.
            AcStrategy::Type best = AcStrategy::DCT;
            size_t best_covered = 1;
            float qf = ac_quant_base;
            for (size_t i = 0; i < sizeof(candidates) / sizeof(*candidates);
                 i++) {
              AcStrategy acs = AcStrategy::FromRawStrategy(candidates[i].first);
              if (y + acs.covered_blocks_y() > y1) continue;
              if (x + acs.covered_blocks_x() > x1) continue;
              bool fits = true;
              for (size_t iy = y; iy < y + acs.covered_blocks_y(); iy++) {
                for (size_t ix = x; ix < x + acs.covered_blocks_x(); ix++) {
                  if (chosen_mask[(iy - y0) * 8 + (ix - x0)]) {
                    fits = false;
                    break;
                  }
                }
              }
              if (!fits) continue;
              float min, max;
              mmacs(x, y, acs, min, max);
              if (max - min > max_range * candidates[i].second) continue;
              size_t cb = acs.covered_blocks_x() * acs.covered_blocks_y();
              if (cb >= best_covered) {
                best_covered = cb;
                best = candidates[i].first;
                // TODO(veluca): make this better.
                qf = ac_quant_base /
                     (3.9312946339134007f + 2.6011435675118082f * min);
              }
            }
            shared.ac_strategy.Set(x, y, best);
            AcStrategy acs = AcStrategy::FromRawStrategy(best);
            for (size_t iy = y; iy < y + acs.covered_blocks_y(); iy++) {
              for (size_t ix = x; ix < x + acs.covered_blocks_x(); ix++) {
                chosen_mask[(iy - y0) * 8 + (ix - x0)] = 1;
                qf_row[ix + (iy - y) * qf_stride] = qf;
              }
            }
            // TODO
            for (size_t iy = y; iy < y + acs.covered_blocks_y(); iy++) {
              for (size_t ix = x; ix < x + acs.covered_blocks_x(); ix++) {
                epf_row[ix + (iy - y) * epf_stride] = 4;
              }
            }
          }
        }
      },
      "QF+ACS+EPF");
  aux_out->DumpPlaneNormalized("qf", quant_field);
  aux_out->DumpPlaneNormalized("epf", shared.epf_sharpness);
  DumpAcStrategy(shared.ac_strategy, frame_dim.xsize_padded,
                 frame_dim.ysize_padded, "acs", aux_out);

  shared.quantizer.SetQuantField(dc_quant, quant_field,
                                 &shared.raw_quant_field);

  return true;
}
}  // namespace
// NOLINTNEXTLINE(google-readability-namespace-comments)
}  // namespace HWY_NAMESPACE
}  // namespace jxl
HWY_AFTER_NAMESPACE();

#if HWY_ONCE
namespace jxl {
HWY_EXPORT(Heuristics);
Status FastEncoderHeuristics::LossyFrameHeuristics(
    PassesEncoderState* enc_state, ModularFrameEncoder* modular_frame_encoder,
    const ImageBundle* linear, Image3F* opsin, ThreadPool* pool,
    AuxOut* aux_out) {
  return HWY_DYNAMIC_DISPATCH(Heuristics)(enc_state, modular_frame_encoder,
                                          linear, opsin, pool, aux_out);
}

}  // namespace jxl
#endif
