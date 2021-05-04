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

#include "lib/jxl/filters.h"

#include "lib/jxl/base/profiler.h"

namespace jxl {

void FilterWeights::Init(const LoopFilter& lf,
                         const FrameDimensions& frame_dim) {
  if (lf.epf_iters > 0) {
    sigma = ImageF(frame_dim.xsize_blocks + 2 * kSigmaPadding,
                   frame_dim.ysize_blocks + 2 * kSigmaPadding);
  }
  if (lf.gab) {
    GaborishWeights(lf);
  }
}

void FilterWeights::GaborishWeights(const LoopFilter& lf) {
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
    const float mul =
        1.0f / (gab_weights[3 * c] +
                4 * (gab_weights[3 * c + 1] + gab_weights[3 * c + 2]));
    gab_weights[3 * c] *= mul;
    gab_weights[3 * c + 1] *= mul;
    gab_weights[3 * c + 2] *= mul;
  }
}

void FilterPipeline::ApplyFiltersRow(const LoopFilter& lf,
                                     const FilterWeights& filter_weights,
                                     const Rect& rect, ssize_t y) {
  PROFILER_ZONE("Gaborish+EPF");
  JXL_DASSERT(num_filters != 0);  // Must be initialized.

  JXL_ASSERT(y < static_cast<ssize_t>(rect.ysize() + lf.Padding()));

  // The minimum value of the center row "y" needed to process the current
  // filter.
  ssize_t rows_needed = -static_cast<ssize_t>(lf.Padding());

  for (size_t i = 0; i < num_filters; i++) {
    const FilterStep& filter = filters[i];

    rows_needed += filter.filter_def.border;

    // After this "y" points to the rect row for the center of the filter.
    y -= filter.filter_def.border;
    if (y < rows_needed) return;

    // Compute the region where we need to apply this filter. Depending on the
    // step we might need to compute a larger portion than the original rect.
    const size_t filter_x0 = kMaxFilterPadding - filter.output_col_border;
    const size_t filter_x1 =
        filter_x0 + rect.xsize() + 2 * filter.output_col_border;

    // Apply filter to the given region.
    FilterRows rows(filter.filter_def.border);
    filter.set_input_rows(filter, &rows, y);
    filter.set_output_rows(filter, &rows, y);

    // The "y" coordinate used for the sigma image in EPF1. Sigma is padded
    // with kMaxFilterPadding (or kMaxFilterPadding/kBlockDim rows in sigma)
    // above and below.
    const size_t sigma_y = kMaxFilterPadding + rect.y0() + y;
    if (compute_sigma) {
      rows.SetSigma(filter_weights.sigma, sigma_y, rect.x0());
    }

    filter.filter_def.apply(rows, lf, filter_weights, filter_x0, filter_x1,
                            rect.x0() % kBlockDim, sigma_y % kBlockDim);
  }
  JXL_DASSERT(rows_needed == 0);
}

}  // namespace jxl
