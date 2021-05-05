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

#include "lib/jxl/enc_ac_strategy.h"

#include <stdint.h>
#include <string.h>

#include <algorithm>
#include <cmath>
#include <cstdio>

#undef HWY_TARGET_INCLUDE
#define HWY_TARGET_INCLUDE "lib/jxl/enc_ac_strategy.cc"
#include <hwy/foreach_target.h>
#include <hwy/highway.h>

#include "lib/jxl/ac_strategy.h"
#include "lib/jxl/ans_params.h"
#include "lib/jxl/base/bits.h"
#include "lib/jxl/base/compiler_specific.h"
#include "lib/jxl/base/profiler.h"
#include "lib/jxl/base/status.h"
#include "lib/jxl/coeff_order_fwd.h"
#include "lib/jxl/convolve.h"
#include "lib/jxl/dct_scales.h"
#include "lib/jxl/enc_params.h"
#include "lib/jxl/enc_transforms-inl.h"
#include "lib/jxl/entropy_coder.h"
#include "lib/jxl/fast_math-inl.h"

// This must come before the begin/end_target, but HWY_ONCE is only true
// after that, so use an "include guard".
#ifndef LIB_JXL_ENC_AC_STRATEGY_
#define LIB_JXL_ENC_AC_STRATEGY_
// Parameters of the heuristic are marked with a OPTIMIZE comment.
namespace jxl {

// Debugging utilities.

// Returns a linear sRGB color (as bytes) for each AC strategy.
const uint8_t* TypeColor(const uint8_t& raw_strategy) {
  JXL_ASSERT(AcStrategy::IsRawStrategyValid(raw_strategy));
  static_assert(AcStrategy::kNumValidStrategies == 27, "Change colors");
  static constexpr uint8_t kColors[][3] = {
      {0xFF, 0xFF, 0x00},  // DCT8
      {0xFF, 0x80, 0x80},  // HORNUSS
      {0xFF, 0x80, 0x80},  // DCT2x2
      {0xFF, 0x80, 0x80},  // DCT4x4
      {0x80, 0xFF, 0x00},  // DCT16x16
      {0x00, 0xC0, 0x00},  // DCT32x32
      {0xC0, 0xFF, 0x00},  // DCT16x8
      {0xC0, 0xFF, 0x00},  // DCT8x16
      {0x00, 0xFF, 0x00},  // DCT32x8
      {0x00, 0xFF, 0x00},  // DCT8x32
      {0x00, 0xFF, 0x00},  // DCT32x16
      {0x00, 0xFF, 0x00},  // DCT16x32
      {0xFF, 0x80, 0x00},  // DCT4x8
      {0xFF, 0x80, 0x00},  // DCT8x4
      {0xFF, 0xFF, 0x80},  // AFV0
      {0xFF, 0xFF, 0x80},  // AFV1
      {0xFF, 0xFF, 0x80},  // AFV2
      {0xFF, 0xFF, 0x80},  // AFV3
      {0x00, 0xC0, 0xFF},  // DCT64x64
      {0x00, 0xFF, 0xFF},  // DCT64x32
      {0x00, 0xFF, 0xFF},  // DCT32x64
      {0x00, 0x40, 0xFF},  // DCT128x128
      {0x00, 0x80, 0xFF},  // DCT128x64
      {0x00, 0x80, 0xFF},  // DCT64x128
      {0x00, 0x00, 0xC0},  // DCT256x256
      {0x00, 0x00, 0xFF},  // DCT256x128
      {0x00, 0x00, 0xFF},  // DCT128x256
  };
  return kColors[raw_strategy];
}

const uint8_t* TypeMask(const uint8_t& raw_strategy) {
  JXL_ASSERT(AcStrategy::IsRawStrategyValid(raw_strategy));
  static_assert(AcStrategy::kNumValidStrategies == 27, "Add masks");
  // implicitly, first row and column is made dark
  static constexpr uint8_t kMask[][64] = {
      {
          0, 0, 0, 0, 0, 0, 0, 0,  //
          0, 0, 0, 0, 0, 0, 0, 0,  //
          0, 0, 0, 0, 0, 0, 0, 0,  //
          0, 0, 0, 0, 0, 0, 0, 0,  //
          0, 0, 0, 0, 0, 0, 0, 0,  //
          0, 0, 0, 0, 0, 0, 0, 0,  //
          0, 0, 0, 0, 0, 0, 0, 0,  //
          0, 0, 0, 0, 0, 0, 0, 0,  //
      },                           // DCT8
      {
          0, 0, 0, 0, 0, 0, 0, 0,  //
          0, 0, 0, 0, 0, 0, 0, 0,  //
          0, 0, 1, 0, 0, 1, 0, 0,  //
          0, 0, 1, 0, 0, 1, 0, 0,  //
          0, 0, 1, 1, 1, 1, 0, 0,  //
          0, 0, 1, 0, 0, 1, 0, 0,  //
          0, 0, 1, 0, 0, 1, 0, 0,  //
          0, 0, 0, 0, 0, 0, 0, 0,  //
      },                           // HORNUSS
      {
          1, 1, 1, 1, 1, 1, 1, 1,  //
          1, 0, 1, 0, 1, 0, 1, 0,  //
          1, 1, 1, 1, 1, 1, 1, 1,  //
          1, 0, 1, 0, 1, 0, 1, 0,  //
          1, 1, 1, 1, 1, 1, 1, 1,  //
          1, 0, 1, 0, 1, 0, 1, 0,  //
          1, 1, 1, 1, 1, 1, 1, 1,  //
          1, 0, 1, 0, 1, 0, 1, 0,  //
      },                           // 2x2
      {
          0, 0, 0, 0, 1, 0, 0, 0,  //
          0, 0, 0, 0, 1, 0, 0, 0,  //
          0, 0, 0, 0, 1, 0, 0, 0,  //
          0, 0, 0, 0, 1, 0, 0, 0,  //
          1, 1, 1, 1, 1, 1, 1, 1,  //
          0, 0, 0, 0, 1, 0, 0, 0,  //
          0, 0, 0, 0, 1, 0, 0, 0,  //
          0, 0, 0, 0, 1, 0, 0, 0,  //
      },                           // 4x4
      {},                          // DCT16x16 (unused)
      {},                          // DCT32x32 (unused)
      {},                          // DCT16x8 (unused)
      {},                          // DCT8x16 (unused)
      {},                          // DCT32x8 (unused)
      {},                          // DCT8x32 (unused)
      {},                          // DCT32x16 (unused)
      {},                          // DCT16x32 (unused)
      {
          0, 0, 0, 0, 0, 0, 0, 0,  //
          0, 0, 0, 0, 0, 0, 0, 0,  //
          0, 0, 0, 0, 0, 0, 0, 0,  //
          0, 0, 0, 0, 0, 0, 0, 0,  //
          1, 1, 1, 1, 1, 1, 1, 1,  //
          0, 0, 0, 0, 0, 0, 0, 0,  //
          0, 0, 0, 0, 0, 0, 0, 0,  //
          0, 0, 0, 0, 0, 0, 0, 0,  //
      },                           // DCT4x8
      {
          0, 0, 0, 0, 1, 0, 0, 0,  //
          0, 0, 0, 0, 1, 0, 0, 0,  //
          0, 0, 0, 0, 1, 0, 0, 0,  //
          0, 0, 0, 0, 1, 0, 0, 0,  //
          0, 0, 0, 0, 1, 0, 0, 0,  //
          0, 0, 0, 0, 1, 0, 0, 0,  //
          0, 0, 0, 0, 1, 0, 0, 0,  //
          0, 0, 0, 0, 1, 0, 0, 0,  //
      },                           // DCT8x4
      {
          1, 1, 1, 1, 1, 0, 0, 0,  //
          1, 1, 1, 1, 0, 0, 0, 0,  //
          1, 1, 1, 0, 0, 0, 0, 0,  //
          1, 1, 0, 0, 0, 0, 0, 0,  //
          1, 0, 0, 0, 0, 0, 0, 0,  //
          0, 0, 0, 0, 0, 0, 0, 0,  //
          0, 0, 0, 0, 0, 0, 0, 0,  //
          0, 0, 0, 0, 0, 0, 0, 0,  //
      },                           // AFV0
      {
          0, 0, 0, 0, 1, 1, 1, 1,  //
          0, 0, 0, 0, 0, 1, 1, 1,  //
          0, 0, 0, 0, 0, 0, 1, 1,  //
          0, 0, 0, 0, 0, 0, 0, 1,  //
          0, 0, 0, 0, 0, 0, 0, 0,  //
          0, 0, 0, 0, 0, 0, 0, 0,  //
          0, 0, 0, 0, 0, 0, 0, 0,  //
          0, 0, 0, 0, 0, 0, 0, 0,  //
      },                           // AFV1
      {
          0, 0, 0, 0, 0, 0, 0, 0,  //
          0, 0, 0, 0, 0, 0, 0, 0,  //
          0, 0, 0, 0, 0, 0, 0, 0,  //
          0, 0, 0, 0, 0, 0, 0, 0,  //
          1, 0, 0, 0, 0, 0, 0, 0,  //
          1, 1, 0, 0, 0, 0, 0, 0,  //
          1, 1, 1, 0, 0, 0, 0, 0,  //
          1, 1, 1, 1, 0, 0, 0, 0,  //
      },                           // AFV2
      {
          0, 0, 0, 0, 0, 0, 0, 0,  //
          0, 0, 0, 0, 0, 0, 0, 0,  //
          0, 0, 0, 0, 0, 0, 0, 0,  //
          0, 0, 0, 0, 0, 0, 0, 0,  //
          0, 0, 0, 0, 0, 0, 0, 0,  //
          0, 0, 0, 0, 0, 0, 0, 1,  //
          0, 0, 0, 0, 0, 0, 1, 1,  //
          0, 0, 0, 0, 0, 1, 1, 1,  //
      },                           // AFV3
  };
  return kMask[raw_strategy];
}

void DumpAcStrategy(const AcStrategyImage& ac_strategy, size_t xsize,
                    size_t ysize, const char* tag, AuxOut* aux_out) {
  Image3F color_acs(xsize, ysize);
  for (size_t y = 0; y < ysize; y++) {
    float* JXL_RESTRICT rows[3] = {
        color_acs.PlaneRow(0, y),
        color_acs.PlaneRow(1, y),
        color_acs.PlaneRow(2, y),
    };
    const AcStrategyRow acs_row = ac_strategy.ConstRow(y / kBlockDim);
    for (size_t x = 0; x < xsize; x++) {
      AcStrategy acs = acs_row[x / kBlockDim];
      const uint8_t* JXL_RESTRICT color = TypeColor(acs.RawStrategy());
      for (size_t c = 0; c < 3; c++) {
        rows[c][x] = color[c] / 255.f;
      }
    }
  }
  size_t stride = color_acs.PixelsPerRow();
  for (size_t c = 0; c < 3; c++) {
    for (size_t by = 0; by < DivCeil(ysize, kBlockDim); by++) {
      float* JXL_RESTRICT row = color_acs.PlaneRow(c, by * kBlockDim);
      const AcStrategyRow acs_row = ac_strategy.ConstRow(by);
      for (size_t bx = 0; bx < DivCeil(xsize, kBlockDim); bx++) {
        AcStrategy acs = acs_row[bx];
        if (!acs.IsFirstBlock()) continue;
        const uint8_t* JXL_RESTRICT color = TypeColor(acs.RawStrategy());
        const uint8_t* JXL_RESTRICT mask = TypeMask(acs.RawStrategy());
        if (acs.covered_blocks_x() == 1 && acs.covered_blocks_y() == 1) {
          for (size_t iy = 0; iy < kBlockDim && by * kBlockDim + iy < ysize;
               iy++) {
            for (size_t ix = 0; ix < kBlockDim && bx * kBlockDim + ix < xsize;
                 ix++) {
              if (mask[iy * kBlockDim + ix]) {
                row[iy * stride + bx * kBlockDim + ix] = color[c] / 800.f;
              }
            }
          }
        }
        // draw block edges
        for (size_t ix = 0; ix < kBlockDim * acs.covered_blocks_x() &&
                            bx * kBlockDim + ix < xsize;
             ix++) {
          row[0 * stride + bx * kBlockDim + ix] = color[c] / 350.f;
        }
        for (size_t iy = 0; iy < kBlockDim * acs.covered_blocks_y() &&
                            by * kBlockDim + iy < ysize;
             iy++) {
          row[iy * stride + bx * kBlockDim + 0] = color[c] / 350.f;
        }
      }
    }
  }
  aux_out->DumpImage(tag, color_acs);
}

// AC strategy selection: recursive block splitting.

namespace {
template <size_t N>
size_t ACSCandidates(const AcStrategy::Type (&in)[N],
                     AcStrategy::Type* JXL_RESTRICT out) {
  memcpy(out, in, N * sizeof(AcStrategy::Type));
  return N;
}

// Order in which transforms are tested for max delta: the first
// acceptable one is chosen as initial guess.
constexpr AcStrategy::Type kACSOrder[] = {
    AcStrategy::Type::DCT64X64,
    AcStrategy::Type::DCT64X32,
    AcStrategy::Type::DCT32X64,
    AcStrategy::Type::DCT32X32,
    AcStrategy::Type::DCT32X16,
    AcStrategy::Type::DCT16X32,
    AcStrategy::Type::DCT16X16,
    // TODO(Jyrki): Restore these when we have better heuristics.
    // AcStrategy::Type::DCT8X32,
    // AcStrategy::Type::DCT32X8,
    AcStrategy::Type::DCT16X8,
    AcStrategy::Type::DCT8X16,
    // DCT8x8 is the "fallback" option if no bigger transform can be used.
    AcStrategy::Type::DCT,
};

size_t ACSPossibleReplacements(AcStrategy::Type current,
                               AcStrategy::Type* JXL_RESTRICT out) {
  // TODO(veluca): is this decision tree optimal?
  if (current == AcStrategy::Type::DCT64X64) {
    return ACSCandidates(
        {AcStrategy::Type::DCT64X32, AcStrategy::Type::DCT32X64,
         AcStrategy::Type::DCT32X32, AcStrategy::Type::DCT16X16,
         AcStrategy::Type::DCT},
        out);
  }
  if (current == AcStrategy::Type::DCT64X32 ||
      current == AcStrategy::Type::DCT32X64) {
    return ACSCandidates({AcStrategy::Type::DCT32X32,
                          AcStrategy::Type::DCT16X16, AcStrategy::Type::DCT},
                         out);
  }
  if (current == AcStrategy::Type::DCT32X32) {
    return ACSCandidates(
        {AcStrategy::Type::DCT32X16, AcStrategy::Type::DCT16X32,
         AcStrategy::Type::DCT16X16, AcStrategy::Type::DCT16X8,
         AcStrategy::Type::DCT8X16, AcStrategy::Type::DCT},
        out);
  }
  if (current == AcStrategy::Type::DCT32X16) {
    return ACSCandidates({AcStrategy::Type::DCT32X8, AcStrategy::Type::DCT16X16,
                          AcStrategy::Type::DCT},
                         out);
  }
  if (current == AcStrategy::Type::DCT16X32) {
    return ACSCandidates({AcStrategy::Type::DCT8X32, AcStrategy::Type::DCT16X16,
                          AcStrategy::Type::DCT},
                         out);
  }
  if (current == AcStrategy::Type::DCT32X8) {
    return ACSCandidates({AcStrategy::Type::DCT16X8, AcStrategy::Type::DCT},
                         out);
  }
  if (current == AcStrategy::Type::DCT8X32) {
    return ACSCandidates({AcStrategy::Type::DCT8X16, AcStrategy::Type::DCT},
                         out);
  }
  if (current == AcStrategy::Type::DCT16X16) {
    return ACSCandidates({AcStrategy::Type::DCT8X16, AcStrategy::Type::DCT16X8},
                         out);
  }
  if (current == AcStrategy::Type::DCT16X8 ||
      current == AcStrategy::Type::DCT8X16) {
    return ACSCandidates({AcStrategy::Type::DCT}, out);
  }
  if (current == AcStrategy::Type::DCT) {
    return ACSCandidates({AcStrategy::Type::DCT4X8, AcStrategy::Type::DCT8X4,
                          AcStrategy::Type::DCT4X4, AcStrategy::Type::DCT2X2,
                          AcStrategy::Type::IDENTITY, AcStrategy::Type::AFV0,
                          AcStrategy::Type::AFV1, AcStrategy::Type::AFV2,
                          AcStrategy::Type::AFV3},
                         out);
  }
  // Other 8x8 have no replacements - they already were chosen as the best
  // between all the 8x8s.
  return 0;
}

void InitEntropyAdjustTable(float* entropy_adjust) {
  // Precomputed FMA: premultiply `add` by `mul` so that the previous
  // entropy *= add; entropy *= mul becomes entropy = MulAdd(entropy, mul, add).
  const auto set = [entropy_adjust](size_t raw_strategy, float add, float mul) {
    entropy_adjust[2 * raw_strategy + 0] = add * mul;
    entropy_adjust[2 * raw_strategy + 1] = mul;
  };
  set(AcStrategy::Type::DCT, 0.0f, 0.80f);
  set(AcStrategy::Type::DCT4X4, 4.0f, 0.79f);
  set(AcStrategy::Type::DCT2X2, 4.0f, 1.1f);
  set(AcStrategy::Type::DCT16X16, 0.0f, 0.83f);
  set(AcStrategy::Type::DCT64X64, 0.0f, 1.3f);
  set(AcStrategy::Type::DCT64X32, 0.0f, 1.15f);
  set(AcStrategy::Type::DCT32X64, 0.0f, 1.15f);
  set(AcStrategy::Type::DCT32X32, 0.0f, 0.97f);
  set(AcStrategy::Type::DCT16X32, 0.0f, 0.94f);
  set(AcStrategy::Type::DCT32X16, 0.0f, 0.94f);
  set(AcStrategy::Type::DCT32X8, 0.0f, 2.261390410971102f);
  set(AcStrategy::Type::DCT8X32, 0.0f, 2.261390410971102f);
  set(AcStrategy::Type::DCT16X8, 0.0f, 0.86f);
  set(AcStrategy::Type::DCT8X16, 0.0f, 0.86f);
  set(AcStrategy::Type::DCT4X8, 3.0f, 0.81f);
  set(AcStrategy::Type::DCT8X4, 3.0f, 0.81f);
  set(AcStrategy::Type::IDENTITY, 8.0f, 1.2f);
  set(AcStrategy::Type::AFV0, 3.0f, 0.77f);
  set(AcStrategy::Type::AFV1, 3.0f, 0.77f);
  set(AcStrategy::Type::AFV2, 3.0f, 0.77f);
  set(AcStrategy::Type::AFV3, 3.0f, 0.77f);
  set(AcStrategy::Type::DCT128X128, 0.0f, 1.0f);
  set(AcStrategy::Type::DCT128X64, 0.0f, 0.73f);
  set(AcStrategy::Type::DCT64X128, 0.0f, 0.73f);
  set(AcStrategy::Type::DCT256X256, 0.0f, 1.0f);
  set(AcStrategy::Type::DCT256X128, 0.0f, 0.73f);
  set(AcStrategy::Type::DCT128X256, 0.0f, 0.73f);
  static_assert(AcStrategy::kNumValidStrategies == 27, "Keep in sync");
}
}  // namespace

}  // namespace jxl
#endif  // LIB_JXL_ENC_AC_STRATEGY_

