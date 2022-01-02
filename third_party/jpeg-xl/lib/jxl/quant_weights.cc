// Copyright (c) the JPEG XL Project Authors. All rights reserved.
//
// Use of this source code is governed by a BSD-style
// license that can be found in the LICENSE file.
#include "lib/jxl/quant_weights.h"

#include <stdio.h>
#include <stdlib.h>

#include <algorithm>
#include <cmath>
#include <limits>
#include <utility>

#include "lib/jxl/base/bits.h"
#include "lib/jxl/base/printf_macros.h"
#include "lib/jxl/base/status.h"
#include "lib/jxl/common.h"
#include "lib/jxl/dct_scales.h"
#include "lib/jxl/dec_modular.h"
#include "lib/jxl/fields.h"
#include "lib/jxl/image.h"

namespace jxl {

// kQuantWeights[N * N * c + N * y + x] is the relative weight of the (x, y)
// coefficient in component c. Higher weights correspond to finer quantization
// intervals and more bits spent in encoding.

namespace {

static constexpr const float kAlmostZero = 1e-8f;

void GetQuantWeightsDCT2(const QuantEncoding::DCT2Weights& dct2weights,
                         float* weights) {
  for (size_t c = 0; c < 3; c++) {
    size_t start = c * 64;
    weights[start] = 0xBAD;
    weights[start + 1] = weights[start + 8] = dct2weights[c][0];
    weights[start + 9] = dct2weights[c][1];
    for (size_t y = 0; y < 2; y++) {
      for (size_t x = 0; x < 2; x++) {
        weights[start + y * 8 + x + 2] = dct2weights[c][2];
        weights[start + (y + 2) * 8 + x] = dct2weights[c][2];
      }
    }
    for (size_t y = 0; y < 2; y++) {
      for (size_t x = 0; x < 2; x++) {
        weights[start + (y + 2) * 8 + x + 2] = dct2weights[c][3];
      }
    }
    for (size_t y = 0; y < 4; y++) {
      for (size_t x = 0; x < 4; x++) {
        weights[start + y * 8 + x + 4] = dct2weights[c][4];
        weights[start + (y + 4) * 8 + x] = dct2weights[c][4];
      }
    }
    for (size_t y = 0; y < 4; y++) {
      for (size_t x = 0; x < 4; x++) {
        weights[start + (y + 4) * 8 + x + 4] = dct2weights[c][5];
      }
    }
  }
}

void GetQuantWeightsIdentity(const QuantEncoding::IdWeights& idweights,
                             float* weights) {
  for (size_t c = 0; c < 3; c++) {
    for (int i = 0; i < 64; i++) {
      weights[64 * c + i] = idweights[c][0];
    }
    weights[64 * c + 1] = idweights[c][1];
    weights[64 * c + 8] = idweights[c][1];
    weights[64 * c + 9] = idweights[c][2];
  }
}

float Mult(float v) {
  if (v > 0) return 1 + v;
  return 1 / (1 - v);
}

float Interpolate(float pos, float max, const float* array, size_t len) {
  float scaled_pos = pos * (len - 1) / max;
  size_t idx = scaled_pos;
  JXL_ASSERT(idx + 1 < len);
  float a = array[idx];
  float b = array[idx + 1];
  return a * pow(b / a, scaled_pos - idx);
}

// Computes quant weights for a COLS*ROWS-sized transform, using num_bands
// eccentricity bands and num_ebands eccentricity bands. If print_mode is 1,
// prints the resulting matrix; if print_mode is 2, prints the matrix in a
// format suitable for a 3d plot with gnuplot.
template <size_t print_mode = 0>
Status GetQuantWeights(
    size_t ROWS, size_t COLS,
    const DctQuantWeightParams::DistanceBandsArray& distance_bands,
    size_t num_bands, float* out) {
  for (size_t c = 0; c < 3; c++) {
    if (print_mode) {
      fprintf(stderr, "Channel %" PRIuS "\n", c);
    }
    float bands[DctQuantWeightParams::kMaxDistanceBands] = {
        distance_bands[c][0]};
    if (bands[0] < kAlmostZero) return JXL_FAILURE("Invalid distance bands");
    for (size_t i = 1; i < num_bands; i++) {
      bands[i] = bands[i - 1] * Mult(distance_bands[c][i]);
      if (bands[i] < kAlmostZero) return JXL_FAILURE("Invalid distance bands");
    }
    for (size_t y = 0; y < ROWS; y++) {
      for (size_t x = 0; x < COLS; x++) {
        float dx = 1.0f * x / (COLS - 1);
        float dy = 1.0f * y / (ROWS - 1);
        float distance = std::sqrt(dx * dx + dy * dy);
        float weight =
            num_bands == 1
                ? bands[0]
                : Interpolate(distance, std::sqrt(2) + 1e-6f, bands, num_bands);

        if (print_mode == 1) {
          fprintf(stderr, "%15.12f, ", weight);
        }
        if (print_mode == 2) {
          fprintf(stderr, "%" PRIuS " %" PRIuS " %15.12f\n", x, y, weight);
        }
        out[c * COLS * ROWS + y * COLS + x] = weight;
      }
      if (print_mode) fprintf(stderr, "\n");
      if (print_mode == 1) fprintf(stderr, "\n");
    }
    if (print_mode) fprintf(stderr, "\n");
  }
  return true;
}

Status DecodeDctParams(BitReader* br, DctQuantWeightParams* params) {
  params->num_distance_bands =
      br->ReadFixedBits<DctQuantWeightParams::kLog2MaxDistanceBands>() + 1;
  for (size_t c = 0; c < 3; c++) {
    for (size_t i = 0; i < params->num_distance_bands; i++) {
      JXL_RETURN_IF_ERROR(F16Coder::Read(br, &params->distance_bands[c][i]));
    }
    if (params->distance_bands[c][0] < kAlmostZero) {
      return JXL_FAILURE("Distance band seed is too small");
    }
    params->distance_bands[c][0] *= 64.0f;
  }
  return true;
}

Status Decode(BitReader* br, QuantEncoding* encoding, size_t required_size_x,
              size_t required_size_y, size_t idx,
              ModularFrameDecoder* modular_frame_decoder) {
  size_t required_size = required_size_x * required_size_y;
  required_size_x *= kBlockDim;
  required_size_y *= kBlockDim;
  int mode = br->ReadFixedBits<kLog2NumQuantModes>();
  switch (mode) {
    case QuantEncoding::kQuantModeLibrary: {
      encoding->predefined = br->ReadFixedBits<kCeilLog2NumPredefinedTables>();
      if (encoding->predefined >= kNumPredefinedTables) {
        return JXL_FAILURE("Invalid predefined table");
      }
      break;
    }
    case QuantEncoding::kQuantModeID: {
      if (required_size != 1) return JXL_FAILURE("Invalid mode");
      for (size_t c = 0; c < 3; c++) {
        for (size_t i = 0; i < 3; i++) {
          JXL_RETURN_IF_ERROR(F16Coder::Read(br, &encoding->idweights[c][i]));
          if (std::abs(encoding->idweights[c][i]) < kAlmostZero) {
            return JXL_FAILURE("ID Quantizer is too small");
          }
          encoding->idweights[c][i] *= 64;
        }
      }
      break;
    }
    case QuantEncoding::kQuantModeDCT2: {
      if (required_size != 1) return JXL_FAILURE("Invalid mode");
      for (size_t c = 0; c < 3; c++) {
        for (size_t i = 0; i < 6; i++) {
          JXL_RETURN_IF_ERROR(F16Coder::Read(br, &encoding->dct2weights[c][i]));
          if (std::abs(encoding->dct2weights[c][i]) < kAlmostZero) {
            return JXL_FAILURE("Quantizer is too small");
          }
          encoding->dct2weights[c][i] *= 64;
        }
      }
      break;
    }
    case QuantEncoding::kQuantModeDCT4X8: {
      if (required_size != 1) return JXL_FAILURE("Invalid mode");
      for (size_t c = 0; c < 3; c++) {
        JXL_RETURN_IF_ERROR(
            F16Coder::Read(br, &encoding->dct4x8multipliers[c]));
        if (std::abs(encoding->dct4x8multipliers[c]) < kAlmostZero) {
          return JXL_FAILURE("DCT4X8 multiplier is too small");
        }
      }
      JXL_RETURN_IF_ERROR(DecodeDctParams(br, &encoding->dct_params));
      break;
    }
    case QuantEncoding::kQuantModeDCT4: {
      if (required_size != 1) return JXL_FAILURE("Invalid mode");
      for (size_t c = 0; c < 3; c++) {
        for (size_t i = 0; i < 2; i++) {
          JXL_RETURN_IF_ERROR(
              F16Coder::Read(br, &encoding->dct4multipliers[c][i]));
          if (std::abs(encoding->dct4multipliers[c][i]) < kAlmostZero) {
            return JXL_FAILURE("DCT4 multiplier is too small");
          }
        }
      }
      JXL_RETURN_IF_ERROR(DecodeDctParams(br, &encoding->dct_params));
      break;
    }
    case QuantEncoding::kQuantModeAFV: {
      if (required_size != 1) return JXL_FAILURE("Invalid mode");
      for (size_t c = 0; c < 3; c++) {
        for (size_t i = 0; i < 9; i++) {
          JXL_RETURN_IF_ERROR(F16Coder::Read(br, &encoding->afv_weights[c][i]));
        }
        for (size_t i = 0; i < 6; i++) {
          encoding->afv_weights[c][i] *= 64;
        }
        JXL_RETURN_IF_ERROR(DecodeDctParams(br, &encoding->dct_params));
        JXL_RETURN_IF_ERROR(DecodeDctParams(br, &encoding->dct_params_afv_4x4));
      }
      break;
    }
    case QuantEncoding::kQuantModeDCT: {
      JXL_RETURN_IF_ERROR(DecodeDctParams(br, &encoding->dct_params));
      break;
    }
    case QuantEncoding::kQuantModeRAW: {
      // Set mode early, to avoid mem-leak.
      encoding->mode = QuantEncoding::kQuantModeRAW;
      JXL_RETURN_IF_ERROR(ModularFrameDecoder::DecodeQuantTable(
          required_size_x, required_size_y, br, encoding, idx,
          modular_frame_decoder));
      break;
    }
    default:
      return JXL_FAILURE("Invalid quantization table encoding");
  }
  encoding->mode = QuantEncoding::Mode(mode);
  return true;
}

// TODO(veluca): SIMD-fy. With 256x256, this is actually slow.
Status ComputeQuantTable(const QuantEncoding& encoding,
                         float* JXL_RESTRICT table,
                         float* JXL_RESTRICT inv_table, size_t table_num,
                         DequantMatrices::QuantTable kind, size_t* pos) {
  std::vector<float> weights(3 * kMaxQuantTableSize);

  constexpr size_t N = kBlockDim;
  size_t wrows = 8 * DequantMatrices::required_size_x[kind],
         wcols = 8 * DequantMatrices::required_size_y[kind];
  size_t num = wrows * wcols;

  switch (encoding.mode) {
    case QuantEncoding::kQuantModeLibrary: {
      // Library and copy quant encoding should get replaced by the actual
      // parameters by the caller.
      JXL_ASSERT(false);
      break;
    }
    case QuantEncoding::kQuantModeID: {
      JXL_ASSERT(num == kDCTBlockSize);
      GetQuantWeightsIdentity(encoding.idweights, weights.data());
      break;
    }
    case QuantEncoding::kQuantModeDCT2: {
      JXL_ASSERT(num == kDCTBlockSize);
      GetQuantWeightsDCT2(encoding.dct2weights, weights.data());
      break;
    }
    case QuantEncoding::kQuantModeDCT4: {
      JXL_ASSERT(num == kDCTBlockSize);
      float weights4x4[3 * 4 * 4];
      // Always use 4x4 GetQuantWeights for DCT4 quantization tables.
      JXL_RETURN_IF_ERROR(
          GetQuantWeights(4, 4, encoding.dct_params.distance_bands,
                          encoding.dct_params.num_distance_bands, weights4x4));
      for (size_t c = 0; c < 3; c++) {
        for (size_t y = 0; y < kBlockDim; y++) {
          for (size_t x = 0; x < kBlockDim; x++) {
            weights[c * num + y * kBlockDim + x] =
                weights4x4[c * 16 + (y / 2) * 4 + (x / 2)];
          }
        }
        weights[c * num + 1] /= encoding.dct4multipliers[c][0];
        weights[c * num + N] /= encoding.dct4multipliers[c][0];
        weights[c * num + N + 1] /= encoding.dct4multipliers[c][1];
      }
      break;
    }
    case QuantEncoding::kQuantModeDCT4X8: {
      JXL_ASSERT(num == kDCTBlockSize);
      float weights4x8[3 * 4 * 8];
      // Always use 4x8 GetQuantWeights for DCT4X8 quantization tables.
      JXL_RETURN_IF_ERROR(
          GetQuantWeights(4, 8, encoding.dct_params.distance_bands,
                          encoding.dct_params.num_distance_bands, weights4x8));
      for (size_t c = 0; c < 3; c++) {
        for (size_t y = 0; y < kBlockDim; y++) {
          for (size_t x = 0; x < kBlockDim; x++) {
            weights[c * num + y * kBlockDim + x] =
                weights4x8[c * 32 + (y / 2) * 8 + x];
          }
        }
        weights[c * num + N] /= encoding.dct4x8multipliers[c];
      }
      break;
    }
    case QuantEncoding::kQuantModeDCT: {
      JXL_RETURN_IF_ERROR(GetQuantWeights(
          wrows, wcols, encoding.dct_params.distance_bands,
          encoding.dct_params.num_distance_bands, weights.data()));
      break;
    }
    case QuantEncoding::kQuantModeRAW: {
      if (!encoding.qraw.qtable || encoding.qraw.qtable->size() != 3 * num) {
        return JXL_FAILURE("Invalid table encoding");
      }
      for (size_t i = 0; i < 3 * num; i++) {
        weights[i] =
            1.f / (encoding.qraw.qtable_den * (*encoding.qraw.qtable)[i]);
      }
      break;
    }
    case QuantEncoding::kQuantModeAFV: {
      constexpr float kFreqs[] = {
          0xBAD,
          0xBAD,
          0.8517778890324296,
          5.37778436506804,
          0xBAD,
          0xBAD,
          4.734747904497923,
          5.449245381693219,
          1.6598270267479331,
          4,
          7.275749096817861,
          10.423227632456525,
          2.662932286148962,
          7.630657783650829,
          8.962388608184032,
          12.97166202570235,
      };

      float weights4x8[3 * 4 * 8];
      JXL_RETURN_IF_ERROR((
          GetQuantWeights(4, 8, encoding.dct_params.distance_bands,
                          encoding.dct_params.num_distance_bands, weights4x8)));
      float weights4x4[3 * 4 * 4];
      JXL_RETURN_IF_ERROR((GetQuantWeights(
          4, 4, encoding.dct_params_afv_4x4.distance_bands,
          encoding.dct_params_afv_4x4.num_distance_bands, weights4x4)));

      constexpr float lo = 0.8517778890324296;
      constexpr float hi = 12.97166202570235 - lo + 1e-6;
      for (size_t c = 0; c < 3; c++) {
        float bands[4];
        bands[0] = encoding.afv_weights[c][5];
        if (bands[0] < kAlmostZero) return JXL_FAILURE("Invalid AFV bands");
        for (size_t i = 1; i < 4; i++) {
          bands[i] = bands[i - 1] * Mult(encoding.afv_weights[c][i + 5]);
          if (bands[i] < kAlmostZero) return JXL_FAILURE("Invalid AFV bands");
        }
        size_t start = c * 64;
        auto set_weight = [&start, &weights](size_t x, size_t y, float val) {
          weights[start + y * 8 + x] = val;
        };
        weights[start] = 1;  // Not used, but causes MSAN error otherwise.
        // Weights for (0, 1) and (1, 0).
        set_weight(0, 1, encoding.afv_weights[c][0]);
        set_weight(1, 0, encoding.afv_weights[c][1]);
        // AFV special weights for 3-pixel corner.
        set_weight(0, 2, encoding.afv_weights[c][2]);
        set_weight(2, 0, encoding.afv_weights[c][3]);
        set_weight(2, 2, encoding.afv_weights[c][4]);

        // All other AFV weights.
        for (size_t y = 0; y < 4; y++) {
          for (size_t x = 0; x < 4; x++) {
            if (x < 2 && y < 2) continue;
            float val = Interpolate(kFreqs[y * 4 + x] - lo, hi, bands, 4);
            set_weight(2 * x, 2 * y, val);
          }
        }

        // Put 4x8 weights in odd rows, except (1, 0).
        for (size_t y = 0; y < kBlockDim / 2; y++) {
          for (size_t x = 0; x < kBlockDim; x++) {
            if (x == 0 && y == 0) continue;
            weights[c * num + (2 * y + 1) * kBlockDim + x] =
                weights4x8[c * 32 + y * 8 + x];
          }
        }
        // Put 4x4 weights in even rows / odd columns, except (0, 1).
        for (size_t y = 0; y < kBlockDim / 2; y++) {
          for (size_t x = 0; x < kBlockDim / 2; x++) {
            if (x == 0 && y == 0) continue;
            weights[c * num + (2 * y) * kBlockDim + 2 * x + 1] =
                weights4x4[c * 16 + y * 4 + x];
          }
        }
      }
      break;
    }
  }
  size_t prev_pos = *pos;
  for (size_t c = 0; c < 3; c++) {
    for (size_t i = 0; i < num; i++) {
      float inv_val = weights[c * num + i];
      if (inv_val > 1.0f / kAlmostZero || inv_val < kAlmostZero) {
        return JXL_FAILURE("Invalid quantization table");
      }
      float val = 1.0f / inv_val;
      table[*pos] = val;
      inv_table[*pos] = inv_val;
      (*pos)++;
    }
  }
  // Ensure that the lowest frequencies have a 0 inverse table.
  // This does not affect en/decoding, but allows AC strategy selection to be
  // slightly simpler.
  size_t xs = DequantMatrices::required_size_x[kind];
  size_t ys = DequantMatrices::required_size_y[kind];
  CoefficientLayout(&ys, &xs);
  for (size_t c = 0; c < 3; c++) {
    for (size_t y = 0; y < ys; y++) {
      for (size_t x = 0; x < xs; x++) {
        inv_table[prev_pos + c * ys * xs * kDCTBlockSize + y * kBlockDim * xs +
                  x] = 0;
      }
    }
  }
  return true;
}

}  // namespace

// These definitions are needed before C++17.
constexpr size_t DequantMatrices::required_size_[];
constexpr size_t DequantMatrices::required_size_x[];
constexpr size_t DequantMatrices::required_size_y[];
constexpr DequantMatrices::QuantTable DequantMatrices::kQuantTable[];

Status DequantMatrices::Decode(BitReader* br,
                               ModularFrameDecoder* modular_frame_decoder) {
  size_t all_default = br->ReadBits(1);
  size_t num_tables = all_default ? 0 : static_cast<size_t>(kNum);
  encodings_.clear();
  encodings_.resize(kNum, QuantEncoding::Library(0));
  for (size_t i = 0; i < num_tables; i++) {
    JXL_RETURN_IF_ERROR(
        jxl::Decode(br, &encodings_[i], required_size_x[i % kNum],
                    required_size_y[i % kNum], i, modular_frame_decoder));
  }
  return DequantMatrices::Compute();
}

Status DequantMatrices::DecodeDC(BitReader* br) {
  bool all_default = br->ReadBits(1);
  if (!all_default) {
    for (size_t c = 0; c < 3; c++) {
      JXL_RETURN_IF_ERROR(F16Coder::Read(br, &dc_quant_[c]));
      dc_quant_[c] *= 1.0f / 128.0f;
      // Negative values and nearly zero are invalid values.
      if (dc_quant_[c] < kAlmostZero) {
        return JXL_FAILURE("Invalid dc_quant: coefficient is too small.");
      }
      inv_dc_quant_[c] = 1.0f / dc_quant_[c];
    }
  }
  return true;
}

constexpr float V(float v) { return static_cast<float>(v); }

namespace {
struct DequantMatricesLibraryDef {
  // DCT8
  static constexpr const QuantEncodingInternal DCT() {
    return QuantEncodingInternal::DCT(DctQuantWeightParams({{{{
                                                                 V(3150.0),
                                                                 V(0.0),
                                                                 V(-0.4),
                                                                 V(-0.4),
                                                                 V(-0.4),
                                                                 V(-2.0),
                                                             }},
                                                             {{
                                                                 V(560.0),
                                                                 V(0.0),
                                                                 V(-0.3),
                                                                 V(-0.3),
                                                                 V(-0.3),
                                                                 V(-0.3),
                                                             }},
                                                             {{
                                                                 V(512.0),
                                                                 V(-2.0),
                                                                 V(-1.0),
                                                                 V(0.0),
                                                                 V(-1.0),
                                                                 V(-2.0),
                                                             }}}},
                                                           6));
  }

