// Copyright (c) the JPEG XL Project Authors. All rights reserved.
//
// Use of this source code is governed by a BSD-style
// license that can be found in the LICENSE file.

#ifndef LIB_EXTRAS_DECODE_JPEG_H_
#define LIB_EXTRAS_DECODE_JPEG_H_

#include <stdint.h>

#include <vector>

#include "lib/extras/packed_image.h"
#include "lib/jxl/base/data_parallel.h"

namespace jxl {
namespace extras {

Status DecodeJpeg(const std::vector<uint8_t>& compressed,
                  JxlDataType output_data_type, ThreadPool* pool,
                  PackedPixelFile* ppf);

}  // namespace extras
}  // namespace jxl

#endif  // LIB_EXTRAS_DECODE_JPEG_H_
