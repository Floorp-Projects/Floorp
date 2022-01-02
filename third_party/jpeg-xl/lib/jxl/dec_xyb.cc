// Copyright (c) the JPEG XL Project Authors. All rights reserved.
//
// Use of this source code is governed by a BSD-style
// license that can be found in the LICENSE file.

#include "lib/jxl/dec_xyb.h"

#include <string.h>

#undef HWY_TARGET_INCLUDE
#define HWY_TARGET_INCLUDE "lib/jxl/dec_xyb.cc"
#include <hwy/foreach_target.h>
#include <hwy/highway.h>

#include "lib/jxl/base/compiler_specific.h"
#include "lib/jxl/base/profiler.h"
#include "lib/jxl/base/status.h"
#include "lib/jxl/dec_group_border.h"
#include "lib/jxl/dec_xyb-inl.h"
#include "lib/jxl/fields.h"
#include "lib/jxl/image.h"
#include "lib/jxl/opsin_params.h"
#include "lib/jxl/quantizer.h"
#include "lib/jxl/sanitizers.h"
HWY_BEFORE_NAMESPACE();
namespace jxl {
namespace HWY_NAMESPACE {

// These templates are not found via ADL.
using hwy::HWY_NAMESPACE::Broadcast;

void OpsinToLinearInplace(Image3F* JXL_RESTRICT inout, ThreadPool* pool,
                          const OpsinParams& opsin_params) {
  PROFILER_FUNC;
  JXL_CHECK_IMAGE_INITIALIZED(*inout, Rect(*inout));

  const size_t xsize = inout->xsize();  // not padded
  RunOnPool(
      pool, 0, inout->ysize(), ThreadPool::SkipInit(),
      [&](const int task, const int thread) {
        const size_t y = task;

        // Faster than adding via ByteOffset at end of loop.
        float* JXL_RESTRICT row0 = inout->PlaneRow(0, y);
        float* JXL_RESTRICT row1 = inout->PlaneRow(1, y);
        float* JXL_RESTRICT row2 = inout->PlaneRow(2, y);

        const HWY_FULL(float) d;

        for (size_t x = 0; x < xsize; x += Lanes(d)) {
          const auto in_opsin_x = Load(d, row0 + x);
          const auto in_opsin_y = Load(d, row1 + x);
          const auto in_opsin_b = Load(d, row2 + x);
          JXL_COMPILER_FENCE;
          auto linear_r = Undefined(d);
          auto linear_g = Undefined(d);
          auto linear_b = Undefined(d);
          XybToRgb(d, in_opsin_x, in_opsin_y, in_opsin_b, opsin_params,
                   &linear_r, &linear_g, &linear_b);

          Store(linear_r, d, row0 + x);
          Store(linear_g, d, row1 + x);
          Store(linear_b, d, row2 + x);
        }
      },
      "OpsinToLinear");
}

// Same, but not in-place.
void OpsinToLinear(const Image3F& opsin, const Rect& rect, ThreadPool* pool,
                   Image3F* JXL_RESTRICT linear,
                   const OpsinParams& opsin_params) {
  PROFILER_FUNC;

  JXL_ASSERT(SameSize(rect, *linear));
  JXL_CHECK_IMAGE_INITIALIZED(opsin, rect);

  RunOnPool(
      pool, 0, static_cast<int>(rect.ysize()), ThreadPool::SkipInit(),
      [&](const int task, int /*thread*/) {
        const size_t y = static_cast<size_t>(task);

        // Faster than adding via ByteOffset at end of loop.
        const float* JXL_RESTRICT row_opsin_0 = rect.ConstPlaneRow(opsin, 0, y);
        const float* JXL_RESTRICT row_opsin_1 = rect.ConstPlaneRow(opsin, 1, y);
        const float* JXL_RESTRICT row_opsin_2 = rect.ConstPlaneRow(opsin, 2, y);
        float* JXL_RESTRICT row_linear_0 = linear->PlaneRow(0, y);
        float* JXL_RESTRICT row_linear_1 = linear->PlaneRow(1, y);
        float* JXL_RESTRICT row_linear_2 = linear->PlaneRow(2, y);

        const HWY_FULL(float) d;

        for (size_t x = 0; x < rect.xsize(); x += Lanes(d)) {
          const auto in_opsin_x = Load(d, row_opsin_0 + x);
          const auto in_opsin_y = Load(d, row_opsin_1 + x);
          const auto in_opsin_b = Load(d, row_opsin_2 + x);
          JXL_COMPILER_FENCE;
          auto linear_r = Undefined(d);
          auto linear_g = Undefined(d);
          auto linear_b = Undefined(d);
          XybToRgb(d, in_opsin_x, in_opsin_y, in_opsin_b, opsin_params,
                   &linear_r, &linear_g, &linear_b);

          Store(linear_r, d, row_linear_0 + x);
          Store(linear_g, d, row_linear_1 + x);
          Store(linear_b, d, row_linear_2 + x);
        }
      },
      "OpsinToLinear(Rect)");
  JXL_CHECK_IMAGE_INITIALIZED(*linear, rect);
}

// Transform YCbCr to RGB.
// Could be performed in-place (i.e. Y, Cb and Cr could alias R, B and B).
void YcbcrToRgb(const Image3F& ycbcr, Image3F* rgb, const Rect& rect) {
  JXL_CHECK_IMAGE_INITIALIZED(ycbcr, rect);
  const HWY_CAPPED(float, GroupBorderAssigner::kPaddingXRound) df;
  const size_t S = Lanes(df);  // Step.

  const size_t xsize = rect.xsize();
  const size_t ysize = rect.ysize();
  if ((xsize == 0) || (ysize == 0)) return;

  // Full-range BT.601 as defined by JFIF Clause 7:
  // https://www.itu.int/rec/T-REC-T.871-201105-I/en
  const auto c128 = Set(df, 128.0f / 255);
  const auto crcr = Set(df, 1.402f);
  const auto cgcb = Set(df, -0.114f * 1.772f / 0.587f);
  const auto cgcr = Set(df, -0.299f * 1.402f / 0.587f);
  const auto cbcb = Set(df, 1.772f);

  for (size_t y = 0; y < ysize; y++) {
    const float* y_row = rect.ConstPlaneRow(ycbcr, 1, y);
    const float* cb_row = rect.ConstPlaneRow(ycbcr, 0, y);
    const float* cr_row = rect.ConstPlaneRow(ycbcr, 2, y);
    float* r_row = rect.PlaneRow(rgb, 0, y);
    float* g_row = rect.PlaneRow(rgb, 1, y);
    float* b_row = rect.PlaneRow(rgb, 2, y);
    for (size_t x = 0; x < xsize; x += S) {
      const auto y_vec = Load(df, y_row + x) + c128;
      const auto cb_vec = Load(df, cb_row + x);
      const auto cr_vec = Load(df, cr_row + x);
      const auto r_vec = crcr * cr_vec + y_vec;
      const auto g_vec = cgcr * cr_vec + cgcb * cb_vec + y_vec;
      const auto b_vec = cbcb * cb_vec + y_vec;
      Store(r_vec, df, r_row + x);
      Store(g_vec, df, g_row + x);
      Store(b_vec, df, b_row + x);
    }
  }
  JXL_CHECK_IMAGE_INITIALIZED(*rgb, rect);
}

// NOLINTNEXTLINE(google-readability-namespace-comments)
}  // namespace HWY_NAMESPACE
}  // namespace jxl
HWY_AFTER_NAMESPACE();

