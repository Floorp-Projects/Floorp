// Copyright (c) the JPEG XL Project Authors. All rights reserved.
//
// Use of this source code is governed by a BSD-style
// license that can be found in the LICENSE file.

#ifndef LIB_JPEGLI_COLOR_TRANSFORM_H_
#define LIB_JPEGLI_COLOR_TRANSFORM_H_

#include <stddef.h>

#include "lib/jxl/base/compiler_specific.h"

namespace jpegli {

void YCbCrToRGB(float* JXL_RESTRICT row0, float* JXL_RESTRICT row1,
                float* JXL_RESTRICT row2, size_t xsize);

}  // namespace jpegli

#endif  // LIB_JPEGLI_COLOR_TRANSFORM_H_
