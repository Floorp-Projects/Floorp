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

#include "lib/jxl/blending.h"

#include "lib/jxl/alpha.h"
#include "lib/jxl/image_ops.h"

namespace jxl {

Status ImageBlender::PrepareBlending(PassesDecoderState* dec_state,
                                     ImageBundle* foreground,
                                     ImageBundle* output) {
  const PassesSharedState& state = *dec_state->shared;
  // No need to blend anything in this case.
  if (!(state.frame_header.frame_type == FrameType::kRegularFrame ||
        state.frame_header.frame_type == FrameType::kSkipProgressive)) {
    *output = std::move(*foreground);
    done_ = true;
    return true;
  }
  info_ = state.frame_header.blending_info;
  bool replace_all = (info_.mode == BlendMode::kReplace);
  // This value should be 0 if there is no alpha channel.
  first_alpha_ = 0;
  const std::vector<jxl::ExtraChannelInfo>& extra_channels =
      state.metadata->m.extra_channel_info;
  for (size_t i = 0; i < extra_channels.size(); i++) {
    if (extra_channels[i].type == jxl::ExtraChannel::kAlpha) {
      first_alpha_ = i;
      break;
    }
  }

  ec_info_ = &state.frame_header.extra_channel_blending_info;
  if (info_.mode != BlendMode::kReplace &&
      info_.alpha_channel != first_alpha_) {
    return JXL_FAILURE(
        "Blending using non-first alpha channel not yet implemented");
  }
  for (const auto& ec_i : *ec_info_) {
    if (ec_i.mode != BlendMode::kReplace) {
      replace_all = false;
    }
    if (info_.source != ec_i.source)
      return JXL_FAILURE("Blending from different sources not yet implemented");
  }

  // Replace the full frame: nothing to do.
  if (!state.frame_header.custom_size_or_origin && replace_all) {
    *output = std::move(*foreground);
    done_ = true;
    return true;
  }

  size_t image_xsize = state.frame_header.nonserialized_metadata->xsize();
  size_t image_ysize = state.frame_header.nonserialized_metadata->ysize();

  if ((dec_state->pre_color_transform_frame.xsize() != 0) &&
      ((image_xsize != foreground->xsize()) ||
       (image_ysize != foreground->ysize()))) {
    // Extra channels are going to be resized. Make a copy.
    if (foreground->HasExtraChannels()) {
      dec_state->pre_color_transform_ec.clear();
      for (const auto& ec : foreground->extra_channels()) {
        dec_state->pre_color_transform_ec.emplace_back(CopyImage(ec));
      }
    }
  }

  // the rect in the canvas that needs to be updated
  cropbox_ = Rect(0, 0, image_xsize, image_ysize);
  // the rect of this frame that overlaps with the canvas
  overlap_ = cropbox_;
  // Image to write to.
  if (state.frame_header.custom_size_or_origin) {
    o_ = foreground->origin;
    int x0 = (o_.x0 >= 0 ? o_.x0 : 0);
    int y0 = (o_.y0 >= 0 ? o_.y0 : 0);
    int xsize = foreground->xsize();
    if (o_.x0 < 0) xsize += o_.x0;
    int ysize = foreground->ysize();
    if (o_.y0 < 0) ysize += o_.y0;
    xsize = Clamp1(xsize, 0, (int)cropbox_.xsize() - x0);
    ysize = Clamp1(ysize, 0, (int)cropbox_.ysize() - y0);
    cropbox_ = Rect(x0, y0, xsize, ysize);
    x0 = (o_.x0 < 0 ? -o_.x0 : 0);
    y0 = (o_.y0 < 0 ? -o_.y0 : 0);
    overlap_ = Rect(x0, y0, xsize, ysize);
  }
  if (overlap_.xsize() == image_xsize && overlap_.ysize() == image_ysize &&
      replace_all) {
    // frame is larger than image and fully replaces it, this is OK, just need
    // to crop
    *output = foreground->Copy();
    output->RemoveColor();
    std::vector<ImageF>* ec = nullptr;
    size_t num_ec = 0;
    if (foreground->HasExtraChannels()) {
      num_ec = foreground->extra_channels().size();
      ec = &output->extra_channels();
      ec->clear();
    }
    Image3F croppedcolor(image_xsize, image_ysize);
    Rect crop(-foreground->origin.x0, -foreground->origin.y0, image_xsize,
              image_ysize);
    CopyImageTo(crop, *foreground->color(), &croppedcolor);
    output->SetFromImage(std::move(croppedcolor), foreground->c_current());
    for (size_t i = 0; i < num_ec; i++) {
      const auto& ec_meta = foreground->metadata()->extra_channel_info[i];
      if (ec_meta.dim_shift != 0) {
        return JXL_FAILURE(
            "Blending of downsampled extra channels is not yet implemented");
      }
      ImageF cropped_ec(image_xsize, image_ysize);
      CopyImageTo(crop, foreground->extra_channels()[i], &cropped_ec);
      ec->push_back(std::move(cropped_ec));
    }
    done_ = true;
    return true;
  }

  ImageBundle& bg = *state.reference_frames[info_.source].frame;
  if (bg.xsize() == 0 && bg.ysize() == 0) {
    // there is no background, assume it to be all zeroes
    ImageBundle empty(foreground->metadata());
    Image3F color(image_xsize, image_ysize);
    ZeroFillImage(&color);
    empty.SetFromImage(std::move(color), foreground->c_current());
    if (foreground->HasExtraChannels()) {
      std::vector<ImageF> ec;
      for (const auto& ec_meta : foreground->metadata()->extra_channel_info) {
        ImageF eci(ec_meta.Size(image_xsize), ec_meta.Size(image_ysize));
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

  if (!overlap_.IsInside(*foreground)) {
    return JXL_FAILURE("Trying to use a %zux%zu crop as a foreground",
                       foreground->xsize(), foreground->ysize());
  }

  if (!cropbox_.IsInside(bg)) {
    return JXL_FAILURE(
        "Trying blend %zux%zu to (%zu,%zu), but background is %zux%zu",
        cropbox_.xsize(), cropbox_.ysize(), cropbox_.x0(), cropbox_.y0(),
        bg.xsize(), bg.ysize());
  }

  if (foreground->HasExtraChannels()) {
    for (const auto& ec_meta : foreground->metadata()->extra_channel_info) {
      if (ec_meta.dim_shift != 0) {
        return JXL_FAILURE(
            "Blending of downsampled extra channels is not yet implemented");
      }
    }
    for (const auto& ec : foreground->extra_channels()) {
      if (!overlap_.IsInside(ec)) {
        return JXL_FAILURE("Trying to use a %zux%zu crop as a foreground",
                           foreground->xsize(), foreground->ysize());
      }
    }
  }

  dest_ = output;
  if (state.frame_header.CanBeReferenced() &&
      &bg == &state.reference_frames[state.frame_header.save_as_reference]
                  .storage) {
    *dest_ = std::move(bg);
  } else {
    *dest_ = bg.Copy();
  }

  return true;
}

ImageBlender::RectBlender ImageBlender::PrepareRect(
    const Rect& rect, const ImageBundle& foreground) const {
  if (done_) return RectBlender(true);
  RectBlender blender(false);
  blender.info_ = info_;
  blender.dest_ = dest_;
  blender.ec_info_ = ec_info_;
  blender.first_alpha_ = first_alpha_;

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
  Image3F cropped_foreground(blender.current_overlap_.xsize(),
                             blender.current_overlap_.ysize());
  CopyImageTo(blender.current_overlap_, foreground.color(),
              &cropped_foreground);
  blender.foreground_ = ImageBundle(dest_->metadata());
  blender.foreground_.SetFromImage(std::move(cropped_foreground),
                                   foreground.c_current());
  const auto& eci = foreground.metadata()->extra_channel_info;
  if (!eci.empty()) {
    std::vector<ImageF> ec;
    for (size_t i = 0; i < eci.size(); ++i) {
      ImageF ec_image(eci[i].Size(blender.current_overlap_.xsize()),
                      eci[i].Size(blender.current_overlap_.ysize()));
      CopyImageTo(blender.current_overlap_, foreground.extra_channels()[i],
                  &ec_image);
      ec.push_back(std::move(ec_image));
    }
    blender.foreground_.SetExtraChannels(std::move(ec));
  }

  // Turn current_overlap_ from being relative to the full foreground to being
  // relative to the rect.
  blender.current_overlap_ =
      Rect(blender.current_overlap_.x0() - rect.x0(),
           blender.current_overlap_.y0() - rect.y0(),
           blender.current_overlap_.xsize(), blender.current_overlap_.ysize());

  return blender;
}

Status ImageBlender::RectBlender::DoBlending(size_t y) const {
  if (done_ || y < current_overlap_.y0() ||
      y >= current_overlap_.y0() + current_overlap_.ysize()) {
    return true;
  }
  y -= current_overlap_.y0();
  Rect cropbox_row = current_cropbox_.Line(y);
  Rect overlap_row = Rect(0, y, current_overlap_.xsize(), 1);

  // Blend extra channels first so that we use the pre-blending alpha.
  for (size_t i = 0; i < ec_info_->size(); i++) {
    if (i == first_alpha_) continue;
    if ((*ec_info_)[i].mode == BlendMode::kAdd) {
      AddTo(overlap_row, foreground_.extra_channels()[i], cropbox_row,
            &dest_->extra_channels()[i]);
    } else if ((*ec_info_)[i].mode == BlendMode::kBlend) {
      if ((*ec_info_)[i].alpha_channel != first_alpha_)
        return JXL_FAILURE("Not implemented: blending using non-first alpha");
      bool is_premultiplied = foreground_.AlphaIsPremultiplied();
      const float* JXL_RESTRICT a1 =
          overlap_row.ConstRow(foreground_.alpha(), 0);
      float* JXL_RESTRICT p1 = overlap_row.Row(&dest_->extra_channels()[i], 0);
      const float* JXL_RESTRICT a = cropbox_row.ConstRow(*dest_->alpha(), 0);
      float* JXL_RESTRICT p = cropbox_row.Row(&dest_->extra_channels()[i], 0);
      PerformAlphaBlending(p, a, p1, a1, p, cropbox_row.xsize(),
                           is_premultiplied);
    } else if ((*ec_info_)[i].mode == BlendMode::kAlphaWeightedAdd) {
      if ((*ec_info_)[i].alpha_channel != first_alpha_)
        return JXL_FAILURE("Not implemented: blending using non-first alpha");
      const float* JXL_RESTRICT a1 =
          overlap_row.ConstRow(foreground_.alpha(), 0);
      float* JXL_RESTRICT p1 = overlap_row.Row(&dest_->extra_channels()[i], 0);
      float* JXL_RESTRICT p = cropbox_row.Row(&dest_->extra_channels()[i], 0);
      PerformAlphaWeightedAdd(p, p1, a1, p, cropbox_row.xsize());
    } else if ((*ec_info_)[i].mode == BlendMode::kMul) {
      if ((*ec_info_)[i].alpha_channel != first_alpha_)
        return JXL_FAILURE("Not implemented: blending using non-first alpha");
      float* JXL_RESTRICT p1 = overlap_row.Row(&dest_->extra_channels()[i], 0);
      float* JXL_RESTRICT p = cropbox_row.Row(&dest_->extra_channels()[i], 0);
      PerformMulBlending(p, p1, p, cropbox_row.xsize());
    } else if ((*ec_info_)[i].mode == BlendMode::kReplace) {
      CopyImageTo(overlap_row, foreground_.extra_channels()[i], cropbox_row,
                  &dest_->extra_channels()[i]);
    } else {
      return JXL_FAILURE("Blend mode not implemented for extra channel %zu", i);
    }
  }

  if (info_.mode == BlendMode::kAdd ||
      (info_.mode == BlendMode::kAlphaWeightedAdd && !foreground_.HasAlpha())) {
    for (int p = 0; p < 3; p++) {
      AddTo(overlap_row, foreground_.color().Plane(p), cropbox_row,
            &dest_->color()->Plane(p));
    }
    if (foreground_.HasAlpha()) {
      AddTo(overlap_row, foreground_.alpha(), cropbox_row, dest_->alpha());
    }
  } else if (info_.mode == BlendMode::kBlend
             // blend without alpha is just replace
             && foreground_.HasAlpha()) {
    bool is_premultiplied = foreground_.AlphaIsPremultiplied();
    // Foreground.
    const float* JXL_RESTRICT a1 = overlap_row.ConstRow(foreground_.alpha(), 0);
    const float* JXL_RESTRICT r1 =
        overlap_row.ConstRow(foreground_.color().Plane(0), 0);
    const float* JXL_RESTRICT g1 =
        overlap_row.ConstRow(foreground_.color().Plane(1), 0);
    const float* JXL_RESTRICT b1 =
        overlap_row.ConstRow(foreground_.color().Plane(2), 0);
    // Background & destination.
    float* JXL_RESTRICT a = cropbox_row.Row(dest_->alpha(), 0);
    float* JXL_RESTRICT r = cropbox_row.Row(&dest_->color()->Plane(0), 0);
    float* JXL_RESTRICT g = cropbox_row.Row(&dest_->color()->Plane(1), 0);
    float* JXL_RESTRICT b = cropbox_row.Row(&dest_->color()->Plane(2), 0);
    PerformAlphaBlending(/*bg=*/{r, g, b, a}, /*fg=*/{r1, g1, b1, a1},
                         /*out=*/{r, g, b, a}, cropbox_row.xsize(),
                         is_premultiplied);
  } else if (info_.mode == BlendMode::kAlphaWeightedAdd) {
    // Foreground.
    const float* JXL_RESTRICT a1 = overlap_row.ConstRow(foreground_.alpha(), 0);
    const float* JXL_RESTRICT r1 =
        overlap_row.ConstRow(foreground_.color().Plane(0), 0);
    const float* JXL_RESTRICT g1 =
        overlap_row.ConstRow(foreground_.color().Plane(1), 0);
    const float* JXL_RESTRICT b1 =
        overlap_row.ConstRow(foreground_.color().Plane(2), 0);
    // Background & destination.
    float* JXL_RESTRICT a = cropbox_row.Row(dest_->alpha(), 0);
    float* JXL_RESTRICT r = cropbox_row.Row(&dest_->color()->Plane(0), 0);
    float* JXL_RESTRICT g = cropbox_row.Row(&dest_->color()->Plane(1), 0);
    float* JXL_RESTRICT b = cropbox_row.Row(&dest_->color()->Plane(2), 0);
    PerformAlphaWeightedAdd(/*bg=*/{r, g, b, a}, /*fg=*/{r1, g1, b1, a1},
                            /*out=*/{r, g, b, a}, cropbox_row.xsize());
  } else if (info_.mode == BlendMode::kMul) {
    for (int p = 0; p < 3; p++) {
      // Foreground.
      const float* JXL_RESTRICT c1 =
          overlap_row.ConstRow(foreground_.color().Plane(p), 0);
      // Background & destination.
      float* JXL_RESTRICT c = cropbox_row.Row(&dest_->color()->Plane(p), 0);
      PerformMulBlending(c, c1, c, cropbox_row.xsize());
    }
    if (foreground_.HasAlpha()) {
      const float* JXL_RESTRICT a1 =
          overlap_row.ConstRow(foreground_.alpha(), 0);
      float* JXL_RESTRICT a = cropbox_row.Row(dest_->alpha(), 0);
      PerformMulBlending(a, a1, a, cropbox_row.xsize());
    }
  } else {  // kReplace
    CopyImageTo(overlap_row, foreground_.color(), cropbox_row, dest_->color());
    if (foreground_.HasAlpha()) {
      CopyImageTo(overlap_row, foreground_.alpha(), cropbox_row,
                  dest_->alpha());
    }
  }
  return true;
}

}  // namespace jxl
