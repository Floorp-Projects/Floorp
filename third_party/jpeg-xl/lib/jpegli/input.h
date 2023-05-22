// Copyright (c) the JPEG XL Project Authors. All rights reserved.
//
// Use of this source code is governed by a BSD-style
// license that can be found in the LICENSE file.

#ifndef LIB_JPEGLI_INPUT_H_
#define LIB_JPEGLI_INPUT_H_

/* clang-format off */
#include <stdio.h>
#include <jpeglib.h>
/* clang-format on */

namespace jpegli {

void ChooseInputMethod(j_compress_ptr cinfo);

}  // namespace jpegli

#endif  // LIB_JPEGLI_INPUT_H_
