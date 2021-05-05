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

#ifndef LIB_JXL_MODULAR_OPTIONS_H_
#define LIB_JXL_MODULAR_OPTIONS_H_

#include <stdint.h>

#include <array>
#include <vector>

namespace jxl {

using PropertyVal = int32_t;
using Properties = std::vector<PropertyVal>;

enum class Predictor : uint32_t {
  Zero = 0,
  Left = 1,
  Top = 2,
  Average0 = 3,
  Select = 4,
  Gradient = 5,
  Weighted = 6,
  TopRight = 7,
  TopLeft = 8,
  LeftLeft = 9,
  Average1 = 10,
  Average2 = 11,
  Average3 = 12,
  Average4 = 13,
  // The following predictors are encoder-only.
  Best = 14,  // Best of Gradient and Weighted
  Variable =
      15,  // Find the best decision tree for predictors/predictor per row
};

inline const char* PredictorName(Predictor p) {
  switch (p) {
    case Predictor::Zero:
      return "Zero";
    case Predictor::Left:
      return "Left";
    case Predictor::Top:
      return "Top";
    case Predictor::Average0:
      return "Avg0";
    case Predictor::Average1:
      return "Avg1";
    case Predictor::Average2:
      return "Avg2";
    case Predictor::Average3:
      return "Avg3";
    case Predictor::Average4:
      return "Avg4";
    case Predictor::Select:
      return "Sel";
    case Predictor::Gradient:
      return "Grd";
    case Predictor::Weighted:
      return "Wgh";
    case Predictor::TopLeft:
      return "TopL";
    case Predictor::TopRight:
      return "TopR";
    case Predictor::LeftLeft:
      return "LL";
    default:
      return "INVALID";
  };
}

inline std::array<uint8_t, 3> PredictorColor(Predictor p) {
  switch (p) {
    case Predictor::Zero:
      return {0, 0, 0};
    case Predictor::Left:
      return {255, 0, 0};
    case Predictor::Top:
      return {0, 255, 0};
    case Predictor::Average0:
      return {0, 0, 255};
    case Predictor::Average4:
      return {192, 128, 128};
    case Predictor::Select:
      return {255, 255, 0};
    case Predictor::Gradient:
      return {255, 0, 255};
    case Predictor::Weighted:
      return {0, 255, 255};
      // TODO
    default:
      return {255, 255, 255};
  };
}

constexpr size_t kNumModularPredictors = static_cast<size_t>(Predictor::Best);

static constexpr ssize_t kNumStaticProperties = 2;  // channel, group_id.

using StaticPropRange =
    std::array<std::array<uint32_t, 2>, kNumStaticProperties>;

struct ModularMultiplierInfo {
  StaticPropRange range;
  uint32_t multiplier;
};

struct ModularOptions {
  /// Used in both encode and decode:

  // Stop encoding/decoding when reaching a (non-meta) channel that has a
  // dimension bigger than max_chan_size.
  size_t max_chan_size = 0xFFFFFF;

  /// Encode options:
  // Fraction of pixels to look at to learn a MA tree
  // Number of iterations to do to learn a MA tree
  // (if zero there is no MA context model)
  float nb_repeats = .5f;

  // Maximum number of (previous channel) properties to use in the MA trees
  int max_properties = 0;  // no previous channels

  // Alternative heuristic tweaks.
  // Properties default to channel, group, weighted, gradient residual, W-NW,
  // NW-N, N-NE, N-NN
  std::vector<uint32_t> splitting_heuristics_properties = {0,  1,  15, 9,
                                                           10, 11, 12, 13};
  float splitting_heuristics_node_threshold = 96;
  size_t max_property_values = 32;

  // Predictor to use for each channel.
  Predictor predictor = static_cast<Predictor>(-1);

  int wp_mode = 0;

  float fast_decode_multiplier = 1.01f;

  // Forces the encoder to produce a tree that is compatible with the WP-only
  // decode path (or with the no-wp path).
  enum class WPTreeMode { kWPOnly, kNoWP, kDefault };
  WPTreeMode wp_tree_mode = WPTreeMode::kDefault;

  // Skip fast paths in the encoder.
  bool skip_encoder_fast_path = false;

  // Kind of tree to use.
  // TODO(veluca): add tree kinds for JPEG recompression with CfL enabled,
  // general AC metadata, different DC qualities, and others.
  enum class TreeKind {
    kLearn,
    kJpegTranscodeACMeta,
    kFalconACMeta,
    kACMeta,
    kWPFixedDC,
    kGradientFixedDC,
  };
  TreeKind tree_kind = TreeKind::kLearn;

  // Ignore the image and just pretend all tokens are zeroes
  bool zero_tokens = false;
};

}  // namespace jxl

#endif  // LIB_JXL_MODULAR_OPTIONS_H_
