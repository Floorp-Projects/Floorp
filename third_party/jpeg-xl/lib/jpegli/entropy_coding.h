// Copyright (c) the JPEG XL Project Authors. All rights reserved.
//
// Use of this source code is governed by a BSD-style
// license that can be found in the LICENSE file.

#ifndef LIB_JPEGLI_ENTROPY_CODING_H_
#define LIB_JPEGLI_ENTROPY_CODING_H_

#include "lib/jxl/jpeg/jpeg_data.h"

namespace jpegli {

void OptimizeHuffmanCodes(jxl::jpeg::JPEGData* out);

}  // namespace jpegli

#endif  // LIB_JPEGLI_ENTROPY_CODING_H_