HWY_BEFORE_NAMESPACE();
namespace jxl {
namespace HWY_NAMESPACE {

using hwy::HWY_NAMESPACE::ShiftLeft;
using hwy::HWY_NAMESPACE::ShiftRight;

float EstimateEntropy(const AcStrategy& acs, size_t x, size_t y,
                      const ACSConfig& config,
                      const float* JXL_RESTRICT cmap_factors, float* block,
                      float* scratch_space, uint32_t* quantized) {
  const size_t size = (1 << acs.log2_covered_blocks()) * kDCTBlockSize;

  // Apply transform.
  for (size_t c = 0; c < 3; c++) {
    float* JXL_RESTRICT block_c = block + size * c;
    TransformFromPixels(acs.Strategy(), &config.Pixel(c, x, y),
                        config.src_stride, block_c, scratch_space);
  }

  HWY_FULL(float) df;
  HWY_FULL(int) di;

  const size_t num_blocks = acs.covered_blocks_x() * acs.covered_blocks_y();
  float quant = 0;
  float quant_norm8 = 0;
  float masking = 0;
  {
    float masking_norm2 = 0;
    float masking_max = 0;
    // Load QF value, calculate empirical heuristic on masking field
    // for weighting the information loss. Information loss manifests
    // itself as ringing, and masking could hide it.
    for (size_t iy = 0; iy < acs.covered_blocks_y(); iy++) {
      for (size_t ix = 0; ix < acs.covered_blocks_x(); ix++) {
        float qval = config.Quant(x / 8 + ix, y / 8 + iy);
        quant = std::max(quant, qval);
        qval *= qval;
        qval *= qval;
        quant_norm8 += qval * qval;
        float maskval = config.Masking(x / 8 + ix, y / 8 + iy);
        masking_max = std::max<float>(masking_max, maskval);
        masking_norm2 += maskval * maskval;
      }
    }
    quant_norm8 /= num_blocks;
    quant_norm8 = FastPowf(quant_norm8, 1.0f / 8.0f);
    masking_norm2 = sqrt(masking_norm2 / num_blocks);
    // This is a highly empirical formula.
    masking = (masking_norm2 + masking_max);
  }
  const auto q = Set(df, quant_norm8);

  // Compute entropy.
  float entropy = config.base_entropy;
  auto info_loss = Zero(df);

  for (size_t c = 0; c < 3; c++) {
    const float* inv_matrix = config.dequant->InvMatrix(acs.RawStrategy(), c);
    const auto cmap_factor = Set(df, cmap_factors[c]);

    auto entropy_v = Zero(df);
    auto nzeros_v = Zero(di);
    auto cost1 = Set(df, config.cost1);
    auto cost2 = Set(df, config.cost2);
    auto cost_delta = Set(df, config.cost_delta);
    for (size_t i = 0; i < num_blocks * kDCTBlockSize; i += Lanes(df)) {
      const auto in = Load(df, block + c * size + i);
      const auto in_y = Load(df, block + size + i) * cmap_factor;
      const auto im = Load(df, inv_matrix + i);
      const auto val = (in - in_y) * im * q;
      const auto rval = Round(val);
      info_loss += AbsDiff(val, rval);
      const auto q = Abs(rval);
      const auto q_is_zero = q == Zero(df);
      entropy_v += IfThenElseZero(q >= Set(df, 0.5f), cost1);
      entropy_v += IfThenElseZero(q >= Set(df, 1.5f), cost2);
      // We used to have q * C here, but that cost model seems to
      // be punishing large values more than necessary. Sqrt tries
      // to avoid large values less aggressively. Having high accuracy
      // around zero is most important at low qualities, and there
      // we have directly specified costs for 0, 1, and 2.
      entropy_v += Sqrt(q) * cost_delta;
      nzeros_v +=
          BitCast(di, IfThenZeroElse(q_is_zero, BitCast(df, Set(di, 1))));
    }
    entropy += GetLane(SumOfLanes(entropy_v));
    size_t num_nzeros = GetLane(SumOfLanes(nzeros_v));
    // Add #bit of num_nonzeros, as an estimate of the cost for encoding the
    // number of non-zeros of the block.
    size_t nbits = CeilLog2Nonzero(num_nzeros + 1) + 1;
    // Also add #bit of #bit of num_nonzeros, to estimate the ANS cost, with a
    // bias.
    entropy += config.zeros_mul * (CeilLog2Nonzero(nbits + 17) + nbits);
  }
  float ret = entropy + masking * config.info_loss_multiplier *
                            GetLane(SumOfLanes(info_loss));
  return ret;
}

void MaybeReplaceACS(size_t bx, size_t by, const ACSConfig& config,
                     const float* JXL_RESTRICT cmap_factors,
                     AcStrategyImage* JXL_RESTRICT ac_strategy,
                     const float* JXL_RESTRICT entropy_adjust,
                     float* JXL_RESTRICT entropy_estimate, float* block,
                     float* scratch_space, uint32_t* quantized) {
  AcStrategy::Type current =
      AcStrategy::Type(ac_strategy->ConstRow(by)[bx].RawStrategy());
  AcStrategy::Type candidates[AcStrategy::kNumValidStrategies];
  size_t num_candidates = ACSPossibleReplacements(current, candidates);
  if (num_candidates == 0) return;
  size_t best = num_candidates;
  float best_ee = entropy_estimate[0];
  // For each candidate replacement strategy, keep track of its entropy
  // estimate.
  constexpr size_t kFit64X64DctInBlocks = 64 * 64 / (8 * 8);
  float ee_val[AcStrategy::kNumValidStrategies][kFit64X64DctInBlocks];
  AcStrategy current_acs = AcStrategy::FromRawStrategy(current);
  for (size_t cand = 0; cand < num_candidates; cand++) {
    AcStrategy acs = AcStrategy::FromRawStrategy(candidates[cand]);
    size_t idx = 0;
    float total_entropy = 0;
    for (size_t iy = 0; iy < current_acs.covered_blocks_y();
         iy += acs.covered_blocks_y()) {
      for (size_t ix = 0; ix < current_acs.covered_blocks_x();
           ix += acs.covered_blocks_x()) {
        const HWY_CAPPED(float, 1) df1;
        auto entropy1 =
            Set(df1,
                EstimateEntropy(acs, (bx + ix) * 8, (by + iy) * 8, config,
                                cmap_factors, block, scratch_space, quantized));
        entropy1 = MulAdd(entropy1,
                          Set(df1, entropy_adjust[2 * acs.RawStrategy() + 1]),
                          Set(df1, entropy_adjust[2 * acs.RawStrategy() + 0]));
        const float entropy = GetLane(entropy1);
        ee_val[cand][idx] = entropy;
        total_entropy += entropy;
        idx++;
      }
    }
    if (total_entropy < best_ee) {
      best_ee = total_entropy;
      best = cand;
    }
  }
  // Nothing changed.
  if (best == num_candidates) return;
  AcStrategy acs = AcStrategy::FromRawStrategy(candidates[best]);
  size_t idx = 0;
  for (size_t y = 0; y < current_acs.covered_blocks_y();
       y += acs.covered_blocks_y()) {
    for (size_t x = 0; x < current_acs.covered_blocks_x();
         x += acs.covered_blocks_x()) {
      ac_strategy->Set(bx + x, by + y, candidates[best]);
      for (size_t iy = y; iy < y + acs.covered_blocks_y(); iy++) {
        for (size_t ix = x; ix < x + acs.covered_blocks_x(); ix++) {
          entropy_estimate[iy * 8 + ix] = ee_val[best][idx];
        }
      }
      idx++;
    }
  }
}

void ProcessRectACS(PassesEncoderState* JXL_RESTRICT enc_state,
                    const ACSConfig& config, float* entropy_adjust,
                    const Rect& rect) {
  const CompressParams& cparams = enc_state->cparams;
  const float butteraugli_target = cparams.butteraugli_distance;
  AcStrategyImage* ac_strategy = &enc_state->shared.ac_strategy;

  const size_t xsize_blocks = enc_state->shared.frame_dim.xsize_blocks;
  const size_t ysize_blocks = enc_state->shared.frame_dim.ysize_blocks;

  // Maximum delta that every strategy type is allowed to have in the area
  // it covers. Ignored for 8x8 transforms. This heuristic is now mostly
  // disabled.
  const float kMaxDelta =
      0.5f * std::sqrt(butteraugli_target + 0.5);  // OPTIMIZE

  // TODO(veluca): reuse allocations
  auto mem = hwy::AllocateAligned<float>(5 * AcStrategy::kMaxCoeffArea);
  auto qmem = hwy::AllocateAligned<uint32_t>(AcStrategy::kMaxCoeffArea);
  uint32_t* JXL_RESTRICT quantized = qmem.get();
  float* JXL_RESTRICT block = mem.get();
  float* JXL_RESTRICT scratch_space = mem.get() + 3 * AcStrategy::kMaxCoeffArea;
  size_t bx = rect.x0();
  size_t by = rect.y0();
  JXL_ASSERT(rect.xsize() <= 8);
  JXL_ASSERT(rect.ysize() <= 8);
  size_t tx = bx / kColorTileDimInBlocks;
  size_t ty = by / kColorTileDimInBlocks;
  const float cmap_factors[3] = {
      enc_state->shared.cmap.YtoXRatio(
          enc_state->shared.cmap.ytox_map.ConstRow(ty)[tx]),
      0.0f,
      enc_state->shared.cmap.YtoBRatio(
          enc_state->shared.cmap.ytob_map.ConstRow(ty)[tx]),
  };
  HWY_CAPPED(float, kBlockDim) d;
  HWY_CAPPED(uint32_t, kBlockDim) di;

  // Padded, see UpdateMaxFlatness.
  HWY_ALIGN float pixels[3][8 + 64 + 8];
  for (size_t c = 0; c < 3; ++c) {
    pixels[c][8 - 2] = pixels[c][8 - 1] = 0.0f;  // value does not matter
    pixels[c][64] = pixels[c][64 + 1] = 0.0f;    // value does not matter
  }

  // Scale of channels when computing delta.
  const float kDeltaScale[3] = {3.0f, 1.0f, 0.2f};

  // Pre-compute maximum delta in each 8x8 block.
  // Find a minimum delta of three options:
  // 1) all, 2) not accounting vertical, 3) not accounting horizontal
  float max_delta[3][64] = {};
  float entropy_estimate[64] = {};
  for (size_t c = 0; c < 3; c++) {
    for (size_t iy = 0; iy < rect.ysize(); iy++) {
      size_t dy = by + iy;
      for (size_t ix = 0; ix < rect.xsize(); ix++) {
        size_t dx = bx + ix;
        for (size_t y = 0; y < 8; y++) {
          for (size_t x = 0; x < 8; x += Lanes(d)) {
            const auto v = Load(d, &config.Pixel(c, dx * 8 + x, dy * 8 + y));
            Store(v, d, &pixels[c][y * 8 + x + 8]);
          }
        }

        auto delta = Zero(d);
        for (size_t x = 0; x < 8; x += Lanes(d)) {
          HWY_ALIGN const uint32_t kMask[] = {0u,  ~0u, ~0u, ~0u,
                                              ~0u, ~0u, ~0u, 0u};
          auto mask = BitCast(d, Load(di, kMask + x));
          for (size_t y = 1; y < 7; y++) {
            float* pix = &pixels[c][y * 8 + x + 8];
            const auto p = Load(d, pix);
            const auto n = Load(d, pix + 8);
            const auto s = Load(d, pix - 8);
            const auto w = LoadU(d, pix - 1);
            const auto e = LoadU(d, pix + 1);
            // Compute amount of per-pixel variation.
            const auto m1 = Max(AbsDiff(n, p), AbsDiff(s, p));
            const auto m2 = Max(AbsDiff(w, p), AbsDiff(e, p));
            const auto m3 = Max(AbsDiff(e, w), AbsDiff(s, n));
            const auto m4 = Max(m1, m2);
            const auto m5 = Max(m3, m4);
            delta = Max(delta, m5);
          }
          const float mdelta = GetLane(MaxOfLanes(And(mask, delta)));
          max_delta[c][iy * 8 + ix] =
              std::max(max_delta[c][iy * 8 + ix], mdelta * kDeltaScale[c]);
        }
      }
    }
  }

  // Choose the first transform that can be used to cover each block.
  uint8_t chosen_mask[64] = {0};
  for (size_t iy = 0; iy < rect.ysize(); iy++) {
    for (size_t ix = 0; ix < rect.xsize(); ix++) {
      if (chosen_mask[iy * 8 + ix]) continue;
      for (auto i : kACSOrder) {
        AcStrategy acs = AcStrategy::FromRawStrategy(i);
        size_t cx = acs.covered_blocks_x();
        size_t cy = acs.covered_blocks_y();
        // Only blocks up to a certain size if targeting faster decoding.
        if (cparams.decoding_speed_tier >= 1) {
          if (cx * cy > 16) continue;
        }
        if (cparams.decoding_speed_tier >= 2) {
          if (cx * cy > 8) continue;
        }
        float max_delta_v[3] = {max_delta[0][iy * 8 + ix],
                                max_delta[1][iy * 8 + ix],
                                max_delta[2][iy * 8 + ix]};
        float max2_delta_v[3] = {0, 0, 0};
        float max_delta_acs =
            std::max(std::max(max_delta_v[0], max_delta_v[1]), max_delta_v[2]);
        float min_delta_v[3] = {1e30f, 1e30f, 1e30f};
        float ave_delta_v[3] = {};
        // Check if strategy is usable
        if (cx != 1 || cy != 1) {
          // Alignment
          if ((iy & (cy - 1)) != 0) continue;
          if ((ix & (cx - 1)) != 0) continue;
          // Out of block64 bounds
          if (iy + cy > 8) continue;
          if (ix + cx > 8) continue;
          // Out of image bounds
          if (by + iy + cy > ysize_blocks) continue;
          if (bx + ix + cx > xsize_blocks) continue;
          // Block would overwrite an already-chosen block
          bool overwrites_covered = false;
          for (size_t y = 0; y < cy; y++) {
            for (size_t x = 0; x < cx; x++) {
              if (chosen_mask[(y + iy) * 8 + x + ix]) overwrites_covered = true;
            }
          }
          if (overwrites_covered) continue;
          for (size_t c = 0; c < 3; ++c) {
            max_delta_v[c] = 0;
            max2_delta_v[c] = 0;
            min_delta_v[c] = 1e30f;
            ave_delta_v[c] = 0;
            // Max delta in covered area
            for (size_t y = 0; y < cy; y++) {
              for (size_t x = 0; x < cx; x++) {
                int pix = (iy + y) * 8 + ix + x;
                if (max_delta_v[c] < max_delta[c][pix]) {
                  max2_delta_v[c] = max_delta_v[c];
                  max_delta_v[c] = max_delta[c][pix];
                } else if (max2_delta_v[c] < max_delta[c][pix]) {
                  max2_delta_v[c] = max_delta[c][pix];
                }
                min_delta_v[c] = std::min(min_delta_v[c], max_delta[c][pix]);
                ave_delta_v[c] += max_delta[c][pix];
              }
            }
            ave_delta_v[c] -= max_delta_v[c];
            if (cy * cx >= 5) {
              ave_delta_v[c] -= max2_delta_v[c];
              ave_delta_v[c] /= (cy * cx - 2);
            } else {
              ave_delta_v[c] /= (cy * cx - 1);
            }
            max_delta_v[c] -= 0.03f * max2_delta_v[c];
            max_delta_v[c] -= 0.25f * min_delta_v[c];
            max_delta_v[c] -= 0.25f * ave_delta_v[c];
          }
          max_delta_acs = max_delta_v[0] + max_delta_v[1] + max_delta_v[2];
          max_delta_acs *= std::pow(1.044f, cx * cy);
          if (max_delta_acs > kMaxDelta) continue;
        }
        // Estimate entropy and qf value
        float entropy = 0.0f;
        // In modes faster than Wombat mode, AC strategy replacement is not
        // attempted: no need to estimate entropy.
        if (cparams.speed_tier <= SpeedTier::kWombat) {
          entropy =
              EstimateEntropy(acs, (bx + ix) * 8, (by + iy) * 8, config,
                              cmap_factors, block, scratch_space, quantized);
          entropy *= entropy_adjust[i * 2 + 1];
        }
        // In modes faster than Hare mode, we don't use InitialQuantField -
        // hence, we need to come up with quant field values.
        if (cparams.speed_tier > SpeedTier::kHare &&
            cparams.uniform_quant <= 0) {
          // OPTIMIZE
          float quant = 1.1f / (1.0f + max_delta_acs) / butteraugli_target;
          for (size_t y = 0; y < cy; y++) {
            for (size_t x = 0; x < cx; x++) {
              config.SetQuant(bx + ix + x, by + iy + y, quant);
            }
          }
        }
        // Mark blocks as chosen and write to acs image.
        ac_strategy->Set(bx + ix, by + iy, i);
        for (size_t y = 0; y < cy; y++) {
          for (size_t x = 0; x < cx; x++) {
            chosen_mask[(y + iy) * 8 + x + ix] = 1;
            entropy_estimate[(iy + y) * 8 + ix + x] = entropy;
          }
        }
        break;
      }
    }
  }
  // Do not try to replace ACS in modes faster than wombat mode.
  if (cparams.speed_tier > SpeedTier::kWombat) return;
  // Iterate through the 32-block attempting to replace the current strategy.
  // If replaced, repeat for the top-left new block and let the other ones be
  // taken care of by future iterations.
  uint8_t computed_mask[64] = {};
  for (size_t iy = 0; iy < rect.ysize(); iy++) {
    for (size_t ix = 0; ix < rect.xsize(); ix++) {
      if (computed_mask[iy * 8 + ix]) continue;
      uint8_t prev = AcStrategy::kNumValidStrategies;
      while (prev != ac_strategy->ConstRow(by + iy)[bx + ix].RawStrategy()) {
        prev = ac_strategy->ConstRow(by + iy)[bx + ix].RawStrategy();
        MaybeReplaceACS(bx + ix, by + iy, config, cmap_factors, ac_strategy,
                        entropy_adjust, entropy_estimate + (iy * 8 + ix), block,
                        scratch_space, quantized);
      }
      AcStrategy acs = ac_strategy->ConstRow(by + iy)[bx + ix];
      for (size_t y = 0; y < acs.covered_blocks_y(); y++) {
        for (size_t x = 0; x < acs.covered_blocks_x(); x++) {
          computed_mask[(iy + y) * 8 + ix + x] = 1;
        }
      }
    }
  }
}

// NOLINTNEXTLINE(google-readability-namespace-comments)
}  // namespace HWY_NAMESPACE
}  // namespace jxl
HWY_AFTER_NAMESPACE();

