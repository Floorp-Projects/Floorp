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

#ifndef LIB_JXL_ENC_PARAMS_H_
#define LIB_JXL_ENC_PARAMS_H_

// Parameters and flags that govern JXL compression.

#include <stddef.h>
#include <stdint.h>

#include <string>

#include "lib/jxl/base/override.h"
#include "lib/jxl/butteraugli/butteraugli.h"
#include "lib/jxl/frame_header.h"
#include "lib/jxl/modular/options.h"

namespace jxl {

enum class SpeedTier {
  // Turns on FindBestQuantizationHQ loop. Equivalent to "guetzli" mode.
  kTortoise = 1,
  // Turns on FindBestQuantization butteraugli loop.
  kKitten = 2,
  // Turns on dots, patches, and spline detection by default, as well as full
  // context clustering. Default.
  kSquirrel = 3,
  // Turns on error diffusion and full AC strategy heuristics. Equivalent to
  // "fast" mode.
  kWombat = 4,
  // Turns on gaborish by default, non-default cmap, initial quant field.
  kHare = 5,
  // Turns on simple heuristics for AC strategy, quant field, and clustering;
  // also enables coefficient reordering.
  kCheetah = 6,
  // Turns off most encoder features, for the fastest possible encoding time.
  kFalcon = 7,
};

inline bool ParseSpeedTier(const std::string& s, SpeedTier* out) {
  if (s == "falcon") {
    *out = SpeedTier::kFalcon;
    return true;
  } else if (s == "cheetah") {
    *out = SpeedTier::kCheetah;
    return true;
  } else if (s == "hare") {
    *out = SpeedTier::kHare;
    return true;
  } else if (s == "fast" || s == "wombat") {
    *out = SpeedTier::kWombat;
    return true;
  } else if (s == "squirrel") {
    *out = SpeedTier::kSquirrel;
    return true;
  } else if (s == "kitten") {
    *out = SpeedTier::kKitten;
    return true;
  } else if (s == "guetzli" || s == "tortoise") {
    *out = SpeedTier::kTortoise;
    return true;
  }
  size_t st = 10 - static_cast<size_t>(strtoull(s.c_str(), nullptr, 0));
  if (st <= static_cast<size_t>(SpeedTier::kFalcon) &&
      st >= static_cast<size_t>(SpeedTier::kTortoise)) {
    *out = SpeedTier(st);
    return true;
  }
  return false;
}

inline const char* SpeedTierName(SpeedTier speed_tier) {
  switch (speed_tier) {
    case SpeedTier::kFalcon:
      return "falcon";
    case SpeedTier::kCheetah:
      return "cheetah";
    case SpeedTier::kHare:
      return "hare";
    case SpeedTier::kWombat:
      return "wombat";
    case SpeedTier::kSquirrel:
      return "squirrel";
    case SpeedTier::kKitten:
      return "kitten";
    case SpeedTier::kTortoise:
      return "tortoise";
  }
  return "INVALID";
}

// NOLINTNEXTLINE(clang-analyzer-optin.performance.Padding)
struct CompressParams {
  float butteraugli_distance = 1.0f;
  size_t target_size = 0;
  float target_bitrate = 0.0f;

  // 0.0 means search for the adaptive quantization map that matches the
  // butteraugli distance, positive values mean quantize everywhere with that
  // value.
  float uniform_quant = 0.0f;
  float quant_border_bias = 0.0f;

  // Try to achieve a maximum pixel-by-pixel error on each channel.
  bool max_error_mode = false;
  float max_error[3] = {0.0, 0.0, 0.0};

  SpeedTier speed_tier = SpeedTier::kSquirrel;

  // 0 = default.
  // 1 = slightly worse quality.
  // 4 = fastest speed, lowest quality
  // TODO(veluca): hook this up to the C API.
  size_t decoding_speed_tier = 0;

  int max_butteraugli_iters = 4;

  int max_butteraugli_iters_guetzli_mode = 100;

  ColorTransform color_transform = ColorTransform::kXYB;
  YCbCrChromaSubsampling chroma_subsampling;

  // If true, the "modular mode options" members below are used.
  bool modular_mode = false;

  // Change group size in modular mode (0=128, 1=256, 2=512, 3=1024).
  size_t modular_group_size_shift = 1;

