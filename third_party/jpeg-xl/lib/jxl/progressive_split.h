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

#ifndef LIB_JXL_PROGRESSIVE_SPLIT_H_
#define LIB_JXL_PROGRESSIVE_SPLIT_H_

#include <stddef.h>
#include <stdint.h>

#include <limits>
#include <memory>
#include <vector>

#include "lib/jxl/ac_strategy.h"
#include "lib/jxl/base/compiler_specific.h"
#include "lib/jxl/base/status.h"
#include "lib/jxl/chroma_from_luma.h"
#include "lib/jxl/codec_in_out.h"
#include "lib/jxl/common.h"
#include "lib/jxl/dct_util.h"
#include "lib/jxl/frame_header.h"
#include "lib/jxl/image.h"
#include "lib/jxl/image_ops.h"
#include "lib/jxl/splines.h"

// Functions to split DCT coefficients in multiple passes. All the passes of a
// single frame are added together.

namespace jxl {

constexpr size_t kNoDownsamplingFactor = std::numeric_limits<size_t>::max();

struct PassDefinition {
  // Side of the square of the coefficients that should be kept in each 8x8
  // block. Must be greater than 1, and at most 8. Should be in non-decreasing
  // order.
  size_t num_coefficients;

  // How much to shift the encoded values by, with rounding.
  size_t shift;

  // Whether or not we should include only salient blocks.
  // TODO(veluca): ignored for now.
  bool salient_only;

  // If specified, this indicates that if the requested downsampling factor is
  // sufficiently high, then it is fine to stop decoding after this pass.
  // By default, passes are not marked as being suitable for any downsampling.
  size_t suitable_for_downsampling_of_at_least;
};

struct ProgressiveMode {
  size_t num_passes = 1;
  PassDefinition passes[kMaxNumPasses] = {PassDefinition{
      /*num_coefficients=*/8, /*shift=*/0, /*salient_only=*/false,
      /*suitable_for_downsampling_of_at_least=*/1}};

  ProgressiveMode() = default;

  template <size_t nump>
  explicit ProgressiveMode(const PassDefinition (&p)[nump]) {
    JXL_ASSERT(nump <= kMaxNumPasses);
    num_passes = nump;
    PassDefinition previous_pass{
        /*num_coefficients=*/1, /*shift=*/0,
        /*salient_only=*/false,
        /*suitable_for_downsampling_of_at_least=*/kNoDownsamplingFactor};
    size_t last_downsampling_factor = kNoDownsamplingFactor;
    for (size_t i = 0; i < nump; i++) {
      JXL_ASSERT(p[i].num_coefficients > previous_pass.num_coefficients ||
                 (p[i].num_coefficients == previous_pass.num_coefficients &&
                  !p[i].salient_only && previous_pass.salient_only) ||
                 (p[i].num_coefficients == previous_pass.num_coefficients &&
                  p[i].shift < previous_pass.shift));
      JXL_ASSERT(p[i].suitable_for_downsampling_of_at_least ==
                     kNoDownsamplingFactor ||
                 p[i].suitable_for_downsampling_of_at_least <=
                     last_downsampling_factor);
      if (p[i].suitable_for_downsampling_of_at_least != kNoDownsamplingFactor) {
        last_downsampling_factor = p[i].suitable_for_downsampling_of_at_least;
      }
      previous_pass = passes[i] = p[i];
    }
  }
};

class ProgressiveSplitter {
 public:
  void SetProgressiveMode(ProgressiveMode mode) { mode_ = mode; }

  void SetSaliencyMap(const ImageF* saliency_map) {
    saliency_map_ = saliency_map;
  }

  void SetSaliencyThreshold(float threshold) {
    saliency_threshold_ = threshold;
  }

  size_t GetNumPasses() const { return mode_.num_passes; }

  void InitPasses(Passes* JXL_RESTRICT passes) const {
    passes->num_passes = static_cast<uint32_t>(GetNumPasses());
    passes->num_downsample = 0;
    JXL_ASSERT(passes->num_passes != 0);
    if (passes->num_passes == 1) return;  // Done, arrays are empty

    for (uint32_t i = 0; i < mode_.num_passes - 1; ++i) {
      const size_t min_downsampling_factor =
          mode_.passes[i].suitable_for_downsampling_of_at_least;
      passes->shift[i] = mode_.passes[i].shift;
      if (1 < min_downsampling_factor &&
          min_downsampling_factor != kNoDownsamplingFactor) {
        passes->downsample[passes->num_downsample] = min_downsampling_factor;
        passes->last_pass[passes->num_downsample] = i;
        passes->num_downsample += 1;
      }
    }
  }

  template <typename T>
  void SplitACCoefficients(const T* JXL_RESTRICT block, size_t size,
                           const AcStrategy& acs, size_t bx, size_t by,
                           size_t offset,
                           T* JXL_RESTRICT output[kMaxNumPasses][3]);

 private:
  bool SuperblockIsSalient(size_t row_start, size_t col_start, size_t num_rows,
                           size_t num_cols) const;
  ProgressiveMode mode_;

  // Not owned, must remain valid.
  const ImageF* saliency_map_ = nullptr;
  float saliency_threshold_ = 0.0;
};

extern template void ProgressiveSplitter::SplitACCoefficients<int32_t>(
    const int32_t* JXL_RESTRICT, size_t, const AcStrategy&, size_t, size_t,
    size_t, int32_t* JXL_RESTRICT[kMaxNumPasses][3]);

extern template void ProgressiveSplitter::SplitACCoefficients<int16_t>(
    const int16_t* JXL_RESTRICT, size_t, const AcStrategy&, size_t, size_t,
    size_t, int16_t* JXL_RESTRICT[kMaxNumPasses][3]);

}  // namespace jxl

#endif  // LIB_JXL_PROGRESSIVE_SPLIT_H_
