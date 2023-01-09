// Copyright (c) the JPEG XL Project Authors. All rights reserved.
//
// Use of this source code is governed by a BSD-style
// license that can be found in the LICENSE file.

#ifndef LIB_JPEGLI_QUANT_H_
#define LIB_JPEGLI_QUANT_H_

#include <vector>

#include "lib/jxl/common.h"
#include "lib/jxl/image.h"
#include "lib/jxl/jpeg/jpeg_data.h"

namespace jpegli {

void AddJpegQuantMatrices(const jxl::ImageF& qf, bool xyb, float dc_quant,
                          float global_scale,
                          std::vector<jxl::jpeg::JPEGQuantTable>* quant_tables,
                          float* qm);

}  // namespace jpegli

#endif  // LIB_JPEGLI_QUANT_H_
