// Copyright (c) the JPEG XL Project Authors. All rights reserved.
//
// Use of this source code is governed by a BSD-style
// license that can be found in the LICENSE file.

#include "lib/jxl/render_pipeline/render_pipeline.h"

#include <algorithm>

#include "lib/jxl/render_pipeline/low_memory_render_pipeline.h"
#include "lib/jxl/render_pipeline/simple_render_pipeline.h"
#include "lib/jxl/sanitizers.h"

namespace jxl {

void RenderPipeline::Builder::AddStage(
    std::unique_ptr<RenderPipelineStage> stage) {
  stages_.push_back(std::move(stage));
}

std::unique_ptr<RenderPipeline> RenderPipeline::Builder::Finalize(
    FrameDimensions frame_dimensions) && {
#if JXL_ENABLE_ASSERT
  // Check that the last stage is not an kInOut stage for any channel, and that
  // there is at least one stage.
  JXL_ASSERT(!stages_.empty());
  for (size_t c = 0; c < num_c_; c++) {
    JXL_ASSERT(stages_.back()->GetChannelMode(c) !=
               RenderPipelineChannelMode::kInOut);
  }
#endif

  std::unique_ptr<RenderPipeline> res;
  if (use_simple_implementation_) {
    res = jxl::make_unique<SimpleRenderPipeline>();
  } else {
    res = jxl::make_unique<LowMemoryRenderPipeline>();
  }
  res->padding_.resize(stages_.size());
  std::vector<size_t> channel_border(num_c_);
  for (size_t i = stages_.size(); i > 0; i--) {
    const auto& stage = stages_[i - 1];
    for (size_t c = 0; c < num_c_; c++) {
      if (stage->GetChannelMode(c) == RenderPipelineChannelMode::kInOut) {
        channel_border[c] = DivCeil(
            channel_border[c],
            1 << std::max(stage->settings_.shift_x, stage->settings_.shift_y));
        channel_border[c] +=
            std::max(stage->settings_.border_x, stage->settings_.border_y);
      }
    }
    res->padding_[i - 1] =
        *std::max_element(channel_border.begin(), channel_border.end());
  }
  res->frame_dimensions_ = frame_dimensions;
  res->uses_noise_ = uses_noise_;
  res->group_completed_passes_.resize(frame_dimensions.num_groups);
  res->num_passes_ = num_passes_;
  res->channel_shifts_.resize(stages_.size());
  res->channel_shifts_[0].resize(num_c_);
  for (size_t i = 1; i < stages_.size(); i++) {
    auto& stage = stages_[i - 1];
    for (size_t c = 0; c < num_c_; c++) {
      if (stage->GetChannelMode(c) == RenderPipelineChannelMode::kInOut) {
        res->channel_shifts_[0][c].first += stage->settings_.shift_x;
        res->channel_shifts_[0][c].second += stage->settings_.shift_y;
      }
    }
  }
  for (size_t i = 1; i < stages_.size(); i++) {
    auto& stage = stages_[i - 1];
    res->channel_shifts_[i].resize(num_c_);
    for (size_t c = 0; c < num_c_; c++) {
      if (stage->GetChannelMode(c) == RenderPipelineChannelMode::kInOut) {
        res->channel_shifts_[i][c].first =
            res->channel_shifts_[i - 1][c].first - stage->settings_.shift_x;
        res->channel_shifts_[i][c].second =
            res->channel_shifts_[i - 1][c].second - stage->settings_.shift_y;
      }
    }
  }
  res->stages_ = std::move(stages_);
  return res;
}

RenderPipelineInput RenderPipeline::GetInputBuffers(size_t group_id,
                                                    size_t thread_id) {
  RenderPipelineInput ret;
  JXL_DASSERT(group_id < group_completed_passes_.size());
  ret.group_id_ = group_id;
  ret.thread_id_ = thread_id;
  ret.pipeline_ = this;
  ret.buffers_ = PrepareBuffers(group_id, thread_id);
  return ret;
}

void RenderPipeline::InputReady(
    size_t group_id, size_t thread_id,
    const std::vector<std::pair<ImageF*, Rect>>& buffers) {
  JXL_DASSERT(group_id < group_completed_passes_.size());
  group_completed_passes_[group_id]++;
  for (const auto& buf : buffers) {
    (void)buf;
    JXL_CHECK_IMAGE_INITIALIZED(*buf.first, buf.second);
  }

  ProcessBuffers(group_id, thread_id);
}

void RenderPipeline::PrepareForThreads(size_t num) {
  temp_buffers_.resize(num);
  for (auto& thread_buffer : temp_buffers_) {
    size_t size = 0;
    for (const auto& stage : stages_) {
      size = std::max(stage->settings_.temp_buffer_size, size);
    }
    if (size) {
      thread_buffer = AllocateArray(sizeof(float) * size);
    }
  }
  PrepareForThreadsInternal(num);
}

void RenderPipelineInput::Done() {
  JXL_ASSERT(pipeline_);
  pipeline_->InputReady(group_id_, thread_id_, buffers_);
}

}  // namespace jxl
