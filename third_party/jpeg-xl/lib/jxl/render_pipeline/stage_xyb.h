// Copyright (c) the JPEG XL Project Authors. All rights reserved.
//
// Use of this source code is governed by a BSD-style
// license that can be found in the LICENSE file.

#ifndef LIB_JXL_RENDER_PIPELINE_STAGE_XYB_H_
#define LIB_JXL_RENDER_PIPELINE_STAGE_XYB_H_
#include <math.h>
#include <stdint.h>
#include <stdio.h>

#include <algorithm>
#include <utility>
#include <vector>

#include "lib/jxl/dec_xyb.h"
#include "lib/jxl/render_pipeline/render_pipeline_stage.h"

namespace jxl {

// Converts the color channels from XYB to the specified output encoding.
std::unique_ptr<RenderPipelineStage> GetXYBStage(
    const OutputEncodingInfo& output_encoding_info);
}  // namespace jxl

#endif  // LIB_JXL_RENDER_PIPELINE_STAGE_XYB_H_
