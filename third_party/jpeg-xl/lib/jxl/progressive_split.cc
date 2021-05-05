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

#include "lib/jxl/progressive_split.h"

#include <string.h>

#include <algorithm>
#include <memory>

#include "lib/jxl/common.h"
#include "lib/jxl/image.h"

namespace jxl {

bool ProgressiveSplitter::SuperblockIsSalient(size_t row_start,
                                              size_t col_start, size_t num_rows,
                                              size_t num_cols) const {
  if (saliency_map_ == nullptr || saliency_map_->xsize() == 0 ||
      saliency_threshold_ == 0.0) {
    // If we do not have a saliency-map, or the threshold says to include
    // every block, we straightaway classify the superblock as 'salient'.
    return true;
  }
  const size_t row_end = std::min(saliency_map_->ysize(), row_start + num_rows);
  const size_t col_end = std::min(saliency_map_->xsize(), col_start + num_cols);
  for (size_t num_row = row_start; num_row < row_end; num_row++) {
    const float* JXL_RESTRICT map_row = saliency_map_->ConstRow(num_row);
    for (size_t num_col = col_start; num_col < col_end; num_col++) {
      if (map_row[num_col] >= saliency_threshold_) {
        // One of the blocks covered by this superblock is above the saliency
        // threshold.
        return true;
      }
    }
  }
  // We did not see any block above the saliency threshold.
  return false;
}

template <typename T>
void ProgressiveSplitter::SplitACCoefficients(
    const T* JXL_RESTRICT block, size_t size, const AcStrategy& acs, size_t bx,
    size_t by, size_t offset, T* JXL_RESTRICT output[kMaxNumPasses][3]) {
  auto shift_right_round0 = [&](T v, int shift) {
    T one_if_negative = static_cast<uint32_t>(v) >> 31;
    T add = (one_if_negative << shift) - one_if_negative;
    return (v + add) >> shift;
  };
  // Early quit for the simple case of only one pass.
  if (mode_.num_passes == 1) {
    for (size_t c = 0; c < 3; c++) {
      memcpy(output[0][c] + offset, block + c * size, sizeof(T) * size);
    }
    return;
  }
  size_t ncoeffs_all_done_from_earlier_passes = 1;
  size_t previous_pass_salient_only = false;

  int previous_pass_shift = 0;
  for (size_t num_pass = 0; num_pass < mode_.num_passes; num_pass++) {  // pass
    // Zero out output block.
    for (size_t c = 0; c < 3; c++) {
      memset(output[num_pass][c] + offset, 0, size * sizeof(T));
    }
    const bool current_pass_salient_only = mode_.passes[num_pass].salient_only;
    const int pass_shift = mode_.passes[num_pass].shift;
    size_t frame_ncoeffs = mode_.passes[num_pass].num_coefficients;
    for (size_t c = 0; c < 3; c++) {  // color-channel
      size_t xsize = acs.covered_blocks_x();
      size_t ysize = acs.covered_blocks_y();
      CoefficientLayout(&ysize, &xsize);
      if (current_pass_salient_only || previous_pass_salient_only) {
        // Current or previous pass is salient-only.
        const bool superblock_is_salient =
            SuperblockIsSalient(by, bx, ysize, xsize);
        if (current_pass_salient_only != superblock_is_salient) {
          // Current pass is salient-only, but block is not salient,
          // OR last pass was salient-only, and block is salient
          // (hence was already included in last pass).
          continue;
        }
      }
      for (size_t y = 0; y < ysize * frame_ncoeffs; y++) {    // superblk-y
        for (size_t x = 0; x < xsize * frame_ncoeffs; x++) {  // superblk-x
          size_t pos = y * xsize * kBlockDim + x;
          if (x < xsize * ncoeffs_all_done_from_earlier_passes &&
              y < ysize * ncoeffs_all_done_from_earlier_passes) {
            // This coefficient was already included in an earlier pass,
            // which included a genuinely smaller set of coefficients
            // (= is not about saliency-splitting).
            continue;
          }
          T v = block[c * size + pos];
          // Previous pass discarded some bits: do not encode them again.
          if (previous_pass_shift != 0) {
            T previous_v = shift_right_round0(v, previous_pass_shift) *
                           (1 << previous_pass_shift);
            v -= previous_v;
          }
          output[num_pass][c][offset + pos] = shift_right_round0(v, pass_shift);
        }  // superblk-x
      }    // superblk-y
    }      // color-channel
    if (!current_pass_salient_only) {
      // We just finished a non-salient pass.
      // Hence, we are now guaranteed to have included all coeffs up to
      // frame_ncoeffs in every block, unless the current pass is shifted.
      if (mode_.passes[num_pass].shift == 0) {
        ncoeffs_all_done_from_earlier_passes = frame_ncoeffs;
      }
    }
    previous_pass_salient_only = current_pass_salient_only;
    previous_pass_shift = mode_.passes[num_pass].shift;
  }  // num_pass
}

template void ProgressiveSplitter::SplitACCoefficients<int32_t>(
    const int32_t* JXL_RESTRICT, size_t, const AcStrategy&, size_t, size_t,
    size_t, int32_t* JXL_RESTRICT[kMaxNumPasses][3]);

template void ProgressiveSplitter::SplitACCoefficients<int16_t>(
    const int16_t* JXL_RESTRICT, size_t, const AcStrategy&, size_t, size_t,
    size_t, int16_t* JXL_RESTRICT[kMaxNumPasses][3]);

}  // namespace jxl
