// Copyright (c) the JPEG XL Project Authors. All rights reserved.
//
// Use of this source code is governed by a BSD-style
// license that can be found in the LICENSE file.

#ifndef LIB_JXL_FILTERS_INTERNAL_H_
#define LIB_JXL_FILTERS_INTERNAL_H_

#include <stddef.h>

#include "lib/jxl/base/status.h"
#include "lib/jxl/image_ops.h"

namespace jxl {

// Maps a row to the range [0, image_ysize) mirroring it when outside the [0,
// image_ysize) range. The input row is offset by `full_image_y_offset`, i.e.
// row `y` corresponds to row `y + full_image_y_offset` in the full frame.
struct RowMapMirror {
  RowMapMirror(ssize_t full_image_y_offset, size_t image_ysize)
      : full_image_y_offset_(full_image_y_offset), image_ysize_(image_ysize) {}
  size_t operator()(ssize_t y) {
    return Mirror(y + full_image_y_offset_, image_ysize_) -
           full_image_y_offset_;
  }
  ssize_t full_image_y_offset_;
  size_t image_ysize_;
};

// Maps a row in the range [-16, \inf) to a row number in the range [0, m) using
// the modulo operation.
template <size_t m>
struct RowMapMod {
  RowMapMod() = default;
  RowMapMod(ssize_t /*full_image_y_offset*/, size_t /*image_ysize*/) {}
  size_t operator()(ssize_t y) {
    JXL_DASSERT(y >= -16);
    // The `m > 16 ? m : 16 * m` is evaluated at compile time and is a multiple
    // of m of at least 16. This is to make sure that the left operand is
    // positive.
    return static_cast<size_t>(y + (m > 16 ? m : 16 * m)) % m;
  }
};

// Identity mapping. Maps a row in the range [0, ysize) to the same value.
struct RowMapId {
  size_t operator()(ssize_t y) {
    JXL_DASSERT(y >= 0);
    return y;
  }
};

}  // namespace jxl

#endif  // LIB_JXL_FILTERS_INTERNAL_H_
