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

#ifndef LIB_JXL_AUX_OUT_FWD_H_
#define LIB_JXL_AUX_OUT_FWD_H_

#include <stddef.h>

#include "lib/jxl/enc_bit_writer.h"

namespace jxl {

struct AuxOut;

// Helper function that ensures the `bits_written` are charged to `layer` in
// `aux_out`. Example usage:
//   BitWriter::Allotment allotment(&writer, max_bits);
//   writer.Write(..); writer.Write(..);
//   ReclaimAndCharge(&writer, &allotment, layer, aux_out);
void ReclaimAndCharge(BitWriter* JXL_RESTRICT writer,
                      BitWriter::Allotment* JXL_RESTRICT allotment,
                      size_t layer, AuxOut* JXL_RESTRICT aux_out);

}  // namespace jxl

#endif  // LIB_JXL_AUX_OUT_FWD_H_
