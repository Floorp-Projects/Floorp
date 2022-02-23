// Copyright (c) the JPEG XL Project Authors. All rights reserved.
//
// Use of this source code is governed by a BSD-style
// license that can be found in the LICENSE file.

#include "lib/jxl/render_pipeline/simple_render_pipeline.h"

namespace jxl {
void SimpleRenderPipeline::PrepareForThreadsInternal(size_t num) {
  if (!channel_data_.empty()) {
    return;
  }
  auto ch_size = [](size_t frame_size, size_t shift) {
    return DivCeil(frame_size, 1 << shift) + kRenderPipelineXOffset * 2;
  };
  for (size_t c = 0; c < channel_shifts_[0].size(); c++) {
    bool is_color_c =
        c < 3 || (uses_noise_ && c >= channel_shifts_[0].size() - 3);
    channel_data_.push_back(
        ImageF(ch_size(frame_dimensions_.GetUpsampledXSize(is_color_c),
                       channel_shifts_[0][c].first),
               ch_size(frame_dimensions_.GetUpsampledYSize(is_color_c),
                       channel_shifts_[0][c].second)));
    msan::PoisonImage(channel_data_.back());
  }
}

std::vector<std::pair<ImageF*, Rect>> SimpleRenderPipeline::PrepareBuffers(
    size_t group_id, size_t thread_id) {
  std::vector<std::pair<ImageF*, Rect>> ret;
  size_t base_color_shift =
      CeilLog2Nonzero(frame_dimensions_.xsize_upsampled_padded /
                      frame_dimensions_.xsize_padded);
  for (size_t c = 0; c < channel_data_.size(); c++) {
    const size_t gx = group_id % frame_dimensions_.xsize_groups;
    const size_t gy = group_id / frame_dimensions_.xsize_groups;
    size_t xgroupdim = (frame_dimensions_.group_dim << base_color_shift) >>
                       channel_shifts_[0][c].first;
    size_t ygroupdim = (frame_dimensions_.group_dim << base_color_shift) >>
                       channel_shifts_[0][c].second;
    bool is_color_c =
        c < 3 || (uses_noise_ && c >= channel_shifts_[0].size() - 3);
    const Rect rect(kRenderPipelineXOffset + gx * xgroupdim,
                    kRenderPipelineXOffset + gy * ygroupdim, xgroupdim,
                    ygroupdim,
                    kRenderPipelineXOffset +
                        DivCeil(frame_dimensions_.GetUpsampledXSize(is_color_c),
                                1 << channel_shifts_[0][c].first),
                    kRenderPipelineXOffset +
                        DivCeil(frame_dimensions_.GetUpsampledYSize(is_color_c),
                                1 << channel_shifts_[0][c].second));
    ret.emplace_back(&channel_data_[c], rect);
  }
  return ret;
}

void SimpleRenderPipeline::ProcessBuffers(size_t group_id, size_t thread_id) {
  if (!ReceivedAllInput()) return;

  for (const auto& ch : channel_data_) {
    (void)ch;
    JXL_CHECK_IMAGE_INITIALIZED(
        ch, Rect(kRenderPipelineXOffset, kRenderPipelineXOffset,
                 ch.xsize() - 2 * kRenderPipelineXOffset,
                 ch.ysize() - 2 * kRenderPipelineXOffset));
  }
  for (size_t stage_id = 0; stage_id < stages_.size(); stage_id++) {
    const auto& stage = stages_[stage_id];
    // Prepare buffers for kInOut channels.
    std::vector<ImageF> new_channels(channel_data_.size());
    std::vector<ImageF*> output_channels(channel_data_.size());

    std::vector<std::pair<size_t, size_t>> input_sizes(channel_data_.size());
    for (size_t c = 0; c < channel_data_.size(); c++) {
      input_sizes[c] =
          std::make_pair(channel_data_[c].xsize() - kRenderPipelineXOffset * 2,
                         channel_data_[c].ysize() - kRenderPipelineXOffset * 2);
    }

    for (size_t c = 0; c < channel_data_.size(); c++) {
      if (stage->GetChannelMode(c) != RenderPipelineChannelMode::kInOut) {
        continue;
      }
      new_channels[c] =
          ImageF((input_sizes[c].first << stage->settings_.shift_x) +
                     kRenderPipelineXOffset * 2,
                 (input_sizes[c].second << stage->settings_.shift_y) +
                     kRenderPipelineXOffset * 2);
      output_channels[c] = &new_channels[c];
    }

    auto get_row = [&](size_t c, int64_t y) {
      return channel_data_[c].Row(kRenderPipelineXOffset + y) +
             kRenderPipelineXOffset;
    };

    // Add mirrored pixes to all kInOut channels.
    for (size_t c = 0; c < channel_data_.size(); c++) {
      if (stage->GetChannelMode(c) != RenderPipelineChannelMode::kInOut) {
        continue;
      }
      // Horizontal mirroring.
      for (size_t y = 0; y < input_sizes[c].second; y++) {
        float* row = get_row(c, y);
        for (size_t ix = 0; ix < stage->settings_.border_x; ix++) {
          *(row - ix - 1) = *(row + ix);
          *(row + ix + input_sizes[c].first) =
              *(row + input_sizes[c].first - ix - 1);
        }
      }
      // Vertical mirroring.
      for (int iy = 0; iy < static_cast<int>(stage->settings_.border_y); iy++) {
        memcpy(get_row(c, -iy - 1) - stage->settings_.border_x,
               get_row(c, iy) - stage->settings_.border_x,
               sizeof(float) *
                   (input_sizes[c].first + 2 * stage->settings_.border_x));
        memcpy(
            get_row(c, input_sizes[c].second + iy) - stage->settings_.border_x,
            get_row(c, input_sizes[c].second - iy - 1) -
                stage->settings_.border_x,
            sizeof(float) *
                (input_sizes[c].first + 2 * stage->settings_.border_x));
      }
    }

    size_t ysize = 0;
    size_t xsize = 0;
    for (size_t c = 0; c < channel_data_.size(); c++) {
      if (stage->GetChannelMode(c) == RenderPipelineChannelMode::kIgnored) {
        continue;
      }
      ysize = std::max(input_sizes[c].second, ysize);
      xsize = std::max(input_sizes[c].first, xsize);
    }

    JXL_ASSERT(ysize != 0);
    JXL_ASSERT(xsize != 0);

    RenderPipelineStage::RowInfo input_rows(channel_data_.size());
    RenderPipelineStage::RowInfo output_rows(channel_data_.size());

    // Run the pipeline.
    {
      int border_y = stage->settings_.border_y;
      for (size_t y = 0; y < ysize; y++) {
        // Prepare input rows.
        for (size_t c = 0; c < channel_data_.size(); c++) {
          input_rows[c].resize(2 * border_y + 1);
          for (int iy = -border_y; iy <= border_y; iy++) {
            input_rows[c][iy + border_y] =
                channel_data_[c].Row(y + kRenderPipelineXOffset + iy);
          }
        }
        // Prepare output rows.
        for (size_t c = 0; c < channel_data_.size(); c++) {
          if (!output_channels[c]) continue;
          output_rows[c].resize(1 << stage->settings_.shift_y);
          for (size_t iy = 0; iy < output_rows[c].size(); iy++) {
            output_rows[c][iy] = output_channels[c]->Row(
                (y << stage->settings_.shift_y) + iy + kRenderPipelineXOffset);
          }
        }
        stage->ProcessRow(
            input_rows, output_rows, /*xextra=*/0, xsize,
            /*xpos=*/0, y,
            reinterpret_cast<float*>(temp_buffers_[thread_id].get()));
      }
    }

    // Move new channels to current channels.
    for (size_t c = 0; c < channel_data_.size(); c++) {
      if (stage->GetChannelMode(c) != RenderPipelineChannelMode::kInOut) {
        continue;
      }
      channel_data_[c] = std::move(new_channels[c]);
    }
    for (const auto& ch : channel_data_) {
      (void)ch;
      JXL_CHECK_IMAGE_INITIALIZED(
          ch, Rect(kRenderPipelineXOffset, kRenderPipelineXOffset,
                   ch.xsize() - 2 * kRenderPipelineXOffset,
                   ch.ysize() - 2 * kRenderPipelineXOffset));
    }
  }
}
}  // namespace jxl
