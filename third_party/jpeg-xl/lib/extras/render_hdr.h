// Copyright (c) the JPEG XL Project Authors. All rights reserved.
//
// Use of this source code is governed by a BSD-style
// license that can be found in the LICENSE file.

#ifndef LIB_EXTRAS_RENDER_HDR_H_
#define LIB_EXTRAS_RENDER_HDR_H_

#include "lib/jxl/codec_in_out.h"

namespace jxl {

// If `io` has an original color space using PQ or HLG, this renders it
// appropriately for a display with a peak luminance of `display_nits` and
// converts the result to a Rec. 2020 / PQ image. Otherwise, leaves the image as
// is.
// PQ images are tone-mapped using the method described in Rep. ITU-R BT.2408-5
// annex 5, while HLG images are rendered using the HLG OOTF with a gamma
// appropriate for the given target luminance.
// With a sufficiently bright SDR display, converting the output of this
// function to an SDR colorspace may look decent.
Status RenderHDR(CodecInOut* io, float display_nits,
                 ThreadPool* pool = nullptr);

}  // namespace jxl

#endif  // LIB_EXTRAS_RENDER_HDR_H_
