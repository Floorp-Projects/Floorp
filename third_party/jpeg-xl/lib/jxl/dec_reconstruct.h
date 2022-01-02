// Copyright (c) the JPEG XL Project Authors. All rights reserved.
//
// Use of this source code is governed by a BSD-style
// license that can be found in the LICENSE file.

#ifndef LIB_JXL_DEC_RECONSTRUCT_H_
#define LIB_JXL_DEC_RECONSTRUCT_H_

#include <stddef.h>

#include "lib/jxl/aux_out.h"
#include "lib/jxl/aux_out_fwd.h"
#include "lib/jxl/base/compiler_specific.h"
#include "lib/jxl/base/data_parallel.h"
#include "lib/jxl/base/status.h"
#include "lib/jxl/dec_cache.h"
#include "lib/jxl/frame_header.h"
#include "lib/jxl/image.h"
#include "lib/jxl/loop_filter.h"
#include "lib/jxl/quantizer.h"
#include "lib/jxl/splines.h"

namespace jxl {

// Finalizes the decoding of a frame by applying image features if necessary,
// doing color transforms (unless the frame header specifies
// `SaveBeforeColorTransform()`) and applying upsampling.
//
// Writes pixels in the appropriate colorspace to `idct`, shrinking it if
// necessary.
// `skip_blending` is necessary because the encoder butteraugli loop does not
// (yet) handle blending.
// TODO(veluca): remove the "force_fir" parameter, and call EPF directly in
// those use cases where this is needed.
Status FinalizeFrameDecoding(ImageBundle* JXL_RESTRICT decoded,
                             PassesDecoderState* dec_state, ThreadPool* pool,
                             bool force_fir, bool skip_blending, bool move_ec);

// Renders the `frame_rect` portion of the final image to `output_image`
// (unless the frame is upsampled - in which case, `frame_rect` is scaled
// accordingly). `input_rect` should have the same shape. `input_rect` always
// refers to the non-padded pixels. `frame_rect.x0()` is guaranteed to be a
// multiple of GroupBorderAssigner::kPaddingRoundX. `frame_rect.xsize()` is
// either a multiple of GroupBorderAssigner::kPaddingRoundX, or is such that
// `frame_rect.x0() + frame_rect.xsize() == frame_dim.xsize`. `input_image`
// may be mutated by adding padding. If `frame_rect` is on an image border, the
// input will be padded. Otherwise, appropriate padding must already be present.
Status FinalizeImageRect(
    Image3F* input_image, const Rect& input_rect,
    const std::vector<std::pair<ImageF*, Rect>>& extra_channels,
    PassesDecoderState* dec_state, size_t thread,
    ImageBundle* JXL_RESTRICT output_image, const Rect& frame_rect);

// Fills padding around `img:rect` in the x direction by mirroring. Padding is
// applied so that a full border of xpadding and ypadding is available, except
// if `image_rect` points to an area of the full image that touches the top or
// the bottom. It is expected that padding is already in place for inputs such
// that the corresponding image_rect is not at an image border.
void EnsurePaddingInPlace(Image3F* img, const Rect& rect,
                          const Rect& image_rect, size_t image_xsize,
                          size_t image_ysize, size_t xpadding, size_t ypadding);
void EnsurePaddingInPlace(ImageF* img, const Rect& rect, const Rect& image_rect,
                          size_t image_xsize, size_t image_ysize,
                          size_t xpadding, size_t ypadding);

// For DC in the API.
void UndoXYB(const Image3F& src, Image3F* dst,
             const OutputEncodingInfo& output_info, ThreadPool* pool);

}  // namespace jxl

#endif  // LIB_JXL_DEC_RECONSTRUCT_H_
