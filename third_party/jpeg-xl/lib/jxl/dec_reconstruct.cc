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

#include "lib/jxl/dec_reconstruct.h"

#include <atomic>
#include <utility>

#include "lib/jxl/filters.h"
#include "lib/jxl/image_ops.h"

#undef HWY_TARGET_INCLUDE
#define HWY_TARGET_INCLUDE "lib/jxl/dec_reconstruct.cc"
#include <hwy/foreach_target.h>
#include <hwy/highway.h>

#include "lib/jxl/aux_out.h"
#include "lib/jxl/base/data_parallel.h"
#include "lib/jxl/base/profiler.h"
#include "lib/jxl/blending.h"
#include "lib/jxl/color_management.h"
#include "lib/jxl/common.h"
#include "lib/jxl/dec_noise.h"
#include "lib/jxl/dec_upsample.h"
#include "lib/jxl/dec_xyb-inl.h"
#include "lib/jxl/dec_xyb.h"
#include "lib/jxl/epf.h"
#include "lib/jxl/fast_math-inl.h"
#include "lib/jxl/frame_header.h"
#include "lib/jxl/loop_filter.h"
#include "lib/jxl/passes_state.h"
#include "lib/jxl/transfer_functions-inl.h"
HWY_BEFORE_NAMESPACE();
namespace jxl {
namespace HWY_NAMESPACE {

Status UndoXYBInPlace(Image3F* idct, const Rect& rect,
                      const OutputEncodingInfo& output_encoding_info) {
  PROFILER_ZONE("UndoXYB");

  // The size of `rect` might not be a multiple of Lanes(d), but is guaranteed
  // to be a multiple of kBlockDim or at the margin of the image.
  for (size_t y = 0; y < rect.ysize(); y++) {
    float* JXL_RESTRICT row0 = rect.PlaneRow(idct, 0, y);
    float* JXL_RESTRICT row1 = rect.PlaneRow(idct, 1, y);
    float* JXL_RESTRICT row2 = rect.PlaneRow(idct, 2, y);

    const HWY_CAPPED(float, GroupBorderAssigner::kPaddingXRound) d;

    if (output_encoding_info.color_encoding.tf.IsLinear()) {
      for (size_t x = 0; x < rect.xsize(); x += Lanes(d)) {
        const auto in_opsin_x = Load(d, row0 + x);
        const auto in_opsin_y = Load(d, row1 + x);
        const auto in_opsin_b = Load(d, row2 + x);
        JXL_COMPILER_FENCE;
        auto linear_r = Undefined(d);
        auto linear_g = Undefined(d);
        auto linear_b = Undefined(d);
        XybToRgb(d, in_opsin_x, in_opsin_y, in_opsin_b,
                 output_encoding_info.opsin_params, &linear_r, &linear_g,
                 &linear_b);

        Store(linear_r, d, row0 + x);
        Store(linear_g, d, row1 + x);
        Store(linear_b, d, row2 + x);
      }
    } else if (output_encoding_info.color_encoding.tf.IsSRGB()) {
      for (size_t x = 0; x < rect.xsize(); x += Lanes(d)) {
        const auto in_opsin_x = Load(d, row0 + x);
        const auto in_opsin_y = Load(d, row1 + x);
        const auto in_opsin_b = Load(d, row2 + x);
        JXL_COMPILER_FENCE;
        auto linear_r = Undefined(d);
        auto linear_g = Undefined(d);
        auto linear_b = Undefined(d);
        XybToRgb(d, in_opsin_x, in_opsin_y, in_opsin_b,
                 output_encoding_info.opsin_params, &linear_r, &linear_g,
                 &linear_b);

#if JXL_HIGH_PRECISION
        Store(TF_SRGB().EncodedFromDisplay(d, linear_r), d, row0 + x);
        Store(TF_SRGB().EncodedFromDisplay(d, linear_g), d, row1 + x);
        Store(TF_SRGB().EncodedFromDisplay(d, linear_b), d, row2 + x);
#else
        Store(FastLinearToSRGB(d, linear_r), d, row0 + x);
        Store(FastLinearToSRGB(d, linear_g), d, row1 + x);
        Store(FastLinearToSRGB(d, linear_b), d, row2 + x);
#endif
      }
    } else if (output_encoding_info.color_encoding.tf.IsPQ()) {
      for (size_t x = 0; x < rect.xsize(); x += Lanes(d)) {
        const auto in_opsin_x = Load(d, row0 + x);
        const auto in_opsin_y = Load(d, row1 + x);
        const auto in_opsin_b = Load(d, row2 + x);
        JXL_COMPILER_FENCE;
        auto linear_r = Undefined(d);
        auto linear_g = Undefined(d);
        auto linear_b = Undefined(d);
        XybToRgb(d, in_opsin_x, in_opsin_y, in_opsin_b,
                 output_encoding_info.opsin_params, &linear_r, &linear_g,
                 &linear_b);
        Store(TF_PQ().EncodedFromDisplay(d, linear_r), d, row0 + x);
        Store(TF_PQ().EncodedFromDisplay(d, linear_g), d, row1 + x);
        Store(TF_PQ().EncodedFromDisplay(d, linear_b), d, row2 + x);
      }
    } else if (output_encoding_info.color_encoding.tf.IsHLG()) {
      for (size_t x = 0; x < rect.xsize(); x += Lanes(d)) {
        const auto in_opsin_x = Load(d, row0 + x);
        const auto in_opsin_y = Load(d, row1 + x);
        const auto in_opsin_b = Load(d, row2 + x);
        JXL_COMPILER_FENCE;
        auto linear_r = Undefined(d);
        auto linear_g = Undefined(d);
        auto linear_b = Undefined(d);
        XybToRgb(d, in_opsin_x, in_opsin_y, in_opsin_b,
                 output_encoding_info.opsin_params, &linear_r, &linear_g,
                 &linear_b);
        Store(TF_HLG().EncodedFromDisplay(d, linear_r), d, row0 + x);
        Store(TF_HLG().EncodedFromDisplay(d, linear_g), d, row1 + x);
        Store(TF_HLG().EncodedFromDisplay(d, linear_b), d, row2 + x);
      }
    } else if (output_encoding_info.color_encoding.tf.Is709()) {
      for (size_t x = 0; x < rect.xsize(); x += Lanes(d)) {
        const auto in_opsin_x = Load(d, row0 + x);
        const auto in_opsin_y = Load(d, row1 + x);
        const auto in_opsin_b = Load(d, row2 + x);
        JXL_COMPILER_FENCE;
        auto linear_r = Undefined(d);
        auto linear_g = Undefined(d);
        auto linear_b = Undefined(d);
        XybToRgb(d, in_opsin_x, in_opsin_y, in_opsin_b,
                 output_encoding_info.opsin_params, &linear_r, &linear_g,
                 &linear_b);
        Store(TF_709().EncodedFromDisplay(d, linear_r), d, row0 + x);
        Store(TF_709().EncodedFromDisplay(d, linear_g), d, row1 + x);
        Store(TF_709().EncodedFromDisplay(d, linear_b), d, row2 + x);
      }
    } else if (output_encoding_info.color_encoding.tf.IsGamma() ||
               output_encoding_info.color_encoding.tf.IsDCI()) {
      auto gamma_tf = [&](hwy::HWY_NAMESPACE::Vec<decltype(d)> v) {
        return IfThenZeroElse(
            v <= Set(d, 1e-5f),
            FastPowf(d, v, Set(d, output_encoding_info.inverse_gamma)));
      };
      for (size_t x = 0; x < rect.xsize(); x += Lanes(d)) {
        const auto in_opsin_x = Load(d, row0 + x);
        const auto in_opsin_y = Load(d, row1 + x);
        const auto in_opsin_b = Load(d, row2 + x);
        JXL_COMPILER_FENCE;
        auto linear_r = Undefined(d);
        auto linear_g = Undefined(d);
        auto linear_b = Undefined(d);
        XybToRgb(d, in_opsin_x, in_opsin_y, in_opsin_b,
                 output_encoding_info.opsin_params, &linear_r, &linear_g,
                 &linear_b);
        Store(gamma_tf(linear_r), d, row0 + x);
        Store(gamma_tf(linear_g), d, row1 + x);
        Store(gamma_tf(linear_b), d, row2 + x);
      }
    } else {
      return JXL_FAILURE("Invalid target encoding");
    }
  }
  return true;
}

template <typename D, typename V>
void StoreRGBA(D d, V r, V g, V b, V a, bool alpha, size_t n, size_t extra,
               uint8_t* buf) {
#if HWY_TARGET == HWY_SCALAR
  buf[0] = r.raw;
  buf[1] = g.raw;
  buf[2] = b.raw;
  if (alpha) {
    buf[3] = a.raw;
  }
#elif HWY_TARGET == HWY_NEON
  if (alpha) {
    uint8x8x4_t data = {r.raw, g.raw, b.raw, a.raw};
    if (extra >= 8) {
      vst4_u8(buf, data);
    } else {
      uint8_t tmp[8 * 4];
      vst4_u8(tmp, data);
      memcpy(buf, tmp, n * 4);
    }
  } else {
    uint8x8x3_t data = {r.raw, g.raw, b.raw};
    if (extra >= 8) {
      vst3_u8(buf, data);
    } else {
      uint8_t tmp[8 * 3];
      vst3_u8(tmp, data);
      memcpy(buf, tmp, n * 3);
    }
  }
#else
  // TODO(veluca): implement this for x86.
  size_t mul = alpha ? 4 : 3;
  HWY_ALIGN uint8_t bytes[16];
  Store(r, d, bytes);
  for (size_t i = 0; i < n; i++) {
    buf[mul * i] = bytes[i];
  }
  Store(g, d, bytes);
  for (size_t i = 0; i < n; i++) {
    buf[mul * i + 1] = bytes[i];
  }
  Store(b, d, bytes);
  for (size_t i = 0; i < n; i++) {
    buf[mul * i + 2] = bytes[i];
  }
  if (alpha) {
    Store(a, d, bytes);
    for (size_t i = 0; i < n; i++) {
      buf[4 * i + 3] = bytes[i];
    }
  }
#endif
}

// Outputs floating point image to RGBA 8-bit buffer. Does not support alpha
// channel in the input, but outputs opaque alpha channel for the case where the
// output buffer to write to is in the 4-byte per pixel RGBA format.
void FloatToRGBA8(const Image3F& input, const Rect& input_rect, bool is_rgba,
                  const ImageF* alpha_in, const Rect& alpha_rect,
                  const Rect& output_buf_rect, uint8_t* JXL_RESTRICT output_buf,
                  size_t stride) {
  size_t bytes = is_rgba ? 4 : 3;
  for (size_t y = 0; y < output_buf_rect.ysize(); y++) {
    const float* JXL_RESTRICT row_in_r = input_rect.ConstPlaneRow(input, 0, y);
    const float* JXL_RESTRICT row_in_g = input_rect.ConstPlaneRow(input, 1, y);
    const float* JXL_RESTRICT row_in_b = input_rect.ConstPlaneRow(input, 2, y);
    const float* JXL_RESTRICT row_in_a =
        alpha_in ? alpha_rect.ConstRow(*alpha_in, y) : nullptr;
    size_t base_ptr =
        (y + output_buf_rect.y0()) * stride + bytes * output_buf_rect.x0();
    using D = HWY_CAPPED(float, 4);
    const D d;
    D::Rebind<uint32_t> du;
    auto zero = Zero(d);
    auto one = Set(d, 1.0f);
    auto mul = Set(d, 255.0f);
    for (size_t x = 0; x < output_buf_rect.xsize(); x += Lanes(d)) {
      auto rf = Clamp(zero, Load(d, row_in_r + x), one) * mul;
      auto gf = Clamp(zero, Load(d, row_in_g + x), one) * mul;
      auto bf = Clamp(zero, Load(d, row_in_b + x), one) * mul;
      auto af = row_in_a ? Clamp(zero, Load(d, row_in_a + x), one) * mul
                         : Set(d, 255.0f);
      auto r8 = U8FromU32(BitCast(du, NearestInt(rf)));
      auto g8 = U8FromU32(BitCast(du, NearestInt(gf)));
      auto b8 = U8FromU32(BitCast(du, NearestInt(bf)));
      auto a8 = U8FromU32(BitCast(du, NearestInt(af)));
      size_t n = output_buf_rect.xsize() - x;
      if (JXL_LIKELY(n >= Lanes(d))) {
        StoreRGBA(D::Rebind<uint8_t>(), r8, g8, b8, a8, is_rgba, Lanes(d), n,
                  output_buf + base_ptr + bytes * x);
      } else {
        StoreRGBA(D::Rebind<uint8_t>(), r8, g8, b8, a8, is_rgba, n, n,
                  output_buf + base_ptr + bytes * x);
      }
    }
  }
}

// NOLINTNEXTLINE(google-readability-namespace-comments)
}  // namespace HWY_NAMESPACE
}  // namespace jxl
HWY_AFTER_NAMESPACE();

