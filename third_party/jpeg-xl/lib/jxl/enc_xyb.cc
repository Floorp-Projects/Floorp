// Copyright (c) the JPEG XL Project Authors. All rights reserved.
//
// Use of this source code is governed by a BSD-style
// license that can be found in the LICENSE file.

#include "lib/jxl/enc_xyb.h"

#include <algorithm>
#include <cstdlib>

#undef HWY_TARGET_INCLUDE
#define HWY_TARGET_INCLUDE "lib/jxl/enc_xyb.cc"
#include <hwy/foreach_target.h>
#include <hwy/highway.h>

#include "lib/jxl/aux_out_fwd.h"
#include "lib/jxl/base/compiler_specific.h"
#include "lib/jxl/base/data_parallel.h"
#include "lib/jxl/base/profiler.h"
#include "lib/jxl/base/status.h"
#include "lib/jxl/color_encoding_internal.h"
#include "lib/jxl/color_management.h"
#include "lib/jxl/enc_bit_writer.h"
#include "lib/jxl/enc_image_bundle.h"
#include "lib/jxl/fast_math-inl.h"
#include "lib/jxl/fields.h"
#include "lib/jxl/image_bundle.h"
#include "lib/jxl/image_ops.h"
#include "lib/jxl/opsin_params.h"
#include "lib/jxl/transfer_functions-inl.h"

