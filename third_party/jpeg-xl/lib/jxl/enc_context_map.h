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

#ifndef LIB_JXL_ENC_CONTEXT_MAP_H_
#define LIB_JXL_ENC_CONTEXT_MAP_H_

#include <stddef.h>
#include <stdint.h>

#include <vector>

#include "lib/jxl/ac_context.h"
#include "lib/jxl/aux_out.h"
#include "lib/jxl/enc_bit_writer.h"

namespace jxl {

// Max limit is 255 because encoding assumes numbers < 255
// More clusters can help compression, but makes encode/decode somewhat slower
static const size_t kClustersLimit = 128;

// Encodes the given context map to the bit stream. The number of different
// histogram ids is given by num_histograms.
void EncodeContextMap(const std::vector<uint8_t>& context_map,
                      size_t num_histograms, BitWriter* writer);

void EncodeBlockCtxMap(const BlockCtxMap& block_ctx_map, BitWriter* writer,
                       AuxOut* aux_out);
}  // namespace jxl

#endif  // LIB_JXL_ENC_CONTEXT_MAP_H_
