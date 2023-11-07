// Copyright (c) the JPEG XL Project Authors. All rights reserved.
//
// Use of this source code is governed by a BSD-style
// license that can be found in the LICENSE file.

#ifndef LIB_JXL_COLOR_MANAGEMENT_H_
#define LIB_JXL_COLOR_MANAGEMENT_H_

// ICC profiles and color space conversions.

#include <cstdint>
#include <vector>

#include "lib/jxl/base/compiler_specific.h"
#include "lib/jxl/base/status.h"

namespace jxl {

enum class ExtraTF {
  kNone,
  kPQ,
  kHLG,
  kSRGB,
};

namespace cms {
struct ColorEncoding;
struct CIExy;
}  // namespace cms

// NOTE: for XYB colorspace, the created profile can be used to transform a
// *scaled* XYB image (created by ScaleXYB()) to another colorspace.
Status MaybeCreateProfile(const jxl::cms::ColorEncoding& c,
                          std::vector<uint8_t>* JXL_RESTRICT icc);

Status CIEXYZFromWhiteCIExy(const jxl::cms::CIExy& xy, float XYZ[3]);

Status PrimariesToXYZ(float rx, float ry, float gx, float gy, float bx,
                      float by, float wx, float wy, float matrix[9]);
// Adapts whitepoint x, y to D50
Status AdaptToXYZD50(float wx, float wy, float matrix[9]);
Status PrimariesToXYZD50(float rx, float ry, float gx, float gy, float bx,
                         float by, float wx, float wy, float matrix[9]);

}  // namespace jxl

#endif  // LIB_JXL_COLOR_MANAGEMENT_H_
