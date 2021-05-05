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

#ifndef LIB_JXL_ENC_HEURISTICS_H_
#define LIB_JXL_ENC_HEURISTICS_H_

// Hook for custom encoder heuristics (VarDCT only for now).

#include <stddef.h>
#include <stdint.h>

#include <string>

#include "lib/jxl/aux_out_fwd.h"
#include "lib/jxl/base/data_parallel.h"
#include "lib/jxl/base/status.h"
#include "lib/jxl/image.h"
#include "lib/jxl/modular/encoding/enc_ma.h"

namespace jxl {

struct PassesEncoderState;
class ImageBundle;
class ModularFrameEncoder;

class EncoderHeuristics {
 public:
  virtual ~EncoderHeuristics() = default;
  // Initializes encoder structures in `enc_state` using the original image data
  // in `original_pixels`, and the XYB image data in `opsin`. Also modifies the
  // `opsin` image by applying Gaborish, and doing other modifications if
  // necessary. `pool` is used for running the computations on multiple threads.
  // `aux_out` collects statistics and can be used to print debug images.
  virtual Status LossyFrameHeuristics(
      PassesEncoderState* enc_state, ModularFrameEncoder* modular_frame_encoder,
      const ImageBundle* original_pixels, Image3F* opsin, ThreadPool* pool,
      AuxOut* aux_out) = 0;

  // Custom fixed tree for lossless mode. Must set `tree` to a valid tree if
  // the function returns true.
  virtual bool CustomFixedTreeLossless(const FrameDimensions& frame_dim,
                                       Tree* tree) {
    return false;
  }

  // If this method returns `true`, the `opsin` parameter to
  // LossyFrameHeuristics will not be initialized, and should be initialized
  // during the call. Moreover, `original_pixels` may not be in a linear
  // colorspace (but will be the same as the `ib` value passed to this
  // function).
  virtual bool HandlesColorConversion(const CompressParams& cparams,
                                      const ImageBundle& ib) {
    return false;
  }
};

class DefaultEncoderHeuristics : public EncoderHeuristics {
 public:
  Status LossyFrameHeuristics(PassesEncoderState* enc_state,
                              ModularFrameEncoder* modular_frame_encoder,
                              const ImageBundle* original_pixels,
                              Image3F* opsin, ThreadPool* pool,
                              AuxOut* aux_out) override;
  bool HandlesColorConversion(const CompressParams& cparams,
                              const ImageBundle& ib) override;
};

class FastEncoderHeuristics : public EncoderHeuristics {
 public:
  Status LossyFrameHeuristics(PassesEncoderState* enc_state,
                              ModularFrameEncoder* modular_frame_encoder,
                              const ImageBundle* linear, Image3F* opsin,
                              ThreadPool* pool, AuxOut* aux_out) override;
};

// Exposed here since it may be used by other EncoderHeuristics implementations
// outside this project.
void FindBestDequantMatrices(const CompressParams& cparams,
                             const Image3F& opsin,
                             ModularFrameEncoder* modular_frame_encoder,
                             DequantMatrices* dequant_matrices);

}  // namespace jxl

#endif  // LIB_JXL_ENC_HEURISTICS_H_
