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
using hwy::HWY_NAMESPACE::MulAdd;

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

// NOLINTNEXTLINE(google-readability-namespace-comments)
}  // namespace HWY_NAMESPACE
}  // namespace jpegli
HWY_AFTER_NAMESPACE();

#if HWY_ONCE
namespace jpegli {

HWY_EXPORT(YCbCrToRGB);

void YCbCrToRGB(float* JXL_RESTRICT row0, float* JXL_RESTRICT row1,
                float* JXL_RESTRICT row2, size_t xsize) {
  return HWY_DYNAMIC_DISPATCH(YCbCrToRGB)(row0, row1, row2, xsize);
}

}  // namespace jpegli
#endif  // HWY_ONCE
