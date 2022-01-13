// Copyright (c) the JPEG XL Project Authors. All rights reserved.
//
// Use of this source code is governed by a BSD-style
// license that can be found in the LICENSE file.

#ifndef LIB_JXL_RENDER_PIPELINE_SIMPLE_RENDER_PIPELINE_H_
#define LIB_JXL_RENDER_PIPELINE_SIMPLE_RENDER_PIPELINE_H_

#include <stdint.h>

#include "lib/jxl/filters.h"
#include "lib/jxl/render_pipeline/render_pipeline.h"

namespace jxl {

// A RenderPipeline that is "obviously correct"; it may use potentially large
// amounts of memory and be slow. It is intended to be used mostly for testing
// purposes.
class SimpleRenderPipeline : public RenderPipeline {
  std::vector<std::pair<ImageF*, Rect>> PrepareBuffers(
      size_t group_id, size_t thread_id) override;

  void ProcessBuffers(size_t group_id, size_t thread_id) override;

  void RunPipeline();

  void PrepareForThreadsInternal(size_t num) override;

  // Full frame buffers. Both X and Y dimensions are padded by
  // kRenderPipelineXOffset.
  std::vector<ImageF> channel_data_;
};

}  // namespace jxl

#endif  // LIB_JXL_RENDER_PIPELINE_SIMPLE_RENDER_PIPELINE_H_
