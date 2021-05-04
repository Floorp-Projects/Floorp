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

#ifndef LIB_JXL_ENC_GROUP_H_
#define LIB_JXL_ENC_GROUP_H_

#include <stddef.h>
#include <stdint.h>

#include "lib/jxl/aux_out_fwd.h"
#include "lib/jxl/base/status.h"
#include "lib/jxl/enc_bit_writer.h"
#include "lib/jxl/enc_cache.h"

namespace jxl {

// Fills DC
void ComputeCoefficients(size_t group_idx, PassesEncoderState* enc_state,
                         const Image3F& opsin, Image3F* dc);

Status EncodeGroupTokenizedCoefficients(size_t group_idx, size_t pass_idx,
                                        size_t histogram_idx,
                                        const PassesEncoderState& enc_state,
                                        BitWriter* writer, AuxOut* aux_out);

}  // namespace jxl

#endif  // LIB_JXL_ENC_GROUP_H_