HWY_BEFORE_NAMESPACE();
namespace jxl {
namespace HWY_NAMESPACE {

// These templates are not found via ADL.
using hwy::HWY_NAMESPACE::Add;
using hwy::HWY_NAMESPACE::Mul;
using hwy::HWY_NAMESPACE::MulAdd;
using hwy::HWY_NAMESPACE::Sub;
using hwy::HWY_NAMESPACE::ZeroIfNegative;

// 4x3 matrix * 3x1 SIMD vectors
template <class V>
JXL_INLINE void OpsinAbsorbance(const V r, const V g, const V b,
                                const float* JXL_RESTRICT premul_absorb,
                                V* JXL_RESTRICT mixed0, V* JXL_RESTRICT mixed1,
                                V* JXL_RESTRICT mixed2) {
  const float* bias = &kOpsinAbsorbanceBias[0];
  const HWY_FULL(float) d;
  const size_t N = Lanes(d);
  const auto m0 = Load(d, premul_absorb + 0 * N);
  const auto m1 = Load(d, premul_absorb + 1 * N);
  const auto m2 = Load(d, premul_absorb + 2 * N);
  const auto m3 = Load(d, premul_absorb + 3 * N);
  const auto m4 = Load(d, premul_absorb + 4 * N);
  const auto m5 = Load(d, premul_absorb + 5 * N);
  const auto m6 = Load(d, premul_absorb + 6 * N);
  const auto m7 = Load(d, premul_absorb + 7 * N);
  const auto m8 = Load(d, premul_absorb + 8 * N);
  *mixed0 = MulAdd(m0, r, MulAdd(m1, g, MulAdd(m2, b, Set(d, bias[0]))));
  *mixed1 = MulAdd(m3, r, MulAdd(m4, g, MulAdd(m5, b, Set(d, bias[1]))));
  *mixed2 = MulAdd(m6, r, MulAdd(m7, g, MulAdd(m8, b, Set(d, bias[2]))));
}

template <class V>
void StoreXYB(const V r, V g, const V b, float* JXL_RESTRICT valx,
              float* JXL_RESTRICT valy, float* JXL_RESTRICT valz) {
  const HWY_FULL(float) d;
  const V half = Set(d, 0.5f);
  Store(Mul(half, Sub(r, g)), d, valx);
  Store(Mul(half, Add(r, g)), d, valy);
  Store(b, d, valz);
}

// Converts one RGB vector to XYB.
template <class V>
void LinearRGBToXYB(const V r, const V g, const V b,
                    const float* JXL_RESTRICT premul_absorb,
                    float* JXL_RESTRICT valx, float* JXL_RESTRICT valy,
                    float* JXL_RESTRICT valz) {
  V mixed0, mixed1, mixed2;
  OpsinAbsorbance(r, g, b, premul_absorb, &mixed0, &mixed1, &mixed2);

  // mixed* should be non-negative even for wide-gamut, so clamp to zero.
  mixed0 = ZeroIfNegative(mixed0);
  mixed1 = ZeroIfNegative(mixed1);
  mixed2 = ZeroIfNegative(mixed2);

  const HWY_FULL(float) d;
  const size_t N = Lanes(d);
  mixed0 = CubeRootAndAdd(mixed0, Load(d, premul_absorb + 9 * N));
  mixed1 = CubeRootAndAdd(mixed1, Load(d, premul_absorb + 10 * N));
  mixed2 = CubeRootAndAdd(mixed2, Load(d, premul_absorb + 11 * N));
  StoreXYB(mixed0, mixed1, mixed2, valx, valy, valz);

  // For wide-gamut inputs, r/g/b and valx (but not y/z) are often negative.
}

// Input/output uses the codec.h scaling: nominally 0-1 if in-gamut.
template <class V>
V LinearFromSRGB(V encoded) {
  return TF_SRGB().DisplayFromEncoded(encoded);
}

Status LinearSRGBToXYB(const Image3F& linear,
                       const float* JXL_RESTRICT premul_absorb,
                       ThreadPool* pool, Image3F* JXL_RESTRICT xyb) {
  const size_t xsize = linear.xsize();

  const HWY_FULL(float) d;
  return RunOnPool(
      pool, 0, static_cast<uint32_t>(linear.ysize()), ThreadPool::NoInit,
      [&](const uint32_t task, size_t /*thread*/) {
        const size_t y = static_cast<size_t>(task);
        const float* JXL_RESTRICT row_in0 = linear.ConstPlaneRow(0, y);
        const float* JXL_RESTRICT row_in1 = linear.ConstPlaneRow(1, y);
        const float* JXL_RESTRICT row_in2 = linear.ConstPlaneRow(2, y);
        float* JXL_RESTRICT row_xyb0 = xyb->PlaneRow(0, y);
        float* JXL_RESTRICT row_xyb1 = xyb->PlaneRow(1, y);
        float* JXL_RESTRICT row_xyb2 = xyb->PlaneRow(2, y);

        for (size_t x = 0; x < xsize; x += Lanes(d)) {
          const auto in_r = Load(d, row_in0 + x);
          const auto in_g = Load(d, row_in1 + x);
          const auto in_b = Load(d, row_in2 + x);
          LinearRGBToXYB(in_r, in_g, in_b, premul_absorb, row_xyb0 + x,
                         row_xyb1 + x, row_xyb2 + x);
        }
      },
      "LinearToXYB");
}

Status SRGBToXYB(const Image3F& srgb, const float* JXL_RESTRICT premul_absorb,
                 ThreadPool* pool, Image3F* JXL_RESTRICT xyb) {
  const size_t xsize = srgb.xsize();

  const HWY_FULL(float) d;
  return RunOnPool(
      pool, 0, static_cast<uint32_t>(srgb.ysize()), ThreadPool::NoInit,
      [&](const uint32_t task, size_t /*thread*/) {
        const size_t y = static_cast<size_t>(task);
        const float* JXL_RESTRICT row_srgb0 = srgb.ConstPlaneRow(0, y);
        const float* JXL_RESTRICT row_srgb1 = srgb.ConstPlaneRow(1, y);
        const float* JXL_RESTRICT row_srgb2 = srgb.ConstPlaneRow(2, y);
        float* JXL_RESTRICT row_xyb0 = xyb->PlaneRow(0, y);
        float* JXL_RESTRICT row_xyb1 = xyb->PlaneRow(1, y);
        float* JXL_RESTRICT row_xyb2 = xyb->PlaneRow(2, y);

        for (size_t x = 0; x < xsize; x += Lanes(d)) {
          const auto in_r = LinearFromSRGB(Load(d, row_srgb0 + x));
          const auto in_g = LinearFromSRGB(Load(d, row_srgb1 + x));
          const auto in_b = LinearFromSRGB(Load(d, row_srgb2 + x));
          LinearRGBToXYB(in_r, in_g, in_b, premul_absorb, row_xyb0 + x,
                         row_xyb1 + x, row_xyb2 + x);
        }
      },
      "SRGBToXYB");
}

Status SRGBToXYBAndLinear(const Image3F& srgb,
                          const float* JXL_RESTRICT premul_absorb,
                          ThreadPool* pool, Image3F* JXL_RESTRICT xyb,
                          Image3F* JXL_RESTRICT linear) {
  const size_t xsize = srgb.xsize();

  const HWY_FULL(float) d;
  return RunOnPool(
      pool, 0, static_cast<uint32_t>(srgb.ysize()), ThreadPool::NoInit,
      [&](const uint32_t task, size_t /*thread*/) {
        const size_t y = static_cast<size_t>(task);
        const float* JXL_RESTRICT row_srgb0 = srgb.ConstPlaneRow(0, y);
        const float* JXL_RESTRICT row_srgb1 = srgb.ConstPlaneRow(1, y);
        const float* JXL_RESTRICT row_srgb2 = srgb.ConstPlaneRow(2, y);

        float* JXL_RESTRICT row_linear0 = linear->PlaneRow(0, y);
        float* JXL_RESTRICT row_linear1 = linear->PlaneRow(1, y);
        float* JXL_RESTRICT row_linear2 = linear->PlaneRow(2, y);

        float* JXL_RESTRICT row_xyb0 = xyb->PlaneRow(0, y);
        float* JXL_RESTRICT row_xyb1 = xyb->PlaneRow(1, y);
        float* JXL_RESTRICT row_xyb2 = xyb->PlaneRow(2, y);

        for (size_t x = 0; x < xsize; x += Lanes(d)) {
          const auto in_r = LinearFromSRGB(Load(d, row_srgb0 + x));
          const auto in_g = LinearFromSRGB(Load(d, row_srgb1 + x));
          const auto in_b = LinearFromSRGB(Load(d, row_srgb2 + x));

          Store(in_r, d, row_linear0 + x);
          Store(in_g, d, row_linear1 + x);
          Store(in_b, d, row_linear2 + x);

          LinearRGBToXYB(in_r, in_g, in_b, premul_absorb, row_xyb0 + x,
                         row_xyb1 + x, row_xyb2 + x);
        }
      },
      "SRGBToXYBAndLinear");
}

// This is different from Butteraugli's OpsinDynamicsImage() in the sense that
// it does not contain a sensitivity multiplier based on the blurred image.
const ImageBundle* ToXYB(const ImageBundle& in, ThreadPool* pool,
                         Image3F* JXL_RESTRICT xyb, const JxlCmsInterface& cms,
                         ImageBundle* const JXL_RESTRICT linear) {
  PROFILER_FUNC;

  const size_t xsize = in.xsize();
  const size_t ysize = in.ysize();
  JXL_ASSERT(SameSize(in, *xyb));

  const HWY_FULL(float) d;
  // Pre-broadcasted constants
  HWY_ALIGN float premul_absorb[MaxLanes(d) * 12];
  const size_t N = Lanes(d);
  for (size_t i = 0; i < 9; ++i) {
    const auto absorb = Set(d, kOpsinAbsorbanceMatrix[i] *
                                   (in.metadata()->IntensityTarget() / 255.0f));
    Store(absorb, d, premul_absorb + i * N);
  }
  for (size_t i = 0; i < 3; ++i) {
    const auto neg_bias_cbrt = Set(d, -cbrtf(kOpsinAbsorbanceBias[i]));
    Store(neg_bias_cbrt, d, premul_absorb + (9 + i) * N);
  }

  const bool want_linear = linear != nullptr;

  const ColorEncoding& c_linear_srgb = ColorEncoding::LinearSRGB(in.IsGray());
  // Linear sRGB inputs are rare but can be useful for the fastest encoders, for
  // which undoing the sRGB transfer function would be a large part of the cost.
  if (c_linear_srgb.SameColorEncoding(in.c_current())) {
    JXL_CHECK(LinearSRGBToXYB(in.color(), premul_absorb, pool, xyb));
    // This only happens if kitten or slower, moving ImageBundle might be
    // possible but the encoder is much slower than this copy.
    if (want_linear) {
      *linear = in.Copy();
      return linear;
    }
    return &in;
  }

  // Common case: already sRGB, can avoid the color transform
  if (in.IsSRGB()) {
    // Common case: can avoid allocating/copying
    if (!want_linear) {
      JXL_CHECK(SRGBToXYB(in.color(), premul_absorb, pool, xyb));
      return &in;
    }

    // Slow encoder also wants linear sRGB.
    linear->SetFromImage(Image3F(xsize, ysize), c_linear_srgb);
    JXL_CHECK(SRGBToXYBAndLinear(in.color(), premul_absorb, pool, xyb,
                                 linear->color()));
    return linear;
  }

  // General case: not sRGB, need color transform.
  ImageBundle linear_storage;  // Local storage only used if !want_linear.

  ImageBundle* linear_storage_ptr;
  if (want_linear) {
    // Caller asked for linear, use that storage directly.
    linear_storage_ptr = linear;
  } else {
    // Caller didn't ask for linear, create our own local storage
    // OK to reuse metadata, it will not be changed.
    linear_storage = ImageBundle(const_cast<ImageMetadata*>(in.metadata()));
    linear_storage_ptr = &linear_storage;
  }

  const ImageBundle* ptr;
  JXL_CHECK(TransformIfNeeded(in, c_linear_srgb, cms, pool, linear_storage_ptr,
                              &ptr));
  // If no transform was necessary, should have taken the above codepath.
  JXL_ASSERT(ptr == linear_storage_ptr);

  JXL_CHECK(
      LinearSRGBToXYB(*linear_storage_ptr->color(), premul_absorb, pool, xyb));
  return want_linear ? linear : &in;
}

// Transform RGB to YCbCr.
// Could be performed in-place (i.e. Y, Cb and Cr could alias R, B and B).
Status RgbToYcbcr(const ImageF& r_plane, const ImageF& g_plane,
                  const ImageF& b_plane, ImageF* y_plane, ImageF* cb_plane,
                  ImageF* cr_plane, ThreadPool* pool) {
  const HWY_FULL(float) df;
  const size_t S = Lanes(df);  // Step.

  const size_t xsize = r_plane.xsize();
  const size_t ysize = r_plane.ysize();
  if ((xsize == 0) || (ysize == 0)) return true;

  // Full-range BT.601 as defined by JFIF Clause 7:
  // https://www.itu.int/rec/T-REC-T.871-201105-I/en
  const auto k128 = Set(df, 128.0f / 255);
  const auto kR = Set(df, 0.299f);  // NTSC luma
  const auto kG = Set(df, 0.587f);
  const auto kB = Set(df, 0.114f);
  const auto kAmpR = Set(df, 0.701f);
  const auto kAmpB = Set(df, 0.886f);
  const auto kDiffR = Add(kAmpR, kR);
  const auto kDiffB = Add(kAmpB, kB);
  const auto kNormR = Div(Set(df, 1.0f), (Add(kAmpR, Add(kG, kB))));
  const auto kNormB = Div(Set(df, 1.0f), (Add(kR, Add(kG, kAmpB))));

  constexpr size_t kGroupArea = kGroupDim * kGroupDim;
  const size_t lines_per_group = DivCeil(kGroupArea, xsize);
  const size_t num_stripes = DivCeil(ysize, lines_per_group);
  const auto transform = [&](int idx, int /* thread*/) {
    const size_t y0 = idx * lines_per_group;
    const size_t y1 = std::min<size_t>(y0 + lines_per_group, ysize);
    for (size_t y = y0; y < y1; ++y) {
      const float* r_row = r_plane.ConstRow(y);
      const float* g_row = g_plane.ConstRow(y);
      const float* b_row = b_plane.ConstRow(y);
      float* y_row = y_plane->Row(y);
      float* cb_row = cb_plane->Row(y);
      float* cr_row = cr_plane->Row(y);
      for (size_t x = 0; x < xsize; x += S) {
        const auto r = Load(df, r_row + x);
        const auto g = Load(df, g_row + x);
        const auto b = Load(df, b_row + x);
        const auto r_base = Mul(r, kR);
        const auto r_diff = Mul(r, kDiffR);
        const auto g_base = Mul(g, kG);
        const auto b_base = Mul(b, kB);
        const auto b_diff = Mul(b, kDiffB);
        const auto y_base = Add(r_base, Add(g_base, b_base));
        const auto y_vec = Sub(y_base, k128);
        const auto cb_vec = Mul(Sub(b_diff, y_base), kNormB);
        const auto cr_vec = Mul(Sub(r_diff, y_base), kNormR);
        Store(y_vec, df, y_row + x);
        Store(cb_vec, df, cb_row + x);
        Store(cr_vec, df, cr_row + x);
      }
    }
  };
  return RunOnPool(pool, 0, static_cast<int>(num_stripes), ThreadPool::NoInit,
                   transform, "RgbToYcbCr");
}

// NOLINTNEXTLINE(google-readability-namespace-comments)
}  // namespace HWY_NAMESPACE
}  // namespace jxl
HWY_AFTER_NAMESPACE();

