// Copyright (c) the JPEG XL Project Authors. All rights reserved.
//
// Use of this source code is governed by a BSD-style
// license that can be found in the LICENSE file.

#include "lib/jxl/blending.h"

#include "lib/jxl/alpha.h"
#include "lib/jxl/base/printf_macros.h"
#include "lib/jxl/image_ops.h"

namespace jxl {

namespace {

// Given two rects A and B, returns a set of rects whose union is A \ B. This
// may require from 0 to 4 rects, one for each non-empty side of B. `storage`
// must have room to accommodate that many rects. The order is consistent when
// called successively with "parallel" rects.
//
// +----------------------+
// |         top          |
// +------+-------+-------+
// | left | inner | right |
// +------+-------+-------+
// |        bottom        |
// +----------------------+
Span<const Rect> SubtractRect(const Rect& outer, const Rect& inner,
                              Rect* storage) {
  size_t num_rects = 0;

  const Rect intersection = inner.Intersection(outer);
  if (intersection.xsize() == 0 && intersection.ysize() == 0) {
    storage[num_rects++] = outer;
    return Span<const Rect>(storage, num_rects);
  }

  // Left, same height as inner
  if (outer.x0() < inner.x0()) {
    storage[num_rects++] =
        Rect(outer.x0(), inner.y0(),
             std::min(outer.xsize(), inner.x0() - outer.x0()), inner.ysize());
  }

  // Right, same height as inner
  if (outer.x0() + outer.xsize() > inner.x0() + inner.xsize()) {
    storage[num_rects++] =
        Rect(inner.x0() + inner.xsize(), inner.y0(),
             std::min(outer.xsize(), outer.x0() + outer.xsize() -
                                         (inner.x0() + inner.xsize())),
             inner.ysize());
  }

  // Top, full width
  if (outer.y0() < inner.y0()) {
    storage[num_rects++] =
        Rect(outer.x0(), outer.y0(), outer.xsize(),
             std::min(outer.ysize(), inner.y0() - outer.y0()));
  }

  // Bottom, full width
  if (outer.y0() + outer.ysize() > inner.y0() + inner.ysize()) {
    storage[num_rects++] =
        Rect(outer.x0(), inner.y0() + inner.ysize(), outer.xsize(),
             std::min(outer.ysize(), outer.y0() + outer.ysize() -
                                         (inner.y0() + inner.ysize())));
  }

  return Span<const Rect>(storage, num_rects);
}

}  // namespace

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

Status ImageBlender::PrepareBlending(
    PassesDecoderState* dec_state, FrameOrigin foreground_origin,
    size_t foreground_xsize, size_t foreground_ysize,
    const std::vector<ExtraChannelInfo>* extra_channel_info,
    const ColorEncoding& frame_color_encoding, const Rect& frame_rect,
    Image3F* output, const Rect& output_rect,
    std::vector<ImageF>* output_extra_channels,
    std::vector<Rect> output_extra_channels_rects) {
  const PassesSharedState& state = *dec_state->shared;
  info_ = state.frame_header.blending_info;

  ec_info_ = &state.frame_header.extra_channel_blending_info;

  frame_rect_ = frame_rect;
  extra_channel_info_ = extra_channel_info;
  output_ = output;
  output_rect_ = output_rect;
  output_extra_channels_ = output_extra_channels;
  output_extra_channels_rects_ = std::move(output_extra_channels_rects);

  size_t image_xsize = state.frame_header.nonserialized_metadata->xsize();
  size_t image_ysize = state.frame_header.nonserialized_metadata->ysize();

  // the rect in the canvas that needs to be updated
  cropbox_ = frame_rect;
  // the rect of the foreground that overlaps with the canvas
  overlap_ = cropbox_;
  o_ = foreground_origin;
  o_.x0 -= frame_rect.x0();
  o_.y0 -= frame_rect.y0();
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
  bg_ = &bg;
  if (bg.xsize() == 0 && bg.ysize() == 0) {
    // there is no background, assume it to be all zeroes
    ImageBundle empty(&state.metadata->m);
    Image3F color(image_xsize, image_ysize);
    ZeroFillImage(&color);
    empty.SetFromImage(std::move(color), frame_color_encoding);
    if (!output_extra_channels_->empty()) {
      std::vector<ImageF> ec;
      for (size_t i = 0; i < output_extra_channels_->size(); ++i) {
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

  if (bg.xsize() < image_xsize || bg.ysize() < image_ysize ||
      bg.origin.x0 != 0 || bg.origin.y0 != 0) {
    return JXL_FAILURE("Trying to use a %" PRIuS "x%" PRIuS
                       " crop as a background",
                       bg.xsize(), bg.ysize());
  }
  if (state.metadata->m.xyb_encoded) {
    if (!dec_state->output_encoding_info.color_encoding_is_original) {
      return JXL_FAILURE("Blending in unsupported color space");
    }
  }

  if (!overlap_.IsInside(Rect(0, 0, foreground_xsize, foreground_ysize))) {
    return JXL_FAILURE("Trying to use a %" PRIuS "x%" PRIuS
                       " crop as a foreground",
                       foreground_xsize, foreground_ysize);
  }

  if (!cropbox_.IsInside(bg)) {
    return JXL_FAILURE("Trying blend %" PRIuS "x%" PRIuS " to (%" PRIuS
                       ",%" PRIuS "), but background is %" PRIuS "x%" PRIuS,
                       cropbox_.xsize(), cropbox_.ysize(), cropbox_.x0(),
                       cropbox_.y0(), bg.xsize(), bg.ysize());
  }

  Rect frame_rects_storage[4], output_rects_storage[4];
  Span<const Rect> frame_rects = SubtractRect(
      frame_rect, cropbox_.Translate(frame_rect.x0(), frame_rect.y0()),
      frame_rects_storage);
  Span<const Rect> output_rects = SubtractRect(
      output_rect, cropbox_.Translate(output_rect.x0(), output_rect.y0()),
      output_rects_storage);
  JXL_ASSERT(frame_rects.size() == output_rects.size());
  for (size_t i = 0; i < frame_rects.size(); ++i) {
    CopyImageTo(frame_rects[i], *bg.color(), output_rects[i], output);
  }
  for (size_t i = 0; i < ec_info_->size(); ++i) {
    const auto& eci = (*ec_info_)[i];
    const auto& src = *state.reference_frames[eci.source].frame;
    output_rects =
        SubtractRect(output_extra_channels_rects_[i],
                     cropbox_.Translate(output_extra_channels_rects_[i].x0(),
                                        output_extra_channels_rects_[i].y0()),
                     output_rects_storage);
    if (src.xsize() == 0 && src.ysize() == 0) {
      for (size_t j = 0; j < output_rects.size(); ++j) {
        ZeroFillPlane(&(*output_extra_channels_)[i], output_rects[j]);
      }
    } else {
      if (src.extra_channels()[i].xsize() < image_xsize ||
          src.extra_channels()[i].ysize() < image_ysize || src.origin.x0 != 0 ||
          src.origin.y0 != 0) {
        return JXL_FAILURE(
            "Invalid size %" PRIuS "x%" PRIuS
            " or origin %+d%+d for extra channel %" PRIuS
            " of "
            "reference frame %" PRIuS ", expected at least %" PRIuS "x%" PRIuS
            "+0+0",
            src.extra_channels()[i].xsize(), src.extra_channels()[i].ysize(),
            static_cast<int>(src.origin.x0), static_cast<int>(src.origin.y0), i,
            static_cast<size_t>(eci.source), image_xsize, image_ysize);
      }
      for (size_t j = 0; j < frame_rects.size(); ++j) {
        CopyImageTo(frame_rects[j], src.extra_channels()[i], output_rects[j],
                    &(*output_extra_channels_)[i]);
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
  blender.extra_channel_info_ = extra_channel_info_;

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
    blender.bg_ptrs_.push_back(
        cropbox_row.Translate(frame_rect_.x0(), frame_rect_.y0())
            .PlaneRow(bg_->color(), c, 0));
    blender.bg_strides_.push_back(bg_->color()->PixelsPerRow());
    blender.out_ptrs_.push_back(
        cropbox_row.Translate(output_rect_.x0(), output_rect_.y0())
            .PlaneRow(output_, c, 0));
    blender.out_strides_.push_back(output_->PixelsPerRow());
  }
  for (size_t c = 0; c < extra_channels.size(); c++) {
    blender.fg_ptrs_.push_back(overlap_row.ConstRow(extra_channels[c], 0));
    blender.fg_strides_.push_back(extra_channels[c].PixelsPerRow());
    blender.bg_ptrs_.push_back(
        cropbox_row.Translate(frame_rect_.x0(), frame_rect_.y0())
            .Row(&bg_->extra_channels()[c], 0));
    blender.bg_strides_.push_back(bg_->extra_channels()[c].PixelsPerRow());
    blender.out_ptrs_.push_back(
        cropbox_row
            .Translate(output_extra_channels_rects_[c].x0(),
                       output_extra_channels_rects_[c].y0())
            .Row(&(*output_extra_channels_)[c], 0));
    blender.out_strides_.push_back((*output_extra_channels_)[c].PixelsPerRow());
  }

  return blender;
}

void PerformBlending(const float* const* bg, const float* const* fg,
                     float* const* out, size_t x0, size_t xsize,
                     const PatchBlending& color_blending,
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
        tmp.Row(3 + i)[x] = bg[3 + i][x + x0] + fg[3 + i][x + x0];
      }
    } else if (ec_blending[i].mode == PatchBlendMode::kBlendAbove) {
      size_t alpha = ec_blending[i].alpha_channel;
      bool is_premultiplied = extra_channel_info[alpha].alpha_associated;
      PerformAlphaBlending(bg[3 + i] + x0, bg[3 + alpha] + x0, fg[3 + i] + x0,
                           fg[3 + alpha] + x0, tmp.Row(3 + i), xsize,
                           is_premultiplied, ec_blending[i].clamp);
    } else if (ec_blending[i].mode == PatchBlendMode::kBlendBelow) {
      size_t alpha = ec_blending[i].alpha_channel;
      bool is_premultiplied = extra_channel_info[alpha].alpha_associated;
      PerformAlphaBlending(fg[3 + i] + x0, fg[3 + alpha] + x0, bg[3 + i] + x0,
                           bg[3 + alpha] + x0, tmp.Row(3 + i), xsize,
                           is_premultiplied, ec_blending[i].clamp);
    } else if (ec_blending[i].mode == PatchBlendMode::kAlphaWeightedAddAbove) {
      size_t alpha = ec_blending[i].alpha_channel;
      PerformAlphaWeightedAdd(bg[3 + i] + x0, fg[3 + i] + x0,
                              fg[3 + alpha] + x0, tmp.Row(3 + i), xsize,
                              ec_blending[i].clamp);
    } else if (ec_blending[i].mode == PatchBlendMode::kAlphaWeightedAddBelow) {
      size_t alpha = ec_blending[i].alpha_channel;
      PerformAlphaWeightedAdd(fg[3 + i] + x0, bg[3 + i] + x0,
                              bg[3 + alpha] + x0, tmp.Row(3 + i), xsize,
                              ec_blending[i].clamp);
    } else if (ec_blending[i].mode == PatchBlendMode::kMul) {
      PerformMulBlending(bg[3 + i] + x0, fg[3 + i] + x0, tmp.Row(3 + i), xsize,
                         ec_blending[i].clamp);
    } else if (ec_blending[i].mode == PatchBlendMode::kReplace) {
      memcpy(tmp.Row(3 + i), fg[3 + i] + x0, xsize * sizeof(**fg));
    } else if (ec_blending[i].mode == PatchBlendMode::kNone) {
      memcpy(tmp.Row(3 + i), bg[3 + i] + x0, xsize * sizeof(**fg));
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
        out[x] = bg[p][x + x0] + fg[p][x + x0];
      }
    }
  } else if (color_blending.mode == PatchBlendMode::kBlendAbove
             // blend without alpha is just replace
             && has_alpha) {
    bool is_premultiplied = extra_channel_info[alpha].alpha_associated;
    PerformAlphaBlending(
        {bg[0] + x0, bg[1] + x0, bg[2] + x0, bg[3 + alpha] + x0},
        {fg[0] + x0, fg[1] + x0, fg[2] + x0, fg[3 + alpha] + x0},
        {tmp.Row(0), tmp.Row(1), tmp.Row(2), tmp.Row(3 + alpha)}, xsize,
        is_premultiplied, color_blending.clamp);
  } else if (color_blending.mode == PatchBlendMode::kBlendBelow
             // blend without alpha is just replace
             && has_alpha) {
    bool is_premultiplied = extra_channel_info[alpha].alpha_associated;
    PerformAlphaBlending(
        {fg[0] + x0, fg[1] + x0, fg[2] + x0, fg[3 + alpha] + x0},
        {bg[0] + x0, bg[1] + x0, bg[2] + x0, bg[3 + alpha] + x0},
        {tmp.Row(0), tmp.Row(1), tmp.Row(2), tmp.Row(3 + alpha)}, xsize,
        is_premultiplied, color_blending.clamp);
  } else if (color_blending.mode == PatchBlendMode::kAlphaWeightedAddAbove) {
    JXL_DASSERT(has_alpha);
    for (size_t c = 0; c < 3; c++) {
      PerformAlphaWeightedAdd(bg[c] + x0, fg[c] + x0, fg[3 + alpha] + x0,
                              tmp.Row(c), xsize, color_blending.clamp);
    }
  } else if (color_blending.mode == PatchBlendMode::kAlphaWeightedAddBelow) {
    JXL_DASSERT(has_alpha);
    for (size_t c = 0; c < 3; c++) {
      PerformAlphaWeightedAdd(fg[c] + x0, bg[c] + x0, bg[3 + alpha] + x0,
                              tmp.Row(c), xsize, color_blending.clamp);
    }
  } else if (color_blending.mode == PatchBlendMode::kMul) {
    for (int p = 0; p < 3; p++) {
      PerformMulBlending(bg[p] + x0, fg[p] + x0, tmp.Row(p), xsize,
                         color_blending.clamp);
    }
  } else if (color_blending.mode == PatchBlendMode::kReplace ||
             color_blending.mode == PatchBlendMode::kBlendAbove ||
             color_blending.mode == PatchBlendMode::kBlendBelow) {  // kReplace
    for (size_t p = 0; p < 3; p++) {
      memcpy(tmp.Row(p), fg[p] + x0, xsize * sizeof(**fg));
    }
  } else if (color_blending.mode == PatchBlendMode::kNone) {
    for (size_t p = 0; p < 3; p++) {
      memcpy(tmp.Row(p), bg[p] + x0, xsize * sizeof(**fg));
    }
  } else {
    JXL_ABORT("Unreachable");
  }
  for (size_t i = 0; i < 3 + num_ec; i++) {
    memcpy(out[i] + x0, tmp.Row(i), xsize * sizeof(**out));
  }
}

Status ImageBlender::RectBlender::DoBlending(size_t y) {
  if (done_ || y < current_overlap_.y0() ||
      y >= current_overlap_.y0() + current_overlap_.ysize()) {
    return true;
  }
  y -= current_overlap_.y0();
  fg_row_ptrs_.resize(fg_ptrs_.size());
  bg_row_ptrs_.resize(bg_ptrs_.size());
  out_row_ptrs_.resize(out_ptrs_.size());
  for (size_t c = 0; c < fg_row_ptrs_.size(); c++) {
    fg_row_ptrs_[c] = fg_ptrs_[c] + y * fg_strides_[c];
    bg_row_ptrs_[c] = bg_ptrs_[c] + y * bg_strides_[c];
    out_row_ptrs_[c] = out_ptrs_[c] + y * out_strides_[c];
  }
  PerformBlending(bg_row_ptrs_.data(), fg_row_ptrs_.data(),
                  out_row_ptrs_.data(), 0, current_overlap_.xsize(),
                  blending_info_[0], blending_info_.data() + 1,
                  *extra_channel_info_);
  return true;
}

}  // namespace jxl
