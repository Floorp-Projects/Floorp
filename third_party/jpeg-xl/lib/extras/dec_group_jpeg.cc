// Copyright (c) the JPEG XL Project Authors. All rights reserved.
//
// Use of this source code is governed by a BSD-style
// license that can be found in the LICENSE file.

#include "lib/extras/dec_group_jpeg.h"

#include <stdint.h>
#include <string.h>

#include <algorithm>
#include <memory>
#include <utility>

#undef HWY_TARGET_INCLUDE
#define HWY_TARGET_INCLUDE "lib/extras/dec_group_jpeg.cc"
#include <hwy/foreach_target.h>
#include <hwy/highway.h>

#include "lib/jxl/base/status.h"
#include "lib/jxl/dct_scales.h"
#include "lib/jxl/dec_transforms-inl.h"
#include "lib/jxl/sanitizers.h"
#include "lib/jxl/simd_util-inl.h"

HWY_BEFORE_NAMESPACE();
namespace jxl {
namespace HWY_NAMESPACE {

// These templates are not found via ADL.
using hwy::HWY_NAMESPACE::Abs;
using hwy::HWY_NAMESPACE::Clamp;
using hwy::HWY_NAMESPACE::Gt;
using hwy::HWY_NAMESPACE::IfThenElseZero;
using hwy::HWY_NAMESPACE::NearestInt;
using hwy::HWY_NAMESPACE::Rebind;
using hwy::HWY_NAMESPACE::Vec;
using hwy::HWY_NAMESPACE::Xor;

using D = HWY_FULL(float);
using DI = HWY_FULL(int32_t);
constexpr D d;
constexpr DI di;

void GatherBlockStats(const int16_t* coeffs, const size_t coeffs_size,
                      int32_t* JXL_RESTRICT nonzeros,
                      int32_t* JXL_RESTRICT sumabs) {
  for (size_t i = 0; i < coeffs_size; i += Lanes(d)) {
    size_t k = i % kDCTBlockSize;
    const Rebind<int16_t, DI> di16;
    const Vec<DI> coeff = PromoteTo(di, Load(di16, coeffs + i));
    const auto abs_coeff = Abs(coeff);
    const auto not_0 = Gt(abs_coeff, Zero(di));
    const auto nzero = IfThenElseZero(not_0, Set(di, 1));
    Store(Add(nzero, Load(di, nonzeros + k)), di, nonzeros + k);
    Store(Add(abs_coeff, Load(di, sumabs + k)), di, sumabs + k);
  }
}

void DequantBlock(const int16_t* JXL_RESTRICT qblock,
                  const float* JXL_RESTRICT dequant,
                  const float* JXL_RESTRICT biases, float* JXL_RESTRICT block) {
  for (size_t k = 0; k < kDCTBlockSize; k += Lanes(d)) {
    const auto mul = Load(d, dequant + k);
    const auto bias = Load(d, biases + k);
    const Rebind<int16_t, DI> di16;
    const Vec<DI> quant_i = PromoteTo(di, Load(di16, qblock + k));
    const Rebind<float, DI> df;
    const auto quant = ConvertTo(df, quant_i);
    const auto abs_quant = Abs(quant);
    const auto not_0 = Gt(abs_quant, Zero(df));
    const auto sign_quant = Xor(quant, abs_quant);
    const auto biased_quant = Sub(quant, Xor(bias, sign_quant));
    const auto dequant = IfThenElseZero(not_0, Mul(biased_quant, mul));
    Store(dequant, d, block + k);
  }
}

void DecodeJpegBlock(const int16_t* qblock, const float* JXL_RESTRICT dequant,
                     const float* JXL_RESTRICT biases,
                     float* JXL_RESTRICT scratch_space,
                     float* JXL_RESTRICT output, size_t output_stride) {
  HWY_ALIGN float* const block = scratch_space + kDCTBlockSize;
  DequantBlock(qblock, dequant, biases, scratch_space);
  // JPEG XL transposes the DCT, JPEG doesn't.
  Transpose<8, 8>::Run(DCTFrom(scratch_space, 8), DCTTo(block, 8));
  TransformToPixels(AcStrategy::DCT, block, output, output_stride,
                    scratch_space);
}

void Upsample2Horizontal(float* JXL_RESTRICT row_in,
                         float* JXL_RESTRICT row_out, size_t len_out) {
  HWY_FULL(float) df;
  auto threefour = Set(df, 0.75f);
  auto onefour = Set(df, 0.25f);
  const size_t len_in = len_out >> 1;
  row_in[-1] = row_in[0];
  row_in[len_in] = row_in[len_in - 1];
  for (size_t x = 0; x < len_in; x += Lanes(df)) {
    auto current = Mul(Load(df, row_in + x), threefour);
    auto prev = LoadU(df, row_in + x - 1);
    auto next = LoadU(df, row_in + x + 1);
    auto left = MulAdd(onefour, prev, current);
    auto right = MulAdd(onefour, next, current);
    StoreInterleaved(df, left, right, row_out + x * 2);
  }
}

void Upsample2Vertical(const float* JXL_RESTRICT row_top,
                       const float* JXL_RESTRICT row_mid,
                       const float* JXL_RESTRICT row_bot,
                       float* JXL_RESTRICT row_out0,
                       float* JXL_RESTRICT row_out1, size_t len) {
  HWY_FULL(float) df;
  auto threefour = Set(df, 0.75f);
  auto onefour = Set(df, 0.25f);
  for (size_t x = 0; x < len; x += Lanes(df)) {
    auto it = Load(df, row_top + x);
    auto im = Load(df, row_mid + x);
    auto ib = Load(df, row_bot + x);
    auto im_scaled = Mul(im, threefour);
    Store(MulAdd(it, onefour, im_scaled), df, row_out0 + x);
    Store(MulAdd(ib, onefour, im_scaled), df, row_out1 + x);
  }
}

void YCbCrToRGB(float* JXL_RESTRICT row0, float* JXL_RESTRICT row1,
                float* JXL_RESTRICT row2, size_t xsize) {
  const HWY_FULL(float) df;

  // Full-range BT.601 as defined by JFIF Clause 7:
  // https://www.itu.int/rec/T-REC-T.871-201105-I/en
  const auto c128 = Set(df, 128.0f / 255);
  const auto crcr = Set(df, 1.402f);
  const auto cgcb = Set(df, -0.114f * 1.772f / 0.587f);
  const auto cgcr = Set(df, -0.299f * 1.402f / 0.587f);
  const auto cbcb = Set(df, 1.772f);

  for (size_t x = 0; x < xsize; x += Lanes(df)) {
    const auto y_vec = Add(Load(df, row0 + x), c128);
    const auto cb_vec = Load(df, row1 + x);
    const auto cr_vec = Load(df, row2 + x);
    const auto r_vec = MulAdd(crcr, cr_vec, y_vec);
    const auto g_vec = MulAdd(cgcr, cr_vec, MulAdd(cgcb, cb_vec, y_vec));
    const auto b_vec = MulAdd(cbcb, cb_vec, y_vec);
    Store(r_vec, df, row0 + x);
    Store(g_vec, df, row1 + x);
    Store(b_vec, df, row2 + x);
  }
}

void DecenterRow(float* row, size_t xsize) {
  const HWY_FULL(float) df;
  const auto c128 = Set(df, 128.0f / 255);
  for (size_t x = 0; x < xsize; x += Lanes(df)) {
    Store(Add(Load(df, row + x), c128), df, row + x);
  }
}

template <typename T>
void StoreUnsignedRow(float* JXL_RESTRICT input[3], size_t x0, size_t len,
                      size_t num_channels, T* output) {
  const HWY_FULL(float) d;
  auto zero = Zero(d);
  auto one = Set(d, 1.0f);
  auto mul = Set(d, (1u << (sizeof(T) * 8)) - 1);
  const Rebind<T, decltype(d)> du;
  const size_t padding = RoundUpTo(len, Lanes(d)) - len;
  for (size_t c = 0; c < num_channels; ++c) {
    msan::UnpoisonMemory(input[c] + x0 + len, sizeof(input[c][0]) * padding);
  }
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
  msan::PoisonMemory(output + num_channels * len,
                     sizeof(output[0]) * num_channels * padding);
}

void WriteToPackedImage(float* JXL_RESTRICT rows[3], size_t x0, size_t y0,
                        size_t len, uint8_t* JXL_RESTRICT scratch_space,
                        extras::PackedImage* image) {
  if (y0 >= image->ysize) return;
  JxlPixelFormat format = image->format;
  uint8_t* pixels = reinterpret_cast<uint8_t*>(image->pixels());
  if (format.data_type == JXL_TYPE_UINT8) {
    size_t offset = y0 * image->stride + x0 * format.num_channels;
    JXL_CHECK(offset + len * format.num_channels <= image->pixels_size);
    StoreUnsignedRow(rows, x0, len, format.num_channels, scratch_space);
    memcpy(pixels + offset, scratch_space, len * format.num_channels);
  } else if (format.data_type == JXL_TYPE_UINT16) {
    size_t offset = y0 * image->stride + x0 * format.num_channels * 2;
    JXL_CHECK(offset + len * format.num_channels * 2 <= image->pixels_size);
    uint16_t* tmp = reinterpret_cast<uint16_t*>(scratch_space);
    StoreUnsignedRow(rows, x0, len, format.num_channels, tmp);
    // TODO(szabadka) Handle endianness.
    memcpy(pixels + offset, tmp, len * format.num_channels * 2);
  }
}

// NOLINTNEXTLINE(google-readability-namespace-comments)
}  // namespace HWY_NAMESPACE
}  // namespace jxl
HWY_AFTER_NAMESPACE();

