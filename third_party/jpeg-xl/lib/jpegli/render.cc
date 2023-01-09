// Copyright (c) the JPEG XL Project Authors. All rights reserved.
//
// Use of this source code is governed by a BSD-style
// license that can be found in the LICENSE file.

#include "lib/jpegli/render.h"

#include <string.h>

#include <cmath>

#include "hwy/aligned_allocator.h"
#include "lib/jpegli/color_transform.h"
#include "lib/jpegli/decode_internal.h"
#include "lib/jpegli/idct.h"
#include "lib/jpegli/upsample.h"
#include "lib/jxl/base/compiler_specific.h"
#include "lib/jxl/base/status.h"

#ifdef MEMORY_SANITIZER
#define JXL_MEMORY_SANITIZER 1
#elif defined(__has_feature)
#if __has_feature(memory_sanitizer)
#define JXL_MEMORY_SANITIZER 1
#else
#define JXL_MEMORY_SANITIZER 0
#endif
#else
#define JXL_MEMORY_SANITIZER 0
#endif

#if JXL_MEMORY_SANITIZER
#include "sanitizer/msan_interface.h"
#endif

#undef HWY_TARGET_INCLUDE
#define HWY_TARGET_INCLUDE "lib/jpegli/render.cc"
#include <hwy/foreach_target.h>
#include <hwy/highway.h>

HWY_BEFORE_NAMESPACE();
namespace jpegli {
namespace HWY_NAMESPACE {

// These templates are not found via ADL.
using hwy::HWY_NAMESPACE::Abs;
using hwy::HWY_NAMESPACE::Add;
using hwy::HWY_NAMESPACE::Clamp;
using hwy::HWY_NAMESPACE::Gt;
using hwy::HWY_NAMESPACE::IfThenElseZero;
using hwy::HWY_NAMESPACE::Mul;
using hwy::HWY_NAMESPACE::NearestInt;
using hwy::HWY_NAMESPACE::Rebind;
using hwy::HWY_NAMESPACE::Vec;

using D = HWY_FULL(float);
using DI = HWY_FULL(int32_t);
constexpr D d;
constexpr DI di;

void GatherBlockStats(const int16_t* JXL_RESTRICT coeffs,
                      const size_t coeffs_size, int32_t* JXL_RESTRICT nonzeros,
                      int32_t* JXL_RESTRICT sumabs) {
  for (size_t i = 0; i < coeffs_size; i += Lanes(d)) {
    size_t k = i % DCTSIZE2;
    const Rebind<int16_t, DI> di16;
    const Vec<DI> coeff = PromoteTo(di, Load(di16, coeffs + i));
    const auto abs_coeff = Abs(coeff);
    const auto not_0 = Gt(abs_coeff, Zero(di));
    const auto nzero = IfThenElseZero(not_0, Set(di, 1));
    Store(Add(nzero, Load(di, nonzeros + k)), di, nonzeros + k);
    Store(Add(abs_coeff, Load(di, sumabs + k)), di, sumabs + k);
  }
}

void DecenterRow(float* row, size_t xsize) {
  const HWY_CAPPED(float, 8) df;
  const auto c128 = Set(df, 128.0f / 255);
  for (size_t x = 0; x < xsize; x += Lanes(df)) {
    Store(Add(Load(df, row + x), c128), df, row + x);
  }
}

template <typename T>
void StoreUnsignedRow(float* JXL_RESTRICT input[3], size_t x0, size_t len,
                      size_t num_channels, float multiplier, T* output) {
  const HWY_CAPPED(float, 8) d;
  auto zero = Zero(d);
  auto one = Set(d, 1.0f);
  auto mul = Set(d, multiplier);
  const Rebind<T, decltype(d)> du;
#if JXL_MEMORY_SANITIZER
  const size_t padding = hwy::RoundUpTo(len, Lanes(d)) - len;
  for (size_t c = 0; c < num_channels; ++c) {
    __msan_unpoison(input[c] + x0 + len, sizeof(input[c][0]) * padding);
  }
#endif
  if (num_channels == 1) {
    for (size_t i = 0; i < len; i += Lanes(d)) {
      auto v0 = Mul(Clamp(zero, Load(d, &input[0][x0 + i]), one), mul);
      Store(DemoteTo(du, NearestInt(v0)), du, &output[i]);
    }
  } else if (num_channels == 3) {
    for (size_t i = 0; i < len; i += Lanes(d)) {
      auto v0 = Mul(Clamp(zero, Load(d, &input[0][x0 + i]), one), mul);
      auto v1 = Mul(Clamp(zero, Load(d, &input[1][x0 + i]), one), mul);
      auto v2 = Mul(Clamp(zero, Load(d, &input[2][x0 + i]), one), mul);
      StoreInterleaved3(DemoteTo(du, NearestInt(v0)),
                        DemoteTo(du, NearestInt(v1)),
                        DemoteTo(du, NearestInt(v2)), du, &output[3 * i]);
    }
  }
#if JXL_MEMORY_SANITIZER
  __msan_poison(output + num_channels * len,
                sizeof(output[0]) * num_channels * padding);
#endif
}

void WriteToOutput(float* JXL_RESTRICT rows[3], size_t xoffset, size_t x0,
                   size_t len, size_t num_channels, size_t bit_depth,
                   uint8_t* JXL_RESTRICT scratch_space,
                   uint8_t* JXL_RESTRICT output) {
  const float mul = (1u << bit_depth) - 1;
  if (bit_depth <= 8) {
    size_t offset = x0 * num_channels;
    StoreUnsignedRow(rows, xoffset + x0, len, num_channels, mul, scratch_space);
    memcpy(output + offset, scratch_space, len * num_channels);
  } else {
    size_t offset = x0 * num_channels * 2;
    uint16_t* tmp = reinterpret_cast<uint16_t*>(scratch_space);
    StoreUnsignedRow(rows, xoffset + x0, len, num_channels, mul, tmp);
    // TODO(szabadka) Handle endianness.
    memcpy(output + offset, tmp, len * num_channels * 2);
  }
}

// NOLINTNEXTLINE(google-readability-namespace-comments)
}  // namespace HWY_NAMESPACE
}  // namespace jpegli
HWY_AFTER_NAMESPACE();

