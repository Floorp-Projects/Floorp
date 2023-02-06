// Copyright (c) the JPEG XL Project Authors. All rights reserved.
//
// Use of this source code is governed by a BSD-style
// license that can be found in the LICENSE file.

#ifndef LIB_JPEGLI_ADAPTIVE_QUANTIZATION_H_
#define LIB_JPEGLI_ADAPTIVE_QUANTIZATION_H_

/* clang-format off */
#include <stdio.h>
#include <jpeglib.h>
#include <stddef.h>
/* clang-format on */

namespace jpegli {

void ComputeAdaptiveQuantField(j_compress_ptr cinfo);

float InitialQuantDC(float butteraugli_target);

}  // namespace jpegli

#endif  // LIB_JPEGLI_ADAPTIVE_QUANTIZATION_H_
