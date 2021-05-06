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

#include <algorithm>

#include "lib/jxl/ans_params.h"
#include "lib/jxl/base/status.h"
#include "lib/jxl/chroma_from_luma.h"
#include "lib/jxl/common.h"
#include "lib/jxl/dct_scales.h"
#include "lib/jxl/enc_ans.h"
#include "lib/jxl/entropy_coder.h"
#include "lib/jxl/opsin_params.h"
#include "lib/jxl/splines.h"

namespace jxl {

class QuantizedSplineEncoder {
 public:
  // Only call if HasAny().
  static void Tokenize(const QuantizedSpline& spline,
                       std::vector<Token>* const tokens) {
    tokens->emplace_back(kNumControlPointsContext,
                         spline.control_points_.size());
    for (const auto& point : spline.control_points_) {
      tokens->emplace_back(kControlPointsContext, PackSigned(point.first));
      tokens->emplace_back(kControlPointsContext, PackSigned(point.second));
    }
    const auto encode_dct = [tokens](const int dct[32]) {
      for (int i = 0; i < 32; ++i) {
        tokens->emplace_back(kDCTContext, PackSigned(dct[i]));
      }
    };
    for (int c = 0; c < 3; ++c) {
      encode_dct(spline.color_dct_[c]);
    }
    encode_dct(spline.sigma_dct_);
  }
};

namespace {

void EncodeAllStartingPoints(const std::vector<Spline::Point>& points,
                             std::vector<Token>* tokens) {
  int64_t last_x = 0;
  int64_t last_y = 0;
  for (size_t i = 0; i < points.size(); i++) {
    const int64_t x = lroundf(points[i].x);
    const int64_t y = lroundf(points[i].y);
    if (i == 0) {
      tokens->emplace_back(kStartingPositionContext, x);
      tokens->emplace_back(kStartingPositionContext, y);
    } else {
      tokens->emplace_back(kStartingPositionContext, PackSigned(x - last_x));
      tokens->emplace_back(kStartingPositionContext, PackSigned(y - last_y));
    }
    last_x = x;
    last_y = y;
  }
}

}  // namespace

void EncodeSplines(const Splines& splines, BitWriter* writer,
                   const size_t layer, const HistogramParams& histogram_params,
                   AuxOut* aux_out) {
  JXL_ASSERT(splines.HasAny());

  const std::vector<QuantizedSpline>& quantized_splines =
      splines.QuantizedSplines();
  std::vector<std::vector<Token>> tokens(1);
  tokens[0].emplace_back(kNumSplinesContext, quantized_splines.size() - 1);
  EncodeAllStartingPoints(splines.StartingPoints(), &tokens[0]);

  tokens[0].emplace_back(kQuantizationAdjustmentContext,
                         PackSigned(splines.GetQuantizationAdjustment()));

  for (const QuantizedSpline& spline : quantized_splines) {
    QuantizedSplineEncoder::Tokenize(spline, &tokens[0]);
  }

  EntropyEncodingData codes;
  std::vector<uint8_t> context_map;
  BuildAndEncodeHistograms(histogram_params, kNumSplineContexts, tokens, &codes,
                           &context_map, writer, layer, aux_out);
  WriteTokens(tokens[0], codes, context_map, writer, layer, aux_out);
}

Splines FindSplines(const Image3F& opsin) {
  // TODO: implement spline detection.
  return {};
}

}  // namespace jxl
