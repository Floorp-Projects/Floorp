// Copyright (c) the JPEG XL Project Authors. All rights reserved.
//
// Use of this source code is governed by a BSD-style
// license that can be found in the LICENSE file.

#include "lib/jxl/render_pipeline/stage_write.h"

#include "lib/jxl/alpha.h"
#include "lib/jxl/common.h"
#include "lib/jxl/dec_cache.h"
#include "lib/jxl/image_bundle.h"
#include "lib/jxl/sanitizers.h"

#undef HWY_TARGET_INCLUDE
#define HWY_TARGET_INCLUDE "lib/jxl/render_pipeline/stage_write.cc"
#include <hwy/foreach_target.h>
#include <hwy/highway.h>

HWY_BEFORE_NAMESPACE();
namespace jxl {
namespace HWY_NAMESPACE {

// These templates are not found via ADL.
using hwy::HWY_NAMESPACE::Clamp;
using hwy::HWY_NAMESPACE::Div;
using hwy::HWY_NAMESPACE::Max;
using hwy::HWY_NAMESPACE::Mul;
using hwy::HWY_NAMESPACE::NearestInt;
using hwy::HWY_NAMESPACE::Or;
using hwy::HWY_NAMESPACE::Rebind;
using hwy::HWY_NAMESPACE::ShiftLeftSame;
using hwy::HWY_NAMESPACE::ShiftRightSame;

class WriteToOutputStage : public RenderPipelineStage {
 public:
  WriteToOutputStage(const PixelCallback& pixel_callback, void* buffer,
                     size_t width, size_t height, size_t stride,
                     JxlPixelFormat format, bool has_alpha, bool unpremul_alpha,
                     size_t alpha_c, Orientation undo_orientation)
      : RenderPipelineStage(RenderPipelineStage::Settings()),
        pixel_callback_(pixel_callback),
        buffer_(buffer),
        width_(width),
        height_(height),
        stride_(stride),
        num_channels_(format.num_channels),
        num_color_(num_channels_ < 3 ? 1 : 3),
        want_alpha_(num_channels_ == 2 || num_channels_ == 4),
        has_alpha_(has_alpha),
        unpremul_alpha_(unpremul_alpha),
        alpha_c_(alpha_c),
        swap_endianness_(SwapEndianness(format.endianness)),
        flip_x_(ShouldFlipX(undo_orientation)),
        flip_y_(ShouldFlipY(undo_orientation)),
        transpose_(ShouldTranspose(undo_orientation)),
        data_type_(format.data_type),
        opaque_alpha_(kMaxPixelsPerCall, 1.0f) {}

  WriteToOutputStage(const WriteToOutputStage&) = delete;
  WriteToOutputStage& operator=(const WriteToOutputStage&) = delete;
  WriteToOutputStage(WriteToOutputStage&&) = delete;
  WriteToOutputStage& operator=(WriteToOutputStage&&) = delete;

  ~WriteToOutputStage() override {
    if (run_opaque_) {
      pixel_callback_.destroy(run_opaque_);
    }
  }

