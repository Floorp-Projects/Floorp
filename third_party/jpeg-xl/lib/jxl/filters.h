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

#ifndef LIB_JXL_FILTERS_H_
#define LIB_JXL_FILTERS_H_

#include <stddef.h>

#include "lib/jxl/common.h"
#include "lib/jxl/dec_group_border.h"
#include "lib/jxl/filters_internal.h"
#include "lib/jxl/image.h"
#include "lib/jxl/loop_filter.h"

namespace jxl {

struct FilterWeights {
  // Initialize the FilterWeights for the passed LoopFilter and FrameDimensions.
  void Init(const LoopFilter& lf, const FrameDimensions& frame_dim);

  // Normalized weights for gaborish, in XYB order, each weight for Manhattan
  // distance of 0, 1 and 2 respectively.
  float gab_weights[9];

  // Sigma values for EPF, if enabled.
  // Note that, for speed reasons, this is actually kInvSigmaNum / sigma.
  ImageF sigma;

 private:
  void GaborishWeights(const LoopFilter& lf);
};

static constexpr size_t kMaxFinalizeRectPadding = 9;

// Line-based EPF only needs to keep in cache 21 lines of the image, so 256 is
// sufficient for everything to fit in the L2 cache. We add
// 2*RoundUpTo(kMaxFinalizeRectPadding, kBlockDim) pixels as we might have up to
// two extra borders on each side.
constexpr size_t kApplyImageFeaturesTileDim =
    256 + 2 * RoundUpToBlockDim(kMaxFinalizeRectPadding);

// The maximum row storage needed by the filtering pipeline. This is the sum of
// the number of input rows needed by each step.
constexpr size_t kTotalStorageRows = 7 + 5 + 3;  // max is EPF0 + EPF1 + EPF2.

// The maximum sum of all the borders in a chain of filters.
constexpr size_t kMaxFilterBorder = 1 * kBlockDim;

// The maximum horizontal filter padding ever needed to apply a chain of
// filters. Intermediate storage must have at least as much padding on each
// left and right sides. This value must be a multiple of kBlockDim.
constexpr size_t kMaxFilterPadding = kMaxFilterBorder + kBlockDim;
static_assert(kMaxFilterPadding % kBlockDim == 0,
              "kMaxFilterPadding must be a multiple of block size.");

// Same as FilterBorder and FilterPadding but for Sigma.
constexpr size_t kSigmaBorder = kMaxFilterBorder / kBlockDim;
constexpr size_t kSigmaPadding = kMaxFilterPadding / kBlockDim;

// Utility struct to define input/output rows of row-based loop filters.
constexpr size_t kMaxBorderSize = 3;
struct FilterRows {
  explicit FilterRows(int border_size) : border_size_(border_size) {
    JXL_DASSERT(border_size <= static_cast<int>(kMaxBorderSize));
  }

  JXL_INLINE const float* GetInputRow(int row, size_t c) const {
    // Check that row is within range.
    JXL_DASSERT(-border_size_ <= row && row <= border_size_);
    return rows_in_[c] + offsets_in_[kMaxBorderSize + row];
  }

  float* GetOutputRow(size_t c) const { return rows_out_[c]; }

  const float* GetSigmaRow() const {
    JXL_DASSERT(row_sigma_ != nullptr);
    return row_sigma_;
  }

  template <typename RowMap>
  void SetInput(const Image3F& in, size_t y_offset, ssize_t y0, ssize_t x0,
                ssize_t full_image_y_offset = 0, ssize_t image_ysize = 0) {
    RowMap row_map(full_image_y_offset, image_ysize);
    for (size_t c = 0; c < 3; c++) {
      rows_in_[c] = in.ConstPlaneRow(c, 0);
    }
    for (int32_t i = -border_size_; i <= border_size_; i++) {
      size_t y = row_map(y0 + i);
      offsets_in_[i + kMaxBorderSize] =
          static_cast<ssize_t>((y + y_offset) * in.PixelsPerRow()) + x0;
    }
  }

  template <typename RowMap>
  void SetOutput(Image3F* out, size_t y_offset, ssize_t y0, ssize_t x0) {
    size_t y = RowMap()(y0);
    for (size_t c = 0; c < 3; c++) {
      rows_out_[c] = out->PlaneRow(c, y + y_offset) + x0;
    }
  }

