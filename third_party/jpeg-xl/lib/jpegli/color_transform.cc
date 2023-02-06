// Copyright (c) the JPEG XL Project Authors. All rights reserved.
//
// Use of this source code is governed by a BSD-style
// license that can be found in the LICENSE file.

#include "lib/jpegli/color_transform.h"

#undef HWY_TARGET_INCLUDE
#define HWY_TARGET_INCLUDE "lib/jpegli/color_transform.cc"
#include <hwy/foreach_target.h>
#include <hwy/highway.h>

HWY_BEFORE_NAMESPACE();
namespace jpegli {
namespace HWY_NAMESPACE {

// These templates are not found via ADL.
using hwy::HWY_NAMESPACE::Add;
using hwy::HWY_NAMESPACE::Div;
using hwy::HWY_NAMESPACE::Mul;
using hwy::HWY_NAMESPACE::MulAdd;
using hwy::HWY_NAMESPACE::Sub;

void YCCKToCMYK(float* JXL_RESTRICT row0, float* JXL_RESTRICT row1,
                float* JXL_RESTRICT row2, float* JXL_RESTRICT row3,
                size_t xsize) {
  const HWY_CAPPED(float, 8) df;

  // Full-range BT.601 as defined by JFIF Clause 7:
  // https://www.itu.int/rec/T-REC-T.871-201105-I/en
  // After, C = 1-R, M = 1-G, Y = 1-B, K = K.
  const auto c128 = Set(df, 128.0f / 255);
  const auto crcr = Set(df, 1.402f);
  const auto cgcb = Set(df, -0.114f * 1.772f / 0.587f);
  const auto cgcr = Set(df, -0.299f * 1.402f / 0.587f);
  const auto cbcb = Set(df, 1.772f);
  const auto unity = Set(df, 1.0f);

  for (size_t x = 0; x < xsize; x += Lanes(df)) {
    const auto y_vec = Add(Load(df, row0 + x), c128);
    const auto cb_vec = Load(df, row1 + x);
    const auto cr_vec = Load(df, row2 + x);
    const auto r_vec = Sub(unity, MulAdd(crcr, cr_vec, y_vec));
    const auto g_vec =
        Sub(unity, MulAdd(cgcr, cr_vec, MulAdd(cgcb, cb_vec, y_vec)));
    const auto b_vec = Sub(unity, MulAdd(cbcb, cb_vec, y_vec));
    Store(r_vec, df, row0 + x);
    Store(g_vec, df, row1 + x);
    Store(b_vec, df, row2 + x);
    Store(Add(Load(df, row3 + x), c128), df, row3 + x);
  }
}

void YCbCrToRGB(float* JXL_RESTRICT row0, float* JXL_RESTRICT row1,
                float* JXL_RESTRICT row2, size_t xsize) {
  const HWY_CAPPED(float, 8) df;

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

void RGBToYCbCr(float* JXL_RESTRICT row0, float* JXL_RESTRICT row1,
                float* JXL_RESTRICT row2, size_t xsize) {
  const HWY_CAPPED(float, 8) df;

  // Full-range BT.601 as defined by JFIF Clause 7:
  // https://www.itu.int/rec/T-REC-T.871-201105-I/en
  const auto c128 = Set(df, 128.0f / 255);
  const auto kR = Set(df, 0.299f);  // NTSC luma
  const auto kG = Set(df, 0.587f);
  const auto kB = Set(df, 0.114f);
  const auto kAmpR = Set(df, 0.701f);
  const auto kAmpB = Set(df, 0.886f);
  const auto kDiffR = Add(kAmpR, kR);
  const auto kDiffB = Add(kAmpB, kB);
  const auto kNormR = Div(Set(df, 1.0f), (Add(kAmpR, Add(kG, kB))));
  const auto kNormB = Div(Set(df, 1.0f), (Add(kR, Add(kG, kAmpB))));

  for (size_t x = 0; x < xsize; x += Lanes(df)) {
    const auto r = Load(df, row0 + x);
    const auto g = Load(df, row1 + x);
    const auto b = Load(df, row2 + x);
    const auto r_base = Mul(r, kR);
    const auto r_diff = Mul(r, kDiffR);
    const auto g_base = Mul(g, kG);
    const auto b_base = Mul(b, kB);
    const auto b_diff = Mul(b, kDiffB);
    const auto y_base = Add(r_base, Add(g_base, b_base));
    const auto cb_vec = MulAdd(Sub(b_diff, y_base), kNormB, c128);
    const auto cr_vec = MulAdd(Sub(r_diff, y_base), kNormR, c128);
    Store(y_base, df, row0 + x);
    Store(cb_vec, df, row1 + x);
    Store(cr_vec, df, row2 + x);
  }
}

void CMYKToYCCK(float* JXL_RESTRICT row0, float* JXL_RESTRICT row1,
                float* JXL_RESTRICT row2, float* JXL_RESTRICT row3,
                size_t xsize) {
  const HWY_CAPPED(float, 8) df;
  const auto unity = Set(df, 1.0f);
  for (size_t x = 0; x < xsize; x += Lanes(df)) {
    Store(Sub(unity, Load(df, row0 + x)), df, row0 + x);
    Store(Sub(unity, Load(df, row1 + x)), df, row1 + x);
    Store(Sub(unity, Load(df, row2 + x)), df, row2 + x);
  }
  RGBToYCbCr(row0, row1, row2, xsize);
}

// NOLINTNEXTLINE(google-readability-namespace-comments)
}  // namespace HWY_NAMESPACE
}  // namespace jpegli
HWY_AFTER_NAMESPACE();

#if HWY_ONCE
namespace jpegli {

HWY_EXPORT(CMYKToYCCK);
HWY_EXPORT(YCCKToCMYK);
HWY_EXPORT(YCbCrToRGB);
HWY_EXPORT(RGBToYCbCr);

void CMYKToYCCK(float* JXL_RESTRICT row0, float* JXL_RESTRICT row1,
                float* JXL_RESTRICT row2, float* JXL_RESTRICT row3,
                size_t xsize) {
  return HWY_DYNAMIC_DISPATCH(CMYKToYCCK)(row0, row1, row2, row3, xsize);
}

void YCCKToCMYK(float* JXL_RESTRICT row0, float* JXL_RESTRICT row1,
                float* JXL_RESTRICT row2, float* JXL_RESTRICT row3,
                size_t xsize) {
  return HWY_DYNAMIC_DISPATCH(YCCKToCMYK)(row0, row1, row2, row3, xsize);
}

void YCbCrToRGB(float* JXL_RESTRICT row0, float* JXL_RESTRICT row1,
                float* JXL_RESTRICT row2, size_t xsize) {
  return HWY_DYNAMIC_DISPATCH(YCbCrToRGB)(row0, row1, row2, xsize);
}

void RGBToYCbCr(float* JXL_RESTRICT row0, float* JXL_RESTRICT row1,
                float* JXL_RESTRICT row2, size_t xsize) {
  return HWY_DYNAMIC_DISPATCH(RGBToYCbCr)(row0, row1, row2, xsize);
}

}  // namespace jpegli
#endif  // HWY_ONCE
