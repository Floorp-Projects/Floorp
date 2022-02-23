// Copyright (c) the JPEG XL Project Authors. All rights reserved.
//
// Use of this source code is governed by a BSD-style
// license that can be found in the LICENSE file.

#include "lib/jxl/render_pipeline/stage_write.h"

#include "lib/jxl/common.h"
#include "lib/jxl/image_bundle.h"

#undef HWY_TARGET_INCLUDE
#define HWY_TARGET_INCLUDE "lib/jxl/render_pipeline/stage_write.cc"
#include <hwy/foreach_target.h>
#include <hwy/highway.h>

HWY_BEFORE_NAMESPACE();
namespace jxl {
namespace HWY_NAMESPACE {

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

class WriteToU8Stage : public RenderPipelineStage {
 public:
  WriteToU8Stage(uint8_t* rgb, size_t stride, size_t width, size_t height,
                 bool rgba, bool has_alpha, size_t alpha_c)
      : RenderPipelineStage(RenderPipelineStage::Settings()),
        rgb_(rgb),
        stride_(stride),
        width_(width),
        height_(height),
        rgba_(rgba),
        has_alpha_(has_alpha),
        alpha_c_(alpha_c) {}

  void ProcessRow(const RowInfo& input_rows, const RowInfo& output_rows,
                  size_t xextra, size_t xsize, size_t xpos, size_t ypos,
                  float* JXL_RESTRICT temp) const final {
    if (ypos >= height_) return;
    size_t bytes = rgba_ ? 4 : 3;
    const float* JXL_RESTRICT row_in_r = GetInputRow(input_rows, 0, 0);
    const float* JXL_RESTRICT row_in_g = GetInputRow(input_rows, 1, 0);
    const float* JXL_RESTRICT row_in_b = GetInputRow(input_rows, 2, 0);
    const float* JXL_RESTRICT row_in_a =
        has_alpha_ ? GetInputRow(input_rows, alpha_c_, 0) : nullptr;
    size_t base_ptr = ypos * stride_ + bytes * (xpos - xextra);
    using D = HWY_CAPPED(float, 4);
    const D d;
    D::Rebind<uint32_t> du;
    auto zero = Zero(d);
    auto one = Set(d, 1.0f);
    auto mul = Set(d, 255.0f);

    ssize_t x0 = -RoundUpTo(xextra, Lanes(d));
    ssize_t x1 = RoundUpTo(xsize + xextra, Lanes(d));

    for (ssize_t x = x0; x < x1; x += Lanes(d)) {
      auto rf = Clamp(zero, Load(d, row_in_r + x), one) * mul;
      auto gf = Clamp(zero, Load(d, row_in_g + x), one) * mul;
      auto bf = Clamp(zero, Load(d, row_in_b + x), one) * mul;
      auto af = row_in_a ? Clamp(zero, Load(d, row_in_a + x), one) * mul
                         : Set(d, 255.0f);
      auto r8 = U8FromU32(BitCast(du, NearestInt(rf)));
      auto g8 = U8FromU32(BitCast(du, NearestInt(gf)));
      auto b8 = U8FromU32(BitCast(du, NearestInt(bf)));
      auto a8 = U8FromU32(BitCast(du, NearestInt(af)));
      size_t n = width_ - xpos - x;
      if (JXL_LIKELY(n >= Lanes(d))) {
        StoreRGBA(D::Rebind<uint8_t>(), r8, g8, b8, a8, rgba_, Lanes(d), n,
                  rgb_ + base_ptr + bytes * x);
      } else {
        StoreRGBA(D::Rebind<uint8_t>(), r8, g8, b8, a8, rgba_, n, n,
                  rgb_ + base_ptr + bytes * x);
      }
    }
  }

  RenderPipelineChannelMode GetChannelMode(size_t c) const final {
    return c < 3 || (has_alpha_ && c == alpha_c_)
               ? RenderPipelineChannelMode::kInput
               : RenderPipelineChannelMode::kIgnored;
  }

 private:
  uint8_t* rgb_;
  size_t stride_;
  size_t width_;
  size_t height_;
  bool rgba_;
  bool has_alpha_;
  size_t alpha_c_;
  std::vector<float> opaque_alpha_;
};

std::unique_ptr<RenderPipelineStage> GetWriteToU8Stage(
    uint8_t* rgb, size_t stride, size_t width, size_t height, bool rgba,
    bool has_alpha, size_t alpha_c) {
  return jxl::make_unique<WriteToU8Stage>(rgb, stride, width, height, rgba,
                                          has_alpha, alpha_c);
}

// NOLINTNEXTLINE(google-readability-namespace-comments)
}  // namespace HWY_NAMESPACE
}  // namespace jxl
HWY_AFTER_NAMESPACE();

#if HWY_ONCE

namespace jxl {

HWY_EXPORT(GetWriteToU8Stage);

namespace {
class WriteToImageBundleStage : public RenderPipelineStage {
 public:
  explicit WriteToImageBundleStage(ImageBundle* image_bundle)
      : RenderPipelineStage(RenderPipelineStage::Settings()),
        image_bundle_(image_bundle) {}

  void ProcessRow(const RowInfo& input_rows, const RowInfo& output_rows,
                  size_t xextra, size_t xsize, size_t xpos, size_t ypos,
                  float* JXL_RESTRICT temp) const final {
    for (size_t c = 0; c < 3; c++) {
      memcpy(image_bundle_->color()->PlaneRow(c, ypos) + xpos - xextra,
             GetInputRow(input_rows, c, 0) - xextra,
             sizeof(float) * (xsize + 2 * xextra));
    }
    for (size_t ec = 0; ec < image_bundle_->extra_channels().size(); ec++) {
      JXL_ASSERT(ec < image_bundle_->extra_channels().size());
      JXL_ASSERT(image_bundle_->extra_channels()[ec].xsize() <=
                 xpos + xsize + xextra);
      memcpy(image_bundle_->extra_channels()[ec].Row(ypos) + xpos - xextra,
             GetInputRow(input_rows, 3 + ec, 0) - xextra,
             sizeof(float) * (xsize + 2 * xextra));
    }
  }

