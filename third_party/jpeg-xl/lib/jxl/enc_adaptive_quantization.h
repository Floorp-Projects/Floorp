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

#ifndef LIB_JXL_ENC_ADAPTIVE_QUANTIZATION_H_
#define LIB_JXL_ENC_ADAPTIVE_QUANTIZATION_H_

#include <stddef.h>

#include "lib/jxl/ac_strategy.h"
#include "lib/jxl/aux_out.h"
#include "lib/jxl/base/data_parallel.h"
#include "lib/jxl/chroma_from_luma.h"
#include "lib/jxl/common.h"
#include "lib/jxl/enc_cache.h"
#include "lib/jxl/enc_params.h"
#include "lib/jxl/frame_header.h"
#include "lib/jxl/image.h"
#include "lib/jxl/image_bundle.h"
#include "lib/jxl/loop_filter.h"
#include "lib/jxl/quant_weights.h"
#include "lib/jxl/quantizer.h"
#include "lib/jxl/splines.h"

// Heuristics to find a good quantizer for a given image. InitialQuantField
// produces a quantization field (i.e. relative quantization amounts for each
// block) out of an opsin-space image. `InitialQuantField` uses heuristics,
// `FindBestQuantizer` (in non-fast mode) will run multiple encoding-decoding
// steps and try to improve the given quant field.

namespace jxl {

// Computes the decoded image for a given set of compression parameters. Mainly
// used in the FindBestQuantization loops and in some tests.
// TODO(veluca): this doesn't seem the best possible file for this function.
ImageBundle RoundtripImage(const Image3F& opsin, PassesEncoderState* enc_state,
                           ThreadPool* pool);

// Returns an image subsampled by kBlockDim in each direction. If the value
// at pixel (x,y) in the returned image is greater than 1.0, it means that
// more fine-grained quantization should be used in the corresponding block
// of the input image, while a value less than 1.0 indicates that less
// fine-grained quantization should be enough. Returns a mask, too, which
// can later be used to make better decisions about ac strategy.
ImageF InitialQuantField(float butteraugli_target, const Image3F& opsin,
                         const FrameDimensions& frame_dim, ThreadPool* pool,
                         float rescale, ImageF* initial_quant_mask);

float InitialQuantDC(float butteraugli_target);

void AdjustQuantField(const AcStrategyImage& ac_strategy, const Rect& rect,
                      ImageF* quant_field);

// Returns a quantizer that uses an adjusted version of the provided
// quant_field. Also computes the dequant_map corresponding to the given
// dequant_float_map and chosen quantization levels.
// `linear` is only used in Kitten mode or slower.
void FindBestQuantizer(const ImageBundle* linear, const Image3F& opsin,
                       PassesEncoderState* enc_state, ThreadPool* pool,
                       AuxOut* aux_out, double rescale = 1.0);

}  // namespace jxl

#endif  // LIB_JXL_ENC_ADAPTIVE_QUANTIZATION_H_