#if HWY_ONCE
namespace jxl {
namespace {
HWY_EXPORT(GatherBlockStats);
HWY_EXPORT(DecodeJpegBlock);
HWY_EXPORT(Upsample2Horizontal);
HWY_EXPORT(Upsample2Vertical);
HWY_EXPORT(YCbCrToRGB);
HWY_EXPORT(DecenterRow);
HWY_EXPORT(WriteToPackedImage);
}  // namespace

namespace extras {

void GatherBlockStats(const int16_t* coeffs, const size_t coeffs_size,
                      int32_t* JXL_RESTRICT nonzeros,
                      int32_t* JXL_RESTRICT sumabs) {
  return HWY_DYNAMIC_DISPATCH(GatherBlockStats)(coeffs, coeffs_size, nonzeros,
                                                sumabs);
}

void DecodeJpegBlock(const int16_t* qblock,
                     const float* JXL_RESTRICT dequant_matrices,
                     const float* JXL_RESTRICT biases,
                     float* JXL_RESTRICT scratch_space,
                     float* JXL_RESTRICT output, size_t output_stride) {
  return HWY_DYNAMIC_DISPATCH(DecodeJpegBlock)(
      qblock, dequant_matrices, biases, scratch_space, output, output_stride);
}

void Upsample2Horizontal(float* JXL_RESTRICT row_in,
                         float* JXL_RESTRICT row_out, size_t len_out) {
  return HWY_DYNAMIC_DISPATCH(Upsample2Horizontal)(row_in, row_out, len_out);
}

void Upsample2Vertical(const float* JXL_RESTRICT row_top,
                       const float* JXL_RESTRICT row_mid,
                       const float* JXL_RESTRICT row_bot,
                       float* JXL_RESTRICT row_out0,
                       float* JXL_RESTRICT row_out1, size_t len) {
  return HWY_DYNAMIC_DISPATCH(Upsample2Vertical)(row_top, row_mid, row_bot,
                                                 row_out0, row_out1, len);
}

void YCbCrToRGB(float* JXL_RESTRICT row0, float* JXL_RESTRICT row1,
                float* JXL_RESTRICT row2, size_t xsize) {
  return HWY_DYNAMIC_DISPATCH(YCbCrToRGB)(row0, row1, row2, xsize);
}

void DecenterRow(float* row, size_t xsize) {
  return HWY_DYNAMIC_DISPATCH(DecenterRow)(row, xsize);
}

void WriteToPackedImage(float* JXL_RESTRICT rows[3], size_t x0, size_t y0,
                        size_t len, uint8_t* JXL_RESTRICT scratch_space,
                        extras::PackedImage* image) {
  return HWY_DYNAMIC_DISPATCH(WriteToPackedImage)(rows, x0, y0, len,
                                                  scratch_space, image);
}

}  // namespace extras
}  // namespace jxl
#endif  // HWY_ONCE
