// Copyright (c) the JPEG XL Project Authors. All rights reserved.
//
// Use of this source code is governed by a BSD-style
// license that can be found in the LICENSE file.

#ifndef LIB_JPEGLI_QUANT_H_
#define LIB_JPEGLI_QUANT_H_

/* clang-format off */
#include <stdio.h>
#include <jpeglib.h>
/* clang-format on */

namespace jpegli {

void SetQuantMatrices(j_compress_ptr cinfo, float distances[NUM_QUANT_TBLS],
                      bool add_two_chroma_tables);

void InitQuantizer(j_compress_ptr cinfo);

}  // namespace jpegli

#endif  // LIB_JPEGLI_QUANT_H_