  // Identity
  static constexpr const QuantEncodingInternal IDENTITY() {
    return QuantEncodingInternal::Identity({{{{
                                                 V(280.0),
                                                 V(3160.0),
                                                 V(3160.0),
                                             }},
                                             {{
                                                 V(60.0),
                                                 V(864.0),
                                                 V(864.0),
                                             }},
                                             {{
                                                 V(18.0),
                                                 V(200.0),
                                                 V(200.0),
                                             }}}});
  }

  // DCT2
  static constexpr const QuantEncodingInternal DCT2X2() {
    return QuantEncodingInternal::DCT2({{{{
                                             V(3840.0),
                                             V(2560.0),
                                             V(1280.0),
                                             V(640.0),
                                             V(480.0),
                                             V(300.0),
                                         }},
                                         {{
                                             V(960.0),
                                             V(640.0),
                                             V(320.0),
                                             V(180.0),
                                             V(140.0),
                                             V(120.0),
                                         }},
                                         {{
                                             V(640.0),
                                             V(320.0),
                                             V(128.0),
                                             V(64.0),
                                             V(32.0),
                                             V(16.0),
                                         }}}});
  }

  // DCT4 (quant_kind 3)
  static constexpr const QuantEncodingInternal DCT4X4() {
    return QuantEncodingInternal::DCT4(DctQuantWeightParams({{{{
                                                                  V(2200.0),
                                                                  V(0.0),
                                                                  V(0.0),
                                                                  V(0.0),
                                                              }},
                                                              {{
                                                                  V(392.0),
                                                                  V(0.0),
                                                                  V(0.0),
                                                                  V(0.0),
                                                              }},
                                                              {{
                                                                  V(112.0),
                                                                  V(-0.25),
                                                                  V(-0.25),
                                                                  V(-0.5),
                                                              }}}},
                                                            4),
                                       /* kMul */
                                       {{{{
                                             V(1.0),
                                             V(1.0),
                                         }},
                                         {{
                                             V(1.0),
                                             V(1.0),
                                         }},
                                         {{
                                             V(1.0),
                                             V(1.0),
                                         }}}});
  }