  void ProcessRow(const RowInfo& input_rows, const RowInfo& output_rows,
                  size_t xextra, size_t xsize, size_t xpos, size_t ypos,
                  size_t thread_id) const final {
    JXL_DASSERT(xextra == 0);
    JXL_DASSERT(run_opaque_ || buffer_);
    if (ypos >= height_) return;
    if (xpos >= width_) return;
    if (flip_y_) {
      ypos = height_ - 1u - ypos;
    }

    size_t limit = std::min(xsize, width_ - xpos);
    for (size_t x0 = 0; x0 < limit; x0 += kMaxPixelsPerCall) {
      size_t xstart = xpos + x0;
      size_t len = std::min<size_t>(kMaxPixelsPerCall, limit - x0);

      const float* line_buffers[4];
      for (size_t c = 0; c < num_color_; c++) {
        line_buffers[c] = GetInputRow(input_rows, c, 0) + x0;
      }
      if (has_alpha_) {
        line_buffers[num_color_] = GetInputRow(input_rows, alpha_c_, 0) + x0;
      } else {
        // opaque_alpha_ is a way to set all values to 1.0f.
        line_buffers[num_color_] = opaque_alpha_.data();
      }
      if (has_alpha_ && want_alpha_ && unpremul_alpha_) {
        UnpremulAlpha(thread_id, len, line_buffers);
      }
      if (flip_x_) {
        FlipX(thread_id, len, &xstart, line_buffers);
      }

      if (data_type_ == JXL_TYPE_UINT8) {
        uint8_t* JXL_RESTRICT temp =
            reinterpret_cast<uint8_t*>(temp_out_[thread_id].get());
        StoreUnsignedRow(line_buffers, len, temp);
        WriteToOutput(thread_id, ypos, xstart, len, temp);
      } else if (data_type_ == JXL_TYPE_UINT16 ||
                 data_type_ == JXL_TYPE_FLOAT16) {
        uint16_t* JXL_RESTRICT temp =
            reinterpret_cast<uint16_t*>(temp_out_[thread_id].get());
        if (data_type_ == JXL_TYPE_UINT16) {
          StoreUnsignedRow(line_buffers, len, temp);
        } else {
          StoreFloat16Row(line_buffers, len, temp);
        }
        if (swap_endianness_) {
          const HWY_FULL(uint16_t) du;
          size_t output_len = len * num_channels_;
          for (size_t j = 0; j < output_len; j += Lanes(du)) {
            auto v = LoadU(du, temp + j);
            auto vswap = Or(ShiftRightSame(v, 8), ShiftLeftSame(v, 8));
            StoreU(vswap, du, temp + j);
          }
        }
        WriteToOutput(thread_id, ypos, xstart, len, temp);
      } else if (data_type_ == JXL_TYPE_FLOAT) {
        float* JXL_RESTRICT temp =
            reinterpret_cast<float*>(temp_out_[thread_id].get());
        StoreFloatRow(line_buffers, len, temp);
        if (swap_endianness_) {
          size_t output_len = len * num_channels_;
          for (size_t j = 0; j < output_len; ++j) {
            temp[j] = BSwapFloat(temp[j]);
          }
        }
        WriteToOutput(thread_id, ypos, xstart, len, temp);
      }
    }
  }

  RenderPipelineChannelMode GetChannelMode(size_t c) const final {
    return c < num_color_ || (has_alpha_ && c == alpha_c_)
               ? RenderPipelineChannelMode::kInput
               : RenderPipelineChannelMode::kIgnored;
  }

  const char* GetName() const override { return "WritePixelCB"; }

 private:
  Status PrepareForThreads(size_t num_threads) override {
    if (pixel_callback_.IsPresent()) {
      run_opaque_ =
          pixel_callback_.Init(num_threads, /*num_pixels=*/kMaxPixelsPerCall);
      JXL_RETURN_IF_ERROR(run_opaque_ != nullptr);
    } else {
      JXL_RETURN_IF_ERROR(buffer_ != nullptr);
    }
    temp_out_.resize(num_threads);
    for (CacheAlignedUniquePtr& temp : temp_out_) {
      temp = AllocateArray(sizeof(float) * kMaxPixelsPerCall * num_channels_);
    }
    if ((has_alpha_ && want_alpha_ && unpremul_alpha_) || flip_x_) {
      temp_in_.resize(num_threads * num_channels_);
      for (CacheAlignedUniquePtr& temp : temp_in_) {
        temp = AllocateArray(sizeof(float) * kMaxPixelsPerCall);
      }
    }
    return true;
  }
  static bool ShouldFlipX(Orientation undo_orientation) {
    return (undo_orientation == Orientation::kFlipHorizontal ||
            undo_orientation == Orientation::kRotate180 ||
            undo_orientation == Orientation::kRotate270 ||
            undo_orientation == Orientation::kAntiTranspose);
  }
  static bool ShouldFlipY(Orientation undo_orientation) {
    return (undo_orientation == Orientation::kFlipVertical ||
            undo_orientation == Orientation::kRotate180 ||
            undo_orientation == Orientation::kRotate90 ||
            undo_orientation == Orientation::kAntiTranspose);
  }
  static bool ShouldTranspose(Orientation undo_orientation) {
    return (undo_orientation == Orientation::kTranspose ||
            undo_orientation == Orientation::kRotate90 ||
            undo_orientation == Orientation::kRotate270 ||
            undo_orientation == Orientation::kAntiTranspose);
  }

