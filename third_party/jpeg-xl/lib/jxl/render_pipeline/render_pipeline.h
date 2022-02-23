// Copyright (c) the JPEG XL Project Authors. All rights reserved.
//
// Use of this source code is governed by a BSD-style
// license that can be found in the LICENSE file.

#ifndef LIB_JXL_RENDER_PIPELINE_RENDER_PIPELINE_H_
#define LIB_JXL_RENDER_PIPELINE_RENDER_PIPELINE_H_

#include <stdint.h>

#include "lib/jxl/filters.h"
#include "lib/jxl/render_pipeline/render_pipeline_stage.h"

namespace jxl {

// Interface to provide input to the rendering pipeline. When this object is
// destroyed, all the data in the provided ImageF's Rects must have been
// initialized.
class RenderPipelineInput {
 public:
  RenderPipelineInput(const RenderPipelineInput&) = delete;
  RenderPipelineInput(RenderPipelineInput&& other) noexcept {
    *this = std::move(other);
  }
  RenderPipelineInput& operator=(RenderPipelineInput&& other) noexcept {
    pipeline_ = other.pipeline_;
    group_id_ = other.group_id_;
    thread_id_ = other.thread_id_;
    buffers_ = std::move(other.buffers_);
    other.pipeline_ = nullptr;
    return *this;
  }

  RenderPipelineInput() = default;
  void Done();

  const std::pair<ImageF*, Rect>& GetBuffer(size_t c) const {
    JXL_ASSERT(c < buffers_.size());
    return buffers_[c];
  }

 private:
  RenderPipeline* pipeline_ = nullptr;
  size_t group_id_;
  size_t thread_id_;
  std::vector<std::pair<ImageF*, Rect>> buffers_;
  friend class RenderPipeline;
};

class RenderPipeline {
 public:
  class Builder {
   public:
    explicit Builder(size_t num_c, size_t num_passes)
        : num_c_(num_c), num_passes_(num_passes) {
      JXL_ASSERT(num_c > 0);
      JXL_ASSERT(num_passes > 0);
    }

    // Adds a stage to the pipeline. Must be called at least once; the last
    // added stage cannot have kInOut channels.
    void AddStage(std::unique_ptr<RenderPipelineStage> stage);

    // Enables using the simple (i.e. non-memory-efficient) implementation of
    // the pipeline.
    void UseSimpleImplementation() { use_simple_implementation_ = true; }

    // Marks the 3 last channels as being noise input channels, which use the
    // same size as color channels.
    void UsesNoise() { uses_noise_ = true; }

    // Finalizes setup of the pipeline. Shifts for all channels should be 0 at
    // this point.
    std::unique_ptr<RenderPipeline> Finalize(
        FrameDimensions frame_dimensions) &&;

   private:
    std::vector<std::unique_ptr<RenderPipelineStage>> stages_;
    size_t num_c_;
    size_t num_passes_;
    bool use_simple_implementation_ = false;
    bool uses_noise_ = false;
  };

  friend class Builder;

  virtual ~RenderPipeline() = default;

  // Allocates storage to run with `num` threads.
  void PrepareForThreads(size_t num);

  // Retrieves a buffer where input data should be stored by the callee. When
  // input has been provided for all buffers, the pipeline will complete its
  // processing. This method may be called multiple times concurrently from
  // different threads, provided that a different `thread_id` is given.
  RenderPipelineInput GetInputBuffers(size_t group_id, size_t thread_id);

  bool ReceivedAllInput() const {
    return *std::min_element(group_completed_passes_.begin(),
                             group_completed_passes_.end()) == num_passes_;
  }

 protected:
  std::vector<std::unique_ptr<RenderPipelineStage>> stages_;
  // Shifts for every channel at the input of each stage.
  std::vector<std::vector<std::pair<size_t, size_t>>> channel_shifts_;
  // Amount of (cumulative) padding required by each stage.
  std::vector<size_t> padding_;
  FrameDimensions frame_dimensions_;
  bool uses_noise_;

  std::vector<uint8_t> group_completed_passes_;

  // Indexed by thread_id
  std::vector<CacheAlignedUniquePtr> temp_buffers_;

  size_t num_passes_;

  friend class RenderPipelineInput;

 private:
  void InputReady(size_t group_id, size_t thread_id,
                  const std::vector<std::pair<ImageF*, Rect>>& buffers);

  virtual std::vector<std::pair<ImageF*, Rect>> PrepareBuffers(
      size_t group_id, size_t thread_id) = 0;

  virtual void ProcessBuffers(size_t group_id, size_t thread_id) = 0;

  // Note that this method may be called multiple times with different (or
  // equal) `num`.
  virtual void PrepareForThreadsInternal(size_t num) = 0;
};

}  // namespace jxl

#endif  // LIB_JXL_RENDER_PIPELINE_RENDER_PIPELINE_H_