  // Sets the sigma row for the given y0, x0 input image position. Sigma images
  // have one pixel per input image block, although they are padded with two
  // blocks (pixels in sigma) on each one of the four sides. The (x0, y0) values
  // should include this padding.
  void SetSigma(const ImageF& sigma, size_t y0, size_t x0) {
    JXL_DASSERT(x0 % GroupBorderAssigner::kPaddingXRound == 0);
    row_sigma_ = sigma.ConstRow(y0 / kBlockDim) + x0 / kBlockDim;
  }

 private:
  // Base pointer to each one of the planes.
  const float* JXL_RESTRICT rows_in_[3];

  // Offset to the pixel x0 at the different rows. offsets_in_[kMaxBorderSize]
  // references the center row, regardless of the border_size_. Only the center
  // row, border_size_ before and border_size_ after are initialized. The offset
  // is relative to the base pointer in rows_in_.
  ssize_t offsets_in_[2 * kMaxBorderSize + 1];

  float* JXL_RESTRICT rows_out_[3];

  const float* JXL_RESTRICT row_sigma_{nullptr};

  const int border_size_;
};

// Definition of a filter. This specifies the function to be used to apply the
// filter and its row and column padding requirements.
struct FilterDefinition {
  // Function to apply the filter to a given row. The filter constant parameters
  // are passed in LoopFilter lf and filter_weights. `xoff` is needed to offset
  // the `x0` value so that it will cause correct accesses to
  // rows.GetSigmaRow(): there is just one sigma value per 8 pixels, and if the
  // image rectangle is not aligned to multiples of 8 pixels, we need to
  // compensate for the difference between x0 and the image position modulo 8.
  void (*apply)(const FilterRows& rows, const LoopFilter& lf,
                const FilterWeights& filter_weights, size_t x0, size_t x1,
                size_t image_y_mod_8, size_t image_x_mod_8);

  // Number of source image rows and cols before and after an input pixel needed
  // to compute the output of the filter. For a 3x3 convolution this border will
  // be only 1.
  size_t border;
};

// A chain of filters to be applied to a source image. This instance must be
// initialized by the FilterPipelineInit() function before it can be used.
class FilterPipeline {
 public:
  FilterPipeline() : FilterPipeline(kApplyImageFeaturesTileDim) {}
  explicit FilterPipeline(size_t max_rect_xsize)
      : storage{max_rect_xsize + 2 * kMaxFilterPadding, kTotalStorageRows} {
#if MEMORY_SANITIZER
    // The padding of the storage may be used uninitialized since we process
    // multiple SIMD lanes at a time, aligned to a multiple of lanes.
    // For example, in a hypothetical 3-step filter process where all filters
    // use 1 pixel border the first filter needs to process 2 pixels more on
    // each side than the requested rect.x0(), rect.xsize(), while the second
    // filter needs to process 1 more pixel on each side, however for
    // performance reasons both will process Lanes(df) more pixels on each
    // side assuming this Lanes(df) value is more than one. In that case the
    // second filter will be using one pixel of uninitialized data to generate
    // an output pixel that won't affect the final output but may cause msan
    // failures. For this reason we initialize the padding region.
    for (size_t c = 0; c < 3; c++) {
      for (size_t y = 0; y < storage.ysize(); y++) {
        float* row = storage.PlaneRow(c, y);
        memset(row, 0x77, sizeof(float) * kMaxFilterPadding);
        memset(row + storage.xsize() - kMaxFilterPadding, 0x77,
               sizeof(float) * kMaxFilterPadding);
      }
    }
#endif  // MEMORY_SANITIZER
  }

  FilterPipeline(const FilterPipeline&) = delete;
  FilterPipeline(FilterPipeline&&) = default;

  // Apply the filter chain to a given row. To apply the filter chain to a whole
  // image this must be called for `rect.ysize() + 2 * total_border`
  // values of `y`, in increasing order, starting from `y = -total_border`.
  void ApplyFiltersRow(const LoopFilter& lf,
                       const FilterWeights& filter_weights, const Rect& rect,
                       ssize_t y);

  struct FilterStep {
    // Sets the input of the filter step as an image region.
    void SetInput(const Image3F* im_input, const Rect& input_rect,
                  const Rect& image_rect, size_t image_ysize) {
      input = im_input;
      this->input_rect = input_rect;
      this->image_rect = image_rect;
      this->image_ysize = image_ysize;
      JXL_DASSERT(SameSize(input_rect, image_rect));
      set_input_rows = [](const FilterStep& self, FilterRows* rows,
                          ssize_t y0) {
        ssize_t full_image_y_offset =
            static_cast<ssize_t>(self.image_rect.y0()) -
            static_cast<ssize_t>(self.input_rect.y0());
        rows->SetInput<RowMapMirror>(*(self.input), 0,
                                     self.input_rect.y0() + y0,
                                     self.input_rect.x0() - kMaxFilterPadding,
                                     full_image_y_offset, self.image_ysize);
      };
    }

