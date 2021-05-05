// Copyright (c) the JPEG XL Project
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//      http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

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
