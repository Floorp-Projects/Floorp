// Copyright (c) the JPEG XL Project Authors. All rights reserved.
//
// Use of this source code is governed by a BSD-style
// license that can be found in the LICENSE file.

#ifndef LIB_EXTRAS_CODEC_PSD_H_
#define LIB_EXTRAS_CODEC_PSD_H_

// Decodes Photoshop PSD/PSB, preserving the layers

#include <stddef.h>
#include <stdint.h>

#include "lib/jxl/base/padded_bytes.h"
#include "lib/jxl/base/span.h"
#include "lib/jxl/base/status.h"
#include "lib/jxl/codec_in_out.h"
#include "lib/jxl/color_encoding_internal.h"

namespace jxl {

// Decodes `bytes` into `io`.
Status DecodeImagePSD(const Span<const uint8_t> bytes, ThreadPool* pool,
                      CodecInOut* io);

// Not implemented yet
Status EncodeImagePSD(const CodecInOut* io, const ColorEncoding& c_desired,
                      size_t bits_per_sample, ThreadPool* pool,
                      PaddedBytes* bytes);

}  // namespace jxl

#endif  // LIB_EXTRAS_CODEC_PSD_H_
