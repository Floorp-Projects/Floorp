// Copyright (c) the JPEG XL Project Authors. All rights reserved.
//
// Use of this source code is governed by a BSD-style
// license that can be found in the LICENSE file.

#include "lib/jxl/render_pipeline/low_memory_render_pipeline.h"

#include <algorithm>
#include <queue>
#include <tuple>

#include "lib/jxl/aux_out.h"
#include "lib/jxl/base/arch_macros.h"

namespace jxl {
std::pair<size_t, size_t>
LowMemoryRenderPipeline::ColorDimensionsToChannelDimensions(
    std::pair<size_t, size_t> in, size_t c, size_t stage) const {
  std::pair<size_t, size_t> ret;
  std::pair<size_t, size_t> shift = channel_shifts_[stage][c];
  ret.first =
      ((in.first << base_color_shift_) + (1 << shift.first) - 1) >> shift.first;
  ret.second = ((in.second << base_color_shift_) + (1 << shift.second) - 1) >>
               shift.second;
  return ret;
}

std::pair<size_t, size_t> LowMemoryRenderPipeline::BorderToStore(
    size_t c) const {
  auto ret = ColorDimensionsToChannelDimensions(group_border_, c, 0);
  ret.first += padding_[0][c].first;
  ret.second += padding_[0][c].second;
  return ret;
}

void LowMemoryRenderPipeline::SaveBorders(size_t group_id, size_t c,
                                          const ImageF& in) {
  size_t gy = group_id / frame_dimensions_.xsize_groups;
  size_t gx = group_id % frame_dimensions_.xsize_groups;
  size_t hshift = channel_shifts_[0][c].first;
  size_t vshift = channel_shifts_[0][c].second;
  size_t x0 = gx * GroupInputXSize(c);
  size_t x1 = std::min((gx + 1) * GroupInputXSize(c),
                       DivCeil(frame_dimensions_.xsize_upsampled, 1 << hshift));
  size_t y0 = gy * GroupInputYSize(c);
  size_t y1 = std::min((gy + 1) * GroupInputYSize(c),
                       DivCeil(frame_dimensions_.ysize_upsampled, 1 << vshift));

  auto borders = BorderToStore(c);
  size_t borderx_write = borders.first;
  size_t bordery_write = borders.second;

  if (gy > 0) {
    Rect from(group_data_x_border_, group_data_y_border_, x1 - x0,
              bordery_write);
    Rect to(x0, (gy * 2 - 1) * bordery_write, x1 - x0, bordery_write);
    CopyImageTo(from, in, to, &borders_horizontal_[c]);
  }
  if (gy + 1 < frame_dimensions_.ysize_groups) {
    Rect from(group_data_x_border_,
              group_data_y_border_ + y1 - y0 - bordery_write, x1 - x0,
              bordery_write);
    Rect to(x0, (gy * 2) * bordery_write, x1 - x0, bordery_write);
    CopyImageTo(from, in, to, &borders_horizontal_[c]);
  }
  if (gx > 0) {
    Rect from(group_data_x_border_, group_data_y_border_, borderx_write,
              y1 - y0);
    Rect to((gx * 2 - 1) * borderx_write, y0, borderx_write, y1 - y0);
    CopyImageTo(from, in, to, &borders_vertical_[c]);
  }
  if (gx + 1 < frame_dimensions_.xsize_groups) {
    Rect from(group_data_x_border_ + x1 - x0 - borderx_write,
              group_data_y_border_, borderx_write, y1 - y0);
    Rect to((gx * 2) * borderx_write, y0, borderx_write, y1 - y0);
    CopyImageTo(from, in, to, &borders_vertical_[c]);
  }
}

void LowMemoryRenderPipeline::LoadBorders(size_t group_id, size_t c,
                                          const Rect& r, ImageF* out) {
  size_t gy = group_id / frame_dimensions_.xsize_groups;
  size_t gx = group_id % frame_dimensions_.xsize_groups;
  size_t hshift = channel_shifts_[0][c].first;
  size_t vshift = channel_shifts_[0][c].second;
  // Coordinates of the group in the image.
  size_t x0 = gx * GroupInputXSize(c);
  size_t x1 = std::min((gx + 1) * GroupInputXSize(c),
                       DivCeil(frame_dimensions_.xsize_upsampled, 1 << hshift));
  size_t y0 = gy * GroupInputYSize(c);
  size_t y1 = std::min((gy + 1) * GroupInputYSize(c),
                       DivCeil(frame_dimensions_.ysize_upsampled, 1 << vshift));

  size_t paddingx = padding_[0][c].first;
  size_t paddingy = padding_[0][c].second;

  auto borders = BorderToStore(c);
  size_t borderx_write = borders.first;
  size_t bordery_write = borders.second;

  // Limits of the area to copy from, in image coordinates.
  JXL_DASSERT(r.x0() == 0 || (r.x0() << base_color_shift_) >= paddingx);
  size_t x0src = DivCeil(r.x0() << base_color_shift_, 1 << hshift);
  if (x0src != 0) {
    x0src -= paddingx;
  }
  // r may be such that r.x1 (namely x0() + xsize()) is within paddingx of the
  // right side of the image, so we use min() here.
  size_t x1src =
      DivCeil((r.x0() + r.xsize()) << base_color_shift_, 1 << hshift);
  x1src = std::min(x1src + paddingx,
                   DivCeil(frame_dimensions_.xsize_upsampled, 1 << hshift));

  // Similar computation for y.
  JXL_DASSERT(r.y0() == 0 || (r.y0() << base_color_shift_) >= paddingy);
  size_t y0src = DivCeil(r.y0() << base_color_shift_, 1 << vshift);
  if (y0src != 0) {
    y0src -= paddingy;
  }
  size_t y1src =
      DivCeil((r.y0() + r.ysize()) << base_color_shift_, 1 << vshift);
  y1src = std::min(y1src + paddingy,
                   DivCeil(frame_dimensions_.ysize_upsampled, 1 << vshift));

  // Copy other groups' borders from the border storage.
  if (y0src < y0) {
    JXL_DASSERT(gy > 0);
    CopyImageTo(
        Rect(x0src, (gy * 2 - 2) * bordery_write, x1src - x0src, bordery_write),
        borders_horizontal_[c],
        Rect(group_data_x_border_ + x0src - x0,
             group_data_y_border_ - bordery_write, x1src - x0src,
             bordery_write),
        out);
  }
  if (y1src > y1) {
    // When copying the bottom border we must not be on the bottom groups.
    JXL_DASSERT(gy + 1 < frame_dimensions_.ysize_groups);
    CopyImageTo(
        Rect(x0src, (gy * 2 + 1) * bordery_write, x1src - x0src, bordery_write),
        borders_horizontal_[c],
        Rect(group_data_x_border_ + x0src - x0, group_data_y_border_ + y1 - y0,
             x1src - x0src, bordery_write),
        out);
  }
  if (x0src < x0) {
    JXL_DASSERT(gx > 0);
    CopyImageTo(
        Rect((gx * 2 - 2) * borderx_write, y0src, borderx_write, y1src - y0src),
        borders_vertical_[c],
        Rect(group_data_x_border_ - borderx_write,
             group_data_y_border_ + y0src - y0, borderx_write, y1src - y0src),
        out);
  }
  if (x1src > x1) {
    // When copying the right border we must not be on the rightmost groups.
    JXL_DASSERT(gx + 1 < frame_dimensions_.xsize_groups);
    CopyImageTo(
        Rect((gx * 2 + 1) * borderx_write, y0src, borderx_write, y1src - y0src),
        borders_vertical_[c],
        Rect(group_data_x_border_ + x1 - x0, group_data_y_border_ + y0src - y0,
             borderx_write, y1src - y0src),
        out);
  }
}

size_t LowMemoryRenderPipeline::GroupInputXSize(size_t c) const {
  return (frame_dimensions_.group_dim << base_color_shift_) >>
         channel_shifts_[0][c].first;
}

size_t LowMemoryRenderPipeline::GroupInputYSize(size_t c) const {
  return (frame_dimensions_.group_dim << base_color_shift_) >>
         channel_shifts_[0][c].second;
}

void LowMemoryRenderPipeline::EnsureBordersStorage() {
  const auto& shifts = channel_shifts_[0];
  if (borders_horizontal_.size() < shifts.size()) {
    borders_horizontal_.resize(shifts.size());
    borders_vertical_.resize(shifts.size());
  }
  for (size_t c = 0; c < shifts.size(); c++) {
    auto borders = BorderToStore(c);
    size_t borderx = borders.first;
    size_t bordery = borders.second;
    JXL_DASSERT(frame_dimensions_.xsize_groups > 0);
    size_t num_xborders = (frame_dimensions_.xsize_groups - 1) * 2;
    JXL_DASSERT(frame_dimensions_.ysize_groups > 0);
    size_t num_yborders = (frame_dimensions_.ysize_groups - 1) * 2;
    size_t downsampled_xsize =
        DivCeil(frame_dimensions_.xsize_upsampled_padded, 1 << shifts[c].first);
    size_t downsampled_ysize = DivCeil(frame_dimensions_.ysize_upsampled_padded,
                                       1 << shifts[c].second);
    Rect horizontal = Rect(0, 0, downsampled_xsize, bordery * num_yborders);
    if (!SameSize(horizontal, borders_horizontal_[c])) {
      borders_horizontal_[c] = ImageF(horizontal.xsize(), horizontal.ysize());
    }
    Rect vertical = Rect(0, 0, borderx * num_xborders, downsampled_ysize);
    if (!SameSize(vertical, borders_vertical_[c])) {
      borders_vertical_[c] = ImageF(vertical.xsize(), vertical.ysize());
    }
  }
}

void LowMemoryRenderPipeline::Init() {
  group_border_ = {0, 0};
  base_color_shift_ = CeilLog2Nonzero(frame_dimensions_.xsize_upsampled_padded /
                                      frame_dimensions_.xsize_padded);

  const auto& shifts = channel_shifts_[0];

  // Ensure that each channel has enough many border pixels.
  for (size_t c = 0; c < shifts.size(); c++) {
    group_border_.first =
        std::max(group_border_.first,
                 DivCeil(padding_[0][c].first << channel_shifts_[0][c].first,
                         1 << base_color_shift_));
    group_border_.second =
        std::max(group_border_.second,
                 DivCeil(padding_[0][c].second << channel_shifts_[0][c].second,
                         1 << base_color_shift_));
  }

  // Ensure that all channels have an integer number of border pixels in the
  // input.
  for (size_t c = 0; c < shifts.size(); c++) {
    if (channel_shifts_[0][c].first >= base_color_shift_) {
      group_border_.first =
          RoundUpTo(group_border_.first,
                    1 << (channel_shifts_[0][c].first - base_color_shift_));
    }
    if (channel_shifts_[0][c].second >= base_color_shift_) {
      group_border_.second =
          RoundUpTo(group_border_.second,
                    1 << (channel_shifts_[0][c].second - base_color_shift_));
    }
  }
  // Ensure that the X border on color channels is a multiple of kBlockDim or
  // the vector size (required for EPF stages). Vectors on ARM NEON are never
  // wider than 4 floats, so rounding to multiples of 4 is enough.
#if JXL_ARCH_ARM
  constexpr size_t kGroupXAlign = 4;
#else
  constexpr size_t kGroupXAlign = 16;
#endif
  group_border_.first = RoundUpTo(group_border_.first, kGroupXAlign);
  // Allocate borders in group images that are just enough for storing the
  // borders to be copied in, plus any rounding to ensure alignment.
  std::pair<size_t, size_t> max_border = {0, 0};
  for (size_t c = 0; c < shifts.size(); c++) {
    max_border.first = std::max(BorderToStore(c).first, max_border.first);
    max_border.second = std::max(BorderToStore(c).second, max_border.second);
  }
  group_data_x_border_ = RoundUpTo(max_border.first, kGroupXAlign);
  group_data_y_border_ = max_border.second;

  EnsureBordersStorage();
  group_border_assigner_.Init(frame_dimensions_);

  for (first_trailing_stage_ = stages_.size(); first_trailing_stage_ > 0;
       first_trailing_stage_--) {
    bool has_inout_c = false;
    for (size_t c = 0; c < shifts.size(); c++) {
      if (stages_[first_trailing_stage_ - 1]->GetChannelMode(c) ==
          RenderPipelineChannelMode::kInOut) {
        has_inout_c = true;
      }
    }
    if (has_inout_c) {
      break;
    }
  }

  first_image_dim_stage_ = stages_.size();
  for (size_t i = 0; i < stages_.size(); i++) {
    std::vector<std::pair<size_t, size_t>> input_sizes(shifts.size());
    for (size_t c = 0; c < shifts.size(); c++) {
      input_sizes[c] =
          std::make_pair(DivCeil(frame_dimensions_.xsize_upsampled,
                                 1 << channel_shifts_[i][c].first),
                         DivCeil(frame_dimensions_.ysize_upsampled,
                                 1 << channel_shifts_[i][c].second));
    }
    stages_[i]->SetInputSizes(input_sizes);
    if (stages_[i]->SwitchToImageDimensions()) {
      // We don't allow kInOut after switching to image dimensions.
      JXL_ASSERT(i >= first_trailing_stage_);
      first_image_dim_stage_ = i + 1;
      stages_[i]->GetImageDimensions(&full_image_xsize_, &full_image_ysize_,
                                     &frame_origin_);
      break;
    }
  }
  for (size_t i = first_image_dim_stage_; i < stages_.size(); i++) {
    if (stages_[i]->SwitchToImageDimensions()) {
      JXL_ABORT("Cannot switch to image dimensions multiple times");
    }
    std::vector<std::pair<size_t, size_t>> input_sizes(shifts.size());
    for (size_t c = 0; c < shifts.size(); c++) {
      input_sizes[c] = {full_image_xsize_, full_image_ysize_};
    }
    stages_[i]->SetInputSizes(input_sizes);
  }

  anyc_.resize(stages_.size());
  for (size_t i = 0; i < stages_.size(); i++) {
    for (size_t c = 0; c < shifts.size(); c++) {
      if (stages_[i]->GetChannelMode(c) !=
          RenderPipelineChannelMode::kIgnored) {
        anyc_[i] = c;
      }
    }
  }

  stage_input_for_channel_ = std::vector<std::vector<int32_t>>(
      stages_.size(), std::vector<int32_t>(shifts.size()));
  for (size_t c = 0; c < shifts.size(); c++) {
    int input = -1;
    for (size_t i = 0; i < stages_.size(); i++) {
      stage_input_for_channel_[i][c] = input;
      if (stages_[i]->GetChannelMode(c) == RenderPipelineChannelMode::kInOut) {
        input = i;
      }
    }
  }

  image_rect_.resize(stages_.size());
  for (size_t i = 0; i < stages_.size(); i++) {
    size_t x1 = DivCeil(frame_dimensions_.xsize_upsampled,
                        1 << channel_shifts_[i][anyc_[i]].first);
    size_t y1 = DivCeil(frame_dimensions_.ysize_upsampled,
                        1 << channel_shifts_[i][anyc_[i]].second);
    image_rect_[i] = Rect(0, 0, x1, y1);
  }

  virtual_ypadding_for_output_.resize(stages_.size());
  xpadding_for_output_.resize(stages_.size());
  for (size_t c = 0; c < shifts.size(); c++) {
    int ypad = 0;
    int xpad = 0;
    for (size_t i = stages_.size(); i-- > 0;) {
      if (stages_[i]->GetChannelMode(c) !=
          RenderPipelineChannelMode::kIgnored) {
        virtual_ypadding_for_output_[i] =
            std::max(ypad, virtual_ypadding_for_output_[i]);
        xpadding_for_output_[i] = std::max(xpad, xpadding_for_output_[i]);
      }
      if (stages_[i]->GetChannelMode(c) == RenderPipelineChannelMode::kInOut) {
        ypad = (DivCeil(ypad, 1 << channel_shifts_[i][c].second) +
                stages_[i]->settings_.border_y)
               << channel_shifts_[i][c].second;
        xpad = DivCeil(xpad, 1 << stages_[i]->settings_.shift_x) +
               stages_[i]->settings_.border_x;
      }
    }
  }
}

void LowMemoryRenderPipeline::PrepareForThreadsInternal(size_t num,
                                                        bool use_group_ids) {
  const auto& shifts = channel_shifts_[0];

  use_group_ids_ = use_group_ids;
  size_t num_buffers = use_group_ids_ ? frame_dimensions_.num_groups : num;
  for (size_t t = group_data_.size(); t < num_buffers; t++) {
    group_data_.emplace_back();
    group_data_[t].resize(shifts.size());
    for (size_t c = 0; c < shifts.size(); c++) {
      group_data_[t][c] = ImageF(GroupInputXSize(c) + group_data_x_border_ * 2,
                                 GroupInputYSize(c) + group_data_y_border_ * 2);
    }
  }
  // TODO(veluca): avoid reallocating buffers if not needed.
  stage_data_.resize(num);
  size_t upsampling = 1u << base_color_shift_;
  size_t group_dim = frame_dimensions_.group_dim * upsampling;
  size_t padding =
      2 * group_data_x_border_ * upsampling +  // maximum size of a rect
      2 * kRenderPipelineXOffset;              // extra padding for processing
  size_t stage_buffer_xsize = group_dim + padding;
  for (size_t t = 0; t < num; t++) {
    stage_data_[t].resize(shifts.size());
    for (size_t c = 0; c < shifts.size(); c++) {
      stage_data_[t][c].resize(stages_.size());
      size_t next_y_border = 0;
      for (size_t i = stages_.size(); i-- > 0;) {
        if (stages_[i]->GetChannelMode(c) ==
            RenderPipelineChannelMode::kInOut) {
          size_t stage_buffer_ysize =
              2 * next_y_border + (1 << stages_[i]->settings_.shift_y);
          stage_buffer_ysize = 1 << CeilLog2Nonzero(stage_buffer_ysize);
          next_y_border = stages_[i]->settings_.border_y;
          stage_data_[t][c][i] = ImageF(stage_buffer_xsize, stage_buffer_ysize);
        }
      }
    }
  }
  if (first_image_dim_stage_ != stages_.size()) {
    out_of_frame_data_.resize(num);
    size_t left_padding = std::max<ssize_t>(0, frame_origin_.x0);
    size_t middle_padding = group_dim;
    ssize_t last_x =
        frame_origin_.x0 + std::min(frame_dimensions_.xsize_groups * group_dim,
                                    frame_dimensions_.xsize_upsampled);
    last_x = Clamp1<ssize_t>(last_x, 0, full_image_xsize_);
    size_t right_padding = full_image_xsize_ - last_x;
    size_t out_of_frame_xsize =
        padding +
        std::max(left_padding, std::max(middle_padding, right_padding));
    for (size_t t = 0; t < num; t++) {
      out_of_frame_data_[t] = ImageF(out_of_frame_xsize, shifts.size());
    }
  }
}

std::vector<std::pair<ImageF*, Rect>> LowMemoryRenderPipeline::PrepareBuffers(
    size_t group_id, size_t thread_id) {
  std::vector<std::pair<ImageF*, Rect>> ret(channel_shifts_[0].size());
  const size_t gx = group_id % frame_dimensions_.xsize_groups;
  const size_t gy = group_id / frame_dimensions_.xsize_groups;
  for (size_t c = 0; c < channel_shifts_[0].size(); c++) {
    ret[c].first = &group_data_[use_group_ids_ ? group_id : thread_id][c];
    ret[c].second = Rect(group_data_x_border_, group_data_y_border_,
                         GroupInputXSize(c), GroupInputYSize(c),
                         DivCeil(frame_dimensions_.xsize_upsampled,
                                 1 << channel_shifts_[0][c].first) -
                             gx * GroupInputXSize(c) + group_data_x_border_,
                         DivCeil(frame_dimensions_.ysize_upsampled,
                                 1 << channel_shifts_[0][c].second) -
                             gy * GroupInputYSize(c) + group_data_y_border_);
  }
  return ret;
}

namespace {

JXL_INLINE int GetMirroredY(int y, ssize_t group_y0, ssize_t image_ysize) {
  if (group_y0 == 0 && (y < 0 || y + group_y0 >= image_ysize)) {
    return Mirror(y, image_ysize);
  }
  if (y + group_y0 >= image_ysize) {
    // Here we know that the one mirroring step is sufficient.
    return 2 * image_ysize - (y + group_y0) - 1 - group_y0;
  }
  return y;
}

JXL_INLINE void ApplyXMirroring(float* row, ssize_t borderx, ssize_t group_x0,
                                ssize_t group_xsize, ssize_t image_xsize) {
  if (image_xsize <= borderx) {
    if (group_x0 == 0) {
      for (ssize_t ix = 0; ix < borderx; ix++) {
        row[kRenderPipelineXOffset - ix - 1] =
            row[kRenderPipelineXOffset + Mirror(-ix - 1, image_xsize)];
      }
    }
    if (group_xsize + borderx + group_x0 >= image_xsize) {
      for (ssize_t ix = 0; ix < borderx; ix++) {
        row[kRenderPipelineXOffset + image_xsize + ix - group_x0] =
            row[kRenderPipelineXOffset + Mirror(image_xsize + ix, image_xsize) -
                group_x0];
      }
    }
  } else {
    // Here we know that the one mirroring step is sufficient.
    if (group_x0 == 0) {
      for (ssize_t ix = 0; ix < borderx; ix++) {
        row[kRenderPipelineXOffset - ix - 1] = row[kRenderPipelineXOffset + ix];
      }
    }
    if (group_xsize + borderx + group_x0 >= image_xsize) {
      for (ssize_t ix = 0; ix < borderx; ix++) {
        row[kRenderPipelineXOffset + image_xsize - group_x0 + ix] =
            row[kRenderPipelineXOffset + image_xsize - group_x0 - ix - 1];
      }
    }
  }
}

}  // namespace

void LowMemoryRenderPipeline::RenderRect(size_t thread_id,
                                         std::vector<ImageF>& input_data,
                                         Rect data_max_color_channel_rect,
                                         Rect image_max_color_channel_rect) {
  // For each stage, the rect corresponding to the image area currently being
  // processed.
  std::vector<Rect> group_rect;
  group_rect.resize(stages_.size());
  for (size_t i = 0; i < stages_.size(); i++) {
    size_t x0 = (image_max_color_channel_rect.x0() << base_color_shift_) >>
                channel_shifts_[i][anyc_[i]].first;
    size_t x1 = DivCeil(std::min(frame_dimensions_.xsize_upsampled,
                                 (image_max_color_channel_rect.x0() +
                                  image_max_color_channel_rect.xsize())
                                     << base_color_shift_),
                        1 << channel_shifts_[i][anyc_[i]].first);
    size_t y0 = (image_max_color_channel_rect.y0() << base_color_shift_) >>
                channel_shifts_[i][anyc_[i]].second;
    size_t y1 = DivCeil(std::min(frame_dimensions_.ysize_upsampled,
                                 (image_max_color_channel_rect.y0() +
                                  image_max_color_channel_rect.ysize())
                                     << base_color_shift_),
                        1 << channel_shifts_[i][anyc_[i]].second);
    group_rect[i] = Rect(x0, y0, x1 - x0, y1 - y0);
  }

  struct RowInfo {
    float* base_ptr;
    int ymod_minus_1;
    size_t stride;
  };

  std::vector<std::vector<RowInfo>> rows(
      stages_.size() + 1, std::vector<RowInfo>(input_data.size()));

  for (size_t i = 0; i < stages_.size(); i++) {
    for (size_t c = 0; c < input_data.size(); c++) {
      if (stages_[i]->GetChannelMode(c) == RenderPipelineChannelMode::kInOut) {
        rows[i + 1][c].ymod_minus_1 = stage_data_[thread_id][c][i].ysize() - 1;
        rows[i + 1][c].base_ptr = stage_data_[thread_id][c][i].Row(0);
        rows[i + 1][c].stride = stage_data_[thread_id][c][i].PixelsPerRow();
      }
    }
  }

  for (size_t c = 0; c < input_data.size(); c++) {
    int xoff = int(data_max_color_channel_rect.x0()) -
               int(LowMemoryRenderPipeline::group_data_x_border_);
    xoff = xoff * (1 << base_color_shift_) >> channel_shifts_[0][c].first;
    xoff += LowMemoryRenderPipeline::group_data_x_border_;
    int yoff = int(data_max_color_channel_rect.y0()) -
               int(LowMemoryRenderPipeline::group_data_y_border_);
    yoff = yoff * (1 << base_color_shift_) >> channel_shifts_[0][c].second;
    yoff += LowMemoryRenderPipeline::group_data_y_border_;
    rows[0][c].base_ptr =
        input_data[c].Row(yoff) + xoff - kRenderPipelineXOffset;
    rows[0][c].stride = input_data[c].PixelsPerRow();
    rows[0][c].ymod_minus_1 = -1;
  }

  auto get_row_buffer = [&](int stage, int y, size_t c) {
    const RowInfo& info = rows[stage + 1][c];
    return info.base_ptr + ssize_t(info.stride) * (y & info.ymod_minus_1);
  };

  std::vector<RenderPipelineStage::RowInfo> input_rows(first_trailing_stage_ +
                                                       1);
  for (size_t i = 0; i < first_trailing_stage_; i++) {
    input_rows[i].resize(input_data.size());
  }
  input_rows[first_trailing_stage_].resize(input_data.size(),
                                           std::vector<float*>(1));

  // Maximum possible shift is 3.
  RenderPipelineStage::RowInfo output_rows(input_data.size(),
                                           std::vector<float*>(8));

  // We pretend that every stage has a vertical shift of 0, i.e. it is as tall
  // as the final image.
  // We call each such row a "virtual" row, because it may or may not correspond
  // to an actual row of the current processing stage; actual processing happens
  // when vy % (1<<vshift) == 0.

  int num_extra_rows = *std::max_element(virtual_ypadding_for_output_.begin(),
                                         virtual_ypadding_for_output_.end());

  for (int vy = -num_extra_rows;
       vy < int(group_rect.back().ysize()) + num_extra_rows; vy++) {
    for (size_t i = 0; i < first_trailing_stage_; i++) {
      int virtual_y_offset = num_extra_rows - virtual_ypadding_for_output_[i];
      int stage_vy = vy - virtual_y_offset;

      if (stage_vy % (1 << channel_shifts_[i][anyc_[i]].second) != 0) {
        continue;
      }

      if (stage_vy < -virtual_ypadding_for_output_[i]) {
        continue;
      }

      ssize_t bordery = stages_[i]->settings_.border_y;
      size_t shifty = stages_[i]->settings_.shift_y;
      int y = stage_vy >> channel_shifts_[i][anyc_[i]].second;

      ssize_t image_y = ssize_t(group_rect[i].y0()) + y;
      // Do not produce rows in out-of-bounds areas.
      if (image_y < 0 || image_y >= ssize_t(image_rect_[i].ysize())) {
        continue;
      }

      // Get the actual input rows and potentially apply mirroring.
      for (size_t c = 0; c < input_data.size(); c++) {
        RenderPipelineChannelMode mode = stages_[i]->GetChannelMode(c);
        if (mode == RenderPipelineChannelMode::kIgnored) {
          continue;
        }
        auto make_row = [&](ssize_t iy) {
          size_t mirrored_y = GetMirroredY(y + iy - bordery, group_rect[i].y0(),
                                           image_rect_[i].ysize());
          input_rows[i][c][iy] =
              get_row_buffer(stage_input_for_channel_[i][c], mirrored_y, c);
          ApplyXMirroring(input_rows[i][c][iy], stages_[i]->settings_.border_x,
                          group_rect[i].x0(), group_rect[i].xsize(),
                          image_rect_[i].xsize());
        };
        // If we already have rows from a previous iteration, we can just shift
        // the rows by 1 and insert the new one.
        if (input_rows[i][c].size() == 2 * size_t(bordery) + 1) {
          for (ssize_t iy = 0; iy < 2 * bordery; iy++) {
            input_rows[i][c][iy] = input_rows[i][c][iy + 1];
          }
          make_row(bordery * 2);
        } else {
          input_rows[i][c].resize(2 * bordery + 1);
          for (ssize_t iy = 0; iy < 2 * bordery + 1; iy++) {
            make_row(iy);
          }
        }

        // If necessary, get the output buffers.
        if (mode == RenderPipelineChannelMode::kInOut) {
          for (size_t iy = 0; iy < (1u << shifty); iy++) {
            output_rows[c][iy] = get_row_buffer(i, y * (1 << shifty) + iy, c);
          }
        }
      }
      // Produce output rows.
      stages_[i]->ProcessRow(input_rows[i], output_rows,
                             xpadding_for_output_[i], group_rect[i].xsize(),
                             group_rect[i].x0(), group_rect[i].y0() + y,
                             thread_id);
    }

    // Process trailing stages, i.e. the final set of non-kInOut stages; they
    // all have the same input buffer and no need to use any mirroring.

    int y = vy - num_extra_rows;
    if (y < 0 || y >= ssize_t(frame_dimensions_.ysize_upsampled)) continue;

    for (size_t c = 0; c < input_data.size(); c++) {
      input_rows[first_trailing_stage_][c][0] = get_row_buffer(
          stage_input_for_channel_[first_trailing_stage_][c], y, c);
    }

    for (size_t i = first_trailing_stage_; i < first_image_dim_stage_; i++) {
      stages_[i]->ProcessRow(input_rows[first_trailing_stage_], output_rows,
                             /*xextra=*/0, group_rect[i].xsize(),
                             group_rect[i].x0(), group_rect[i].y0() + y,
                             thread_id);
    }

    if (first_image_dim_stage_ == stages_.size()) continue;

    ssize_t full_image_y =
        y + frame_origin_.y0 + group_rect[first_image_dim_stage_].y0();
    if (full_image_y < 0 || full_image_y >= ssize_t(full_image_ysize_)) {
      continue;
    }

    ssize_t full_image_x0 =
        frame_origin_.x0 + group_rect[first_image_dim_stage_].x0();

    if (full_image_x0 < 0) {
      // Skip pixels.
      for (size_t c = 0; c < input_data.size(); c++) {
        input_rows[first_trailing_stage_][c][0] -= full_image_x0;
      }
      full_image_x0 = 0;
    }
    ssize_t full_image_x1 = frame_origin_.x0 +
                            group_rect[first_image_dim_stage_].x0() +
                            group_rect[first_image_dim_stage_].xsize();
    full_image_x1 = std::min<ssize_t>(full_image_x1, full_image_xsize_);

    if (full_image_x1 <= full_image_x0) continue;

    for (size_t i = first_image_dim_stage_; i < stages_.size(); i++) {
      stages_[i]->ProcessRow(input_rows[first_trailing_stage_], output_rows,
                             /*xextra=*/0, full_image_x1 - full_image_x0,
                             full_image_x0, full_image_y, thread_id);
    }
  }
}

void LowMemoryRenderPipeline::RenderPadding(size_t thread_id, Rect rect) {
  if (rect.xsize() == 0) return;
  size_t numc = channel_shifts_[0].size();
  RenderPipelineStage::RowInfo input_rows(numc, std::vector<float*>(1));
  RenderPipelineStage::RowInfo output_rows;

  for (size_t c = 0; c < numc; c++) {
    input_rows[c][0] = out_of_frame_data_[thread_id].Row(c);
  }

  for (size_t y = 0; y < rect.ysize(); y++) {
    stages_[first_image_dim_stage_ - 1]->ProcessPaddingRow(
        input_rows, rect.xsize(), rect.x0(), rect.y0() + y);
    for (size_t i = first_image_dim_stage_; i < stages_.size(); i++) {
      stages_[i]->ProcessRow(input_rows, output_rows,
                             /*xextra=*/0, rect.xsize(), rect.x0(),
                             rect.y0() + y, thread_id);
    }
  }
}

void LowMemoryRenderPipeline::ProcessBuffers(size_t group_id,
                                             size_t thread_id) {
  std::vector<ImageF>& input_data =
      group_data_[use_group_ids_ ? group_id : thread_id];

  // Copy the group borders to the border storage.
  for (size_t c = 0; c < input_data.size(); c++) {
    SaveBorders(group_id, c, input_data[c]);
  }

  size_t gy = group_id / frame_dimensions_.xsize_groups;
  size_t gx = group_id % frame_dimensions_.xsize_groups;

  if (first_image_dim_stage_ != stages_.size()) {
    size_t group_dim = frame_dimensions_.group_dim << base_color_shift_;
    RectT<ssize_t> group_rect(gx * group_dim, gy * group_dim, group_dim,
                              group_dim);
    RectT<ssize_t> image_rect(0, 0, frame_dimensions_.xsize_upsampled,
                              frame_dimensions_.ysize_upsampled);
    RectT<ssize_t> full_image_rect(0, 0, full_image_xsize_, full_image_ysize_);
    group_rect = group_rect.Translate(frame_origin_.x0, frame_origin_.y0);
    image_rect = image_rect.Translate(frame_origin_.x0, frame_origin_.y0);
    group_rect =
        group_rect.Intersection(image_rect).Intersection(full_image_rect);
    size_t x0 = group_rect.x0();
    size_t y0 = group_rect.y0();
    size_t x1 = group_rect.x1();
    size_t y1 = group_rect.y1();

    // Do not render padding if group is empty; if group is empty x0, y0 might
    // have arbitrary values (from frame_origin).
    if (group_rect.xsize() > 0 && group_rect.ysize() > 0) {
      if (gx == 0 && gy == 0) {
        RenderPadding(thread_id, Rect(0, 0, x0, y0));
      }
      if (gy == 0) {
        RenderPadding(thread_id, Rect(x0, 0, x1 - x0, y0));
      }
      if (gx == 0) {
        RenderPadding(thread_id, Rect(0, y0, x0, y1 - y0));
      }
      if (gx == 0 && gy + 1 == frame_dimensions_.ysize_groups) {
        RenderPadding(thread_id, Rect(0, y1, x0, full_image_ysize_ - y1));
      }
      if (gy + 1 == frame_dimensions_.ysize_groups) {
        RenderPadding(thread_id, Rect(x0, y1, x1 - x0, full_image_ysize_ - y1));
      }
      if (gy == 0 && gx + 1 == frame_dimensions_.xsize_groups) {
        RenderPadding(thread_id, Rect(x1, 0, full_image_xsize_ - x1, y0));
      }
      if (gx + 1 == frame_dimensions_.xsize_groups) {
        RenderPadding(thread_id, Rect(x1, y0, full_image_xsize_ - x1, y1 - y0));
      }
      if (gy + 1 == frame_dimensions_.ysize_groups &&
          gx + 1 == frame_dimensions_.xsize_groups) {
        RenderPadding(thread_id, Rect(x1, y1, full_image_xsize_ - x1,
                                      full_image_ysize_ - y1));
      }
    }
  }

  Rect ready_rects[GroupBorderAssigner::kMaxToFinalize];
  size_t num_ready_rects = 0;
  group_border_assigner_.GroupDone(group_id, group_border_.first,
                                   group_border_.second, ready_rects,
                                   &num_ready_rects);
  for (size_t i = 0; i < num_ready_rects; i++) {
    const Rect& image_max_color_channel_rect = ready_rects[i];
    for (size_t c = 0; c < input_data.size(); c++) {
      LoadBorders(group_id, c, image_max_color_channel_rect, &input_data[c]);
    }
    Rect data_max_color_channel_rect(
        group_data_x_border_ + image_max_color_channel_rect.x0() -
            gx * frame_dimensions_.group_dim,
        group_data_y_border_ + image_max_color_channel_rect.y0() -
            gy * frame_dimensions_.group_dim,
        image_max_color_channel_rect.xsize(),
        image_max_color_channel_rect.ysize());
    RenderRect(thread_id, input_data, data_max_color_channel_rect,
               image_max_color_channel_rect);
  }
}
}  // namespace jxl
