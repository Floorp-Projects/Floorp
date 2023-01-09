// Copyright (c) the JPEG XL Project Authors. All rights reserved.
//
// Use of this source code is governed by a BSD-style
// license that can be found in the LICENSE file.

#ifndef LIB_JPEGLI_SOURCE_MANAGER_H_
#define LIB_JPEGLI_SOURCE_MANAGER_H_

/* clang-format off */
#include <stdint.h>
#include <stdio.h>
#include <jpeglib.h>
/* clang-format on */

#include "lib/jxl/base/compiler_specific.h"
#include "lib/jxl/base/status.h"

namespace jpegli {

static JXL_INLINE void AdvanceInput(j_decompress_ptr cinfo, size_t size) {
  JXL_DASSERT(size <= cinfo->src->bytes_in_buffer);
  cinfo->src->bytes_in_buffer -= size;
  cinfo->src->next_input_byte += size;
}

}  // namespace jpegli

#endif  // LIB_JPEGLI_SOURCE_MANAGER_H_
