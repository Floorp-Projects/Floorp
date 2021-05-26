// Copyright (c) the JPEG XL Project Authors. All rights reserved.
//
// Use of this source code is governed by a BSD-style
// license that can be found in the LICENSE file.

#ifndef LIB_EXTRAS_CODEC_JPG_H_
#define LIB_EXTRAS_CODEC_JPG_H_

// Encodes JPG pixels and metadata in memory.

#include <stdint.h>

#include "lib/extras/codec.h"
#include "lib/jxl/base/data_parallel.h"
#include "lib/jxl/base/padded_bytes.h"
#include "lib/jxl/base/span.h"
#include "lib/jxl/base/status.h"
#include "lib/jxl/codec_in_out.h"

namespace jxl {

enum class JpegEncoder {
  kLibJpeg,
  kSJpeg,
};

static inline bool IsJPG(const Span<const uint8_t> bytes) {
  if (bytes.size() < 2) return false;
  if (bytes[0] != 0xFF || bytes[1] != 0xD8) return false;
  return true;
}

// Decodes `bytes` into `io`. io->dec_hints are ignored.
// `elapsed_deinterleave`, if non-null, will be set to the time (in seconds)
// that it took to deinterleave the raw JSAMPLEs to planar floats.
Status DecodeImageJPG(Span<const uint8_t> bytes, ThreadPool* pool,
                      CodecInOut* io, double* elapsed_deinterleave = nullptr);

// Encodes into `bytes`.
Status EncodeImageJPG(const CodecInOut* io, JpegEncoder encoder, size_t quality,
                      YCbCrChromaSubsampling chroma_subsampling,
                      ThreadPool* pool, PaddedBytes* bytes,
                      DecodeTarget target = DecodeTarget::kPixels);

}  // namespace jxl

#endif  // LIB_EXTRAS_CODEC_JPG_H_
