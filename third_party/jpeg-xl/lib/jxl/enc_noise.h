// Copyright (c) the JPEG XL Project Authors. All rights reserved.
//
// Use of this source code is governed by a BSD-style
// license that can be found in the LICENSE file.

#ifndef LIB_JXL_ENC_NOISE_H_
#define LIB_JXL_ENC_NOISE_H_

// Noise parameter estimation.

#include <stddef.h>

#include "lib/jxl/aux_out_fwd.h"
#include "lib/jxl/base/status.h"
#include "lib/jxl/enc_bit_writer.h"
#include "lib/jxl/image.h"
#include "lib/jxl/noise.h"

namespace jxl {

// Get parameters of the noise for NoiseParams model
// Returns whether a valid noise model (with HasAny()) is set.
Status GetNoiseParameter(const Image3F& opsin, NoiseParams* noise_params,
                         float quality_coef);

// Does not write anything if `noise_params` are empty. Otherwise, caller must
// set FrameHeader.flags.kNoise.
void EncodeNoise(const NoiseParams& noise_params, BitWriter* writer,
                 size_t layer, AuxOut* aux_out);

}  // namespace jxl

#endif  // LIB_JXL_ENC_NOISE_H_
