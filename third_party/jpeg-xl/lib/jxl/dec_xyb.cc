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
HWY_BEFORE_NAMESPACE();
namespace jxl {
namespace HWY_NAMESPACE {

// These templates are not found via ADL.
using hwy::HWY_NAMESPACE::Broadcast;

void OpsinToLinearInplace(Image3F* JXL_RESTRICT inout, ThreadPool* pool,
                          const OpsinParams& opsin_params) {
  PROFILER_FUNC;

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
}

// Transform YCbCr to RGB.
// Could be performed in-place (i.e. Y, Cb and Cr could alias R, B and B).
void YcbcrToRgb(const Image3F& ycbcr, Image3F* rgb, const Rect& rect) {
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
}

/* Vertical upsampling:
 *  input:
 *   (a, b, c) := |a1 a2 a3 a4|
 *                |b1 b2 b3 b4| <- current line
 *                |c1 c2 c3 c4|
 *  intermediate:
 *   u := a + 3 * b
 *   d := c + 3 * b
 *  output:
 *  |u1 u2 u3 u4| =: (u, d)
 *  |d1 d2 d3 d4|
 */
ImageF UpsampleV2(const ImageF& src, ThreadPool* pool) {
  const HWY_FULL(float) df;
  const size_t S = Lanes(df);
  const auto c14 = Set(df, 0.25f);
  const auto c34 = Set(df, 0.75f);

  const size_t xsize = src.xsize();
  const size_t ysize = src.ysize();
  JXL_ASSERT(xsize != 0);
  JXL_ASSERT(ysize != 0);
  ImageF dst(xsize, ysize * 2);
  if (ysize == 1) {
    memcpy(dst.Row(0), src.Row(0), xsize * sizeof(*src.Row(0)));
    memcpy(dst.Row(1), src.Row(0), xsize * sizeof(*src.Row(0)));
  } else {
    constexpr size_t kGroupArea = kGroupDim * kGroupDim;
    const size_t lines_per_group = DivCeil(kGroupArea, xsize);
    const size_t num_stripes = DivCeil(ysize, lines_per_group);
    const auto upsample = [&](int idx, int /* thread*/) {
      const size_t y0 = idx * lines_per_group;
      const size_t y1 = std::min<size_t>(y0 + lines_per_group, ysize);
      for (size_t y = y0; y < y1; ++y) {
        const float* JXL_RESTRICT prev_row = src.ConstRow(y == 0 ? 1 : y - 1);
        const float* JXL_RESTRICT current_row = src.ConstRow(y);
        const float* JXL_RESTRICT next_row =
            src.ConstRow(y == ysize - 1 ? ysize - 2 : y + 1);
        float* JXL_RESTRICT dst1_row = dst.Row(2 * y);
        float* JXL_RESTRICT dst2_row = dst.Row(2 * y + 1);
        for (size_t x = 0; x < xsize; x += S) {
          const auto current34 = Load(df, current_row + x) * c34;
          const auto prev = Load(df, prev_row + x);
          const auto next = Load(df, next_row + x);
          Store(MulAdd(prev, c14, current34), df, dst1_row + x);
          Store(MulAdd(next, c14, current34), df, dst2_row + x);
        }
      }
    };
    RunOnPool(pool, 0, static_cast<int>(num_stripes), ThreadPool::SkipInit(),
              upsample, "UpsampleV2");
  }
  return dst;
}

/* Horizontal upsampling:
 *  input:
 *   (a, b, c) := |a1 a2 a3 a4 b1 b2 b3 b4 c1 c2 c3 c4|
 *                             ^^^^^^^^^^^
 *                            current block
 *  intermediate:
 *   l := (a << 3) {0001} (b >> 1) = [a4 b1 b2 b3]
 *   r := (c >> 3) {1000} (b << 1) = [b2 b3 b4 c1]
 *   o := 3 * b + l
 *   e := 3 * b + r
 *  output:
 *   |o1 e1 o2 e2 o3 e3 o4 e4| =: (o, e)
 */
ImageF UpsampleH2(const ImageF& src, ThreadPool* pool) {
  const size_t xsize = src.xsize();
  const size_t ysize = src.ysize();
  JXL_ASSERT(xsize != 0);
  JXL_ASSERT(ysize != 0);
  ImageF dst(xsize * 2, ysize);

  constexpr size_t kGroupArea = kGroupDim * kGroupDim;
  const size_t lines_per_group = DivCeil(kGroupArea, xsize);
  const size_t num_stripes = DivCeil(ysize, lines_per_group);

  HWY_CAPPED(float, 4) d;  // necessary for interleaving.

  const auto upsample = [&](int idx, int /* thread*/) {
    const size_t y0 = idx * lines_per_group;
    const size_t y1 = std::min<size_t>(y0 + lines_per_group, ysize);
    for (size_t y = y0; y < y1; ++y) {
      const float* JXL_RESTRICT current_row = src.ConstRow(y);
      float* JXL_RESTRICT dst_row = dst.Row(y);
      const auto c34 = Set(d, 0.75f);
      const auto c14 = Set(d, 0.25f);
      for (size_t x = 1; x < xsize - 1; x += Lanes(d)) {
        auto current = LoadU(d, current_row + x) * c34;
        auto prev = LoadU(d, current_row + x - 1);
        auto next = LoadU(d, current_row + x + 1);
        auto left = MulAdd(c14, prev, current);
        auto right = MulAdd(c14, next, current);
#if HWY_TARGET == HWY_SCALAR
        StoreU(left, d, dst_row + x * 2);
        StoreU(right, d, dst_row + x * 2 + 1);
#else
        StoreU(InterleaveLower(left, right), d, dst_row + x * 2);
        StoreU(InterleaveUpper(left, right), d, dst_row + x * 2 + Lanes(d));
#endif
      }
      if (xsize == 1) {
        dst_row[0] = dst_row[1] = current_row[0];
      } else {
        const float leftmost = current_row[0] * 0.75f + current_row[1] * 0.25f;
        dst_row[0] = dst_row[1] = leftmost;
        const float rightmost =
            current_row[xsize - 1] * 0.75f + current_row[xsize - 2] * 0.25f;
        dst_row[xsize * 2 - 2] = dst_row[xsize * 2 - 1] = rightmost;
      }
    }
  };
  RunOnPool(pool, 0, static_cast<int>(num_stripes), ThreadPool::SkipInit(),
            upsample, "UpsampleH2");
  return dst;
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

HWY_EXPORT(UpsampleV2);
ImageF UpsampleV2(const ImageF& src, ThreadPool* pool) {
  return HWY_DYNAMIC_DISPATCH(UpsampleV2)(src, pool);
}

HWY_EXPORT(UpsampleH2);
ImageF UpsampleH2(const ImageF& src, ThreadPool* pool) {
  return HWY_DYNAMIC_DISPATCH(UpsampleH2)(src, pool);
}

HWY_EXPORT(HasFastXYBTosRGB8);
bool HasFastXYBTosRGB8() { return HWY_DYNAMIC_DISPATCH(HasFastXYBTosRGB8)(); }

HWY_EXPORT(FastXYBTosRGB8);
void FastXYBTosRGB8(const Image3F& input, const Rect& input_rect,
                    const Rect& output_buf_rect,
                    uint8_t* JXL_RESTRICT output_buf, size_t xsize) {
  return HWY_DYNAMIC_DISPATCH(FastXYBTosRGB8)(
      input, input_rect, output_buf_rect, output_buf, xsize);
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

Status OutputEncodingInfo::Set(const ImageMetadata& metadata) {
  const auto& im = metadata.transform_data.opsin_inverse_matrix;
  float inverse_matrix[9];
  memcpy(inverse_matrix, im.inverse_matrix, sizeof(inverse_matrix));
  float intensity_target = metadata.IntensityTarget();
  if (metadata.xyb_encoded) {
    const auto& orig_color_encoding = metadata.color_encoding;
    color_encoding = ColorEncoding::LinearSRGB(orig_color_encoding.IsGray());
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
        Inv3x3Matrix(xyzd50_to_original);
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
    color_encoding = metadata.color_encoding;
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
