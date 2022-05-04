// Copyright (c) the JPEG XL Project Authors. All rights reserved.
//
// Use of this source code is governed by a BSD-style
// license that can be found in the LICENSE file.

#ifndef LIB_EXTRAS_DEC_EXR_H_
#define LIB_EXTRAS_DEC_EXR_H_

// Decodes OpenEXR images in memory.

#include "lib/extras/dec/color_hints.h"
#include "lib/extras/packed_image.h"
#include "lib/jxl/base/data_parallel.h"
#include "lib/jxl/base/padded_bytes.h"
#include "lib/jxl/base/span.h"
#include "lib/jxl/base/status.h"
#include "lib/jxl/codec_in_out.h"

namespace jxl {
namespace extras {

// Decodes `bytes` into `ppf`. color_hints are ignored.
Status DecodeImageEXR(Span<const uint8_t> bytes, const ColorHints& color_hints,
                      const SizeConstraints& constraints, PackedPixelFile* ppf);

}  // namespace extras
}  // namespace jxl

#endif  // LIB_EXTRAS_DEC_EXR_H_