  // DCT16
  static constexpr const QuantEncodingInternal DCT16X16() {
    return QuantEncodingInternal::DCT(
        DctQuantWeightParams({{{{
                                   V(8996.8725711814115328),
                                   V(-1.3000777393353804),
                                   V(-0.49424529824571225),
                                   V(-0.439093774457103443),
                                   V(-0.6350101832695744),
                                   V(-0.90177264050827612),
                                   V(-1.6162099239887414),
                               }},
                               {{
                                   V(3191.48366296844234752),
                                   V(-0.67424582104194355),
                                   V(-0.80745813428471001),
                                   V(-0.44925837484843441),
                                   V(-0.35865440981033403),
                                   V(-0.31322389111877305),
                                   V(-0.37615025315725483),
                               }},
                               {{
                                   V(1157.50408145487200256),
                                   V(-2.0531423165804414),
                                   V(-1.4),
                                   V(-0.50687130033378396),
                                   V(-0.42708730624733904),
                                   V(-1.4856834539296244),
                                   V(-4.9209142884401604),
                               }}}},
                             7));
  }

  // DCT32
  static constexpr const QuantEncodingInternal DCT32X32() {
    return QuantEncodingInternal::DCT(
        DctQuantWeightParams({{{{
                                   V(15718.40830982518931456),
                                   V(-1.025),
                                   V(-0.98),
                                   V(-0.9012),
                                   V(-0.4),
                                   V(-0.48819395464),
                                   V(-0.421064),
                                   V(-0.27),
                               }},
                               {{
                                   V(7305.7636810695983104),
                                   V(-0.8041958212306401),
                                   V(-0.7633036457487539),
                                   V(-0.55660379990111464),
                                   V(-0.49785304658857626),
                                   V(-0.43699592683512467),
                                   V(-0.40180866526242109),
                                   V(-0.27321683125358037),
                               }},
                               {{
                                   V(3803.53173721215041536),
                                   V(-3.060733579805728),
                                   V(-2.0413270132490346),
                                   V(-2.0235650159727417),
                                   V(-0.5495389509954993),
                                   V(-0.4),
                                   V(-0.4),
                                   V(-0.3),
                               }}}},
                             8));
  }

