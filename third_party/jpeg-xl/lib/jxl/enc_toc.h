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

#ifndef LIB_JXL_ENC_TOC_H_
#define LIB_JXL_ENC_TOC_H_

#include <stddef.h>
#include <stdint.h>

#include <vector>

#include "lib/jxl/aux_out.h"
#include "lib/jxl/base/compiler_specific.h"
#include "lib/jxl/base/status.h"
#include "lib/jxl/enc_bit_writer.h"

namespace jxl {

// Writes the group offsets. If the permutation vector is nullptr, the identity
// permutation will be used.
Status WriteGroupOffsets(const std::vector<BitWriter>& group_codes,
                         const std::vector<coeff_order_t>* permutation,
                         BitWriter* JXL_RESTRICT writer, AuxOut* aux_out);

}  // namespace jxl

#endif  // LIB_JXL_ENC_TOC_H_
