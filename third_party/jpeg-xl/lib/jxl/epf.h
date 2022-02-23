// Copyright (c) the JPEG XL Project Authors. All rights reserved.
//
// Use of this source code is governed by a BSD-style
// license that can be found in the LICENSE file.

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
// All rects must have the same alignment module
// GroupBorderAssigner::kPaddingXRound pixels.
// `input_rect`, `output_rect` and `image_rect` must all have the same size.
// At least `lf.Padding()` pixels must be accessible and contain valid values
// outside of `image_rect` in `input`. Also, depending on the implementation,
// more pixels in the input up to a vector size boundary should be accessible
// but may contain uninitialized data.
//
// This function only prepares and returns the pipeline, to perform the
// filtering process it must be called on all row from -lf.Padding() to
// image_rect.ysize() + lf.Padding() .
//
// Note: if the output_rect x0 or x1 are not a multiple of kPaddingXRound more
// pixels with potentially uninitialized data will be written to the output left
// and right of the requested rect up to a multiple of kPaddingXRound pixels.
FilterPipeline* PrepareFilterPipeline(
    PassesDecoderState* dec_state, const Rect& image_rect, const Image3F& input,
    const Rect& input_rect, size_t image_ysize, size_t thread,
    Image3F* JXL_RESTRICT out, const Rect& output_rect);

}  // namespace jxl

#endif  // LIB_JXL_EPF_H_