  // DCT16X8
  static constexpr const QuantEncodingInternal DCT8X16() {
    return QuantEncodingInternal::DCT(
        DctQuantWeightParams({{{{
                                   V(7240.7734393502),
                                   V(-0.7),
                                   V(-0.7),
                                   V(-0.2),
                                   V(-0.2),
                                   V(-0.2),
                                   V(-0.5),
                               }},
                               {{
                                   V(1448.15468787004),
                                   V(-0.5),
                                   V(-0.5),
                                   V(-0.5),
                                   V(-0.2),
                                   V(-0.2),
                                   V(-0.2),
                               }},
                               {{
                                   V(506.854140754517),
                                   V(-1.4),
                                   V(-0.2),
                                   V(-0.5),
                                   V(-0.5),
                                   V(-1.5),
                                   V(-3.6),
                               }}}},
                             7));
  }

  // DCT32X8
  static constexpr const QuantEncodingInternal DCT8X32() {
    return QuantEncodingInternal::DCT(
        DctQuantWeightParams({{{{
                                   V(16283.2494710648897),
                                   V(-1.7812845336559429),
                                   V(-1.6309059012653515),
                                   V(-1.0382179034313539),
                                   V(-0.85),
                                   V(-0.7),
                                   V(-0.9),
                                   V(-1.2360638576849587),
                               }},
                               {{
                                   V(5089.15750884921511936),
                                   V(-0.320049391452786891),
                                   V(-0.35362849922161446),
                                   V(-0.30340000000000003),
                                   V(-0.61),
                                   V(-0.5),
                                   V(-0.5),
                                   V(-0.6),
                               }},
                               {{
                                   V(3397.77603275308720128),
                                   V(-0.321327362693153371),
                                   V(-0.34507619223117997),
                                   V(-0.70340000000000003),
                                   V(-0.9),
                                   V(-1.0),
                                   V(-1.0),
                                   V(-1.1754605576265209),
                               }}}},
                             8));
  }

