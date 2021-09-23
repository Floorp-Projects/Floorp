// Copyright (c) the JPEG XL Project Authors. All rights reserved.
//
// Use of this source code is governed by a BSD-style
// license that can be found in the LICENSE file.

#include "lib/jxl/blending.h"

#include "lib/jxl/alpha.h"
#include "lib/jxl/image_ops.h"

namespace jxl {

bool ImageBlender::NeedsBlending(PassesDecoderState* dec_state) {
  const PassesSharedState& state = *dec_state->shared;
  if (!(state.frame_header.frame_type == FrameType::kRegularFrame ||
        state.frame_header.frame_type == FrameType::kSkipProgressive)) {
    return false;
  }
  const auto& info = state.frame_header.blending_info;
  bool replace_all = (info.mode == BlendMode::kReplace);
  for (const auto& ec_i : state.frame_header.extra_channel_blending_info) {
    if (ec_i.mode != BlendMode::kReplace) {
      replace_all = false;
    }
  }
  // Replace the full frame: nothing to do.
  if (!state.frame_header.custom_size_or_origin && replace_all) {
    return false;
  }
  return true;
}

Status ImageBlender::PrepareBlending(PassesDecoderState* dec_state,
                                     FrameOrigin foreground_origin,
                                     size_t foreground_xsize,
                                     size_t foreground_ysize,
                                     const ColorEncoding& frame_color_encoding,
                                     ImageBundle* output) {
  const PassesSharedState& state = *dec_state->shared;
  info_ = state.frame_header.blending_info;
  const std::vector<jxl::ExtraChannelInfo>& extra_channels =
      state.metadata->m.extra_channel_info;

  ec_info_ = &state.frame_header.extra_channel_blending_info;

  size_t image_xsize = state.frame_header.nonserialized_metadata->xsize();
  size_t image_ysize = state.frame_header.nonserialized_metadata->ysize();

  // the rect in the canvas that needs to be updated
  cropbox_ = Rect(0, 0, image_xsize, image_ysize);
  // the rect of this frame that overlaps with the canvas
  overlap_ = cropbox_;
  o_ = foreground_origin;
  int x0 = (o_.x0 >= 0 ? o_.x0 : 0);
  int y0 = (o_.y0 >= 0 ? o_.y0 : 0);
  int xsize = foreground_xsize;
  if (o_.x0 < 0) xsize += o_.x0;
  int ysize = foreground_ysize;
  if (o_.y0 < 0) ysize += o_.y0;
  xsize = Clamp1(xsize, 0, (int)cropbox_.xsize() - x0);
  ysize = Clamp1(ysize, 0, (int)cropbox_.ysize() - y0);
  cropbox_ = Rect(x0, y0, xsize, ysize);
  x0 = (o_.x0 < 0 ? -o_.x0 : 0);
  y0 = (o_.y0 < 0 ? -o_.y0 : 0);
  overlap_ = Rect(x0, y0, xsize, ysize);

  // Image to write to.
  ImageBundle& bg = *state.reference_frames[info_.source].frame;
  if (bg.xsize() == 0 && bg.ysize() == 0) {
    // there is no background, assume it to be all zeroes
    ImageBundle empty(&state.metadata->m);
    Image3F color(image_xsize, image_ysize);
    ZeroFillImage(&color);
    empty.SetFromImage(std::move(color), frame_color_encoding);
    if (!extra_channels.empty()) {
      std::vector<ImageF> ec;
      for (size_t i = 0; i < extra_channels.size(); ++i) {
        ImageF eci(image_xsize, image_ysize);
        ZeroFillImage(&eci);
        ec.push_back(std::move(eci));
      }
      empty.SetExtraChannels(std::move(ec));
    }
    bg = std::move(empty);
  } else if (state.reference_frames[info_.source].ib_is_in_xyb) {
    return JXL_FAILURE(
        "Trying to blend XYB reference frame %i and non-XYB frame",
        info_.source);
  }

  if (bg.xsize() != image_xsize || bg.ysize() != image_ysize ||
      bg.origin.x0 != 0 || bg.origin.y0 != 0) {
    return JXL_FAILURE("Trying to use a %zux%zu crop as a background",
                       bg.xsize(), bg.ysize());
  }
  if (state.metadata->m.xyb_encoded) {
    if (!dec_state->output_encoding_info.color_encoding_is_original) {
      return JXL_FAILURE("Blending in unsupported color space");
    }
  }

  if (!overlap_.IsInside(Rect(0, 0, foreground_xsize, foreground_ysize))) {
    return JXL_FAILURE("Trying to use a %zux%zu crop as a foreground",
                       foreground_xsize, foreground_ysize);
  }

  if (!cropbox_.IsInside(bg)) {
    return JXL_FAILURE(
        "Trying blend %zux%zu to (%zu,%zu), but background is %zux%zu",
        cropbox_.xsize(), cropbox_.ysize(), cropbox_.x0(), cropbox_.y0(),
        bg.xsize(), bg.ysize());
  }

  dest_ = output;
  if (state.frame_header.CanBeReferenced() &&
      &bg == &state.reference_frames[state.frame_header.save_as_reference]
                  .storage) {
    *dest_ = std::move(bg);
  } else {
    *dest_ = bg.Copy();
  }

  for (size_t i = 0; i < ec_info_->size(); ++i) {
    const auto& eci = (*ec_info_)[i];
    if (eci.source != info_.source) {
      const auto& src = *state.reference_frames[eci.source].frame;
      if (src.xsize() == 0 && src.ysize() == 0) {
        ZeroFillImage(&dest_->extra_channels()[i]);
      } else {
        CopyImageTo(src.extra_channels()[i], &dest_->extra_channels()[i]);
      }
    }
  }

  return true;
}

ImageBlender::RectBlender ImageBlender::PrepareRect(
    const Rect& rect, const Image3F& foreground,
    const std::vector<ImageF>& extra_channels, const Rect& input_rect) const {
  JXL_DASSERT(rect.xsize() == input_rect.xsize());
  JXL_DASSERT(rect.ysize() == input_rect.ysize());
  JXL_DASSERT(input_rect.IsInside(foreground));

  RectBlender blender(false);
  blender.dest_ = dest_;

  blender.current_overlap_ = rect.Intersection(overlap_);
  if (blender.current_overlap_.xsize() == 0 ||
      blender.current_overlap_.ysize() == 0) {
    blender.done_ = true;
    return blender;
  }

  blender.current_cropbox_ =
      Rect(o_.x0 + blender.current_overlap_.x0(),
           o_.y0 + blender.current_overlap_.y0(),
           blender.current_overlap_.xsize(), blender.current_overlap_.ysize());

  // Turn current_overlap_ from being relative to the full foreground to being
  // relative to the rect or input_rect.
  blender.current_overlap_ =
      Rect(blender.current_overlap_.x0() - rect.x0(),
           blender.current_overlap_.y0() - rect.y0(),
           blender.current_overlap_.xsize(), blender.current_overlap_.ysize());

  // And this one is relative to the `foreground` subimage.
  const Rect input_overlap(blender.current_overlap_.x0() + input_rect.x0(),
                           blender.current_overlap_.y0() + input_rect.y0(),
                           blender.current_overlap_.xsize(),
                           blender.current_overlap_.ysize());

  blender.blending_info_.resize(extra_channels.size() + 1);
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
  make_blending(info_, &blender.blending_info_[0]);
  for (size_t i = 0; i < extra_channels.size(); i++) {
    make_blending((*ec_info_)[i], &blender.blending_info_[1 + i]);
  }

  Rect cropbox_row = blender.current_cropbox_.Line(0);
  Rect overlap_row = input_overlap.Line(0);
  const auto num_ptrs = 3 + extra_channels.size();
  blender.fg_ptrs_.reserve(num_ptrs);
  blender.fg_strides_.reserve(num_ptrs);
  blender.bg_ptrs_.reserve(num_ptrs);
  blender.bg_strides_.reserve(num_ptrs);
  for (size_t c = 0; c < 3; c++) {
    blender.fg_ptrs_.push_back(overlap_row.ConstPlaneRow(foreground, c, 0));
    blender.fg_strides_.push_back(foreground.PixelsPerRow());
    blender.bg_ptrs_.push_back(cropbox_row.PlaneRow(dest_->color(), c, 0));
    blender.bg_strides_.push_back(dest_->color()->PixelsPerRow());
  }
  for (size_t c = 0; c < extra_channels.size(); c++) {
    blender.fg_ptrs_.push_back(overlap_row.ConstRow(extra_channels[c], 0));
    blender.fg_strides_.push_back(extra_channels[c].PixelsPerRow());
    blender.bg_ptrs_.push_back(cropbox_row.Row(&dest_->extra_channels()[c], 0));
    blender.bg_strides_.push_back(dest_->extra_channels()[c].PixelsPerRow());
  }

  return blender;
}

Status PerformBlending(
    const float* const* bg, const float* const* fg, float* const* out,
    size_t xsize, const PatchBlending& color_blending,
    const PatchBlending* ec_blending,
    const std::vector<ExtraChannelInfo>& extra_channel_info) {
  bool has_alpha = false;
  size_t num_ec = extra_channel_info.size();
  for (size_t i = 0; i < num_ec; i++) {
    if (extra_channel_info[i].type == jxl::ExtraChannel::kAlpha) {
      has_alpha = true;
      break;
    }
  }
  ImageF tmp(xsize, 3 + num_ec);
  // Blend extra channels first so that we use the pre-blending alpha.
  for (size_t i = 0; i < num_ec; i++) {
    if (ec_blending[i].mode == PatchBlendMode::kAdd) {
      for (size_t x = 0; x < xsize; x++) {
        tmp.Row(3 + i)[x] = bg[3 + i][x] + fg[3 + i][x];
      }
    } else if (ec_blending[i].mode == PatchBlendMode::kBlendAbove) {
      size_t alpha = ec_blending[i].alpha_channel;
      bool is_premultiplied = extra_channel_info[alpha].alpha_associated;
      PerformAlphaBlending(bg[3 + i], bg[3 + alpha], fg[3 + i], fg[3 + alpha],
                           tmp.Row(3 + i), xsize, is_premultiplied,
                           ec_blending[i].clamp);
    } else if (ec_blending[i].mode == PatchBlendMode::kBlendBelow) {
      size_t alpha = ec_blending[i].alpha_channel;
      bool is_premultiplied = extra_channel_info[alpha].alpha_associated;
      PerformAlphaBlending(fg[3 + i], fg[3 + alpha], bg[3 + i], bg[3 + alpha],
                           tmp.Row(3 + i), xsize, is_premultiplied,
                           ec_blending[i].clamp);
    } else if (ec_blending[i].mode == PatchBlendMode::kAlphaWeightedAddAbove) {
      size_t alpha = ec_blending[i].alpha_channel;
      PerformAlphaWeightedAdd(bg[3 + i], fg[3 + i], fg[3 + alpha],
                              tmp.Row(3 + i), xsize, ec_blending[i].clamp);
    } else if (ec_blending[i].mode == PatchBlendMode::kAlphaWeightedAddBelow) {
      size_t alpha = ec_blending[i].alpha_channel;
      PerformAlphaWeightedAdd(fg[3 + i], bg[3 + i], bg[3 + alpha],
                              tmp.Row(3 + i), xsize, ec_blending[i].clamp);
    } else if (ec_blending[i].mode == PatchBlendMode::kMul) {
      PerformMulBlending(bg[3 + i], fg[3 + i], tmp.Row(3 + i), xsize,
                         ec_blending[i].clamp);
    } else if (ec_blending[i].mode == PatchBlendMode::kReplace) {
      memcpy(tmp.Row(3 + i), fg[3 + i], xsize * sizeof(**fg));
    } else if (ec_blending[i].mode == PatchBlendMode::kNone) {
      memcpy(tmp.Row(3 + i), bg[3 + i], xsize * sizeof(**fg));
    } else {
      JXL_ABORT("Unreachable");
    }
  }
  size_t alpha = color_blending.alpha_channel;

  if (color_blending.mode == PatchBlendMode::kAdd ||
      (color_blending.mode == PatchBlendMode::kAlphaWeightedAddAbove &&
       !has_alpha) ||
      (color_blending.mode == PatchBlendMode::kAlphaWeightedAddBelow &&
       !has_alpha)) {
    for (int p = 0; p < 3; p++) {
      float* out = tmp.Row(p);
      for (size_t x = 0; x < xsize; x++) {
        out[x] = bg[p][x] + fg[p][x];
      }
    }
  } else if (color_blending.mode == PatchBlendMode::kBlendAbove
             // blend without alpha is just replace
             && has_alpha) {
    bool is_premultiplied = extra_channel_info[alpha].alpha_associated;
    PerformAlphaBlending(
        {bg[0], bg[1], bg[2], bg[3 + alpha]},
        {fg[0], fg[1], fg[2], fg[3 + alpha]},
        {tmp.Row(0), tmp.Row(1), tmp.Row(2), tmp.Row(3 + alpha)}, xsize,
        is_premultiplied, color_blending.clamp);
  } else if (color_blending.mode == PatchBlendMode::kBlendBelow
             // blend without alpha is just replace
             && has_alpha) {
    bool is_premultiplied = extra_channel_info[alpha].alpha_associated;
    PerformAlphaBlending(
        {fg[0], fg[1], fg[2], fg[3 + alpha]},
        {bg[0], bg[1], bg[2], bg[3 + alpha]},
        {tmp.Row(0), tmp.Row(1), tmp.Row(2), tmp.Row(3 + alpha)}, xsize,
        is_premultiplied, color_blending.clamp);
  } else if (color_blending.mode == PatchBlendMode::kAlphaWeightedAddAbove) {
    JXL_DASSERT(has_alpha);
    for (size_t c = 0; c < 3; c++) {
      PerformAlphaWeightedAdd(bg[c], fg[c], fg[3 + alpha], tmp.Row(c), xsize,
                              color_blending.clamp);
    }
  } else if (color_blending.mode == PatchBlendMode::kAlphaWeightedAddBelow) {
    JXL_DASSERT(has_alpha);
    for (size_t c = 0; c < 3; c++) {
      PerformAlphaWeightedAdd(fg[c], bg[c], bg[3 + alpha], tmp.Row(c), xsize,
                              color_blending.clamp);
    }
  } else if (color_blending.mode == PatchBlendMode::kMul) {
    for (int p = 0; p < 3; p++) {
      PerformMulBlending(bg[p], fg[p], tmp.Row(p), xsize, color_blending.clamp);
    }
  } else if (color_blending.mode == PatchBlendMode::kReplace ||
             color_blending.mode == PatchBlendMode::kBlendAbove ||
             color_blending.mode == PatchBlendMode::kBlendBelow) {  // kReplace
    for (size_t p = 0; p < 3; p++) {
      memcpy(tmp.Row(p), fg[p], xsize * sizeof(**fg));
    }
  } else if (color_blending.mode == PatchBlendMode::kNone) {
    for (size_t p = 0; p < 3; p++) {
      memcpy(tmp.Row(p), bg[p], xsize * sizeof(**fg));
    }
  } else {
    JXL_ABORT("Unreachable");
  }
  for (size_t i = 0; i < 3 + num_ec; i++) {
    memcpy(out[i], tmp.Row(i), xsize * sizeof(**out));
  }
  return true;
}

Status ImageBlender::RectBlender::DoBlending(size_t y) {
  if (done_ || y < current_overlap_.y0() ||
      y >= current_overlap_.y0() + current_overlap_.ysize()) {
    return true;
  }
  y -= current_overlap_.y0();
  fg_row_ptrs_.resize(fg_ptrs_.size());
  bg_row_ptrs_.resize(bg_ptrs_.size());
  for (size_t c = 0; c < fg_row_ptrs_.size(); c++) {
    fg_row_ptrs_[c] = fg_ptrs_[c] + y * fg_strides_[c];
    bg_row_ptrs_[c] = bg_ptrs_[c] + y * bg_strides_[c];
  }
  return PerformBlending(bg_row_ptrs_.data(), fg_row_ptrs_.data(),
                         bg_row_ptrs_.data(), current_overlap_.xsize(),
                         blending_info_[0], blending_info_.data() + 1,
                         dest_->metadata()->extra_channel_info);
}

}  // namespace jxl
