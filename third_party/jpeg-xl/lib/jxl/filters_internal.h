// Copyright (c) the JPEG XL Project
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//      http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

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