  // DCT32X16
  static constexpr const QuantEncodingInternal DCT16X32() {
    return QuantEncodingInternal::DCT(
        DctQuantWeightParams({{{{
                                   V(13844.97076442300573),
                                   V(-0.97113799999999995),
                                   V(-0.658),
                                   V(-0.42026),
                                   V(-0.22712),
                                   V(-0.2206),
                                   V(-0.226),
                                   V(-0.6),
                               }},
                               {{
                                   V(4798.964084220744293),
                                   V(-0.61125308982767057),
                                   V(-0.83770786552491361),
                                   V(-0.79014862079498627),
                                   V(-0.2692727459704829),
                                   V(-0.38272769465388551),
                                   V(-0.22924222653091453),
                                   V(-0.20719098826199578),
                               }},
                               {{
                                   V(1807.236946760964614),
                                   V(-1.2),
                                   V(-1.2),
                                   V(-0.7),
                                   V(-0.7),
                                   V(-0.7),
                                   V(-0.4),
                                   V(-0.5),
                               }}}},
                             8));
  }

  // DCT4X8 and 8x4
  static constexpr const QuantEncodingInternal DCT4X8() {
    return QuantEncodingInternal::DCT4X8(
        DctQuantWeightParams({{
                                 {{
                                     V(2198.050556016380522),
                                     V(-0.96269623020744692),
                                     V(-0.76194253026666783),
                                     V(-0.6551140670773547),
                                 }},
                                 {{
                                     V(764.3655248643528689),
                                     V(-0.92630200888366945),
                                     V(-0.9675229603596517),
                                     V(-0.27845290869168118),
                                 }},
                                 {{
                                     V(527.107573587542228),
                                     V(-1.4594385811273854),
                                     V(-1.450082094097871593),
                                     V(-1.5843722511996204),
                                 }},
                             }},
                             4),
        /* kMuls */
        {{
            V(1.0),
            V(1.0),
            V(1.0),
        }});
  }
  // AFV
  static const QuantEncodingInternal AFV0() {
    return QuantEncodingInternal::AFV(DCT4X8().dct_params, DCT4X4().dct_params,
                                      {{{{
                                            // 4x4/4x8 DC tendency.
                                            V(3072.0),
                                            V(3072.0),
                                            // AFV corner.
                                            V(256.0),
                                            V(256.0),
                                            V(256.0),
                                            // AFV high freqs.
                                            V(414.0),
                                            V(0.0),
                                            V(0.0),
                                            V(0.0),
                                        }},
                                        {{
                                            // 4x4/4x8 DC tendency.
                                            V(1024.0),
                                            V(1024.0),
                                            // AFV corner.
                                            V(50),
                                            V(50),
                                            V(50),
                                            // AFV high freqs.
                                            V(58.0),
                                            V(0.0),
                                            V(0.0),
                                            V(0.0),
                                        }},
                                        {{
                                            // 4x4/4x8 DC tendency.
                                            V(384.0),
                                            V(384.0),
                                            // AFV corner.
                                            V(12.0),
                                            V(12.0),
                                            V(12.0),
                                            // AFV high freqs.
                                            V(22.0),
                                            V(-0.25),
                                            V(-0.25),
                                            V(-0.25),
                                        }}}});
  }

