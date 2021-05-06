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

#ifndef LIB_JXL_EPF_H_
#define LIB_JXL_EPF_H_

// Fast SIMD "in-loop" edge preserving filter (adaptive, nonlinear).

#include <stddef.h>

#include "lib/jxl/base/data_parallel.h"
#include "lib/jxl/base/status.h"
#include "lib/jxl/dec_cache.h"
#include "lib/jxl/filters.h"
#include "lib/jxl/passes_state.h"

namespace jxl {

// 4 * (sqrt(0.5)-1), so that Weight(sigma) = 0.5.
static constexpr float kInvSigmaNum = -1.1715728752538099024f;

// Fills the `state->filter_weights.sigma` image with the precomputed sigma
// values in the area inside `block_rect`. Accesses the AC strategy, quant field
// and epf_sharpness fields in the corresponding positions.
void ComputeSigma(const Rect& block_rect, PassesDecoderState* state);

// Applies Gaborish + EPF to the given `image_rect` part of the image (used to
// select the sigma values). Input pixels are taken from `input:input_rect`, and
// the filtering result is written to `out:output_rect`. `dec_state->sigma` must
// be padded with `kMaxFilterPadding/kBlockDim` values along the x axis.
// All rects must be aligned to a multiple of `kBlockDim` pixels.
// `input_rect`, `output_rect` and `image_rect` must all have the same size.
// At least `lf.Padding()` pixels must be accessible (and contain valid values)
// outside of `image_rect` in `input`.
// This function should only ever be called on full images. To do partial
// processing, use PrepareFilterPipeline directly.
void ApplyFilters(PassesDecoderState* dec_state, const Rect& image_rect,
                  const Image3F& input, const Rect& input_rect, size_t thread,
                  Image3F* JXL_RESTRICT out, const Rect& output_rect);

// Same as ApplyFilters, but only prepares the pipeline (which is returned and
// must be run by the caller on -lf.Padding() to image_rect.ysize() +
// lf.Padding()).
FilterPipeline* PrepareFilterPipeline(
    PassesDecoderState* dec_state, const Rect& image_rect, const Image3F& input,
    const Rect& input_rect, size_t image_ysize, size_t thread,
    Image3F* JXL_RESTRICT out, const Rect& output_rect);

}  // namespace jxl

#endif  // LIB_JXL_EPF_H_