#if HWY_ONCE
namespace jxl {

HWY_EXPORT(UndoXYBInPlace);
HWY_EXPORT(FloatToRGBA8);

void UndoXYB(const Image3F& src, Image3F* dst,
             const OutputEncodingInfo& output_info, ThreadPool* pool) {
  CopyImageTo(src, dst);
  pool->Run(0, src.ysize(), ThreadPool::SkipInit(), [&](int y, int /*thread*/) {
    JXL_CHECK(HWY_DYNAMIC_DISPATCH(UndoXYBInPlace)(dst, Rect(*dst).Line(y),
                                                   output_info));
  });
}

namespace {
// Implements EnsurePaddingInPlace, but allows processing data one row at a
// time.
class EnsurePaddingInPlaceRowByRow {
 public:
  void Init(Image3F* img, const Rect& rect, const Rect& image_rect,
            size_t image_xsize, size_t image_ysize, size_t xpadding,
            size_t ypadding, ssize_t* y0, ssize_t* y1) {
    // coordinates relative to rect.
    JXL_DASSERT(SameSize(rect, image_rect));
    *y0 = -std::min(image_rect.y0(), ypadding);
    *y1 = rect.ysize() + std::min(ypadding, image_ysize - image_rect.ysize() -
                                                image_rect.y0());
    if (image_rect.x0() >= xpadding &&
        image_rect.x0() + image_rect.xsize() + xpadding <= image_xsize) {
      // Nothing to do.
      strategy_ = kSkip;
    } else if (image_xsize >= 2 * xpadding) {
      strategy_ = kFast;
    } else {
      strategy_ = kSlow;
    }
    img_ = img;
    y0_ = rect.y0();
    JXL_DASSERT(rect.x0() >= xpadding);
    x0_ = x1_ = rect.x0() - xpadding;
    // If close to the left border - do mirroring.
    if (image_rect.x0() < xpadding) x1_ = rect.x0() - image_rect.x0();
    x2_ = x3_ = rect.x0() + rect.xsize() + xpadding;
    // If close to the right border - do mirroring.
    if (image_rect.x0() + image_rect.xsize() + xpadding > image_xsize) {
      x2_ = rect.x0() + image_xsize - image_rect.x0();
    }
    JXL_DASSERT(x3_ <= img->xsize());
    JXL_DASSERT(image_xsize == (x2_ - x1_) ||
                (x1_ - x0_ <= x2_ - x1_ && x3_ - x2_ <= x2_ - x1_));
  }
  // To be called when row `y` of the input is available, for all the values in
  // [*y0, *y1).
  void Process(ssize_t y) {
    switch (strategy_) {
      case kSkip:
        break;
      case kFast:
        // Image is wide enough that a single Mirror() step is sufficient.
        for (size_t c = 0; c < 3; c++) {
          float* JXL_RESTRICT row = img_->PlaneRow(c, y + y0_);
          for (size_t x = x0_; x < x1_; x++) {
            row[x] = row[2 * x1_ - x - 1];
          }
          for (size_t x = x2_; x < x3_; x++) {
            row[x] = row[2 * x2_ - x - 1];
          }
        }
        break;
      case kSlow:
        // Slow case for small images.
        for (size_t c = 0; c < 3; c++) {
          float* JXL_RESTRICT row = img_->PlaneRow(c, y + y0_) + x1_;
          for (ssize_t x = x0_ - x1_; x < 0; x++) {
            *(row + x) = row[Mirror(x, x2_ - x1_)];
          }
          for (size_t x = x2_ - x1_; x < x3_ - x1_; x++) {
            *(row + x) = row[Mirror(x, x2_ - x1_)];
          }
        }
        break;
    }
  }

