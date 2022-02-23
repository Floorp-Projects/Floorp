// Copyright (c) the JPEG XL Project Authors. All rights reserved.
//
// Use of this source code is governed by a BSD-style
// license that can be found in the LICENSE file.

#ifndef LIB_JXL_RENDER_PIPELINE_LOW_MEMORY_RENDER_PIPELINE_H_
#define LIB_JXL_RENDER_PIPELINE_LOW_MEMORY_RENDER_PIPELINE_H_

#include <stdint.h>

#include "lib/jxl/filters.h"
#include "lib/jxl/render_pipeline/render_pipeline.h"

namespace jxl {

// A multithreaded, low-memory rendering pipeline that only allocates a minimal
// amount of buffers.
class LowMemoryRenderPipeline : public RenderPipeline {
  std::vector<std::pair<ImageF*, Rect>> PrepareBuffers(
      size_t group_id, size_t thread_id) override;

  void PrepareForThreadsInternal(size_t num) override;

  void ProcessBuffers(size_t group_id, size_t thread_id) override;
};

}  // namespace jxl

#endif  // LIB_JXL_RENDER_PIPELINE_LOW_MEMORY_RENDER_PIPELINE_H_
