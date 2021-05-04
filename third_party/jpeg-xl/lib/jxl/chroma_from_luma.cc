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

#include "lib/jxl/chroma_from_luma.h"

#include <float.h>
#include <stdlib.h>

#include <algorithm>
#include <array>
#include <cmath>

#include "lib/jxl/aux_out.h"
#include "lib/jxl/base/bits.h"
#include "lib/jxl/base/padded_bytes.h"
#include "lib/jxl/base/profiler.h"
#include "lib/jxl/base/span.h"
#include "lib/jxl/base/status.h"
#include "lib/jxl/common.h"
#include "lib/jxl/dec_transforms-inl.h"
#include "lib/jxl/enc_transforms-inl.h"
#include "lib/jxl/entropy_coder.h"
#include "lib/jxl/image_ops.h"
#include "lib/jxl/modular/encoding/encoding.h"
#include "lib/jxl/quantizer.h"

namespace jxl {

ColorCorrelationMap::ColorCorrelationMap(size_t xsize, size_t ysize, bool XYB)
    : ytox_map(DivCeil(xsize, kColorTileDim), DivCeil(ysize, kColorTileDim)),
      ytob_map(DivCeil(xsize, kColorTileDim), DivCeil(ysize, kColorTileDim)) {
  ZeroFillImage(&ytox_map);
  ZeroFillImage(&ytob_map);
  if (!XYB) {
    base_correlation_b_ = 0;
  }
  RecomputeDCFactors();
}

}  // namespace jxl