#if HWY_ONCE
namespace jxl {

HWY_EXPORT(OpsinToLinearInplace);
void OpsinToLinearInplace(Image3F* JXL_RESTRICT inout, ThreadPool* pool,
                          const OpsinParams& opsin_params) {
  return HWY_DYNAMIC_DISPATCH(OpsinToLinearInplace)(inout, pool, opsin_params);
}

HWY_EXPORT(OpsinToLinear);
void OpsinToLinear(const Image3F& opsin, const Rect& rect, ThreadPool* pool,
                   Image3F* JXL_RESTRICT linear,
                   const OpsinParams& opsin_params) {
  return HWY_DYNAMIC_DISPATCH(OpsinToLinear)(opsin, rect, pool, linear,
                                             opsin_params);
}

HWY_EXPORT(YcbcrToRgb);
void YcbcrToRgb(const Image3F& ycbcr, Image3F* rgb, const Rect& rect) {
  return HWY_DYNAMIC_DISPATCH(YcbcrToRgb)(ycbcr, rgb, rect);
}

HWY_EXPORT(HasFastXYBTosRGB8);
bool HasFastXYBTosRGB8() { return HWY_DYNAMIC_DISPATCH(HasFastXYBTosRGB8)(); }

HWY_EXPORT(FastXYBTosRGB8);
void FastXYBTosRGB8(const Image3F& input, const Rect& input_rect,
                    const Rect& output_buf_rect, const ImageF* alpha,
                    const Rect& alpha_rect, bool is_rgba,
                    uint8_t* JXL_RESTRICT output_buf, size_t xsize,
                    size_t output_stride) {
  return HWY_DYNAMIC_DISPATCH(FastXYBTosRGB8)(
      input, input_rect, output_buf_rect, alpha, alpha_rect, is_rgba,
      output_buf, xsize, output_stride);
}

void OpsinParams::Init(float intensity_target) {
  InitSIMDInverseMatrix(GetOpsinAbsorbanceInverseMatrix(), inverse_opsin_matrix,
                        intensity_target);
  memcpy(opsin_biases, kNegOpsinAbsorbanceBiasRGB,
         sizeof(kNegOpsinAbsorbanceBiasRGB));
  memcpy(quant_biases, kDefaultQuantBias, sizeof(kDefaultQuantBias));
  for (size_t c = 0; c < 4; c++) {
    opsin_biases_cbrt[c] = cbrtf(opsin_biases[c]);
  }
}

Status OutputEncodingInfo::Set(const CodecMetadata& metadata,
                               const ColorEncoding& default_enc) {
  const auto& im = metadata.transform_data.opsin_inverse_matrix;
  float inverse_matrix[9];
  memcpy(inverse_matrix, im.inverse_matrix, sizeof(inverse_matrix));
  float intensity_target = metadata.m.IntensityTarget();
  if (metadata.m.xyb_encoded) {
    const auto& orig_color_encoding = metadata.m.color_encoding;
    color_encoding = default_enc;
    // Figure out if we can output to this color encoding.
    do {
      if (!orig_color_encoding.HaveFields()) break;
      // TODO(veluca): keep in sync with dec_reconstruct.cc
      if (!orig_color_encoding.tf.IsPQ() && !orig_color_encoding.tf.IsSRGB() &&
          !orig_color_encoding.tf.IsGamma() &&
          !orig_color_encoding.tf.IsLinear() &&
          !orig_color_encoding.tf.IsHLG() && !orig_color_encoding.tf.IsDCI() &&
          !orig_color_encoding.tf.Is709()) {
        break;
      }
      if (orig_color_encoding.tf.IsGamma()) {
        inverse_gamma = orig_color_encoding.tf.GetGamma();
      }
      if (orig_color_encoding.tf.IsDCI()) {
        inverse_gamma = 1.0f / 2.6f;
      }
      if (orig_color_encoding.IsGray() &&
          orig_color_encoding.white_point != WhitePoint::kD65) {
        // TODO(veluca): figure out what should happen here.
        break;
      }

      if ((orig_color_encoding.primaries != Primaries::kSRGB ||
           orig_color_encoding.white_point != WhitePoint::kD65) &&
          !orig_color_encoding.IsGray()) {
        all_default_opsin = false;
        float srgb_to_xyzd50[9];
        const auto& srgb = ColorEncoding::SRGB(/*is_gray=*/false);
        JXL_CHECK(PrimariesToXYZD50(
            srgb.GetPrimaries().r.x, srgb.GetPrimaries().r.y,
            srgb.GetPrimaries().g.x, srgb.GetPrimaries().g.y,
            srgb.GetPrimaries().b.x, srgb.GetPrimaries().b.y,
            srgb.GetWhitePoint().x, srgb.GetWhitePoint().y, srgb_to_xyzd50));
        float xyzd50_to_original[9];
        JXL_RETURN_IF_ERROR(PrimariesToXYZD50(
            orig_color_encoding.GetPrimaries().r.x,
            orig_color_encoding.GetPrimaries().r.y,
            orig_color_encoding.GetPrimaries().g.x,
            orig_color_encoding.GetPrimaries().g.y,
            orig_color_encoding.GetPrimaries().b.x,
            orig_color_encoding.GetPrimaries().b.y,
            orig_color_encoding.GetWhitePoint().x,
            orig_color_encoding.GetWhitePoint().y, xyzd50_to_original));
        JXL_RETURN_IF_ERROR(Inv3x3Matrix(xyzd50_to_original));
        float srgb_to_original[9];
        MatMul(xyzd50_to_original, srgb_to_xyzd50, 3, 3, 3, srgb_to_original);
        MatMul(srgb_to_original, im.inverse_matrix, 3, 3, 3, inverse_matrix);
      }
      color_encoding = orig_color_encoding;
      color_encoding_is_original = true;
      if (color_encoding.tf.IsPQ()) {
        intensity_target = 10000;
      }
    } while (false);
  } else {
    color_encoding = metadata.m.color_encoding;
  }
  if (std::abs(intensity_target - 255.0) > 0.1f || !im.all_default) {
    all_default_opsin = false;
  }
  InitSIMDInverseMatrix(inverse_matrix, opsin_params.inverse_opsin_matrix,
                        intensity_target);
  std::copy(std::begin(im.opsin_biases), std::end(im.opsin_biases),
            opsin_params.opsin_biases);
  for (int i = 0; i < 3; ++i) {
    opsin_params.opsin_biases_cbrt[i] = cbrtf(opsin_params.opsin_biases[i]);
  }
  opsin_params.opsin_biases_cbrt[3] = opsin_params.opsin_biases[3] = 1;
  std::copy(std::begin(im.quant_biases), std::end(im.quant_biases),
            opsin_params.quant_biases);
  return true;
}

}  // namespace jxl
#endif  // HWY_ONCE