#if HWY_ONCE

namespace jpegli {

HWY_EXPORT(GatherBlockStats);
HWY_EXPORT(WriteToOutput);
HWY_EXPORT(DecenterRow);

void GatherBlockStats(const int16_t* JXL_RESTRICT coeffs,
                      const size_t coeffs_size, int32_t* JXL_RESTRICT nonzeros,
                      int32_t* JXL_RESTRICT sumabs) {
  return HWY_DYNAMIC_DISPATCH(GatherBlockStats)(coeffs, coeffs_size, nonzeros,
                                                sumabs);
}

void WriteToOutput(float* JXL_RESTRICT rows[3], size_t xoffset, size_t x0,
                   size_t len, size_t num_channels, size_t bit_depth,
                   uint8_t* JXL_RESTRICT scratch_space,
                   uint8_t* JXL_RESTRICT output) {
  return HWY_DYNAMIC_DISPATCH(WriteToOutput)(
      rows, xoffset, x0, len, num_channels, bit_depth, scratch_space, output);
}

void DecenterRow(float* row, size_t xsize) {
  return HWY_DYNAMIC_DISPATCH(DecenterRow)(row, xsize);
}

// Padding for horizontal chroma upsampling.
constexpr size_t kPaddingLeft = 64;
constexpr size_t kPaddingRight = 64;
constexpr size_t kTempOutputLen = 1024;

bool ShouldApplyDequantBiases(j_decompress_ptr cinfo, int ci) {
  const auto& compinfo = cinfo->comp_info[ci];
  return (compinfo.h_samp_factor == cinfo->max_h_samp_factor &&
          compinfo.v_samp_factor == cinfo->max_v_samp_factor);
}

// See the following article for the details:
// J. R. Price and M. Rabbani, "Dequantization bias for JPEG decompression"
// Proceedings International Conference on Information Technology: Coding and
// Computing (Cat. No.PR00540), 2000, pp. 30-35, doi: 10.1109/ITCC.2000.844179.
void ComputeOptimalLaplacianBiases(const int num_blocks, const int* nonzeros,
                                   const int* sumabs, float* biases) {
  for (size_t k = 1; k < DCTSIZE2; ++k) {
    // Notation adapted from the article
    float N = num_blocks;
    float N1 = nonzeros[k];
    float N0 = num_blocks - N1;
    float S = sumabs[k];
    // Compute gamma from N0, N1, N, S (eq. 11), with A and B being just
    // temporary grouping of terms.
    float A = 4.0 * S + 2.0 * N;
    float B = 4.0 * S - 2.0 * N1;
    float gamma = (-1.0 * N0 + std::sqrt(N0 * N0 * 1.0 + A * B)) / A;
    float gamma2 = gamma * gamma;
    // The bias is computed from gamma with (eq. 5), where the quantization
    // multiplier Q can be factored out and thus the bias can be applied
    // directly on the quantized coefficient.
    biases[k] =
        0.5 * (((1.0 + gamma2) / (1.0 - gamma2)) + 1.0 / std::log(gamma));
  }
}

void PrepareForOutput(j_decompress_ptr cinfo) {
  jpeg_decomp_master* m = cinfo->master;
  for (int c = 0; c < cinfo->num_components; ++c) {
    const auto& comp = cinfo->comp_info[c];
    const size_t stride = m->iMCU_cols_ * cinfo->max_h_samp_factor * DCTSIZE;
    m->raw_height_[c] = cinfo->total_iMCU_rows * comp.v_samp_factor * DCTSIZE;
    m->raw_output_[c].Allocate(3 * comp.v_samp_factor * DCTSIZE, stride);
    m->render_output_[c].Allocate(cinfo->max_v_samp_factor, stride);
  }
  m->idct_scratch_ = hwy::AllocateAligned<float>(DCTSIZE2 * 2);
  size_t MCU_row_stride = m->iMCU_cols_ * cinfo->max_h_samp_factor * DCTSIZE;
  m->upsample_scratch_ = hwy::AllocateAligned<float>(
      MCU_row_stride + kPaddingLeft + kPaddingRight);
  size_t bytes_per_channel = DivCeil(m->output_bit_depth_, 8);
  size_t bytes_per_sample = cinfo->out_color_components * bytes_per_channel;
  m->output_scratch_ =
      hwy::AllocateAligned<uint8_t>(bytes_per_sample * kTempOutputLen);
  size_t coeffs_per_block = cinfo->num_components * DCTSIZE2;
  m->nonzeros_ = hwy::AllocateAligned<int>(coeffs_per_block);
  m->sumabs_ = hwy::AllocateAligned<int>(coeffs_per_block);
  memset(m->nonzeros_.get(), 0, coeffs_per_block * sizeof(m->nonzeros_[0]));
  memset(m->sumabs_.get(), 0, coeffs_per_block * sizeof(m->sumabs_[0]));
  m->num_processed_blocks_.resize(cinfo->num_components);
  for (int c = 0; c < cinfo->num_components; ++c) {
    m->num_processed_blocks_[c] = 0;
  }
  m->biases_ = hwy::AllocateAligned<float>(coeffs_per_block);
  memset(m->biases_.get(), 0, coeffs_per_block * sizeof(m->biases_[0]));
  cinfo->output_iMCU_row = 0;
  cinfo->output_scanline = 0;
  const float kDequantScale = 1.0f / (8 * 255);
  m->dequant_ = hwy::AllocateAligned<float>(coeffs_per_block);
  for (int c = 0; c < cinfo->num_components; c++) {
    const auto& comp = cinfo->comp_info[c];
    JQUANT_TBL* table = comp.quant_table;
    for (size_t k = 0; k < DCTSIZE2; ++k) {
      m->dequant_[c * DCTSIZE2 + k] = table->quantval[k] * kDequantScale;
    }
  }
}

void DecodeCurrentiMCURow(j_decompress_ptr cinfo) {
  jpeg_decomp_master* m = cinfo->master;
  const size_t imcu_row = cinfo->output_iMCU_row;
  for (int c = 0; c < cinfo->num_components; ++c) {
    size_t k0 = c * DCTSIZE2;
    auto& comp = m->components_[c];
    auto& compinfo = cinfo->comp_info[c];
    size_t block_row = imcu_row * compinfo.v_samp_factor;
    if (ShouldApplyDequantBiases(cinfo, c)) {
      // Update statistics for this iMCU row.
      for (int iy = 0; iy < compinfo.v_samp_factor; ++iy) {
        size_t by = block_row + iy;
        size_t bix = by * compinfo.width_in_blocks;
        int16_t* JXL_RESTRICT coeffs = &comp.coeffs[bix * DCTSIZE2];
        size_t num = compinfo.width_in_blocks * DCTSIZE2;
        GatherBlockStats(coeffs, num, &m->nonzeros_[k0], &m->sumabs_[k0]);
        m->num_processed_blocks_[c] += compinfo.width_in_blocks;
      }
      if (imcu_row % 4 == 3) {
        // Re-compute optimal biases every few iMCU-rows.
        ComputeOptimalLaplacianBiases(m->num_processed_blocks_[c],
                                      &m->nonzeros_[k0], &m->sumabs_[k0],
                                      &m->biases_[k0]);
      }
    }
    RowBuffer* raw_out = &m->raw_output_[c];
    for (int iy = 0; iy < compinfo.v_samp_factor; ++iy) {
      size_t by = block_row + iy;
      size_t bix = by * compinfo.width_in_blocks;
      int16_t* JXL_RESTRICT row_in = &comp.coeffs[bix * DCTSIZE2];
      float* JXL_RESTRICT row_out = raw_out->Row(by * DCTSIZE);
      for (size_t bx = 0; bx < compinfo.width_in_blocks; ++bx) {
        InverseTransformBlock(&row_in[bx * DCTSIZE2], &m->dequant_[k0],
                              &m->biases_[k0], m->idct_scratch_.get(),
                              &row_out[bx * DCTSIZE], raw_out->stride());
      }
    }
  }
}

void ProcessRawOutput(j_decompress_ptr cinfo, JSAMPIMAGE data) {
  jpegli::DecodeCurrentiMCURow(cinfo);
  jpeg_decomp_master* m = cinfo->master;
  for (int c = 0; c < cinfo->num_components; ++c) {
    const auto& compinfo = cinfo->comp_info[c];
    size_t comp_width = compinfo.width_in_blocks * DCTSIZE;
    size_t comp_height = compinfo.height_in_blocks * DCTSIZE;
    size_t comp_nrows = compinfo.v_samp_factor * DCTSIZE;
    size_t y0 = cinfo->output_iMCU_row * compinfo.v_samp_factor * DCTSIZE;
    size_t y1 = std::min(y0 + comp_nrows, comp_height);
    for (size_t y = y0; y < y1; ++y) {
      for (size_t x0 = 0; x0 < comp_width; x0 += kTempOutputLen) {
        float* rows[3] = {m->raw_output_[c].Row(y)};
        size_t len = std::min(comp_width - x0, kTempOutputLen);
        uint8_t* output = data[c][y - y0];
        WriteToOutput(rows, 0, x0, len, 1, m->output_bit_depth_,
                      m->output_scratch_.get(), output);
      }
    }
  }
  ++cinfo->output_iMCU_row;
  cinfo->output_scanline += cinfo->max_v_samp_factor * DCTSIZE;
}

void ProcessOutput(j_decompress_ptr cinfo, size_t* num_output_rows,
                   JSAMPARRAY scanlines, size_t max_output_rows) {
  jpeg_decomp_master* m = cinfo->master;
  size_t xsize_blocks = DivCeil(cinfo->image_width, DCTSIZE);
  const int vfactor = cinfo->max_v_samp_factor;
  const size_t imcu_row = cinfo->output_iMCU_row;
  const size_t imcu_height = vfactor * DCTSIZE;
  if (imcu_row == cinfo->total_iMCU_rows ||
      (imcu_row > 1 && cinfo->output_scanline < (imcu_row - 1) * imcu_height)) {
    // We are ready to output some scanlines.
    size_t ybegin = cinfo->output_scanline;
    size_t yend =
        (imcu_row == cinfo->total_iMCU_rows ? cinfo->output_height
                                            : (imcu_row - 1) * imcu_height);
    yend = std::min<size_t>(yend, ybegin + max_output_rows - *num_output_rows);
    size_t yb = (ybegin / vfactor) * vfactor;
    size_t ye = DivCeil(yend, vfactor) * vfactor;
    for (size_t y = yb; y < ye; y += vfactor) {
      for (int c = 0; c < cinfo->num_components; ++c) {
        RowBuffer* raw_out = &m->raw_output_[c];
        RowBuffer* render_out = &m->render_output_[c];
        const auto& compinfo = cinfo->comp_info[c];
        size_t yc = (y / vfactor) * compinfo.v_samp_factor;
        if (compinfo.v_samp_factor < vfactor) {
          const float* JXL_RESTRICT row_mid = raw_out->Row(yc);
          const float* JXL_RESTRICT row_top =
              yc == 0 ? row_mid : raw_out->Row(yc - 1);
          const float* JXL_RESTRICT row_bot =
              yc + 1 == m->raw_height_[c] ? row_mid : raw_out->Row(yc + 1);
          Upsample2Vertical(row_top, row_mid, row_bot, render_out->Row(0),
                            render_out->Row(1), xsize_blocks * DCTSIZE);
        } else {
          JXL_ASSERT(y == static_cast<size_t>(yc));
          for (int yix = 0; yix < vfactor; ++yix) {
            memcpy(render_out->Row(yix), raw_out->Row(yc + yix),
                   raw_out->memstride());
          }
        }
      }
      for (int yix = 0; yix < vfactor; ++yix) {
        if (y + yix < ybegin || y + yix >= yend) continue;
        // TODO(szabadka) Support 4 components JPEGs.
        float* rows[3];
        for (int c = 0; c < cinfo->out_color_components; ++c) {
          rows[c] = m->render_output_[c].Row(yix);
        }
        if (cinfo->jpeg_color_space == JCS_YCbCr) {
          YCbCrToRGB(rows[0], rows[1], rows[2], xsize_blocks * DCTSIZE);
        } else {
          for (int c = 0; c < cinfo->out_color_components; ++c) {
            // Libjpeg encoder converts all unsigned input values to signed
            // ones, i.e. for 8 bit input from [0..255] to [-128..127]. For
            // YCbCr jpegs this is undone in the YCbCr -> RGB conversion above
            // by adding 128 to Y channel, but for grayscale and RGB jpegs we
            // need to undo it here channel by channel.
            DecenterRow(rows[c], xsize_blocks * DCTSIZE);
          }
        }
        for (size_t x0 = 0; x0 < cinfo->output_width; x0 += kTempOutputLen) {
          size_t len = std::min(cinfo->output_width - x0, kTempOutputLen);
          if (scanlines) {
            uint8_t* output = scanlines[*num_output_rows];
            WriteToOutput(rows, m->xoffset_, x0, len,
                          cinfo->out_color_components, m->output_bit_depth_,
                          m->output_scratch_.get(), output);
          }
        }
        JXL_ASSERT(cinfo->output_scanline == y + yix);
        ++cinfo->output_scanline;
        ++(*num_output_rows);
      }
    }
  } else {
    DecodeCurrentiMCURow(cinfo);
    for (int c = 0; c < cinfo->num_components; ++c) {
      const auto& compinfo = cinfo->comp_info[c];
      if (compinfo.h_samp_factor < cinfo->max_h_samp_factor) {
        RowBuffer* raw_out = &m->raw_output_[c];
        size_t y0 = imcu_row * compinfo.v_samp_factor * DCTSIZE;
        for (int iy = 0; iy < compinfo.v_samp_factor * DCTSIZE; ++iy) {
          float* JXL_RESTRICT row = raw_out->Row(y0 + iy);
          Upsample2Horizontal(row, m->upsample_scratch_.get(),
                              xsize_blocks * DCTSIZE);
        }
      }
    }
    ++cinfo->output_iMCU_row;
  }
}

}  // namespace jpegli
#endif  // HWY_ONCE