#if HWY_ONCE
namespace jxl {
HWY_EXPORT(ToXYB);
const ImageBundle* ToXYB(const ImageBundle& in, ThreadPool* pool,
                         Image3F* JXL_RESTRICT xyb, const JxlCmsInterface& cms,
                         ImageBundle* JXL_RESTRICT linear_storage) {
  return HWY_DYNAMIC_DISPATCH(ToXYB)(in, pool, xyb, cms, linear_storage);
}

void ScaleXYB(Image3F* opsin) {
  jxl::SubtractFrom(opsin->Plane(1), &opsin->Plane(2));
  for (size_t y = 0; y < opsin->ysize(); y++) {
    for (size_t c = 0; c < 3; ++c) {
      float* row = opsin->PlaneRow(c, y);
      for (size_t x = 0; x < opsin->xsize(); x++) {
        row[x] = (row[x] + kScaledXYBOffset[c]) * kScaledXYBScale[c];
      }
    }
  }
}

HWY_EXPORT(RgbToYcbcr);
Status RgbToYcbcr(const ImageF& r_plane, const ImageF& g_plane,
                  const ImageF& b_plane, ImageF* y_plane, ImageF* cb_plane,
                  ImageF* cr_plane, ThreadPool* pool) {
  return HWY_DYNAMIC_DISPATCH(RgbToYcbcr)(r_plane, g_plane, b_plane, y_plane,
                                          cb_plane, cr_plane, pool);
}

// DEPRECATED
Image3F OpsinDynamicsImage(const Image3B& srgb8, const JxlCmsInterface& cms) {
  ImageMetadata metadata;
  metadata.SetUintSamples(8);
  metadata.color_encoding = ColorEncoding::SRGB();
  ImageBundle ib(&metadata);
  ib.SetFromImage(ConvertToFloat(srgb8), metadata.color_encoding);
  JXL_CHECK(ib.TransformTo(ColorEncoding::LinearSRGB(ib.IsGray()), cms));
  ThreadPool* null_pool = nullptr;
  Image3F xyb(srgb8.xsize(), srgb8.ysize());

  ImageBundle linear_storage(&metadata);
  (void)ToXYB(ib, null_pool, &xyb, cms, &linear_storage);
  return xyb;
}

}  // namespace jxl
#endif  // HWY_ONCE
