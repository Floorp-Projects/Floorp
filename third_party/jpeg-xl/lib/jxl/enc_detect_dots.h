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

// We attempt to remove dots, or speckle from images using Gaussian blur.
#ifndef LIB_JXL_ENC_DETECT_DOTS_H_
#define LIB_JXL_ENC_DETECT_DOTS_H_

#include <stddef.h>
#include <stdint.h>

#include <array>
#include <vector>

#include "lib/jxl/base/data_parallel.h"
#include "lib/jxl/dec_patch_dictionary.h"
#include "lib/jxl/image.h"

namespace jxl {

struct GaussianDetectParams {
  double t_high = 0;  // at least one pixel must have larger energy than t_high
  double t_low = 0;   // all pixels must have a larger energy than tLow
  uint32_t maxWinSize = 0;  // discard dots larger than this containing window
  double maxL2Loss = 0;
  double maxCustomLoss = 0;
  double minIntensity = 0;     // If the intensity is too low, discard it
  double maxDistMeanMode = 0;  // The mean and the mode must be close
  size_t maxNegPixels = 0;     // Maximum number of negative pixel
  size_t minScore = 0;
  size_t maxCC = 50;   // Maximum number of CC to keep
  size_t percCC = 15;  // Percentage in [0,100] of CC to keep
};

// Ellipse Quantization Params
struct EllipseQuantParams {
  size_t xsize;      // Image size in x
  size_t ysize;      // Image size in y
  size_t qPosition;  // Position quantization delta
  // Quantization for the Gaussian sigma parameters
  double minSigma;
  double maxSigma;
  size_t qSigma;  // number of quantization levels
  // Quantization for the rotation angle (between -pi and pi)
  size_t qAngle;
  // Quantization for the intensity
  std::array<double, 3> minIntensity;
  std::array<double, 3> maxIntensity;
  std::array<size_t, 3> qIntensity;  // number of quantization levels
  // Extra parameters for the encoding
  bool subtractQuantized;  // Should we subtract quantized or detected dots?
  float ytox;
  float ytob;

  void QuantPositionSize(size_t* xsize, size_t* ysize) const;
};

// Detects dots in XYB image.
std::vector<PatchInfo> DetectGaussianEllipses(
    const Image3F& opsin, const GaussianDetectParams& params,
    const EllipseQuantParams& qParams, ThreadPool* pool);

}  // namespace jxl

#endif  // LIB_JXL_ENC_DETECT_DOTS_H_
