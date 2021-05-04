// Copyright (c) the JPEG XL Project
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//      http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#ifndef LIB_JXL_COMPRESSED_DC_H_
#define LIB_JXL_COMPRESSED_DC_H_

#include <stddef.h>
#include <stdint.h>

#include "lib/jxl/ac_context.h"
#include "lib/jxl/base/data_parallel.h"
#include "lib/jxl/frame_header.h"
#include "lib/jxl/image.h"
#include "lib/jxl/modular/modular_image.h"

// DC handling functions: encoding and decoding of DC to and from bitstream, and
// related function to initialize the per-group decoder cache.

namespace jxl {

// Smooth DC in already-smooth areas, to counteract banding.
void AdaptiveDCSmoothing(const float* dc_factors, Image3F* dc,
                         ThreadPool* pool);

void DequantDC(const Rect& r, Image3F* dc, ImageB* quant_dc, const Image& in,
               const float* dc_factors, float mul, const float* cfl_factors,
               YCbCrChromaSubsampling chroma_subsampling,
               const BlockCtxMap& bctx);

}  // namespace jxl

#endif  // LIB_JXL_COMPRESSED_DC_H_