  // DCT64
  static const QuantEncodingInternal DCT64X64() {
    return QuantEncodingInternal::DCT(
        DctQuantWeightParams({{{{
                                   V(0.9 * 26629.073922049845),
                                   V(-1.025),
                                   V(-0.78),
                                   V(-0.65012),
                                   V(-0.19041574084286472),
                                   V(-0.20819395464),
                                   V(-0.421064),
                                   V(-0.32733845535848671),
                               }},
                               {{
                                   V(0.9 * 9311.3238710010046),
                                   V(-0.3041958212306401),
                                   V(-0.3633036457487539),
                                   V(-0.35660379990111464),
                                   V(-0.3443074455424403),
                                   V(-0.33699592683512467),
                                   V(-0.30180866526242109),
                                   V(-0.27321683125358037),
                               }},
                               {{
                                   V(0.9 * 4992.2486445538634),
                                   V(-1.2),
                                   V(-1.2),
                                   V(-0.8),
                                   V(-0.7),
                                   V(-0.7),
                                   V(-0.4),
                                   V(-0.5),
                               }}}},
                             8));
  }

  // DCT64X32
  static const QuantEncodingInternal DCT32X64() {
    return QuantEncodingInternal::DCT(
        DctQuantWeightParams({{{{
                                   V(0.65 * 23629.073922049845),
                                   V(-1.025),
                                   V(-0.78),
                                   V(-0.65012),
                                   V(-0.19041574084286472),
                                   V(-0.20819395464),
                                   V(-0.421064),
                                   V(-0.32733845535848671),
                               }},
                               {{
                                   V(0.65 * 8611.3238710010046),
                                   V(-0.3041958212306401),
                                   V(-0.3633036457487539),
                                   V(-0.35660379990111464),
                                   V(-0.3443074455424403),
                                   V(-0.33699592683512467),
                                   V(-0.30180866526242109),
                                   V(-0.27321683125358037),
                               }},
                               {{
                                   V(0.65 * 4492.2486445538634),
                                   V(-1.2),
                                   V(-1.2),
                                   V(-0.8),
                                   V(-0.7),
                                   V(-0.7),
                                   V(-0.4),
                                   V(-0.5),
                               }}}},
                             8));
  }
  // DCT128X128
  static const QuantEncodingInternal DCT128X128() {
    return QuantEncodingInternal::DCT(
        DctQuantWeightParams({{{{
                                   V(1.8 * 26629.073922049845),
                                   V(-1.025),
                                   V(-0.78),
                                   V(-0.65012),
                                   V(-0.19041574084286472),
                                   V(-0.20819395464),
                                   V(-0.421064),
                                   V(-0.32733845535848671),
                               }},
                               {{
                                   V(1.8 * 9311.3238710010046),
                                   V(-0.3041958212306401),
                                   V(-0.3633036457487539),
                                   V(-0.35660379990111464),
                                   V(-0.3443074455424403),
                                   V(-0.33699592683512467),
                                   V(-0.30180866526242109),
                                   V(-0.27321683125358037),
                               }},
                               {{
                                   V(1.8 * 4992.2486445538634),
                                   V(-1.2),
                                   V(-1.2),
                                   V(-0.8),
                                   V(-0.7),
                                   V(-0.7),
                                   V(-0.4),
                                   V(-0.5),
                               }}}},
                             8));
  }

