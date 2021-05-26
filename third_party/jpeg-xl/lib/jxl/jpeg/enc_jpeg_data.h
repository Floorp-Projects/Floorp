// Copyright (c) the JPEG XL Project Authors. All rights reserved.
//
// Use of this source code is governed by a BSD-style
// license that can be found in the LICENSE file.

#ifndef LIB_JXL_JPEG_ENC_JPEG_DATA_H_
#define LIB_JXL_JPEG_ENC_JPEG_DATA_H_

#include "lib/jxl/base/padded_bytes.h"
#include "lib/jxl/codec_in_out.h"
#include "lib/jxl/jpeg/jpeg_data.h"

namespace jxl {
namespace jpeg {
Status EncodeJPEGData(JPEGData& jpeg_data, PaddedBytes* bytes);

/**
 * Decodes bytes containing JPEG codestream into a CodecInOut as coefficients
 * only, for lossless JPEG transcoding.
 */
Status DecodeImageJPG(const Span<const uint8_t> bytes, CodecInOut* io);
}  // namespace jpeg
}  // namespace jxl

#endif  // LIB_JXL_JPEG_ENC_JPEG_DATA_H_
