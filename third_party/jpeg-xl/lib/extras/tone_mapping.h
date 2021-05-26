// Copyright (c) the JPEG XL Project Authors. All rights reserved.
//
// Use of this source code is governed by a BSD-style
// license that can be found in the LICENSE file.

#ifndef LIB_EXTRAS_TONE_MAPPING_H_
#define LIB_EXTRAS_TONE_MAPPING_H_

#include "lib/jxl/codec_in_out.h"

namespace jxl {

Status ToneMapTo(std::pair<float, float> display_nits, CodecInOut* io,
                 ThreadPool* pool = nullptr);

}  // namespace jxl

#endif  // LIB_EXTRAS_TONE_MAPPING_H_