  // DCT128X64
  static const QuantEncodingInternal DCT64X128() {
    return QuantEncodingInternal::DCT(
        DctQuantWeightParams({{{{
                                   V(1.3 * 23629.073922049845),
                                   V(-1.025),
                                   V(-0.78),
                                   V(-0.65012),
                                   V(-0.19041574084286472),
                                   V(-0.20819395464),
                                   V(-0.421064),
                                   V(-0.32733845535848671),
                               }},
                               {{
                                   V(1.3 * 8611.3238710010046),
                                   V(-0.3041958212306401),
                                   V(-0.3633036457487539),
                                   V(-0.35660379990111464),
                                   V(-0.3443074455424403),
                                   V(-0.33699592683512467),
                                   V(-0.30180866526242109),
                                   V(-0.27321683125358037),
                               }},
                               {{
                                   V(1.3 * 4492.2486445538634),
                                   V(-1.2),
                                   V(-1.2),
                                   V(-0.8),
                                   V(-0.7),
                                   V(-0.7),
                                   V(-0.4),
                                   V(-0.5),
                               }}}},
                             8));
  }
  // DCT256X256
  static const QuantEncodingInternal DCT256X256() {
    return QuantEncodingInternal::DCT(
        DctQuantWeightParams({{{{
                                   V(3.6 * 26629.073922049845),
                                   V(-1.025),
                                   V(-0.78),
                                   V(-0.65012),
                                   V(-0.19041574084286472),
                                   V(-0.20819395464),
                                   V(-0.421064),
                                   V(-0.32733845535848671),
                               }},
                               {{
                                   V(3.6 * 9311.3238710010046),
                                   V(-0.3041958212306401),
                                   V(-0.3633036457487539),
                                   V(-0.35660379990111464),
                                   V(-0.3443074455424403),
                                   V(-0.33699592683512467),
                                   V(-0.30180866526242109),
                                   V(-0.27321683125358037),
                               }},
                               {{
                                   V(3.6 * 4992.2486445538634),
                                   V(-1.2),
                                   V(-1.2),
                                   V(-0.8),
                                   V(-0.7),
                                   V(-0.7),
                                   V(-0.4),
                                   V(-0.5),
                               }}}},
                             8));
  }