  void UnpremulAlpha(size_t thread_id, size_t len,
                     const float** line_buffers) const {
    const HWY_FULL(float) d;
    auto one = Set(d, 1.0f);
    float* temp_in[4];
    for (size_t c = 0; c < num_channels_; ++c) {
      size_t tix = thread_id * num_channels_ + c;
      temp_in[c] = reinterpret_cast<float*>(temp_in_[tix].get());
      memcpy(temp_in[c], line_buffers[c], sizeof(float) * len);
    }
    auto small_alpha = Set(d, kSmallAlpha);
    for (size_t ix = 0; ix < len; ix += Lanes(d)) {
      auto alpha = LoadU(d, temp_in[num_color_] + ix);
      auto mul = Div(one, Max(small_alpha, alpha));
      for (size_t c = 0; c < num_color_; ++c) {
        auto val = LoadU(d, temp_in[c] + ix);
        StoreU(Mul(val, mul), d, temp_in[c] + ix);
      }
    }
    for (size_t c = 0; c < num_channels_; ++c) {
      line_buffers[c] = temp_in[c];
    }
  }

  void FlipX(size_t thread_id, size_t len, size_t* xstart,
             const float** line_buffers) const {
    float* temp_in[4];
    for (size_t c = 0; c < num_channels_; ++c) {
      size_t tix = thread_id * num_channels_ + c;
      temp_in[c] = reinterpret_cast<float*>(temp_in_[tix].get());
      if (temp_in[c] != line_buffers[c]) {
        memcpy(temp_in[c], line_buffers[c], sizeof(float) * len);
      }
    }
    size_t last = (len - 1u);
    size_t num = (len / 2);
    for (size_t i = 0; i < num; ++i) {
      for (size_t c = 0; c < num_channels_; ++c) {
        std::swap(temp_in[c][i], temp_in[c][last - i]);
      }
    }
    for (size_t c = 0; c < num_channels_; ++c) {
      line_buffers[c] = temp_in[c];
    }
    *xstart = width_ - *xstart - len;
  }

  template <typename T>
  void StoreUnsignedRow(const float* input[4], size_t len, T* output) const {
    const HWY_FULL(float) d;
    auto zero = Zero(d);
    auto one = Set(d, 1.0f);
    auto mul = Set(d, (1u << (sizeof(T) * 8)) - 1);
    const Rebind<T, decltype(d)> du;
    const size_t padding = RoundUpTo(len, Lanes(d)) - len;
    for (size_t c = 0; c < num_channels_; ++c) {
      msan::UnpoisonMemory(input[c] + len, sizeof(input[c][0]) * padding);
    }
    if (num_channels_ == 1) {
      for (size_t i = 0; i < len; i += Lanes(d)) {
        auto v0 = Mul(Clamp(zero, LoadU(d, &input[0][i]), one), mul);
        StoreU(DemoteTo(du, NearestInt(v0)), du, &output[i]);
      }
    } else if (num_channels_ == 2) {
      for (size_t i = 0; i < len; i += Lanes(d)) {
        auto v0 = Mul(Clamp(zero, LoadU(d, &input[0][i]), one), mul);
        auto v1 = Mul(Clamp(zero, LoadU(d, &input[1][i]), one), mul);
        StoreInterleaved2(DemoteTo(du, NearestInt(v0)),
                          DemoteTo(du, NearestInt(v1)), du, &output[2 * i]);
      }
    } else if (num_channels_ == 3) {
      for (size_t i = 0; i < len; i += Lanes(d)) {
        auto v0 = Mul(Clamp(zero, LoadU(d, &input[0][i]), one), mul);
        auto v1 = Mul(Clamp(zero, LoadU(d, &input[1][i]), one), mul);
        auto v2 = Mul(Clamp(zero, LoadU(d, &input[2][i]), one), mul);
        StoreInterleaved3(DemoteTo(du, NearestInt(v0)),
                          DemoteTo(du, NearestInt(v1)),
                          DemoteTo(du, NearestInt(v2)), du, &output[3 * i]);
      }
    } else if (num_channels_ == 4) {
      for (size_t i = 0; i < len; i += Lanes(d)) {
        auto v0 = Mul(Clamp(zero, LoadU(d, &input[0][i]), one), mul);
        auto v1 = Mul(Clamp(zero, LoadU(d, &input[1][i]), one), mul);
        auto v2 = Mul(Clamp(zero, LoadU(d, &input[2][i]), one), mul);
        auto v3 = Mul(Clamp(zero, LoadU(d, &input[3][i]), one), mul);
        StoreInterleaved4(DemoteTo(du, NearestInt(v0)),
                          DemoteTo(du, NearestInt(v1)),
                          DemoteTo(du, NearestInt(v2)),
                          DemoteTo(du, NearestInt(v3)), du, &output[4 * i]);
      }
    }
    msan::PoisonMemory(output + num_channels_ * len,
                       sizeof(output[0]) * num_channels_ * padding);
  }

