// Copyright (c) the JPEG XL Project Authors. All rights reserved.
//
// Use of this source code is governed by a BSD-style
// license that can be found in the LICENSE file.

#ifndef LIB_JXL_RENDER_PIPELINE_STAGE_WRITE_H_
#define LIB_JXL_RENDER_PIPELINE_STAGE_WRITE_H_

#include <functional>

#include "lib/jxl/dec_cache.h"
#include "lib/jxl/image_bundle.h"
#include "lib/jxl/render_pipeline/render_pipeline_stage.h"

namespace jxl {

std::unique_ptr<RenderPipelineStage> GetWriteToImageBundleStage(
    ImageBundle* image_bundle, ColorEncoding color_encoding);

// Gets a stage to write color channels to an Image3F.
std::unique_ptr<RenderPipelineStage> GetWriteToImage3FStage(Image3F* image);

// Gets a stage to write to a uint8 buffer.
std::unique_ptr<RenderPipelineStage> GetWriteToU8Stage(uint8_t* rgb,
                                                       size_t stride,
                                                       size_t height, bool rgba,
                                                       bool has_alpha,
                                                       size_t alpha_c);

// Gets a stage to write to a pixel callback.
std::unique_ptr<RenderPipelineStage> GetWriteToPixelCallbackStage(
    const PixelCallback& pixel_callback, size_t width, size_t height, bool rgba,
    bool has_alpha, bool unpremul_alpha, size_t alpha_c);

}  // namespace jxl

#endif  // LIB_JXL_RENDER_PIPELINE_STAGE_WRITE_H_
