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

#ifndef LIB_JXL_SPLINES_H_
#define LIB_JXL_SPLINES_H_

#include <stddef.h>
#include <stdint.h>

#include <utility>
#include <vector>

#include "lib/jxl/ans_params.h"
#include "lib/jxl/aux_out.h"
#include "lib/jxl/aux_out_fwd.h"
#include "lib/jxl/base/status.h"
#include "lib/jxl/chroma_from_luma.h"
#include "lib/jxl/dec_ans.h"
#include "lib/jxl/dec_bit_reader.h"
#include "lib/jxl/entropy_coder.h"
#include "lib/jxl/image.h"

namespace jxl {

static constexpr float kDesiredRenderingDistance = 1.f;

enum SplineEntropyContexts : size_t {
  kQuantizationAdjustmentContext = 0,
  kStartingPositionContext,
  kNumSplinesContext,
  kNumControlPointsContext,
  kControlPointsContext,
  kDCTContext,
  kNumSplineContexts
};

struct Spline {
  struct Point {
    Point() : x(0.0f), y(0.0f) {}
    Point(float x, float y) : x(x), y(y) {}
    float x, y;
    bool operator==(const Point& other) const {
      return std::fabs(x - other.x) < 1e-3f && std::fabs(y - other.y) < 1e-3f;
    }
  };
  std::vector<Point> control_points;
  // X, Y, B.
  float color_dct[3][32];
  // Splines are draws by normalized Gaussian splatting. This controls the
  // Gaussian's parameter along the spline.
  float sigma_dct[32];
};

class QuantizedSplineEncoder;

class QuantizedSpline {
 public:
  QuantizedSpline() = default;
  explicit QuantizedSpline(const Spline& original,
                           int32_t quantization_adjustment, float ytox,
                           float ytob);

  Spline Dequantize(const Spline::Point& starting_point,
                    int32_t quantization_adjustment, float ytox,
                    float ytob) const;

  Status Decode(const std::vector<uint8_t>& context_map,
                ANSSymbolReader* decoder, BitReader* br,
                size_t max_control_points, size_t* total_num_control_points);

 private:
  friend class QuantizedSplineEncoder;

  std::vector<std::pair<int64_t, int64_t>>
      control_points_;  // Double delta-encoded.
  int color_dct_[3][32] = {};
  int sigma_dct_[32] = {};
};

class Splines {
 public:
  Splines() = default;
  explicit Splines(const int32_t quantization_adjustment,
                   std::vector<QuantizedSpline> splines,
                   std::vector<Spline::Point> starting_points)
      : quantization_adjustment_(quantization_adjustment),
        splines_(std::move(splines)),
        starting_points_(std::move(starting_points)) {}

  bool HasAny() const { return !splines_.empty(); }

  Status Decode(BitReader* br, size_t num_pixels);

  Status AddTo(Image3F* opsin, const Rect& opsin_rect, const Rect& image_rect,
               const ColorCorrelationMap& cmap) const;
  Status SubtractFrom(Image3F* opsin, const ColorCorrelationMap& cmap) const;

  const std::vector<QuantizedSpline>& QuantizedSplines() const {
    return splines_;
  }
  const std::vector<Spline::Point>& StartingPoints() const {
    return starting_points_;
  }

  int32_t GetQuantizationAdjustment() const { return quantization_adjustment_; }

 private:
  template <bool>
  Status Apply(Image3F* opsin, const Rect& opsin_rect, const Rect& image_rect,
               const ColorCorrelationMap& cmap) const;

  // If positive, quantization weights are multiplied by 1 + this/8, which
  // increases precision. If negative, they are divided by 1 - this/8. If 0,
  // they are unchanged.
  int32_t quantization_adjustment_ = 0;
  std::vector<QuantizedSpline> splines_;
  std::vector<Spline::Point> starting_points_;
};

}  // namespace jxl

#endif  // LIB_JXL_SPLINES_H_
