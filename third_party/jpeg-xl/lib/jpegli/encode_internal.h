// Copyright (c) the JPEG XL Project Authors. All rights reserved.
//
// Use of this source code is governed by a BSD-style
// license that can be found in the LICENSE file.

#ifndef LIB_JPEGLI_ENCODE_INTERNAL_H_
#define LIB_JPEGLI_ENCODE_INTERNAL_H_

/* clang-format off */
#include <stdint.h>
#include <stdio.h>
#include <jpeglib.h>
/* clang-format on */

#include "lib/jxl/image.h"
#include "lib/jxl/jpeg/jpeg_data.h"

struct jpeg_comp_master {
  jxl::Image3F input;
  float distance;
  bool force_baseline;
  J_COLOR_SPACE jpeg_colorspace;
  jxl::jpeg::JPEGData jpeg_data;
  std::vector<uint8_t>* cur_marker_data;
};

#endif  // LIB_JPEGLI_ENCODE_INTERNAL_H_
