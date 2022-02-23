// Copyright (c) the JPEG XL Project Authors. All rights reserved.
//
// Use of this source code is governed by a BSD-style
// license that can be found in the LICENSE file.

#ifndef LIB_EXTRAS_CODEC_PNG_H_
#define LIB_EXTRAS_CODEC_PNG_H_

// Encodes/decodes PNG pixels and metadata in memory.

#include <stddef.h>
#include <stdint.h>

// TODO(janwas): workaround for incorrect Win64 codegen (cause unknown)
#include <hwy/highway.h>

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

// Decodes `bytes` into `ppf`.
Status DecodeImagePNG(const Span<const uint8_t> bytes,
                      const ColorHints& color_hints,
                      const SizeConstraints& constraints, PackedPixelFile* ppf);

// Transforms from io->c_current to `c_desired` and encodes into `bytes`.
Status EncodeImagePNG(const CodecInOut* io, const ColorEncoding& c_desired,
                      size_t bits_per_sample, ThreadPool* pool,
                      PaddedBytes* bytes);

}  // namespace extras
}  // namespace jxl

#endif  // LIB_EXTRAS_CODEC_PNG_H_
