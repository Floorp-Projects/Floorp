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

#ifndef LIB_JXL_COEFF_ORDER_FWD_H_
#define LIB_JXL_COEFF_ORDER_FWD_H_

// Breaks circular dependency between ac_strategy and coeff_order.

#include <stddef.h>
#include <stdint.h>

#include "base/compiler_specific.h"

namespace jxl {

// Needs at least 16 bits. A 32-bit type speeds up DecodeAC by 2% at the cost of
// more memory.
using coeff_order_t = uint32_t;

// Maximum number of orders to be used. Note that this needs to be multiplied by
// the number of channels. One per "size class" (plus one extra for DCT8),
// shared between transforms of size XxY and of size YxX.
constexpr uint8_t kNumOrders = 13;

// DCT coefficients are laid out in such a way that the number of rows of
// coefficients is always the smaller coordinate.
JXL_INLINE constexpr size_t CoefficientRows(size_t rows, size_t columns) {
  return rows < columns ? rows : columns;
}

JXL_INLINE constexpr size_t CoefficientColumns(size_t rows, size_t columns) {
  return rows < columns ? columns : rows;
}

JXL_INLINE void CoefficientLayout(size_t* JXL_RESTRICT rows,
                                  size_t* JXL_RESTRICT columns) {
  size_t r = *rows;
  size_t c = *columns;
  *rows = CoefficientRows(r, c);
  *columns = CoefficientColumns(r, c);
}

}  // namespace jxl

#endif  // LIB_JXL_COEFF_ORDER_FWD_H_