  Override preview = Override::kDefault;
  Override noise = Override::kDefault;
  Override dots = Override::kDefault;
  Override patches = Override::kDefault;
  Override gaborish = Override::kDefault;
  int epf = -1;

  // TODO(deymo): Remove "gradient" once all clients stop setting this value.
  // This flag is already deprecated and is unused in the encoder.
  Override gradient = Override::kOff;

  // Progressive mode.
  bool progressive_mode = false;

  // Quantized-progressive mode.
  bool qprogressive_mode = false;

  // Put center groups first in the bitstream.
  bool middleout = false;

  int progressive_dc = -1;

  // Ensure invisible pixels are not set to 0.
  bool keep_invisible = false;

  // Progressive-mode saliency.
  //
  // How many progressive saliency-encoding steps to perform.
  // - 1: Encode only DC and lowest-frequency AC. Does not need a saliency-map.
  // - 2: Encode only DC+LF, dropping all HF AC data.
  //      Does not need a saliency-map.
  // - 3: Encode DC+LF+{salient HF}, dropping all non-salient HF data.
  // - 4: Encode DC+LF+{salient HF}+{other HF}.
  // - 5: Encode DC+LF+{quantized HF}+{low HF bits}.
  size_t saliency_num_progressive_steps = 3;
  // Every saliency-heatmap cell with saliency >= threshold will be considered
  // as 'salient'. The default value of 0.0 will consider every AC-block
  // as salient, hence not require a saliency-map, and not actually generate
  // a 4th progressive step.
  float saliency_threshold = 0.0f;
  // Saliency-map (owned by caller).
  ImageF* saliency_map = nullptr;

  // Input and output file name. Will be used to provide pluggable saliency
  // extractor with paths.
  const char* file_in = nullptr;
  const char* file_out = nullptr;

  // Currently unused as of 2020-01.
  bool clear_metadata = false;

  // Prints extra information during/after encoding.
  bool verbose = false;

  ButteraugliParams ba_params;

  // Force usage of CfL when doing JPEG recompression. This can have unexpected
  // effects on the decoded pixels, while still being JPEG-compliant and
  // allowing reconstruction of the original JPEG.
  bool force_cfl_jpeg_recompression = true;

  // modular mode options below
  ModularOptions options;
  int responsive = -1;
  // A pair of <quality, cquality>.
  std::pair<float, float> quality_pair{100.f, 100.f};
  int colorspace = -1;
  // Use Global channel palette if #colors < this percentage of range
  float channel_colors_pre_transform_percent = 95.f;
  // Use Local channel palette if #colors < this percentage of range
  float channel_colors_percent = 80.f;
  int near_lossless = 0;
  int palette_colors = 1 << 10;  // up to 10-bit palette is probably worthwhile
  bool lossy_palette = false;

  // Returns whether these params are lossless as defined by SetLossless();
  bool IsLossless() const {
    return modular_mode && quality_pair.first == 100 &&
           quality_pair.second == 100 &&
           color_transform == jxl::ColorTransform::kNone;
  }

  // Sets the parameters required to make the codec lossless.
  void SetLossless() {
    modular_mode = true;
    quality_pair.first = 100;
    quality_pair.second = 100;
    color_transform = jxl::ColorTransform::kNone;
  }

  bool use_new_heuristics = false;

  // Down/upsample the image before encoding / after decoding by this factor.
  size_t resampling = 1;
};

static constexpr float kMinButteraugliForDynamicAR = 0.5f;
static constexpr float kMinButteraugliForDots = 3.0f;
static constexpr float kMinButteraugliToSubtractOriginalPatches = 3.0f;
static constexpr float kMinButteraugliDistanceForProgressiveDc = 4.5f;

// Always off
static constexpr float kMinButteraugliForNoise = 99.0f;

// Minimum butteraugli distance the encoder accepts.
static constexpr float kMinButteraugliDistance = 0.01f;

// Tile size for encoder-side processing. Must be equal to color tile dim in the
// current implementation.
static constexpr size_t kEncTileDim = 64;
static constexpr size_t kEncTileDimInBlocks = kEncTileDim / kBlockDim;

}  // namespace jxl

#endif  // LIB_JXL_ENC_PARAMS_H_
