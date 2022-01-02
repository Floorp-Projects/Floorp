// Copyright (c) the JPEG XL Project Authors. All rights reserved.
//
// Use of this source code is governed by a BSD-style
// license that can be found in the LICENSE file.

#ifndef LIB_EXTRAS_CODEC_EXR_H_
#define LIB_EXTRAS_CODEC_EXR_H_

// Encodes OpenEXR images in memory.

#include "lib/extras/color_hints.h"
#include "lib/extras/packed_image.h"
#include "lib/jxl/base/data_parallel.h"
#include "lib/jxl/base/padded_bytes.h"
#include "lib/jxl/base/span.h"
#include "lib/jxl/base/status.h"
#include "lib/jxl/codec_in_out.h"
#include "lib/jxl/color_encoding_internal.h"

namespace jxl {
namespace extras {

// Decodes `bytes` into `io`. color_hints are ignored.
Status DecodeImageEXR(Span<const uint8_t> bytes, const ColorHints& color_hints,
                      const SizeConstraints& constraints, float target_nits,
                      ThreadPool* pool, PackedPixelFile* ppf);

// Transforms from io->c_current to `c_desired` (with the transfer function set
// to linear as that is the OpenEXR convention) and encodes into `bytes`.
Status EncodeImageEXR(const CodecInOut* io, const ColorEncoding& c_desired,
                      ThreadPool* pool, PaddedBytes* bytes);

}  // namespace extras
}  // namespace jxl

#endif  // LIB_EXTRAS_CODEC_EXR_H_
