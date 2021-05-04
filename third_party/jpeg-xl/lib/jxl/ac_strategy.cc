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

#include "lib/jxl/ac_strategy.h"

#include <string.h>

#include <algorithm>
#include <numeric>  // iota
#include <type_traits>
#include <utility>

#include "lib/jxl/base/bits.h"
#include "lib/jxl/base/profiler.h"
#include "lib/jxl/common.h"
#include "lib/jxl/image_ops.h"

namespace jxl {

// Tries to generalize zig-zag order to non-square blocks. Surprisingly, in
// square block frequency along the (i + j == const) diagonals is roughly the
// same. For historical reasons, consecutive diagonals are traversed
// in alternating directions - so called "zig-zag" (or "snake") order.
AcStrategy::CoeffOrderAndLut::CoeffOrderAndLut() {
  for (size_t s = 0; s < AcStrategy::kNumValidStrategies; s++) {
    const AcStrategy acs = AcStrategy::FromRawStrategy(s);
    size_t cx = acs.covered_blocks_x();
    size_t cy = acs.covered_blocks_y();
    CoefficientLayout(&cy, &cx);
    JXL_ASSERT((AcStrategy::CoeffOrderAndLut::kOffset[s + 1] -
                AcStrategy::CoeffOrderAndLut::kOffset[s]) == cx * cy);
    coeff_order_t* JXL_RESTRICT order_start =
        order + AcStrategy::CoeffOrderAndLut::kOffset[s] * kDCTBlockSize;
    coeff_order_t* JXL_RESTRICT lut_start =
        lut + AcStrategy::CoeffOrderAndLut::kOffset[s] * kDCTBlockSize;

    // CoefficientLayout ensures cx >= cy.
    // We compute the zigzag order for a cx x cx block, then discard all the
    // lines that are not multiple of the ratio between cx and cy.
    size_t xs = cx / cy;
    size_t xsm = xs - 1;
    size_t xss = CeilLog2Nonzero(xs);
    // First half of the block
    size_t cur = cx * cy;
    for (size_t i = 0; i < cx * kBlockDim; i++) {
      for (size_t j = 0; j <= i; j++) {
        size_t x = j;
        size_t y = i - j;
        if (i % 2) std::swap(x, y);
        if ((y & xsm) != 0) continue;
        y >>= xss;
        size_t val = 0;
        if (x < cx && y < cy) {
          val = y * cx + x;
        } else {
          val = cur++;
        }
        lut_start[y * cx * kBlockDim + x] = val;
        order_start[val] = y * cx * kBlockDim + x;
      }
    }
    // Second half
    for (size_t ip = cx * kBlockDim - 1; ip > 0; ip--) {
      size_t i = ip - 1;
      for (size_t j = 0; j <= i; j++) {
        size_t x = cx * kBlockDim - 1 - (i - j);
        size_t y = cx * kBlockDim - 1 - j;
        if (i % 2) std::swap(x, y);
        if ((y & xsm) != 0) continue;
        y >>= xss;
        size_t val = cur++;
        lut_start[y * cx * kBlockDim + x] = val;
        order_start[val] = y * cx * kBlockDim + x;
      }
    }
  }
}

const AcStrategy::CoeffOrderAndLut* AcStrategy::CoeffOrder() {
  static AcStrategy::CoeffOrderAndLut* order =
      new AcStrategy::CoeffOrderAndLut();
  return order;
}

// These definitions are needed before C++17.
constexpr size_t AcStrategy::kMaxCoeffBlocks;
constexpr size_t AcStrategy::kMaxBlockDim;
constexpr size_t AcStrategy::kMaxCoeffArea;
constexpr size_t AcStrategy::CoeffOrderAndLut::kOffset[];

AcStrategyImage::AcStrategyImage(size_t xsize, size_t ysize)
    : layers_(xsize, ysize) {
  row_ = layers_.Row(0);
  stride_ = layers_.PixelsPerRow();
}

size_t AcStrategyImage::CountBlocks(AcStrategy::Type type) const {
  size_t ret = 0;
  for (size_t y = 0; y < layers_.ysize(); y++) {
    const uint8_t* JXL_RESTRICT row = layers_.ConstRow(y);
    for (size_t x = 0; x < layers_.xsize(); x++) {
      if (row[x] == ((static_cast<uint8_t>(type) << 1) | 1)) ret++;
    }
  }
  return ret;
}

}  // namespace jxl
