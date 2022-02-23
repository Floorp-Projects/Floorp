// Copyright (c) the JPEG XL Project Authors. All rights reserved.
//
// Use of this source code is governed by a BSD-style
// license that can be found in the LICENSE file.

#ifndef LIB_EXTRAS_DEC_JPG_H_
#define LIB_EXTRAS_DEC_JPG_H_

// Decodes JPG pixels and metadata in memory.

#include <stdint.h>

#include "lib/extras/codec.h"
#include "lib/extras/dec/color_hints.h"
#include "lib/jxl/base/data_parallel.h"
#include "lib/jxl/base/padded_bytes.h"
#include "lib/jxl/base/span.h"
#include "lib/jxl/base/status.h"
#include "lib/jxl/codec_in_out.h"

namespace jxl {
namespace extras {

// Decodes `bytes` into `ppf`. color_hints are ignored.
// `elapsed_deinterleave`, if non-null, will be set to the time (in seconds)
// that it took to deinterleave the raw JSAMPLEs to planar floats.
Status DecodeImageJPG(Span<const uint8_t> bytes, const ColorHints& color_hints,
                      const SizeConstraints& constraints, PackedPixelFile* ppf);

}  // namespace extras
}  // namespace jxl

#endif  // LIB_EXTRAS_DEC_JPG_H_
