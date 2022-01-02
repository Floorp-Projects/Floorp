// Copyright (c) the JPEG XL Project Authors. All rights reserved.
//
// Use of this source code is governed by a BSD-style
// license that can be found in the LICENSE file.

#include "lib/jxl/filters.h"

#include <cmath>

#include "lib/jxl/base/profiler.h"

namespace jxl {

Status FilterWeights::Init(const LoopFilter& lf,
                           const FrameDimensions& frame_dim) {
  if (lf.epf_iters > 0) {
    sigma = ImageF(frame_dim.xsize_blocks + 2 * kSigmaPadding,
                   frame_dim.ysize_blocks + 2 * kSigmaPadding);
  }
  if (lf.gab) {
    JXL_RETURN_IF_ERROR(GaborishWeights(lf));
  }
  return true;
}

Status FilterWeights::GaborishWeights(const LoopFilter& lf) {
  const float kZeroEpsilon = 1e-6;

  gab_weights[0] = 1;
  gab_weights[1] = lf.gab_x_weight1;
  gab_weights[2] = lf.gab_x_weight2;
  gab_weights[3] = 1;
  gab_weights[4] = lf.gab_y_weight1;
  gab_weights[5] = lf.gab_y_weight2;
  gab_weights[6] = 1;
  gab_weights[7] = lf.gab_b_weight1;
  gab_weights[8] = lf.gab_b_weight2;
  // Normalize
  for (size_t c = 0; c < 3; c++) {
    const float div = gab_weights[3 * c] +
                      4 * (gab_weights[3 * c + 1] + gab_weights[3 * c + 2]);
    if (std::abs(div) < kZeroEpsilon) {
      return JXL_FAILURE("Gaborish weights lead to near 0 unnormalized kernel");
    }
    const float mul = 1.0f / div;
    gab_weights[3 * c] *= mul;
    gab_weights[3 * c + 1] *= mul;
    gab_weights[3 * c + 2] *= mul;
  }
  return true;
}

void FilterPipeline::ApplyFiltersRow(const LoopFilter& lf,
                                     const FilterWeights& filter_weights,
                                     ssize_t y) {
  PROFILER_ZONE("Gaborish+EPF");
  JXL_DASSERT(num_filters != 0);  // Must be initialized.

  JXL_ASSERT(y < static_cast<ssize_t>(image_rect.ysize() + lf.Padding()));

  // The minimum value of the center row "y" needed to process the current
  // filter.
  ssize_t rows_needed = -static_cast<ssize_t>(lf.Padding());

  // We pass `image_rect.x0() - image_rect.x0() % kBlockDim` as the x0 for
  // the row_sigma, so to go from an `x` value in the filter to the
  // corresponding value in row_sigma we use the fact that we mapped
  // image_rect.x0() in the original image to MaxLeftPadding(image_rect.x0()) in
  // the input/output rows seen by the filters:
  // x_in_sigma_row =
  //    ((x - (image_rect.x0() % kPaddingXRound) + image_rect.x0()) -
  //     (image_rect.x0() - image_rect.x0() % kBlockDim))) / kBlockDim
  // x_in_sigma_row =
  //   x - image_rect.x0() % kPaddingXRound + image_rect.x0() % kBlockDim
  const size_t sigma_x_offset =
      image_rect.x0() % kBlockDim -
      image_rect.x0() % GroupBorderAssigner::kPaddingXRound;

  for (size_t i = 0; i < num_filters; i++) {
    const FilterStep& filter = filters[i];

    rows_needed += filter.filter_def.border;

    // After this "y" points to the rect row for the center of the filter.
    y -= filter.filter_def.border;
    if (y < rows_needed) return;

    // Apply filter to the given region.
    FilterRows rows(filter.filter_def.border);
    filter.set_input_rows(filter, &rows, y);
    filter.set_output_rows(filter, &rows, y);

    // The "y" coordinate used for the sigma image in EPF1. Sigma is padded
    // with kMaxFilterPadding (or kMaxFilterPadding/kBlockDim rows in sigma)
    // above and below.
    const size_t sigma_y = kMaxFilterPadding + image_rect.y0() + y;
    // The offset to subtract to a "x" value in the filter to obtain the
    // corresponding x in the sigma row.
    if (compute_sigma) {
      rows.SetSigma(filter_weights.sigma, sigma_y,
                    image_rect.x0() - image_rect.x0() % kBlockDim);
    }

    filter.filter_def.apply(rows, lf, filter_weights, filter.filter_x0,
                            filter.filter_x1, sigma_x_offset,
                            sigma_y % kBlockDim);
  }

  JXL_DASSERT(rows_needed == 0);
}

}  // namespace jxl
