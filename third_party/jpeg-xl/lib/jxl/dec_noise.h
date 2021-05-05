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

#ifndef LIB_JXL_DEC_NOISE_H_
#define LIB_JXL_DEC_NOISE_H_

// Noise synthesis. Currently disabled.

#include <stddef.h>
#include <stdint.h>

#include "lib/jxl/aux_out_fwd.h"
#include "lib/jxl/base/status.h"
#include "lib/jxl/chroma_from_luma.h"
#include "lib/jxl/dec_bit_reader.h"
#include "lib/jxl/image.h"
#include "lib/jxl/noise.h"

namespace jxl {

// Add a noise to Opsin image, loading generated random noise from `noise_rect`
// in `noise`.
void AddNoise(const NoiseParams& noise_params, const Rect& noise_rect,
              const Image3F& noise, const Rect& opsin_rect,
              const ColorCorrelationMap& cmap, Image3F* opsin);

void RandomImage3(size_t seed, const Rect& rect, Image3F* JXL_RESTRICT noise);

// Must only call if FrameHeader.flags.kNoise.
Status DecodeNoise(BitReader* br, NoiseParams* noise_params);

}  // namespace jxl

#endif  // LIB_JXL_DEC_NOISE_H_
