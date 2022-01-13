// Copyright (c) the JPEG XL Project Authors. All rights reserved.
//
// Use of this source code is governed by a BSD-style
// license that can be found in the LICENSE file.

#ifndef LIB_JXL_ENC_BUTTERAUGLI_PNORM_H_
#define LIB_JXL_ENC_BUTTERAUGLI_PNORM_H_

#include <stdint.h>

#include "lib/jxl/butteraugli/butteraugli.h"
#include "lib/jxl/image_bundle.h"

namespace jxl {

// Computes p-norm given the butteraugli distmap.
double ComputeDistanceP(const ImageF& distmap, const ButteraugliParams& params,
                        double p);

double ComputeDistance2(const ImageBundle& ib1, const ImageBundle& ib2);

}  // namespace jxl

#endif  // LIB_JXL_ENC_BUTTERAUGLI_PNORM_H_