  // DCT256X128
  static const QuantEncodingInternal DCT128X256() {
    return QuantEncodingInternal::DCT(
        DctQuantWeightParams({{{{
                                   V(2.6 * 23629.073922049845),
                                   V(-1.025),
                                   V(-0.78),
                                   V(-0.65012),
                                   V(-0.19041574084286472),
                                   V(-0.20819395464),
                                   V(-0.421064),
                                   V(-0.32733845535848671),
                               }},
                               {{
                                   V(2.6 * 8611.3238710010046),
                                   V(-0.3041958212306401),
                                   V(-0.3633036457487539),
                                   V(-0.35660379990111464),
                                   V(-0.3443074455424403),
                                   V(-0.33699592683512467),
                                   V(-0.30180866526242109),
                                   V(-0.27321683125358037),
                               }},
                               {{
                                   V(2.6 * 4492.2486445538634),
                                   V(-1.2),
                                   V(-1.2),
                                   V(-0.8),
                                   V(-0.7),
                                   V(-0.7),
                                   V(-0.4),
                                   V(-0.5),
                               }}}},
                             8));
  }
};
}  // namespace

const DequantMatrices::DequantLibraryInternal DequantMatrices::LibraryInit() {
  static_assert(kNum == 17,
                "Update this function when adding new quantization kinds.");
  static_assert(kNumPredefinedTables == 1,
                "Update this function when adding new quantization matrices to "
                "the library.");

  // The library and the indices need to be kept in sync manually.
  static_assert(0 == DCT, "Update the DequantLibrary array below.");
  static_assert(1 == IDENTITY, "Update the DequantLibrary array below.");
  static_assert(2 == DCT2X2, "Update the DequantLibrary array below.");
  static_assert(3 == DCT4X4, "Update the DequantLibrary array below.");
  static_assert(4 == DCT16X16, "Update the DequantLibrary array below.");
  static_assert(5 == DCT32X32, "Update the DequantLibrary array below.");
  static_assert(6 == DCT8X16, "Update the DequantLibrary array below.");
  static_assert(7 == DCT8X32, "Update the DequantLibrary array below.");
  static_assert(8 == DCT16X32, "Update the DequantLibrary array below.");
  static_assert(9 == DCT4X8, "Update the DequantLibrary array below.");
  static_assert(10 == AFV0, "Update the DequantLibrary array below.");
  static_assert(11 == DCT64X64, "Update the DequantLibrary array below.");
  static_assert(12 == DCT32X64, "Update the DequantLibrary array below.");
  static_assert(13 == DCT128X128, "Update the DequantLibrary array below.");
  static_assert(14 == DCT64X128, "Update the DequantLibrary array below.");
  static_assert(15 == DCT256X256, "Update the DequantLibrary array below.");
  static_assert(16 == DCT128X256, "Update the DequantLibrary array below.");
  return DequantMatrices::DequantLibraryInternal{{
      DequantMatricesLibraryDef::DCT(),
      DequantMatricesLibraryDef::IDENTITY(),
      DequantMatricesLibraryDef::DCT2X2(),
      DequantMatricesLibraryDef::DCT4X4(),
      DequantMatricesLibraryDef::DCT16X16(),
      DequantMatricesLibraryDef::DCT32X32(),
      DequantMatricesLibraryDef::DCT8X16(),
      DequantMatricesLibraryDef::DCT8X32(),
      DequantMatricesLibraryDef::DCT16X32(),
      DequantMatricesLibraryDef::DCT4X8(),
      DequantMatricesLibraryDef::AFV0(),
      DequantMatricesLibraryDef::DCT64X64(),
      DequantMatricesLibraryDef::DCT32X64(),
      // Same default for large transforms (128+) as for 64x* transforms.
      DequantMatricesLibraryDef::DCT128X128(),
      DequantMatricesLibraryDef::DCT64X128(),
      DequantMatricesLibraryDef::DCT256X256(),
      DequantMatricesLibraryDef::DCT128X256(),
  }};
}

const QuantEncoding* DequantMatrices::Library() {
  static const DequantMatrices::DequantLibraryInternal kDequantLibrary =
      DequantMatrices::LibraryInit();
  // Downcast the result to a const QuantEncoding* from QuantEncodingInternal*
  // since the subclass (QuantEncoding) doesn't add any new members and users
  // will need to upcast to QuantEncodingInternal to access the members of that
  // class. This allows to have kDequantLibrary as a constexpr value while still
  // allowing to create QuantEncoding::RAW() instances that use std::vector in
  // C++11.
  return reinterpret_cast<const QuantEncoding*>(kDequantLibrary.data());
}

Status DequantMatrices::Compute() {
  size_t pos = 0;

  struct DefaultMatrices {
    DefaultMatrices() {
      const QuantEncoding* library = Library();
      size_t pos = 0;
      for (size_t i = 0; i < kNum; i++) {
        JXL_CHECK(ComputeQuantTable(library[i], table, inv_table, i,
                                    QuantTable(i), &pos));
      }
      JXL_CHECK(pos == kTotalTableSize);
    }
    HWY_ALIGN_MAX float table[kTotalTableSize];
    HWY_ALIGN_MAX float inv_table[kTotalTableSize];
  };

  static const DefaultMatrices& default_matrices =
      *hwy::MakeUniqueAligned<DefaultMatrices>().release();

  JXL_ASSERT(encodings_.size() == kNum);

  bool has_nondefault_matrix = false;
  for (const auto& enc : encodings_) {
    if (enc.mode != QuantEncoding::kQuantModeLibrary) {
      has_nondefault_matrix = true;
    }
  }
  if (has_nondefault_matrix) {
    table_storage_ = hwy::AllocateAligned<float>(2 * kTotalTableSize);
    table_ = table_storage_.get();
    inv_table_ = table_storage_.get() + kTotalTableSize;
    for (size_t table = 0; table < kNum; table++) {
      size_t prev_pos = pos;
      if (encodings_[table].mode == QuantEncoding::kQuantModeLibrary) {
        size_t num = required_size_[table] * kDCTBlockSize;
        memcpy(table_storage_.get() + prev_pos,
               default_matrices.table + prev_pos, num * sizeof(float) * 3);
        memcpy(table_storage_.get() + kTotalTableSize + prev_pos,
               default_matrices.inv_table + prev_pos, num * sizeof(float) * 3);
        pos += num * 3;
      } else {
        JXL_RETURN_IF_ERROR(
            ComputeQuantTable(encodings_[table], table_storage_.get(),
                              table_storage_.get() + kTotalTableSize, table,
                              QuantTable(table), &pos));
      }
    }
    JXL_ASSERT(pos == kTotalTableSize);
  } else {
    table_ = default_matrices.table;
    inv_table_ = default_matrices.inv_table;
  }

  return true;
}

}  // namespace jxl