  void StoreFloat16Row(const float* input[4], size_t len,
                       uint16_t* output) const {
    const HWY_FULL(float) d;
    const Rebind<uint16_t, decltype(d)> du;
    const Rebind<hwy::float16_t, decltype(d)> df16;
    const size_t padding = RoundUpTo(len, Lanes(d)) - len;
    for (size_t c = 0; c < num_channels_; ++c) {
      msan::UnpoisonMemory(input[c] + len, sizeof(input[c][0]) * padding);
    }
    if (num_channels_ == 1) {
      for (size_t i = 0; i < len; i += Lanes(d)) {
        auto v0 = LoadU(d, &input[0][i]);
        StoreU(BitCast(du, DemoteTo(df16, v0)), du, &output[i]);
      }
    } else if (num_channels_ == 2) {
      for (size_t i = 0; i < len; i += Lanes(d)) {
        auto v0 = LoadU(d, &input[0][i]);
        auto v1 = LoadU(d, &input[1][i]);
        StoreInterleaved2(BitCast(du, DemoteTo(df16, v0)),
                          BitCast(du, DemoteTo(df16, v1)), du, &output[2 * i]);
      }
    } else if (num_channels_ == 3) {
      for (size_t i = 0; i < len; i += Lanes(d)) {
        auto v0 = LoadU(d, &input[0][i]);
        auto v1 = LoadU(d, &input[1][i]);
        auto v2 = LoadU(d, &input[2][i]);
        StoreInterleaved3(BitCast(du, DemoteTo(df16, v0)),
                          BitCast(du, DemoteTo(df16, v1)),
                          BitCast(du, DemoteTo(df16, v2)), du, &output[3 * i]);
      }
    } else if (num_channels_ == 4) {
      for (size_t i = 0; i < len; i += Lanes(d)) {
        auto v0 = LoadU(d, &input[0][i]);
        auto v1 = LoadU(d, &input[1][i]);
        auto v2 = LoadU(d, &input[2][i]);
        auto v3 = LoadU(d, &input[3][i]);
        StoreInterleaved4(BitCast(du, DemoteTo(df16, v0)),
                          BitCast(du, DemoteTo(df16, v1)),
                          BitCast(du, DemoteTo(df16, v2)),
                          BitCast(du, DemoteTo(df16, v3)), du, &output[4 * i]);
      }
    }
    msan::PoisonMemory(output + num_channels_ * len,
                       sizeof(output[0]) * num_channels_ * padding);
  }

