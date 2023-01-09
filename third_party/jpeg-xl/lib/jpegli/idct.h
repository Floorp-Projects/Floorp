// Copyright (c) the JPEG XL Project Authors. All rights reserved.
//
// Use of this source code is governed by a BSD-style
// license that can be found in the LICENSE file.

#ifndef LIB_JPEGLI_IDCT_H_
#define LIB_JPEGLI_IDCT_H_

#include <stddef.h>
#include <stdint.h>

#include "lib/jxl/base/compiler_specific.h"

namespace jpegli {

// Performs dequantization and inverse DCT.
void InverseTransformBlock(const int16_t* JXL_RESTRICT qblock,
                           const float* JXL_RESTRICT dequant_matrices,
                           const float* JXL_RESTRICT biases,
                           float* JXL_RESTRICT scratch_space,
                           float* JXL_RESTRICT output, size_t output_stride);

}  // namespace jpegli

#endif  // LIB_JPEGLI_IDCT_H_
