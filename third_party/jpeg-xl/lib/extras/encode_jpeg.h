// Copyright (c) the JPEG XL Project Authors. All rights reserved.
//
// Use of this source code is governed by a BSD-style
// license that can be found in the LICENSE file.

#ifndef LIB_EXTRAS_ENCODE_JPEG_H_
#define LIB_EXTRAS_ENCODE_JPEG_H_

#include <stdint.h>

#include <vector>

#include "lib/jxl/base/data_parallel.h"
#include "lib/jxl/image_bundle.h"

namespace jxl {
namespace extras {

Status EncodeJpeg(const ImageBundle& input, float distance, ThreadPool* pool,
                  std::vector<uint8_t>* compressed);

}  // namespace extras
}  // namespace jxl

#endif  // LIB_EXTRAS_ENCODE_JPEG_H_