  void StoreFloatRow(const float* input[4], size_t len, float* output) const {
    const HWY_FULL(float) d;
    if (num_channels_ == 1) {
      memcpy(output, input[0], len * sizeof(output[0]));
    } else if (num_channels_ == 2) {
      for (size_t i = 0; i < len; i += Lanes(d)) {
        StoreInterleaved2(LoadU(d, &input[0][i]), LoadU(d, &input[1][i]), d,
                          &output[2 * i]);
      }
    } else if (num_channels_ == 3) {
      for (size_t i = 0; i < len; i += Lanes(d)) {
        StoreInterleaved3(LoadU(d, &input[0][i]), LoadU(d, &input[1][i]),
                          LoadU(d, &input[2][i]), d, &output[3 * i]);
      }
    } else {
      for (size_t i = 0; i < len; i += Lanes(d)) {
        StoreInterleaved4(LoadU(d, &input[0][i]), LoadU(d, &input[1][i]),
                          LoadU(d, &input[2][i]), LoadU(d, &input[3][i]), d,
                          &output[4 * i]);
      }
    }
  }

  template <typename T>
  void WriteToOutput(size_t thread_id, size_t ypos, size_t xstart, size_t len,
                     T* output) const {
    if (transpose_) {
      // TODO(szabadka) Buffer 8x8 chunks and transpose with SIMD.
      if (run_opaque_) {
        for (size_t i = 0, j = 0; i < len; ++i, j += num_channels_) {
          pixel_callback_.run(run_opaque_, thread_id, ypos, xstart + i, 1,
                              output + j);
        }
      } else {
        size_t offset = xstart * stride_ + ypos * num_channels_ * sizeof(T);
        for (size_t i = 0, j = 0; i < len; ++i, j += num_channels_) {
          memcpy(reinterpret_cast<uint8_t*>(buffer_) + offset + i * stride_,
                 output + j, num_channels_ * sizeof(T));
        }
      }
    } else {
      if (run_opaque_) {
        pixel_callback_.run(run_opaque_, thread_id, xstart, ypos, len, output);
      } else {
        size_t offset = ypos * stride_ + xstart * num_channels_ * sizeof(T);
        memcpy(reinterpret_cast<uint8_t*>(buffer_) + offset, output,
               len * num_channels_ * sizeof(T));
      }
    }
  }

  static constexpr size_t kMaxPixelsPerCall = 1024;
  PixelCallback pixel_callback_;
  void* run_opaque_ = nullptr;
  void* buffer_ = nullptr;
  size_t width_;
  size_t height_;
  size_t stride_;
  size_t num_channels_;
  size_t num_color_;
  bool want_alpha_;
  bool has_alpha_;
  bool unpremul_alpha_;
  size_t alpha_c_;
  bool swap_endianness_;
  bool flip_x_;
  bool flip_y_;
  bool transpose_;
  JxlDataType data_type_;
  std::vector<float> opaque_alpha_;
  std::vector<CacheAlignedUniquePtr> temp_in_;
  std::vector<CacheAlignedUniquePtr> temp_out_;
};

constexpr size_t WriteToOutputStage::kMaxPixelsPerCall;

std::unique_ptr<RenderPipelineStage> GetWriteToOutputStage(
    const PixelCallback& pixel_callback, void* buffer, size_t width,
    size_t height, size_t stride, JxlPixelFormat format, bool has_alpha,
    bool unpremul_alpha, size_t alpha_c, Orientation undo_orientation) {
  return jxl::make_unique<WriteToOutputStage>(
      pixel_callback, buffer, width, height, stride, format, has_alpha,
      unpremul_alpha, alpha_c, undo_orientation);
}

// NOLINTNEXTLINE(google-readability-namespace-comments)
}  // namespace HWY_NAMESPACE
}  // namespace jxl
HWY_AFTER_NAMESPACE();

#if HWY_ONCE

namespace jxl {

HWY_EXPORT(GetWriteToOutputStage);

namespace {
class WriteToImageBundleStage : public RenderPipelineStage {
 public:
  explicit WriteToImageBundleStage(ImageBundle* image_bundle,
                                   ColorEncoding color_encoding)
      : RenderPipelineStage(RenderPipelineStage::Settings()),
        image_bundle_(image_bundle),
        color_encoding_(std::move(color_encoding)) {}

