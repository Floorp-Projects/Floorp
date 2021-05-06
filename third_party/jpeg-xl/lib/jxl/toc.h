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

#ifndef LIB_JXL_TOC_H_
#define LIB_JXL_TOC_H_

#include <stddef.h>
#include <stdint.h>

#include <vector>

#include "lib/jxl/base/compiler_specific.h"
#include "lib/jxl/base/status.h"
#include "lib/jxl/dec_bit_reader.h"
#include "lib/jxl/field_encodings.h"

namespace jxl {

// (2+bits) = 2,3,4 bytes so encoders can patch TOC after encoding.
// 30 is sufficient for 4K channels of uncompressed 16-bit samples.
constexpr U32Enc kTocDist(Bits(10), BitsOffset(14, 1024), BitsOffset(22, 17408),
                          BitsOffset(30, 4211712));

size_t MaxBits(const size_t num_sizes);

// TODO(veluca): move these to FrameDimensions.
static JXL_INLINE size_t AcGroupIndex(size_t pass, size_t group,
                                      size_t num_groups, size_t num_dc_groups,
                                      bool has_ac_global) {
  return 1 + num_dc_groups + static_cast<size_t>(has_ac_global) +
         pass * num_groups + group;
}

static JXL_INLINE size_t NumTocEntries(size_t num_groups, size_t num_dc_groups,
                                       size_t num_passes, bool has_ac_global) {
  if (num_groups == 1 && num_passes == 1) return 1;
  return AcGroupIndex(0, 0, num_groups, num_dc_groups, has_ac_global) +
         num_groups * num_passes;
}

Status ReadGroupOffsets(size_t toc_entries, BitReader* JXL_RESTRICT reader,
                        std::vector<uint64_t>* JXL_RESTRICT offsets,
                        std::vector<uint32_t>* JXL_RESTRICT sizes,
                        uint64_t* total_size);

}  // namespace jxl

#endif  // LIB_JXL_TOC_H_
