// Copyright (c) the JPEG XL Project Authors. All rights reserved.
//
// Use of this source code is governed by a BSD-style
// license that can be found in the LICENSE file.

#ifndef LIB_JXL_DEC_RENDER_PIPELINE_H_
#define LIB_JXL_DEC_RENDER_PIPELINE_H_

#include <stdint.h>

#include "lib/jxl/filters.h"

namespace jxl {

// The first pixel in the input to RenderPipelineStage will be located at
// this position. Pixels before this position may be accessed as padding.
constexpr size_t kRenderPipelineXOffset = 16;

enum class RenderPipelineChannelMode {
  kIgnored = 0,
  kInPlace = 1,
  kInOut = 2,
};

class RenderPipelineStage {
 public:
  // `input` points to `2*MaxPaddingY() + 1` pointers, each of which points to
  // `3+num_non_color_channels` pointer-to-row. So, `input[MaxPaddingY()][0]` is
  // the pointer to the center row of the first color channel.
  //  `MaxPaddingY()` is the maximum value returned by `GetPaddingX()`;
  //  typically, this is a constant.
  // `output` points to `1<<MaxShiftY()` pointers, each of which points to
  // `3+num_non_color_channels` pointer-to-row. So, `output[0][3]` is the
  //  pointer to the top row of the first non-color channel.
  //  `MaxShiftY()` is defined similarly to `MaxPaddingY()`.
  //  `xsize` represents the total number of pixels to be processed in the input
  //  row. `xpos` and `ypos` represent the position of the first pixel in the
  //  center row in the input
  virtual void ProcessRow(float* JXL_RESTRICT** input,
                          float* JXL_RESTRICT** output, size_t xsize,
                          size_t xpos, size_t ypos) const = 0;
  virtual ~RenderPipelineStage() {}

  // Amount of padding required by each channel in the various directions.
  // The value for c=0 indicates padding required for color channels, subsequent
  // values refer to padding for non-color channels, in order.
  virtual size_t GetPaddingX(size_t c) const = 0;
  virtual size_t GetPaddingY(size_t c) const = 0;

  // Log2 of the number of columns/rows of output that this stage will produce
  // for the given channel.
  virtual size_t ShiftX(size_t c) const = 0;
  virtual size_t ShiftY(size_t c) const = 0;

  // How each channel will be processed. If this method returns kIgnored or
  // kInPlace for a given channel, then the corresponding pointer-to-row values
  // in the output of ProcessRow will be null for that channel, and
  // `GetPaddingX`, `GetPaddingY`, `ShiftX` and `ShiftY` for that channel must
  // return 0.
  virtual RenderPipelineChannelMode GetChannelMode(size_t c) const = 0;
};

class RenderPipeline {
 public:
  // Initial shifts for the channels (following the same convention as
  // RenderPipelineStage for naming the channels).
  void Init(const std::vector<std::pair<size_t, size_t>>& channel_shifts) {
    JXL_ABORT("Not implemented");
  }

  // Adds a stage to the pipeline. The shifts for all the channels that are not
  // kIgnored by the stage must be identical at this point.
  void AddStage(std::unique_ptr<RenderPipelineStage> stage) {
    JXL_ABORT("Not implemented");
  }

  // Finalizes setup of the pipeline. Shifts for all channels should be 0 at
  // this point.
  void Finalize() { JXL_ABORT("Not implemented"); }

  // Allocates storage to run with `num` threads.
  void PrepareForThreads(size_t num) { JXL_ABORT("Not implemented"); }

  // TBD: run the pipeline for a given input, on a given thread.
  // void Run(Image3F* color_data, ImageF* ec_data, const Rect& input_rect,
  // size_t thread, size_t xpos, size_t ypos) {}
};

}  // namespace jxl

#endif  // LIB_JXL_DEC_RENDER_PIPELINE_H_