  void SetInputSizes(
      const std::vector<std::pair<size_t, size_t>>& input_sizes) override {
#if JXL_ENABLE_ASSERT
    JXL_ASSERT(input_sizes.size() >= 3);
    for (size_t c = 1; c < input_sizes.size(); c++) {
      JXL_ASSERT(input_sizes[c].first == input_sizes[0].first);
      JXL_ASSERT(input_sizes[c].second == input_sizes[0].second);
    }
#endif
    // TODO(eustas): what should we do in the case of "want only ECs"?
    image_bundle_->SetFromImage(
        Image3F(input_sizes[0].first, input_sizes[0].second), color_encoding_);
    // TODO(veluca): consider not reallocating ECs if not needed.
    image_bundle_->extra_channels().clear();
    for (size_t c = 3; c < input_sizes.size(); c++) {
      image_bundle_->extra_channels().emplace_back(input_sizes[c].first,
                                                   input_sizes[c].second);
    }
  }

  void ProcessRow(const RowInfo& input_rows, const RowInfo& output_rows,
                  size_t xextra, size_t xsize, size_t xpos, size_t ypos,
                  size_t thread_id) const final {
    for (size_t c = 0; c < 3; c++) {
      memcpy(image_bundle_->color()->PlaneRow(c, ypos) + xpos - xextra,
             GetInputRow(input_rows, c, 0) - xextra,
             sizeof(float) * (xsize + 2 * xextra));
    }
    for (size_t ec = 0; ec < image_bundle_->extra_channels().size(); ec++) {
      JXL_ASSERT(image_bundle_->extra_channels()[ec].xsize() >=
                 xpos + xsize + xextra);
      memcpy(image_bundle_->extra_channels()[ec].Row(ypos) + xpos - xextra,
             GetInputRow(input_rows, 3 + ec, 0) - xextra,
             sizeof(float) * (xsize + 2 * xextra));
    }
  }

  RenderPipelineChannelMode GetChannelMode(size_t c) const final {
    return RenderPipelineChannelMode::kInput;
  }

  const char* GetName() const override { return "WriteIB"; }

 private:
  ImageBundle* image_bundle_;
  ColorEncoding color_encoding_;
};

class WriteToImage3FStage : public RenderPipelineStage {
 public:
  explicit WriteToImage3FStage(Image3F* image)
      : RenderPipelineStage(RenderPipelineStage::Settings()), image_(image) {}

  void SetInputSizes(
      const std::vector<std::pair<size_t, size_t>>& input_sizes) override {
#if JXL_ENABLE_ASSERT
    JXL_ASSERT(input_sizes.size() >= 3);
    for (size_t c = 1; c < 3; ++c) {
      JXL_ASSERT(input_sizes[c].first == input_sizes[0].first);
      JXL_ASSERT(input_sizes[c].second == input_sizes[0].second);
    }
#endif
    *image_ = Image3F(input_sizes[0].first, input_sizes[0].second);
  }

  void ProcessRow(const RowInfo& input_rows, const RowInfo& output_rows,
                  size_t xextra, size_t xsize, size_t xpos, size_t ypos,
                  size_t thread_id) const final {
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

  const char* GetName() const override { return "WriteI3F"; }

 private:
  Image3F* image_;
};

}  // namespace

std::unique_ptr<RenderPipelineStage> GetWriteToImageBundleStage(
    ImageBundle* image_bundle, ColorEncoding color_encoding) {
  return jxl::make_unique<WriteToImageBundleStage>(image_bundle,
                                                   std::move(color_encoding));
}

std::unique_ptr<RenderPipelineStage> GetWriteToImage3FStage(Image3F* image) {
  return jxl::make_unique<WriteToImage3FStage>(image);
}

std::unique_ptr<RenderPipelineStage> GetWriteToOutputStage(
    const PixelCallback& pixel_callback, void* buffer, size_t width,
    size_t height, size_t stride, JxlPixelFormat format, bool has_alpha,
    bool unpremul_alpha, size_t alpha_c, Orientation undo_orientation) {
  return HWY_DYNAMIC_DISPATCH(GetWriteToOutputStage)(
      pixel_callback, buffer, width, height, stride, format, has_alpha,
      unpremul_alpha, alpha_c, undo_orientation);
}

}  // namespace jxl

#endif
