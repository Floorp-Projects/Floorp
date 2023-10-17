// Copyright (c) the JPEG XL Project Authors. All rights reserved.
//
// Use of this source code is governed by a BSD-style
// license that can be found in the LICENSE file.

#ifndef LIB_JXL_COLOR_MANAGEMENT_H_
#define LIB_JXL_COLOR_MANAGEMENT_H_

// ICC profiles and color space conversions.

#include "lib/jxl/base/compiler_specific.h"
#include "lib/jxl/base/status.h"

// TODO(eustas): migrate to lib/jxl/cms/color_encoding_cms.h
#include "lib/jxl/color_encoding_internal.h"

namespace jxl {

enum class ExtraTF {
  kNone,
  kPQ,
  kHLG,
  kSRGB,
};

// NOTE: for XYB colorspace, the created profile can be used to transform a
// *scaled* XYB image (created by ScaleXYB()) to another colorspace.
Status MaybeCreateProfile(const ColorEncoding& c, IccBytes* JXL_RESTRICT icc);

Status CIEXYZFromWhiteCIExy(const CIExy& xy, float XYZ[3]);

}  // namespace jxl

#endif  // LIB_JXL_COLOR_MANAGEMENT_H_