  RenderPipelineChannelMode GetChannelMode(size_t c) const final {
    return c < 3 + image_bundle_->extra_channels().size()
               ? RenderPipelineChannelMode::kInput
               : RenderPipelineChannelMode::kIgnored;
  }

 private:
  ImageBundle* image_bundle_;
};

class WriteToImage3FStage : public RenderPipelineStage {
 public:
  explicit WriteToImage3FStage(Image3F* image)
      : RenderPipelineStage(RenderPipelineStage::Settings()), image_(image) {}

  void ProcessRow(const RowInfo& input_rows, const RowInfo& output_rows,
                  size_t xextra, size_t xsize, size_t xpos, size_t ypos,
                  float* JXL_RESTRICT temp) const final {
    for (size_t c = 0; c < 3; c++) {
      memcpy(image_->PlaneRow(c, ypos) + xpos - xextra,
             GetInputRow(input_rows, c, 0) - xextra,
             sizeof(float) * (xsize + 2 * xextra));
    }
  }

  RenderPipelineChannelMode GetChannelMode(size_t c) const final {
    return c < 3 ? RenderPipelineChannelMode::kInput
                 : RenderPipelineChannelMode::kIgnored;
  }

 private:
  Image3F* image_;
};

class WriteToPixelCallbackStage : public RenderPipelineStage {
 public:
  WriteToPixelCallbackStage(
      const std::function<void(const float*, size_t, size_t, size_t)>&
          pixel_callback,
      size_t width, size_t height, bool rgba, bool has_alpha, size_t alpha_c)
      : RenderPipelineStage(RenderPipelineStage::Settings()),
        pixel_callback_(pixel_callback),
        width_(width),
        height_(height),
        rgba_(rgba),
        has_alpha_(has_alpha),
        alpha_c_(alpha_c),
        opaque_alpha_(kMaxPixelsPerCall, 1.0f) {
    settings_.temp_buffer_size = kMaxPixelsPerCall * (rgba_ ? 4 : 3);
  }

  void ProcessRow(const RowInfo& input_rows, const RowInfo& output_rows,
                  size_t xextra, size_t xsize, size_t xpos, size_t ypos,
                  float* JXL_RESTRICT temp) const final {
    if (ypos >= height_) return;
    const float* line_buffers[4];
    for (size_t c = 0; c < 3; c++) {
      line_buffers[c] = GetInputRow(input_rows, c, 0);
    }
    if (has_alpha_) {
      line_buffers[3] = GetInputRow(input_rows, alpha_c_, 0);
    } else {
      line_buffers[3] = opaque_alpha_.data();
    }
    // TODO(veluca): SIMD.
    ssize_t limit = std::min(xextra + xsize, width_ - xpos);
    for (ssize_t x0 = -xextra; x0 < limit; x0 += kMaxPixelsPerCall) {
      size_t j = 0;
      size_t ix = 0;
      for (; ix < kMaxPixelsPerCall && ssize_t(ix) + x0 < limit; ix++) {
        temp[j++] = line_buffers[0][x0 + ix];
        temp[j++] = line_buffers[1][x0 + ix];
        temp[j++] = line_buffers[2][x0 + ix];
        if (rgba_) {
          temp[j++] = line_buffers[3][x0 + ix];
        }
      }
      pixel_callback_(temp, xpos + x0, ypos, ix);
    }
  }

  RenderPipelineChannelMode GetChannelMode(size_t c) const final {
    return c < 3 || (has_alpha_ && c == alpha_c_)
               ? RenderPipelineChannelMode::kInput
               : RenderPipelineChannelMode::kIgnored;
  }

 private:
  static constexpr size_t kMaxPixelsPerCall = 1024;
  const std::function<void(const float*, size_t, size_t, size_t)>&
      pixel_callback_;
  size_t width_;
  size_t height_;
  bool rgba_;
  bool has_alpha_;
  size_t alpha_c_;
  std::vector<float> opaque_alpha_;
};

}  // namespace

std::unique_ptr<RenderPipelineStage> GetWriteToImageBundleStage(
    ImageBundle* image_bundle) {
  return jxl::make_unique<WriteToImageBundleStage>(image_bundle);
}

std::unique_ptr<RenderPipelineStage> GetWriteToImage3FStage(Image3F* image) {
  return jxl::make_unique<WriteToImage3FStage>(image);
}

std::unique_ptr<RenderPipelineStage> GetWriteToU8Stage(
    uint8_t* rgb, size_t stride, size_t width, size_t height, bool rgba,
    bool has_alpha, size_t alpha_c) {
  return HWY_DYNAMIC_DISPATCH(GetWriteToU8Stage)(rgb, stride, width, height,
                                                 rgba, has_alpha, alpha_c);
}

std::unique_ptr<RenderPipelineStage> GetWriteToPixelCallbackStage(
    const std::function<void(const float*, size_t, size_t, size_t)>&
        pixel_callback,
    size_t width, size_t height, bool rgba, bool has_alpha, size_t alpha_c) {
  return jxl::make_unique<WriteToPixelCallbackStage>(
      pixel_callback, width, height, rgba, has_alpha, alpha_c);
}

}  // namespace jxl

#endif
