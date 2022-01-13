// Copyright (c) the JPEG XL Project Authors. All rights reserved.
//
// Use of this source code is governed by a BSD-style
// license that can be found in the LICENSE file.

#include "lib/jxl/render_pipeline/low_memory_render_pipeline.h"

namespace jxl {
void LowMemoryRenderPipeline::PrepareForThreadsInternal(size_t num) {
  JXL_ABORT("Not implemented");
}

std::vector<std::pair<ImageF*, Rect>> LowMemoryRenderPipeline::PrepareBuffers(
    size_t group_id, size_t thread_id) {
  JXL_ABORT("Not implemented");
  return {};
}

void LowMemoryRenderPipeline::ProcessBuffers(size_t group_id,
                                             size_t thread_id) {
  JXL_ABORT("Not implemented");
}
}  // namespace jxl