 private:
  // Initialized to silence spurious compiler warnings.
  Image3F* img_ = nullptr;
  // Will fill [x0_, x1_) and [x2_, x3_) on every row.
  // The [x1_, x2_) range contains valid image pixels. We guarantee that either
  // x1_ - x0_ <= x2_ - x1_, (and similarly for x2_, x3_), or that the [x1_,
  // x2_) contains a full horizontal line of the original image.
  size_t x0_ = 0, x1_ = 0, x2_ = 0, x3_ = 0;
  size_t y0_ = 0;
  // kSlow: use calls to Mirror(), for the case where the border might be larger
  // than the image.
  // kFast: directly use the result of Mirror() when it can be computed in a
  // single iteration.
  // kSkip: do nothing.
  enum Strategy { kFast, kSlow, kSkip };
  Strategy strategy_ = kSkip;
};
}  // namespace

void EnsurePaddingInPlace(Image3F* img, const Rect& rect,
                          const Rect& image_rect, size_t image_xsize,
                          size_t image_ysize, size_t xpadding,
                          size_t ypadding) {
  ssize_t y0, y1;
  EnsurePaddingInPlaceRowByRow impl;
  impl.Init(img, rect, image_rect, image_xsize, image_ysize, xpadding, ypadding,
            &y0, &y1);
  for (ssize_t y = y0; y < y1; y++) {
    impl.Process(y);
  }
}

Status FinalizeImageRect(
    Image3F* input_image, const Rect& input_rect,
    const std::vector<std::pair<const ImageF*, Rect>>& extra_channels,
    PassesDecoderState* dec_state, size_t thread,
    ImageBundle* JXL_RESTRICT output_image, const Rect& output_rect) {
  const ImageFeatures& image_features = dec_state->shared->image_features;
  const FrameHeader& frame_header = dec_state->shared->frame_header;
  const ImageMetadata& metadata = frame_header.nonserialized_metadata->m;
  const LoopFilter& lf = frame_header.loop_filter;
  const FrameDimensions& frame_dim = dec_state->shared->frame_dim;
  JXL_DASSERT(output_rect.xsize() <= kApplyImageFeaturesTileDim);
  JXL_DASSERT(output_rect.ysize() <= kApplyImageFeaturesTileDim);
  JXL_DASSERT(input_rect.xsize() == output_rect.xsize());
  JXL_DASSERT(input_rect.ysize() == output_rect.ysize());
  JXL_DASSERT(output_rect.x0() % GroupBorderAssigner::kPaddingXRound == 0);
  JXL_DASSERT(output_rect.xsize() % GroupBorderAssigner::kPaddingXRound == 0 ||
              output_rect.xsize() + output_rect.x0() == frame_dim.xsize ||
              output_rect.xsize() + output_rect.x0() == frame_dim.xsize_padded);

  // +----------------------------- STEP 1 ------------------------------+
  // | Compute the rects on which patches and splines will be applied.   |
  // | In case we are applying upsampling, we need to apply patches on a |
  // | slightly larger image.                                            |
  // +-------------------------------------------------------------------+

  // If we are applying upsampling, we need 2 more pixels around the actual rect
  // for border. Thus, we also need to apply patches and splines to those
  // pixels. We compute here
  // - The portion of image that corresponds to the area we are applying IF.
  //   (rect_for_if)
  // - The rect where that pixel data is stored in upsampling_input_storage.
  //   (rect_for_if_storage)
  // - The rect where the pixel data that we need to upsample is stored.
  //   (rect_for_upsampling)
  // - The source rect for the pixel data in `input_image`. It is assumed that,
  //   if `output_rect` is not on an image border, `input_image:input_rect` has
  //   enough border available. (rect_for_if_input)

  Image3F* output_color =
      dec_state->rgb_output == nullptr && dec_state->pixel_callback == nullptr
          ? output_image->color()
          : nullptr;

  Image3F* storage_for_if = output_color;
  Rect rect_for_if = output_rect;
  Rect rect_for_if_storage = output_rect;
  Rect rect_for_upsampling = output_rect;
  Rect rect_for_if_input = input_rect;
  size_t extra_rows_t = 0;
  size_t extra_rows_b = 0;
  if (frame_header.upsampling != 1) {
    size_t ifbx0 = 0;
    size_t ifbx1 = 0;
    size_t ifby0 = 0;
    size_t ifby1 = 0;
    if (output_rect.x0() >= 2) {
      JXL_DASSERT(input_rect.x0() >= 2);
      ifbx0 = 2;
    }
    if (output_rect.y0() >= 2) {
      JXL_DASSERT(input_rect.y0() >= 2);
      extra_rows_t = ifby0 = 2;
    }
    for (size_t extra : {1, 2}) {
      if (output_rect.x0() + output_rect.xsize() + extra <=
          dec_state->shared->frame_dim.xsize_padded) {
        JXL_DASSERT(input_rect.x0() + input_rect.xsize() + extra <=
                    input_image->xsize());
        ifbx1 = extra;
      }
      if (output_rect.y0() + output_rect.ysize() + extra <=
          dec_state->shared->frame_dim.ysize_padded) {
        JXL_DASSERT(input_rect.y0() + input_rect.ysize() + extra <=
                    input_image->ysize());
        extra_rows_b = ifby1 = extra;
      }
    }
    rect_for_if = Rect(output_rect.x0() - ifbx0, output_rect.y0() - ifby0,
                       output_rect.xsize() + ifbx0 + ifbx1,
                       output_rect.ysize() + ifby0 + ifby1);
    // Storage for pixel data does not necessarily start at (0, 0) as we need to
    // have the left border of upsampling_rect aligned to a multiple of
    // GroupBorderAssigner::kPaddingXRound.
    rect_for_if_storage =
        Rect(kBlockDim + RoundUpTo(ifbx0, GroupBorderAssigner::kPaddingXRound) -
                 ifbx0,
             kBlockDim, rect_for_if.xsize(), rect_for_if.ysize());
    rect_for_upsampling =
        Rect(kBlockDim + RoundUpTo(ifbx0, GroupBorderAssigner::kPaddingXRound),
             kBlockDim + ifby0, output_rect.xsize(), output_rect.ysize());
    rect_for_if_input =
        Rect(input_rect.x0() - ifbx0, input_rect.y0() - ifby0,
             rect_for_if_storage.xsize(), rect_for_if_storage.ysize());
    storage_for_if = &dec_state->upsampling_input_storage[thread];
  }

  // Variables for upsampling and filtering.
  Rect upsampled_output_rect(output_rect.x0() * frame_header.upsampling,
                             output_rect.y0() * frame_header.upsampling,
                             output_rect.xsize() * frame_header.upsampling,
                             output_rect.ysize() * frame_header.upsampling);
  EnsurePaddingInPlaceRowByRow ensure_padding_upsampling;
  ssize_t ensure_padding_upsampling_y0 = 0;
  ssize_t ensure_padding_upsampling_y1 = 0;

  EnsurePaddingInPlaceRowByRow ensure_padding_filter;
  FilterPipeline* fp = nullptr;
  ssize_t ensure_padding_filter_y0 = 0;
  ssize_t ensure_padding_filter_y1 = 0;
  Rect image_padded_rect;
  if (lf.epf_iters != 0 || lf.gab) {
    fp = &dec_state->filter_pipelines[thread];
    size_t xextra =
        rect_for_if_input.x0() % GroupBorderAssigner::kPaddingXRound;
    image_padded_rect = Rect(rect_for_if.x0() - xextra, rect_for_if.y0(),
                             rect_for_if.xsize() + xextra, rect_for_if.ysize());
  }

  // Also prepare rect for memorizing the pre-color-transform frame.
  const Rect pre_color_output_rect =
      upsampled_output_rect.Crop(dec_state->pre_color_transform_frame);
  Rect pre_color_output_rect_storage = pre_color_output_rect;

  // +----------------------------- STEP 2 ------------------------------+
  // | Change rects and buffer to not use `output_image` if direct       |
  // | output to rgb8 is requested.                                      |
  // +-------------------------------------------------------------------+
  Image3F* output_pixel_data_storage = output_color;
  Rect upsampled_output_rect_for_storage = upsampled_output_rect;
  if (dec_state->rgb_output || dec_state->pixel_callback) {
    size_t log2_upsampling = CeilLog2Nonzero(frame_header.upsampling);
    if (storage_for_if == output_color) {
      storage_for_if =
          &dec_state->output_pixel_data_storage[log2_upsampling][thread];
      rect_for_if_storage =
          Rect(0, 0, rect_for_if_storage.xsize(), rect_for_if_storage.ysize());
    }
    output_pixel_data_storage =
        &dec_state->output_pixel_data_storage[log2_upsampling][thread];
    upsampled_output_rect_for_storage = Rect(
        0, 0, upsampled_output_rect.xsize(), upsampled_output_rect.ysize());
    if (frame_header.upsampling == 1 && fp == nullptr) {
      upsampled_output_rect_for_storage = rect_for_if_storage =
          rect_for_if_input;
      output_pixel_data_storage = storage_for_if = input_image;
    }
    pre_color_output_rect_storage =
        Rect(upsampled_output_rect_for_storage.x0(),
             upsampled_output_rect_for_storage.y0(),
             pre_color_output_rect.xsize(), pre_color_output_rect.ysize());
  }
  // Set up alpha channel.
  const size_t ec =
      metadata.Find(ExtraChannel::kAlpha) - metadata.extra_channel_info.data();
  const ImageF* alpha = nullptr;
  Rect alpha_rect = upsampled_output_rect;
  if (ec < metadata.extra_channel_info.size()) {
    JXL_ASSERT(ec < extra_channels.size());
    alpha = extra_channels[ec].first;
    alpha_rect = extra_channels[ec].second;
  }

  // +----------------------------- STEP 3 ------------------------------+
  // | Set up upsampling.                                                |
  // +-------------------------------------------------------------------+
  if (frame_header.upsampling != 1) {
    ensure_padding_upsampling.Init(
        storage_for_if, rect_for_upsampling, output_rect,
        frame_dim.xsize_padded, frame_dim.ysize_padded, 2, 2,
        &ensure_padding_upsampling_y0, &ensure_padding_upsampling_y1);
  }

  // +----------------------------- STEP 4 ------------------------------+
  // | Set up the filter pipeline.                                       |
  // +-------------------------------------------------------------------+
  if (fp) {
    // If `rect_for_if_input` does not start at a multiple of
    // GroupBorderAssigner::kPaddingXRound, we extend the rect we run EPF on by
    // one full padding length to ensure sigma is handled correctly. We also
    // extend the output and image rects accordingly. To do this, we need 2x the
    // border.
    size_t xextra =
        rect_for_if_input.x0() % GroupBorderAssigner::kPaddingXRound;
    Rect filter_input_padded_rect(
        rect_for_if_input.x0() - xextra, rect_for_if_input.y0(),
        rect_for_if_input.xsize() + xextra, rect_for_if_input.ysize());
    ensure_padding_filter.Init(
        input_image, rect_for_if_input, rect_for_if, frame_dim.xsize_padded,
        frame_dim.ysize_padded, lf.Padding(), lf.Padding(),
        &ensure_padding_filter_y0, &ensure_padding_filter_y1);
    Rect filter_output_padded_rect(
        rect_for_if_storage.x0() - xextra, rect_for_if_storage.y0(),
        rect_for_if_storage.xsize() + xextra, rect_for_if_storage.ysize());
    fp = PrepareFilterPipeline(dec_state, image_padded_rect, *input_image,
                               filter_input_padded_rect, frame_dim.ysize_padded,
                               thread, storage_for_if,
                               filter_output_padded_rect);
  }

  // +----------------------------- STEP 5 ------------------------------+
  // | Run the prepared pipeline of operations.                          |
  // +-------------------------------------------------------------------+

  // y values are relative to rect_for_if.
  // Automatic mirroring in fp->ApplyFiltersRow() implies that we should ensure
  // that padding for the first lines of the image is already present before
  // calling ApplyFiltersRow() with "virtual" rows.
  // Here we rely on the fact that virtual rows at the beginning of the image
  // are only present if input_rect.y0() == 0.
  ssize_t first_ensure_padding_y = ensure_padding_filter_y0;
  if (output_rect.y0() == 0) {
    JXL_DASSERT(ensure_padding_filter_y0 == 0);
    first_ensure_padding_y =
        std::min<ssize_t>(lf.Padding(), ensure_padding_filter_y1);
    for (ssize_t y = 0; y < first_ensure_padding_y; y++) {
      ensure_padding_filter.Process(y);
    }
  }

  for (ssize_t y = -lf.Padding();
       y < static_cast<ssize_t>(lf.Padding() + rect_for_if.ysize()); y++) {
    if (fp) {
      if (y >= first_ensure_padding_y && y < ensure_padding_filter_y1) {
        ensure_padding_filter.Process(y);
      }
      fp->ApplyFiltersRow(lf, dec_state->filter_weights, image_padded_rect, y);
    } else if (output_pixel_data_storage != input_image) {
      for (size_t c = 0; c < 3; c++) {
        memcpy(rect_for_if_storage.PlaneRow(storage_for_if, c, y),
               rect_for_if_input.ConstPlaneRow(*input_image, c, y),
               rect_for_if_input.xsize() * sizeof(float));
      }
    }
    if (y < static_cast<ssize_t>(lf.Padding())) continue;
    // At this point, row `y - lf.Padding()` of `rect_for_if` has been produced
    // by the filters.
    ssize_t available_y = y - lf.Padding();
    image_features.patches.AddTo(storage_for_if,
                                 rect_for_if_storage.Line(available_y),
                                 rect_for_if.Line(available_y));
    JXL_RETURN_IF_ERROR(image_features.splines.AddTo(
        storage_for_if, rect_for_if_storage.Line(available_y),
        rect_for_if.Line(available_y), dec_state->shared->cmap));
    size_t num_ys = 1;
    if (frame_header.upsampling != 1) {
      // Upsampling `y` values are relative to `rect_for_upsampling`, not to
      // `rect_for_if`.
      ssize_t shifted_y = available_y - extra_rows_t;
      if (shifted_y >= ensure_padding_upsampling_y0 &&
          shifted_y < ensure_padding_upsampling_y1) {
        ensure_padding_upsampling.Process(shifted_y);
      }
      // Upsampling will access two rows of border, so the first upsampling
      // output will be available after shifted_y is at least 2, *unless* image
      // height is <= 2.
      if (shifted_y < 2 &&
          shifted_y + 1 != static_cast<ssize_t>(frame_dim.ysize_padded)) {
        continue;
      }
      // Value relative to upsampled_output_rect.
      size_t input_y = std::max<ssize_t>(shifted_y - 2, 0);
      size_t upsampled_available_y = frame_header.upsampling * input_y;
      size_t num_input_rows = 1;
      // If we are going to mirror the last output rows, then we already have 3
      // input lines ready. This happens iff we did not extend rect_for_if on
      // the bottom *and* we are at the last `y` value.
      if (extra_rows_b != 2 &&
          static_cast<size_t>(y) + 1 == lf.Padding() + rect_for_if.ysize()) {
        num_input_rows = 3;
      }
      num_input_rows = std::min(num_input_rows, frame_dim.ysize_padded);
      num_ys = num_input_rows * frame_header.upsampling;
      Rect upsample_input_rect =
          rect_for_upsampling.Lines(input_y, num_input_rows);
      dec_state->upsampler.UpsampleRect(
          *storage_for_if, upsample_input_rect, output_pixel_data_storage,
          upsampled_output_rect_for_storage.Lines(upsampled_available_y,
                                                  num_ys),
          static_cast<ssize_t>(output_rect.y0()) -
              static_cast<ssize_t>(rect_for_upsampling.y0()),
          frame_dim.ysize_padded);
      available_y = upsampled_available_y;
    }

    // The image data is now unconditionally in
    // `output_image_storage:upsampled_output_rect_for_storage`.
    if (frame_header.flags & FrameHeader::kNoise) {
      PROFILER_ZONE("AddNoise");
      AddNoise(image_features.noise_params,
               upsampled_output_rect.Lines(available_y, num_ys),
               dec_state->noise,
               upsampled_output_rect_for_storage.Lines(available_y, num_ys),
               dec_state->shared_storage.cmap, output_pixel_data_storage);
    }

    if (dec_state->pre_color_transform_frame.xsize() != 0) {
      for (size_t c = 0; c < 3; c++) {
        for (size_t y = available_y;
             y < num_ys && y < pre_color_output_rect.ysize(); y++) {
          float* JXL_RESTRICT row_out = pre_color_output_rect.PlaneRow(
              &dec_state->pre_color_transform_frame, c, y);
          const float* JXL_RESTRICT row_in =
              pre_color_output_rect_storage.ConstPlaneRow(
                  *output_pixel_data_storage, c, y);
          memcpy(row_out, row_in,
                 pre_color_output_rect.xsize() * sizeof(*row_in));
        }
      }
    }

    // We skip the color transform entirely if save_before_color_transform and
    // the frame is not supposed to be displayed.

    if (dec_state->fast_xyb_srgb8_conversion) {
      FastXYBTosRGB8(
          *output_pixel_data_storage,
          upsampled_output_rect_for_storage.Lines(available_y, num_ys),
          upsampled_output_rect.Lines(available_y, num_ys)
              .Crop(Rect(0, 0, frame_dim.xsize, frame_dim.ysize)),
          dec_state->rgb_output, frame_dim.xsize);
    } else {
      if (frame_header.needs_color_transform()) {
        if (frame_header.color_transform == ColorTransform::kXYB) {
          JXL_RETURN_IF_ERROR(HWY_DYNAMIC_DISPATCH(UndoXYBInPlace)(
              output_pixel_data_storage,
              upsampled_output_rect_for_storage.Lines(available_y, num_ys),
              dec_state->output_encoding_info));
        } else if (frame_header.color_transform == ColorTransform::kYCbCr) {
          YcbcrToRgb(
              *output_pixel_data_storage, output_pixel_data_storage,
              upsampled_output_rect_for_storage.Lines(available_y, num_ys));
        }
      }

      // TODO(veluca): all blending should happen here.

      if (dec_state->rgb_output != nullptr) {
        HWY_DYNAMIC_DISPATCH(FloatToRGBA8)
        (*output_pixel_data_storage,
         upsampled_output_rect_for_storage.Lines(available_y, num_ys),
         dec_state->rgb_output_is_rgba, alpha,
         alpha_rect.Lines(available_y, num_ys),
         upsampled_output_rect.Lines(available_y, num_ys)
             .Crop(Rect(0, 0, frame_dim.xsize, frame_dim.ysize)),
         dec_state->rgb_output, dec_state->rgb_stride);
      }
      if (dec_state->pixel_callback != nullptr) {
        Rect alpha_line_rect = alpha_rect.Lines(available_y, num_ys);
        Rect color_input_line_rect =
            upsampled_output_rect_for_storage.Lines(available_y, num_ys);
        Rect image_line_rect =
            upsampled_output_rect.Lines(available_y, num_ys)
                .Crop(Rect(0, 0, frame_dim.xsize, frame_dim.ysize));
        const float* line_buffers[4];
        for (size_t iy = 0; iy < image_line_rect.ysize(); iy++) {
          for (size_t c = 0; c < 3; c++) {
            line_buffers[c] = color_input_line_rect.ConstPlaneRow(
                *output_pixel_data_storage, c, iy);
          }
          if (alpha) {
            line_buffers[3] = alpha_line_rect.ConstRow(*alpha, iy);
          } else {
            line_buffers[3] = dec_state->opaque_alpha.data();
          }
          std::vector<float>& interleaved =
              dec_state->pixel_callback_rows[thread];
          size_t j = 0;
          for (size_t i = 0; i < image_line_rect.xsize(); i++) {
            interleaved[j++] = line_buffers[0][i];
            interleaved[j++] = line_buffers[1][i];
            interleaved[j++] = line_buffers[2][i];
            if (dec_state->rgb_output_is_rgba) {
              interleaved[j++] = line_buffers[3][i];
            }
          }
          dec_state->pixel_callback(interleaved.data(), image_line_rect.x0(),
                                    image_line_rect.y0() + iy,
                                    image_line_rect.xsize());
        }
      }
    }
  }

  return true;
}

Status FinalizeFrameDecoding(ImageBundle* decoded,
                             PassesDecoderState* dec_state, ThreadPool* pool,
                             bool force_fir, bool skip_blending) {
  const LoopFilter& lf = dec_state->shared->frame_header.loop_filter;
  const FrameHeader& frame_header = dec_state->shared->frame_header;
  const FrameDimensions& frame_dim = dec_state->shared->frame_dim;

  const Image3F* finalize_image_rect_input = &dec_state->decoded;
  Image3F chroma_upsampled_image;
  // If we used chroma subsampling, we upsample chroma now and run
  // ApplyImageFeatures after.
  // TODO(veluca): this should part of the FinalizeImageRect() pipeline.
  if (!frame_header.chroma_subsampling.Is444()) {
    chroma_upsampled_image = CopyImage(dec_state->decoded);
    finalize_image_rect_input = &chroma_upsampled_image;
    for (size_t c = 0; c < 3; c++) {
      ImageF& plane = const_cast<ImageF&>(chroma_upsampled_image.Plane(c));
      plane.ShrinkTo(
          frame_dim.xsize_padded >> frame_header.chroma_subsampling.HShift(c),
          frame_dim.ysize_padded >> frame_header.chroma_subsampling.VShift(c));
      for (size_t i = 0; i < frame_header.chroma_subsampling.HShift(c); i++) {
        plane.InitializePaddingForUnalignedAccesses();
        plane = UpsampleH2(plane, pool);
      }
      for (size_t i = 0; i < frame_header.chroma_subsampling.VShift(c); i++) {
        plane.InitializePaddingForUnalignedAccesses();
        plane = UpsampleV2(plane, pool);
      }
      JXL_DASSERT(SameSize(plane, chroma_upsampled_image));
    }
  }
  // FinalizeImageRect was not yet run, or we are forcing a run.
  if (!dec_state->EagerFinalizeImageRect() || force_fir) {
    if (lf.epf_iters > 0 && frame_header.encoding == FrameEncoding::kModular) {
      FillImage(kInvSigmaNum / lf.epf_sigma_for_modular,
                &dec_state->filter_weights.sigma);
    }
    std::vector<Rect> rects_to_process;
    for (size_t y = 0; y < frame_dim.ysize_padded; y += kGroupDim) {
      for (size_t x = 0; x < frame_dim.xsize_padded; x += kGroupDim) {
        Rect rect(x, y, kGroupDim, kGroupDim, frame_dim.xsize_padded,
                  frame_dim.ysize_padded);
        if (rect.xsize() == 0 || rect.ysize() == 0) continue;
        rects_to_process.push_back(rect);
      }
    }
    const auto allocate_storage = [&](size_t num_threads) {
      dec_state->EnsureStorage(num_threads);
      return true;
    };

    decoded->SetExtraChannels(std::move(dec_state->extra_channels));

    std::atomic<bool> apply_features_ok{true};
    auto run_apply_features = [&](size_t rect_id, size_t thread) {
      size_t xstart = kBlockDim + dec_state->group_border_assigner.PaddingX(
                                      dec_state->FinalizeRectPadding());
      size_t ystart = dec_state->FinalizeRectPadding();
      Rect group_data_rect(xstart, ystart, rects_to_process[rect_id].xsize(),
                           rects_to_process[rect_id].ysize());
      CopyImageToWithPadding(rects_to_process[rect_id],
                             *finalize_image_rect_input,
                             dec_state->FinalizeRectPadding(), group_data_rect,
                             &dec_state->group_data[thread]);
      std::vector<std::pair<const ImageF*, Rect>> ec_rects;
      ec_rects.reserve(decoded->extra_channels().size());
      Rect r(rects_to_process[rect_id].x0() * frame_header.upsampling,
             rects_to_process[rect_id].y0() * frame_header.upsampling,
             rects_to_process[rect_id].xsize() * frame_header.upsampling,
             rects_to_process[rect_id].ysize() * frame_header.upsampling);
      for (size_t i = 0; i < decoded->extra_channels().size(); i++) {
        ec_rects.emplace_back(&decoded->extra_channels()[i], r);
      }
      if (!FinalizeImageRect(&dec_state->group_data[thread], group_data_rect,
                             ec_rects, dec_state, thread, decoded,
                             rects_to_process[rect_id])) {
        apply_features_ok = false;
      }
    };

    RunOnPool(pool, 0, rects_to_process.size(), allocate_storage,
              run_apply_features, "ApplyFeatures");

    if (!apply_features_ok) {
      return JXL_FAILURE("FinalizeImageRect failed");
    }
  }

  const size_t xsize = frame_dim.xsize_upsampled;
  const size_t ysize = frame_dim.ysize_upsampled;

  decoded->ShrinkTo(xsize, ysize);
  if (dec_state->pre_color_transform_frame.xsize() != 0) {
    dec_state->pre_color_transform_frame.ShrinkTo(xsize, ysize);
  }

  if (!skip_blending) {
    ImageBlender blender;
    ImageBundle foreground = std::move(*decoded);
    JXL_RETURN_IF_ERROR(
        blender.PrepareBlending(dec_state, &foreground, /*output=*/decoded));

    std::vector<Rect> rects_to_process;
    for (size_t y = 0; y < frame_dim.ysize; y += kGroupDim) {
      for (size_t x = 0; x < frame_dim.xsize; x += kGroupDim) {
        Rect rect(x, y, kGroupDim, kGroupDim, frame_dim.xsize, frame_dim.ysize);
        if (rect.xsize() == 0 || rect.ysize() == 0) continue;
        rects_to_process.push_back(rect);
      }
    }

    std::atomic<bool> blending_ok{true};
    JXL_RETURN_IF_ERROR(RunOnPool(
        pool, 0, rects_to_process.size(), ThreadPool::SkipInit(),
        [&](size_t i, size_t /*thread*/) {
          const Rect& rect = rects_to_process[i];
          const auto rect_blender = blender.PrepareRect(rect, foreground);
          for (size_t y = 0; y < rect.ysize(); ++y) {
            if (!rect_blender.DoBlending(y)) {
              blending_ok = false;
              return;
            }
          }
        },
        "Blend"));
    JXL_RETURN_IF_ERROR(blending_ok.load());
  }

  return true;
}

}  // namespace jxl
#endif  // HWY_ONCE
