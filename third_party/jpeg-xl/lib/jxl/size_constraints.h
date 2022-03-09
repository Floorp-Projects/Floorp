// Copyright (c) the JPEG XL Project Authors. All rights reserved.
//
// Use of this source code is governed by a BSD-style
// license that can be found in the LICENSE file.

#ifndef LIB_JXL_SIZE_CONSTRAINTS_H_
#define LIB_JXL_SIZE_CONSTRAINTS_H_

#include <cstdint>

namespace jxl {

struct SizeConstraints {
  // Upper limit on pixel dimensions/area, enforced by VerifyDimensions
  // (called from decoders). Fuzzers set smaller values to limit memory use.
  uint32_t dec_max_xsize = 0xFFFFFFFFu;
  uint32_t dec_max_ysize = 0xFFFFFFFFu;
  uint64_t dec_max_pixels = 0xFFFFFFFFu;  // Might be up to ~0ull
};

}  // namespace jxl

#endif  // LIB_JXL_SIZE_CONSTRAINTS_H_
