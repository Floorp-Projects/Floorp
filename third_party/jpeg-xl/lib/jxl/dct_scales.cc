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

#include "lib/jxl/dct_scales.h"

namespace jxl {

// Definition of constexpr arrays.
constexpr float DCTResampleScales<1, 8>::kScales[];
constexpr float DCTResampleScales<2, 16>::kScales[];
constexpr float DCTResampleScales<4, 32>::kScales[];
constexpr float DCTResampleScales<8, 64>::kScales[];
constexpr float DCTResampleScales<16, 128>::kScales[];
constexpr float DCTResampleScales<32, 256>::kScales[];
constexpr float DCTResampleScales<8, 1>::kScales[];
constexpr float DCTResampleScales<16, 2>::kScales[];
constexpr float DCTResampleScales<32, 4>::kScales[];
constexpr float DCTResampleScales<64, 8>::kScales[];
constexpr float DCTResampleScales<128, 16>::kScales[];
constexpr float DCTResampleScales<256, 32>::kScales[];
constexpr float WcMultipliers<4>::kMultipliers[];
constexpr float WcMultipliers<8>::kMultipliers[];
constexpr float WcMultipliers<16>::kMultipliers[];
constexpr float WcMultipliers<32>::kMultipliers[];
constexpr float WcMultipliers<64>::kMultipliers[];
constexpr float WcMultipliers<128>::kMultipliers[];
constexpr float WcMultipliers<256>::kMultipliers[];

}  // namespace jxl