    // Sets the input of the filter step as the temporary cyclic storage with
    // num_rows rows. The value rect.x0() during application will be mapped to
    // kMaxFilterPadding regardless of the rect being processed.
    template <size_t num_rows>
    void SetInputCyclicStorage(const Image3F* storage, size_t offset_rows) {
      input = storage;
      input_y_offset = offset_rows;
      set_input_rows = [](const FilterStep& self, FilterRows* rows,
                          ssize_t y0) {
        rows->SetInput<RowMapMod<num_rows>>(*(self.input), self.input_y_offset,
                                            y0, 0);
      };
    }

    // Sets the output of the filter step as the temporary cyclic storage with
    // num_rows rows. The value rect.x0() during application will be mapped to
    // kMaxFilterPadding regardless of the rect being processed.
    template <size_t num_rows>
    void SetOutputCyclicStorage(Image3F* storage, size_t offset_rows) {
      output = storage;
      output_y_offset = offset_rows;
      set_output_rows = [](const FilterStep& self, FilterRows* rows,
                           ssize_t y0) {
        rows->SetOutput<RowMapMod<num_rows>>(self.output, self.output_y_offset,
                                             y0, 0);
      };
    }

    // Set the output of the filter step as the output image. The value
    // rect.x0() will be mapped to the same value in the output image.
    void SetOutput(Image3F* im_output, const Rect& output_rect) {
      output = im_output;
      this->output_rect = output_rect;
      set_output_rows = [](const FilterStep& self, FilterRows* rows,
                           ssize_t y0) {
        rows->SetOutput<RowMapId>(
            self.output, 0, self.output_rect.y0() + y0,
            static_cast<ssize_t>(self.output_rect.x0()) - kMaxFilterPadding);
      };
    }

    // The input and output image buffers for the current filter step. Note that
    // the rows used from these images depends on the module used in
    // set_input_rows and set_output_rows functions.
    const Image3F* input;
    size_t input_y_offset = 0;
    Image3F* output;
    size_t output_y_offset = 0;

    // Input/output rect for the first/last steps of the filter.
    Rect input_rect;
    Rect output_rect;

    // Information to properly do RowMapMirror().
    Rect image_rect;
    size_t image_ysize;

    // Functions that compute the list of rows needed to process a region for
    // the given row and starting column.
    void (*set_input_rows)(const FilterStep&, FilterRows* rows, ssize_t y0);
    void (*set_output_rows)(const FilterStep&, FilterRows* rows, ssize_t y0);

    // Actual filter descriptor.
    FilterDefinition filter_def;

    // Number of extra horizontal pixels needed on each side of the output of
    // this filter to produce the requested rect at the end of the chain. This
    // value is always 0 for the last filter of the chain but it depends on the
    // actual filter chain used in other cases.
    size_t output_col_border;
  };

  template <size_t border>
  void AddStep(const FilterDefinition& filter_def) {
    JXL_DASSERT(num_filters < kMaxFilters);
    filters[num_filters].filter_def = filter_def;

    if (num_filters > 0) {
      // If it is not the first step we need to set the previous step output to
      // a portion of the cyclic storage. We only need as many rows as the
      // input of the current stage.
      constexpr size_t num_rows = 2 * border + 1;
      filters[num_filters - 1].SetOutputCyclicStorage<num_rows>(
          &storage, storage_rows_used);
      filters[num_filters].SetInputCyclicStorage<num_rows>(&storage,
                                                           storage_rows_used);
      storage_rows_used += num_rows;
      JXL_DASSERT(storage_rows_used <= kTotalStorageRows);
    }
    num_filters++;
  }

  // Tile storage for ApplyImageFeatures steps. Different groups of rows of this
  // image are used for the intermediate steps.
  Image3F storage;
  size_t storage_rows_used = 0;

  static const size_t kMaxFilters = 4;
  FilterStep filters[kMaxFilters];
  size_t num_filters = 0;

  // Whether we need to compute the sigma_row_ during application.
  bool compute_sigma = false;

  // The total border needed to process this pipeline.
  size_t total_border = 0;
};

}  // namespace jxl

#endif  // LIB_JXL_FILTERS_H_
