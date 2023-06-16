// Copyright (c) the JPEG XL Project Authors. All rights reserved.
//
// Use of this source code is governed by a BSD-style
// license that can be found in the LICENSE file.

#ifndef LIB_JPEGLI_IDCT_H_
#define LIB_JPEGLI_IDCT_H_

/* clang-format off */
#include <stdio.h>
#include <jpeglib.h>
#include <stddef.h>
#include <stdint.h>
/* clang-format on */

#include "lib/jxl/base/compiler_specific.h"

namespace jpegli {

void ChooseInverseTransform(j_decompress_ptr cinfo);

}  // namespace jpegli

#endif  // LIB_JPEGLI_IDCT_H_
