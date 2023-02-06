// Copyright (c) the JPEG XL Project Authors. All rights reserved.
//
// Use of this source code is governed by a BSD-style
// license that can be found in the LICENSE file.

#ifndef LIB_EXTRAS_ENC_JPEGLI_H_
#define LIB_EXTRAS_ENC_JPEGLI_H_

// Encodes JPG pixels and metadata in memory using the libjpegli library.

#include <stdint.h>

#include <vector>

#include "lib/extras/packed_image.h"
#include "lib/jxl/base/data_parallel.h"
#include "lib/jxl/base/status.h"

namespace jxl {
namespace extras {

struct JpegSettings {
  bool xyb = false;
  size_t target_size = 0;
  float distance = 1.f;
};

Status EncodeJpeg(const PackedPixelFile& ppf, const JpegSettings& jpeg_settings,
                  ThreadPool* pool, std::vector<uint8_t>* compressed);

}  // namespace extras
}  // namespace jxl

#endif  // LIB_EXTRAS_ENC_JPEGLI_H_
