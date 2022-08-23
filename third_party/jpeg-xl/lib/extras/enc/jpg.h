// Copyright (c) the JPEG XL Project Authors. All rights reserved.
//
// Use of this source code is governed by a BSD-style
// license that can be found in the LICENSE file.

#ifndef LIB_EXTRAS_ENC_JPG_H_
#define LIB_EXTRAS_ENC_JPG_H_

// Encodes JPG pixels and metadata in memory.

#include <stdint.h>

#include "lib/extras/enc/encode.h"
#include "lib/jxl/base/data_parallel.h"
#include "lib/jxl/base/padded_bytes.h"
#include "lib/jxl/base/span.h"
#include "lib/jxl/base/status.h"
#include "lib/jxl/codec_in_out.h"

namespace jxl {
namespace extras {

enum class JpegEncoder {
  kLibJpeg,
  kSJpeg,
};

std::unique_ptr<Encoder> GetJPEGEncoder();

// Encodes into `bytes`.
Status EncodeImageJPG(const CodecInOut* io, JpegEncoder encoder, size_t quality,
                      YCbCrChromaSubsampling chroma_subsampling,
                      ThreadPool* pool, std::vector<uint8_t>* bytes);

}  // namespace extras
}  // namespace jxl

#endif  // LIB_EXTRAS_ENC_JPG_H_
