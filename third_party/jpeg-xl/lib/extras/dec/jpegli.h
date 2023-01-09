// Copyright (c) the JPEG XL Project Authors. All rights reserved.
//
// Use of this source code is governed by a BSD-style
// license that can be found in the LICENSE file.

#ifndef LIB_EXTRAS_DEC_JPEGLI_H_
#define LIB_EXTRAS_DEC_JPEGLI_H_

// Decodes JPG pixels and metadata in memory using the libjpegli library.

#include <stdint.h>

#include <vector>

#include "jxl/types.h"
#include "lib/extras/packed_image.h"
#include "lib/jxl/base/data_parallel.h"
#include "lib/jxl/base/status.h"

namespace jxl {
namespace extras {

Status DecodeJpeg(const std::vector<uint8_t>& compressed,
                  JxlDataType output_data_type, ThreadPool* pool,
                  PackedPixelFile* ppf);

}  // namespace extras
}  // namespace jxl

#endif  // LIB_EXTRAS_DEC_JPEGLI_H_
