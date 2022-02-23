// Copyright (c) the JPEG XL Project Authors. All rights reserved.
//
// Use of this source code is governed by a BSD-style
// license that can be found in the LICENSE file.

#include "lib/jxl/render_pipeline/stage_blending.h"

#undef HWY_TARGET_INCLUDE
#define HWY_TARGET_INCLUDE "lib/jxl/render_pipeline/stage_blending.cc"
#include <hwy/foreach_target.h>
#include <hwy/highway.h>

#include "lib/jxl/base/printf_macros.h"
#include "lib/jxl/blending.h"

HWY_BEFORE_NAMESPACE();
namespace jxl {
namespace HWY_NAMESPACE {

class BlendingStage : public RenderPipelineStage {
 public:
  explicit BlendingStage(const PassesDecoderState* dec_state,
                         const ColorEncoding& frame_color_encoding)
      : RenderPipelineStage(RenderPipelineStage::Settings()),
        state_(*dec_state->shared) {
    image_xsize_ = state_.frame_header.nonserialized_metadata->xsize();
    image_ysize_ = state_.frame_header.nonserialized_metadata->ysize();
    extra_channel_info_ =
        &state_.frame_header.nonserialized_metadata->m.extra_channel_info;
    info_ = state_.frame_header.blending_info;
    const std::vector<BlendingInfo>& ec_info =
        state_.frame_header.extra_channel_blending_info;
    ImageBundle& bg = *state_.reference_frames[info_.source].frame;
    bg_ = &bg;
    if (bg.xsize() == 0 && bg.ysize() == 0) {
      // there is no background, assume it to be all zeroes
      ImageBundle empty(&state_.metadata->m);
      Image3F color(image_xsize_, image_ysize_);
      ZeroFillImage(&color);
      empty.SetFromImage(std::move(color), frame_color_encoding);
      if (!ec_info.empty()) {
        std::vector<ImageF> ec;
        for (size_t i = 0; i < ec_info.size(); ++i) {
          ImageF eci(image_xsize_, image_ysize_);
          ZeroFillImage(&eci);
          ec.push_back(std::move(eci));
        }
        empty.SetExtraChannels(std::move(ec));
      }
      bg = std::move(empty);
    } else if (state_.reference_frames[info_.source].ib_is_in_xyb) {
      initialized_ = JXL_FAILURE(
          "Trying to blend XYB reference frame %i and non-XYB frame",
          info_.source);
      return;
    }

    if (bg.xsize() < image_xsize_ || bg.ysize() < image_ysize_ ||
        bg.origin.x0 != 0 || bg.origin.y0 != 0) {
      initialized_ = JXL_FAILURE("Trying to use a %" PRIuS "x%" PRIuS
                                 " crop as a background",
                                 bg.xsize(), bg.ysize());
      return;
    }
    if (state_.metadata->m.xyb_encoded) {
      if (!dec_state->output_encoding_info.color_encoding_is_original) {
        initialized_ = JXL_FAILURE("Blending in unsupported color space");
        return;
      }
    }

    blending_info_.resize(ec_info.size() + 1);
    auto make_blending = [&](const BlendingInfo& info, PatchBlending* pb) {
      pb->alpha_channel = info.alpha_channel;
      pb->clamp = info.clamp;
      switch (info.mode) {
        case BlendMode::kReplace: {
          pb->mode = PatchBlendMode::kReplace;
          break;
        }
        case BlendMode::kAdd: {
          pb->mode = PatchBlendMode::kAdd;
          break;
        }
        case BlendMode::kMul: {
          pb->mode = PatchBlendMode::kMul;
          break;
        }
        case BlendMode::kBlend: {
          pb->mode = PatchBlendMode::kBlendAbove;
          break;
        }
        case BlendMode::kAlphaWeightedAdd: {
          pb->mode = PatchBlendMode::kAlphaWeightedAddAbove;
          break;
        }
        default: {
          JXL_ABORT("Invalid blend mode");  // should have failed to decode
        }
      }
    };
    make_blending(info_, &blending_info_[0]);
    for (size_t i = 0; i < ec_info.size(); i++) {
      make_blending(ec_info[i], &blending_info_[1 + i]);
    }
  }

  Status IsInitialized() const override { return initialized_; }

  void ProcessRow(const RowInfo& input_rows, const RowInfo& output_rows,
                  size_t xextra, size_t xsize, size_t xpos, size_t ypos,
                  float* JXL_RESTRICT temp) const final {
    PROFILER_ZONE("Blend");
    JXL_ASSERT(initialized_);
    const FrameOrigin& frame_origin = state_.frame_header.frame_origin;
    ssize_t bg_xpos = frame_origin.x0 + static_cast<ssize_t>(xpos);
    ssize_t bg_ypos = frame_origin.y0 + static_cast<ssize_t>(ypos);
    int offset = 0;
    if (bg_xpos + static_cast<ssize_t>(xsize) <= 0 ||
        frame_origin.x0 >= static_cast<ssize_t>(image_xsize_) || bg_ypos < 0 ||
        bg_ypos >= static_cast<ssize_t>(image_ysize_)) {
      return;
    }
    if (bg_xpos < 0) {
      xpos -= bg_xpos;
      offset -= bg_xpos;
      xsize += bg_xpos;
      bg_xpos = 0;
    }
    if (bg_xpos + xsize > image_xsize_) {
      xsize =
          std::max<ssize_t>(0, static_cast<ssize_t>(image_xsize_) - bg_xpos);
    }
    std::vector<const float*> bg_row_ptrs_(input_rows.size());
    std::vector<float*> fg_row_ptrs_(input_rows.size());
    for (size_t c = 0; c < input_rows.size(); ++c) {
      bg_row_ptrs_[c] =
          (c < 3 ? bg_->color()->ConstPlaneRow(c, bg_ypos)
                 : bg_->extra_channels()[c - 3].ConstRow(bg_ypos)) +
          bg_xpos;
      fg_row_ptrs_[c] = GetInputRow(input_rows, c, offset);
    }
    PerformBlending(bg_row_ptrs_.data(), fg_row_ptrs_.data(),
                    fg_row_ptrs_.data(), 0, xsize, blending_info_[0],
                    blending_info_.data() + 1, *extra_channel_info_);
  }

  RenderPipelineChannelMode GetChannelMode(size_t c) const final {
    return RenderPipelineChannelMode::kInPlace;
  }

 private:
  const PassesSharedState& state_;
  BlendingInfo info_;
  ImageBundle* bg_;
  Status initialized_ = true;
  size_t image_xsize_;
  size_t image_ysize_;
  std::vector<PatchBlending> blending_info_;
  const std::vector<ExtraChannelInfo>* extra_channel_info_;
};

std::unique_ptr<RenderPipelineStage> GetBlendingStage(
    const PassesDecoderState* dec_state,
    const ColorEncoding& frame_color_encoding) {
  return jxl::make_unique<BlendingStage>(dec_state, frame_color_encoding);
}

// NOLINTNEXTLINE(google-readability-namespace-comments)
}  // namespace HWY_NAMESPACE
}  // namespace jxl
HWY_AFTER_NAMESPACE();

#if HWY_ONCE
namespace jxl {

HWY_EXPORT(GetBlendingStage);

std::unique_ptr<RenderPipelineStage> GetBlendingStage(
    const PassesDecoderState* dec_state,
    const ColorEncoding& frame_color_encoding) {
  return HWY_DYNAMIC_DISPATCH(GetBlendingStage)(dec_state,
                                                frame_color_encoding);
}

}  // namespace jxl
#endif
