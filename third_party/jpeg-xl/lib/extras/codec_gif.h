// Copyright (c) the JPEG XL Project Authors. All rights reserved.
//
// Use of this source code is governed by a BSD-style
// license that can be found in the LICENSE file.

#ifndef LIB_EXTRAS_CODEC_GIF_H_
#define LIB_EXTRAS_CODEC_GIF_H_

// Decodes GIF images in memory.

#include <stdint.h>

#include "lib/jxl/base/data_parallel.h"
#include "lib/jxl/base/padded_bytes.h"
#include "lib/jxl/base/span.h"
#include "lib/jxl/base/status.h"
#include "lib/jxl/codec_in_out.h"

namespace jxl {

// Decodes `bytes` into `io`. io->dec_hints are ignored.
Status DecodeImageGIF(const Span<const uint8_t> bytes, ThreadPool* pool,
                      CodecInOut* io);

}  // namespace jxl

#endif  // LIB_EXTRAS_CODEC_GIF_H_
