// Copyright (c) the JPEG XL Project Authors. All rights reserved.
//
// Use of this source code is governed by a BSD-style
// license that can be found in the LICENSE file.

#ifndef LIB_JPEGLI_DCT_H_
#define LIB_JPEGLI_DCT_H_

#include <vector>

#include "lib/jxl/common.h"
#include "lib/jxl/image.h"
#include "lib/jxl/jpeg/jpeg_data.h"

namespace jpegli {

void ComputeDCTCoefficients(const jxl::Image3F& opsin, const bool xyb,
                            const jxl::ImageF& qf,
                            const float* qm,
                            std::vector<jxl::jpeg::JPEGComponent>* components);

}  // namespace jpegli

#endif  // LIB_JPEGLI_DCT_H_
