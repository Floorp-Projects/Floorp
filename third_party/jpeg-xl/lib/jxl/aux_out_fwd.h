// Copyright (c) the JPEG XL Project Authors. All rights reserved.
//
// Use of this source code is governed by a BSD-style
// license that can be found in the LICENSE file.

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
