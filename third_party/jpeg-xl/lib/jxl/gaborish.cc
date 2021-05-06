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

#include "lib/jxl/gaborish.h"

#include <stddef.h>

#include <hwy/base.h>

#include "lib/jxl/base/status.h"
#include "lib/jxl/convolve.h"
#include "lib/jxl/image_ops.h"

namespace jxl {

void GaborishInverse(Image3F* in_out, float mul, ThreadPool* pool) {
  JXL_ASSERT(mul >= 0.0f);

  // Only an approximation. One or even two 3x3, and rank-1 (separable) 5x5
  // are insufficient.
  constexpr float kGaborish[5] = {
      -0.092359145662814029f,  -0.039253623634014627f, 0.016176494530216929f,
      0.00083458437774987476f, 0.004512465323949319f,
  };
  /*
    better would be:
      1.0 - mul * (4 * (kGaborish[0] + kGaborish[1] +
                        kGaborish[2] + kGaborish[4]) +
                   8 * (kGaborish[3]));
  */
  WeightsSymmetric5 weights = {{HWY_REP4(1.0f)},
                               {HWY_REP4(mul * kGaborish[0])},
                               {HWY_REP4(mul * kGaborish[2])},
                               {HWY_REP4(mul * kGaborish[1])},
                               {HWY_REP4(mul * kGaborish[4])},
                               {HWY_REP4(mul * kGaborish[3])}};
  double sum = static_cast<double>(weights.c[0]);
  sum += 4 * weights.r[0];
  sum += 4 * weights.R[0];
  sum += 4 * weights.d[0];
  sum += 4 * weights.D[0];
  sum += 8 * weights.L[0];
  const float normalize = static_cast<float>(1.0 / sum);
  for (size_t i = 0; i < 4; ++i) {
    weights.c[i] *= normalize;
    weights.r[i] *= normalize;
    weights.R[i] *= normalize;
    weights.d[i] *= normalize;
    weights.D[i] *= normalize;
    weights.L[i] *= normalize;
  }

  // Reduce memory footprint by only allocating a single plane and swapping it
  // into the output Image3F. Better still would be tiling.
  // Note that we cannot *allocate* a plane, as doing so might cause Image3F to
  // have planes of different stride. Instead, we copy one plane in a temporary
  // image and reuse the existing planes of the in/out image.
  ImageF temp = CopyImage(in_out->Plane(2));
  Symmetric5(in_out->Plane(0), Rect(*in_out), weights, pool, &in_out->Plane(2));
  Symmetric5(in_out->Plane(1), Rect(*in_out), weights, pool, &in_out->Plane(0));
  Symmetric5(temp, Rect(*in_out), weights, pool, &in_out->Plane(1));
  // Now planes are 1, 2, 0.
  in_out->Plane(0).Swap(in_out->Plane(1));
  // 2 1 0
  in_out->Plane(0).Swap(in_out->Plane(2));
}

}  // namespace jxl
