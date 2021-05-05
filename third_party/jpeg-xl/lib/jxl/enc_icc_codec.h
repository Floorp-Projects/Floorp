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

#ifndef LIB_JXL_ENC_ICC_CODEC_H_
#define LIB_JXL_ENC_ICC_CODEC_H_

// Compressed representation of ICC profiles.

#include <stddef.h>
#include <stdint.h>

#include "lib/jxl/aux_out.h"
#include "lib/jxl/aux_out_fwd.h"
#include "lib/jxl/base/compiler_specific.h"
#include "lib/jxl/base/padded_bytes.h"
#include "lib/jxl/base/status.h"
#include "lib/jxl/dec_bit_reader.h"
#include "lib/jxl/enc_bit_writer.h"

namespace jxl {

// Should still be called if `icc.empty()` - if so, writes only 1 bit.
Status WriteICC(const PaddedBytes& icc, BitWriter* JXL_RESTRICT writer,
                size_t layer, AuxOut* JXL_RESTRICT aux_out);

// Exposed only for testing
Status PredictICC(const uint8_t* icc, size_t size, PaddedBytes* result);

}  // namespace jxl

#endif  // LIB_JXL_ENC_ICC_CODEC_H_
