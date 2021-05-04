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

#ifndef LIB_JXL_MODULAR_ENCODING_MA_COMMON_H_
#define LIB_JXL_MODULAR_ENCODING_MA_COMMON_H_

#include <stddef.h>

namespace jxl {

enum MATreeContext : size_t {
  kSplitValContext = 0,
  kPropertyContext = 1,
  kPredictorContext = 2,
  kOffsetContext = 3,
  kMultiplierLogContext = 4,
  kMultiplierBitsContext = 5,

  kNumTreeContexts = 6,
};

static constexpr size_t kMaxTreeSize = 1 << 26;

}  // namespace jxl

#endif  // LIB_JXL_MODULAR_ENCODING_MA_COMMON_H_
