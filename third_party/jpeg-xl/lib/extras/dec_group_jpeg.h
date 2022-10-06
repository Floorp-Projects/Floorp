// Copyright (c) the JPEG XL Project Authors. All rights reserved.
//
// Use of this source code is governed by a BSD-style
// license that can be found in the LICENSE file.

#ifndef LIB_EXTRAS_DEC_GROUP_JPEG_H_
#define LIB_EXTRAS_DEC_GROUP_JPEG_H_

#include <stddef.h>
#include <stdint.h>

#include <vector>

#include "lib/jxl/base/status.h"
#include "lib/jxl/frame_header.h"
#include "lib/jxl/image.h"
#include "lib/jxl/render_pipeline/render_pipeline.h"

namespace jxl {
namespace extras {

Status DecodeGroupJpeg(const Image3S& coeffs, size_t group_idx,
                       const Rect block_rect, const YCbCrChromaSubsampling& cs,
                       const float* dequant_matrices,
                       float* JXL_RESTRICT group_dec_cache, size_t thread,
                       RenderPipelineInput& render_pipeline_input);

}  // namespace extras
}  // namespace jxl

#endif  // LIB_EXTRAS_DEC_GROUP_JPEG_H_
