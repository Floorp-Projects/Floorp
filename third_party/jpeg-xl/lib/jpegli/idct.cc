// Copyright (c) the JPEG XL Project Authors. All rights reserved.
//
// Use of this source code is governed by a BSD-style
// license that can be found in the LICENSE file.

#include "lib/jpegli/idct.h"

#include "lib/jxl/base/status.h"

#undef HWY_TARGET_INCLUDE
#define HWY_TARGET_INCLUDE "lib/jpegli/idct.cc"
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

void DequantBlock(const int16_t* JXL_RESTRICT qblock,
                  const float* JXL_RESTRICT dequant,
                  const float* JXL_RESTRICT biases, float* JXL_RESTRICT block) {
  for (size_t k = 0; k < 64; k += Lanes(d)) {
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

void InverseTransformBlock(const int16_t* JXL_RESTRICT qblock,
                           const float* JXL_RESTRICT dequant,
                           const float* JXL_RESTRICT biases,
                           float* JXL_RESTRICT scratch_space,
                           float* JXL_RESTRICT output, size_t output_stride) {
  float* JXL_RESTRICT block0 = scratch_space;
  float* JXL_RESTRICT block1 = scratch_space + 64;
  DequantBlock(qblock, dequant, biases, block0);
  ComputeScaledIDCT(block0, block1, output, output_stride);
}

// NOLINTNEXTLINE(google-readability-namespace-comments)
}  // namespace HWY_NAMESPACE
}  // namespace jpegli
HWY_AFTER_NAMESPACE();

#if HWY_ONCE
namespace jpegli {

HWY_EXPORT(InverseTransformBlock);

void InverseTransformBlock(const int16_t* JXL_RESTRICT qblock,
                           const float* JXL_RESTRICT dequant_matrices,
                           const float* JXL_RESTRICT biases,
                           float* JXL_RESTRICT scratch_space,
                           float* JXL_RESTRICT output, size_t output_stride) {
  return HWY_DYNAMIC_DISPATCH(InverseTransformBlock)(
      qblock, dequant_matrices, biases, scratch_space, output, output_stride);
}

}  // namespace jpegli
#endif  // HWY_ONCE
