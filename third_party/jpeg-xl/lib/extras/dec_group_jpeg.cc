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
#define HWY_TARGET_INCLUDE "lib/extras/dec_group_jpeg.cc"
#include <hwy/foreach_target.h>
#include <hwy/highway.h>

HWY_BEFORE_NAMESPACE();
namespace jxl {
namespace HWY_NAMESPACE {

// These templates are not found via ADL.
using hwy::HWY_NAMESPACE::Abs;
using hwy::HWY_NAMESPACE::Add;
using hwy::HWY_NAMESPACE::Clamp;
using hwy::HWY_NAMESPACE::Gt;
using hwy::HWY_NAMESPACE::IfThenElseZero;
using hwy::HWY_NAMESPACE::Mul;
using hwy::HWY_NAMESPACE::MulAdd;
using hwy::HWY_NAMESPACE::NearestInt;
using hwy::HWY_NAMESPACE::NegMulAdd;
using hwy::HWY_NAMESPACE::Rebind;
using hwy::HWY_NAMESPACE::Sub;
using hwy::HWY_NAMESPACE::Vec;
using hwy::HWY_NAMESPACE::Xor;

using D = HWY_FULL(float);
using DI = HWY_FULL(int32_t);
constexpr D d;
constexpr DI di;

using D8 = HWY_CAPPED(float, 8);
constexpr D8 d8;

void GatherBlockStats(const int16_t* JXL_RESTRICT coeffs,
                      const size_t coeffs_size, int32_t* JXL_RESTRICT nonzeros,
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

#if HWY_CAP_GE256
JXL_INLINE void Transpose8x8Block(const float* JXL_RESTRICT from,
                                  float* JXL_RESTRICT to) {
  const D8 d;
  auto i0 = Load(d, from);
  auto i1 = Load(d, from + 1 * 8);
  auto i2 = Load(d, from + 2 * 8);
  auto i3 = Load(d, from + 3 * 8);
  auto i4 = Load(d, from + 4 * 8);
  auto i5 = Load(d, from + 5 * 8);
  auto i6 = Load(d, from + 6 * 8);
  auto i7 = Load(d, from + 7 * 8);

  const auto q0 = InterleaveLower(d, i0, i2);
  const auto q1 = InterleaveLower(d, i1, i3);
  const auto q2 = InterleaveUpper(d, i0, i2);
  const auto q3 = InterleaveUpper(d, i1, i3);
  const auto q4 = InterleaveLower(d, i4, i6);
  const auto q5 = InterleaveLower(d, i5, i7);
  const auto q6 = InterleaveUpper(d, i4, i6);
  const auto q7 = InterleaveUpper(d, i5, i7);

  const auto r0 = InterleaveLower(d, q0, q1);
  const auto r1 = InterleaveUpper(d, q0, q1);
  const auto r2 = InterleaveLower(d, q2, q3);
  const auto r3 = InterleaveUpper(d, q2, q3);
  const auto r4 = InterleaveLower(d, q4, q5);
  const auto r5 = InterleaveUpper(d, q4, q5);
  const auto r6 = InterleaveLower(d, q6, q7);
  const auto r7 = InterleaveUpper(d, q6, q7);

  i0 = ConcatLowerLower(d, r4, r0);
  i1 = ConcatLowerLower(d, r5, r1);
  i2 = ConcatLowerLower(d, r6, r2);
  i3 = ConcatLowerLower(d, r7, r3);
  i4 = ConcatUpperUpper(d, r4, r0);
  i5 = ConcatUpperUpper(d, r5, r1);
  i6 = ConcatUpperUpper(d, r6, r2);
  i7 = ConcatUpperUpper(d, r7, r3);

  Store(i0, d, to);
  Store(i1, d, to + 1 * 8);
  Store(i2, d, to + 2 * 8);
  Store(i3, d, to + 3 * 8);
  Store(i4, d, to + 4 * 8);
  Store(i5, d, to + 5 * 8);
  Store(i6, d, to + 6 * 8);
  Store(i7, d, to + 7 * 8);
}
#elif HWY_TARGET != HWY_SCALAR
JXL_INLINE void Transpose8x8Block(const float* JXL_RESTRICT from,
                                  float* JXL_RESTRICT to) {
  const HWY_CAPPED(float, 4) d;
  for (size_t n = 0; n < 8; n += 4) {
    for (size_t m = 0; m < 8; m += 4) {
      auto p0 = Load(d, from + n * 8 + m);
      auto p1 = Load(d, from + (n + 1) * 8 + m);
      auto p2 = Load(d, from + (n + 2) * 8 + m);
      auto p3 = Load(d, from + (n + 3) * 8 + m);
      const auto q0 = InterleaveLower(d, p0, p2);
      const auto q1 = InterleaveLower(d, p1, p3);
      const auto q2 = InterleaveUpper(d, p0, p2);
      const auto q3 = InterleaveUpper(d, p1, p3);

      const auto r0 = InterleaveLower(d, q0, q1);
      const auto r1 = InterleaveUpper(d, q0, q1);
      const auto r2 = InterleaveLower(d, q2, q3);
      const auto r3 = InterleaveUpper(d, q2, q3);
      Store(r0, d, to + m * 8 + n);
      Store(r1, d, to + (1 + m) * 8 + n);
      Store(r2, d, to + (2 + m) * 8 + n);
      Store(r3, d, to + (3 + m) * 8 + n);
    }
  }
}
#else
JXL_INLINE void Transpose8x8Block(const float* JXL_RESTRICT from,
                                  float* JXL_RESTRICT to) {
  for (size_t n = 0; n < 8; ++n) {
    for (size_t m = 0; m < 8; ++m) {
      to[8 * n + m] = from[8 * m + n];
    }
  }
}
#endif

template <size_t N>
void ForwardEvenOdd(const float* JXL_RESTRICT ain, size_t ain_stride,
                    float* JXL_RESTRICT aout) {
  for (size_t i = 0; i < N / 2; i++) {
    auto in1 = LoadU(d8, ain + 2 * i * ain_stride);
    Store(in1, d8, aout + i * 8);
  }
  for (size_t i = N / 2; i < N; i++) {
    auto in1 = LoadU(d8, ain + (2 * (i - N / 2) + 1) * ain_stride);
    Store(in1, d8, aout + i * 8);
  }
}

template <size_t N>
void BTranspose(float* JXL_RESTRICT coeff) {
  for (size_t i = N - 1; i > 0; i--) {
    auto in1 = Load(d8, coeff + i * 8);
    auto in2 = Load(d8, coeff + (i - 1) * 8);
    Store(Add(in1, in2), d8, coeff + i * 8);
  }
  constexpr float kSqrt2 = 1.41421356237f;
  auto sqrt2 = Set(d8, kSqrt2);
  auto in1 = Load(d8, coeff);
  Store(Mul(in1, sqrt2), d8, coeff);
}

// Constants for DCT implementation. Generated by the following snippet:
// for i in range(N // 2):
//    print(1.0 / (2 * math.cos((i + 0.5) * math.pi / N)), end=", ")
template <size_t N>
struct WcMultipliers;

template <>
struct WcMultipliers<4> {
  static constexpr float kMultipliers[] = {
      0.541196100146197,
      1.3065629648763764,
  };
};

template <>
struct WcMultipliers<8> {
  static constexpr float kMultipliers[] = {
      0.5097955791041592,
      0.6013448869350453,
      0.8999762231364156,
      2.5629154477415055,
  };
};

constexpr float WcMultipliers<4>::kMultipliers[];
constexpr float WcMultipliers<8>::kMultipliers[];

template <size_t N>
void MultiplyAndAdd(const float* JXL_RESTRICT coeff, float* JXL_RESTRICT out,
                    size_t out_stride) {
  for (size_t i = 0; i < N / 2; i++) {
    auto mul = Set(d8, WcMultipliers<N>::kMultipliers[i]);
    auto in1 = Load(d8, coeff + i * 8);
    auto in2 = Load(d8, coeff + (N / 2 + i) * 8);
    auto out1 = MulAdd(mul, in2, in1);
    auto out2 = NegMulAdd(mul, in2, in1);
    StoreU(out1, d8, out + i * out_stride);
    StoreU(out2, d8, out + (N - i - 1) * out_stride);
  }
}

template <size_t N>
struct IDCT1DImpl;

template <>
struct IDCT1DImpl<1> {
  JXL_INLINE void operator()(const float* from, size_t from_stride, float* to,
                             size_t to_stride) {
    StoreU(LoadU(d8, from), d8, to);
  }
};

template <>
struct IDCT1DImpl<2> {
  JXL_INLINE void operator()(const float* from, size_t from_stride, float* to,
                             size_t to_stride) {
    JXL_DASSERT(from_stride >= 8);
    JXL_DASSERT(to_stride >= 8);
    auto in1 = LoadU(d8, from);
    auto in2 = LoadU(d8, from + from_stride);
    StoreU(Add(in1, in2), d8, to);
    StoreU(Sub(in1, in2), d8, to + to_stride);
  }
};

template <size_t N>
struct IDCT1DImpl {
  void operator()(const float* from, size_t from_stride, float* to,
                  size_t to_stride) {
    JXL_DASSERT(from_stride >= 8);
    JXL_DASSERT(to_stride >= 8);
    HWY_ALIGN float tmp[64];
    ForwardEvenOdd<N>(from, from_stride, tmp);
    IDCT1DImpl<N / 2>()(tmp, 8, tmp, 8);
    BTranspose<N / 2>(tmp + N * 4);
    IDCT1DImpl<N / 2>()(tmp + N * 4, 8, tmp + N * 4, 8);
    MultiplyAndAdd<N>(tmp, to, to_stride);
  }
};

template <size_t N>
void IDCT1D(float* JXL_RESTRICT from, float* JXL_RESTRICT output,
            size_t output_stride) {
  for (size_t i = 0; i < 8; i += Lanes(d8)) {
    IDCT1DImpl<N>()(from + i, 8, output + i, output_stride);
  }
}

void ComputeScaledIDCT(float* JXL_RESTRICT block0, float* JXL_RESTRICT block1,
                       float* JXL_RESTRICT output, size_t output_stride) {
  Transpose8x8Block(block0, block1);
  IDCT1D<8>(block1, block0, 8);
  Transpose8x8Block(block0, block1);
  IDCT1D<8>(block1, output, output_stride);
}

void DecodeJpegBlock(const int16_t* JXL_RESTRICT qblock,
                     const float* JXL_RESTRICT dequant,
                     const float* JXL_RESTRICT biases,
                     float* JXL_RESTRICT scratch_space,
                     float* JXL_RESTRICT output, size_t output_stride) {
  float* JXL_RESTRICT block0 = scratch_space;
  float* JXL_RESTRICT block1 = scratch_space + kDCTBlockSize;
  DequantBlock(qblock, dequant, biases, block0);
  ComputeScaledIDCT(block0, block1, output, output_stride);
}

#if HWY_CAP_GE512
using hwy::HWY_NAMESPACE::Half;
using hwy::HWY_NAMESPACE::Vec;
template <size_t i, class DF, class V>
HWY_INLINE Vec<Half<Half<DF>>> Quarter(const DF df, V v) {
  using HF = Half<DF>;
  using HHF = Half<HF>;
  auto half = i >= 2 ? UpperHalf(HF(), v) : LowerHalf(HF(), v);
  return i & 1 ? UpperHalf(HHF(), half) : LowerHalf(HHF(), half);
}

template <class DF, class V>
HWY_INLINE Vec<DF> Concat4(const DF df, V v0, V v1, V v2, V v3) {
  using HF = Half<DF>;
  return Combine(DF(), Combine(HF(), v3, v2), Combine(HF(), v1, v0));
}

#endif

// Stores v0[0], v1[0], v0[1], v1[1], ... to mem, in this order. Mem must be
// aligned.
template <class DF, class V, typename T>
void StoreInterleaved(const DF df, V v0, V v1, T* mem) {
  static_assert(sizeof(T) == 4, "only use StoreInterleaved for 4-byte types");
#if HWY_TARGET == HWY_SCALAR
  Store(v0, df, mem);
  Store(v1, df, mem + 1);
#elif !HWY_CAP_GE256
  Store(InterleaveLower(df, v0, v1), df, mem);
  Store(InterleaveUpper(df, v0, v1), df, mem + Lanes(df));
#else
  if (!HWY_CAP_GE512 || Lanes(df) == 8) {
    auto t0 = InterleaveLower(df, v0, v1);
    auto t1 = InterleaveUpper(df, v0, v1);
    Store(ConcatLowerLower(df, t1, t0), df, mem);
    Store(ConcatUpperUpper(df, t1, t0), df, mem + Lanes(df));
  } else {
#if HWY_CAP_GE512
    auto t0 = InterleaveLower(df, v0, v1);
    auto t1 = InterleaveUpper(df, v0, v1);
    Store(Concat4(df, Quarter<0>(df, t0), Quarter<0>(df, t1),
                  Quarter<1>(df, t0), Quarter<1>(df, t1)),
          df, mem);
    Store(Concat4(df, Quarter<2>(df, t0), Quarter<2>(df, t1),
                  Quarter<3>(df, t0), Quarter<3>(df, t1)),
          df, mem + Lanes(df));
#endif
  }
#endif
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
#if JXL_MEMORY_SANITIZER
  const size_t padding = RoundUpTo(len, Lanes(d)) - len;
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

void GatherBlockStats(const int16_t* JXL_RESTRICT coeffs,
                      const size_t coeffs_size, int32_t* JXL_RESTRICT nonzeros,
                      int32_t* JXL_RESTRICT sumabs) {
  return HWY_DYNAMIC_DISPATCH(GatherBlockStats)(coeffs, coeffs_size, nonzeros,
                                                sumabs);
}

void DecodeJpegBlock(const int16_t* JXL_RESTRICT qblock,
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