#if HWY_ONCE
namespace jxl {
HWY_EXPORT(ProcessRectACS);

void AcStrategyHeuristics::Init(const Image3F& src,
                                PassesEncoderState* enc_state) {
  this->enc_state = enc_state;
  const CompressParams& cparams = enc_state->cparams;
  const float butteraugli_target = cparams.butteraugli_distance;

  config.dequant = &enc_state->shared.matrices;

  // Image row pointers and strides.
  config.quant_field_row = enc_state->initial_quant_field.Row(0);
  config.quant_field_stride = enc_state->initial_quant_field.PixelsPerRow();
  auto& mask = enc_state->initial_quant_masking;
  if (mask.xsize() > 0 && mask.ysize() > 0) {
    config.masking_field_row = mask.Row(0);
    config.masking_field_stride = mask.PixelsPerRow();
  }

  config.src_rows[0] = src.ConstPlaneRow(0, 0);
  config.src_rows[1] = src.ConstPlaneRow(1, 0);
  config.src_rows[2] = src.ConstPlaneRow(2, 0);
  config.src_stride = src.PixelsPerRow();

  InitEntropyAdjustTable(entropy_adjust);

  // Entropy estimate is composed of two factors:
  //  - estimate of the number of bits that will be used by the block
  //  - information loss due to quantization
  // The following constant controls the relative weights of these components.
  // TODO(jyrki): better choice of constants/parameterization.
  config.info_loss_multiplier = 39.2;
  config.base_entropy = 30.0;
  config.zeros_mul = 0.3;  // Possibly a bigger value would work better.
  if (butteraugli_target < 2) {
    config.cost1 = 2.1467536133280064f;
    config.cost2 = 4.5233239814548617f;
    config.cost_delta = 2.7192877948074784f;
  } else if (butteraugli_target < 4) {
    config.cost1 = 3.3478899662356103f;
    config.cost2 = 3.2493410394508086f;
    config.cost_delta = 2.9192251887428096f;
  } else if (butteraugli_target < 8) {
    config.cost1 = 3.9758237938237959f;
    config.cost2 = 1.2423859153559777f;
    config.cost_delta = 3.1181324266623122f;
  } else if (butteraugli_target < 16) {
    config.cost1 = 2.5;
    config.cost2 = 2.2630019747782897f;
    config.cost_delta = 3.8409539247825222f;
  } else {
    config.cost1 = 1.5;
    config.cost2 = 2.6952503610099059f;
    config.cost_delta = 4.316274170126156f;
  }

  JXL_ASSERT(enc_state->shared.ac_strategy.xsize() ==
             enc_state->shared.frame_dim.xsize_blocks);
  JXL_ASSERT(enc_state->shared.ac_strategy.ysize() ==
             enc_state->shared.frame_dim.ysize_blocks);
}

void AcStrategyHeuristics::ProcessRect(const Rect& rect) {
  PROFILER_FUNC;
  const CompressParams& cparams = enc_state->cparams;
  // In Falcon mode, use DCT8 everywhere and uniform quantization.
  if (cparams.speed_tier == SpeedTier::kFalcon) {
    enc_state->shared.ac_strategy.FillDCT8(rect);
    return;
  }
  HWY_DYNAMIC_DISPATCH(ProcessRectACS)
  (enc_state, config, entropy_adjust, rect);
}

void AcStrategyHeuristics::Finalize(AuxOut* aux_out) {
  const auto& ac_strategy = enc_state->shared.ac_strategy;
  // Accounting and debug output.
  if (aux_out != nullptr) {
    aux_out->num_dct2_blocks =
        32 * (ac_strategy.CountBlocks(AcStrategy::Type::DCT32X64) +
              ac_strategy.CountBlocks(AcStrategy::Type::DCT64X32));
    aux_out->num_dct4_blocks =
        64 * ac_strategy.CountBlocks(AcStrategy::Type::DCT64X64);
    aux_out->num_dct4x8_blocks =
        ac_strategy.CountBlocks(AcStrategy::Type::DCT4X8) +
        ac_strategy.CountBlocks(AcStrategy::Type::DCT8X4);
    aux_out->num_afv_blocks = ac_strategy.CountBlocks(AcStrategy::Type::AFV0) +
                              ac_strategy.CountBlocks(AcStrategy::Type::AFV1) +
                              ac_strategy.CountBlocks(AcStrategy::Type::AFV2) +
                              ac_strategy.CountBlocks(AcStrategy::Type::AFV3);
    aux_out->num_dct8_blocks = ac_strategy.CountBlocks(AcStrategy::Type::DCT);
    aux_out->num_dct8x16_blocks =
        ac_strategy.CountBlocks(AcStrategy::Type::DCT8X16) +
        ac_strategy.CountBlocks(AcStrategy::Type::DCT16X8);
    aux_out->num_dct8x32_blocks =
        ac_strategy.CountBlocks(AcStrategy::Type::DCT8X32) +
        ac_strategy.CountBlocks(AcStrategy::Type::DCT32X8);
    aux_out->num_dct16_blocks =
        ac_strategy.CountBlocks(AcStrategy::Type::DCT16X16);
    aux_out->num_dct16x32_blocks =
        ac_strategy.CountBlocks(AcStrategy::Type::DCT16X32) +
        ac_strategy.CountBlocks(AcStrategy::Type::DCT32X16);
    aux_out->num_dct32_blocks =
        ac_strategy.CountBlocks(AcStrategy::Type::DCT32X32);
  }

  if (WantDebugOutput(aux_out)) {
    DumpAcStrategy(ac_strategy, enc_state->shared.frame_dim.xsize,
                   enc_state->shared.frame_dim.ysize, "ac_strategy", aux_out);
  }
}

}  // namespace jxl
#endif  // HWY_ONCE
