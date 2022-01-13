// Copyright (c) the JPEG XL Project Authors. All rights reserved.
//
// Use of this source code is governed by a BSD-style
// license that can be found in the LICENSE file.

#ifndef LIB_EXTRAS_ENC_PGX_H_
#define LIB_EXTRAS_ENC_PGX_H_

// Encodes PGX pixels in memory.

#include <stddef.h>
#include <stdint.h>

#include "lib/extras/packed_image.h"
#include "lib/jxl/base/data_parallel.h"
#include "lib/jxl/base/padded_bytes.h"
#include "lib/jxl/base/span.h"
#include "lib/jxl/base/status.h"
#include "lib/jxl/codec_in_out.h"
#include "lib/jxl/color_encoding_internal.h"

namespace jxl {
namespace extras {

// Transforms from io->c_current to `c_desired` and encodes into `bytes`.
Status EncodeImagePGX(const CodecInOut* io, const ColorEncoding& c_desired,
                      size_t bits_per_sample, ThreadPool* pool,
                      PaddedBytes* bytes);

}  // namespace extras
}  // namespace jxl

#endif  // LIB_EXTRAS_ENC_PGX_H_
