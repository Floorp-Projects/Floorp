// Copyright (c) the JPEG XL Project Authors. All rights reserved.
//
// Use of this source code is governed by a BSD-style
// license that can be found in the LICENSE file.

#ifndef LIB_JXL_ENC_ICC_CODEC_H_
#define LIB_JXL_ENC_ICC_CODEC_H_

// Compressed representation of ICC profiles.

#include <cstddef>
#include <cstdint>
#include <vector>

#include "lib/jxl/base/compiler_specific.h"
#include "lib/jxl/base/status.h"
#include "lib/jxl/enc_bit_writer.h"

namespace jxl {

struct AuxOut;
class PaddedBytes;

// Should still be called if `icc.empty()` - if so, writes only 1 bit.
Status WriteICC(const std::vector<uint8_t>& icc, BitWriter* JXL_RESTRICT writer,
                size_t layer, AuxOut* JXL_RESTRICT aux_out);

}  // namespace jxl

#endif  // LIB_JXL_ENC_ICC_CODEC_H_
