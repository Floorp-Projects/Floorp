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

#ifndef LIB_JXL_ENC_COMPARATOR_H_
#define LIB_JXL_ENC_COMPARATOR_H_

#include "lib/jxl/base/data_parallel.h"
#include "lib/jxl/base/status.h"
#include "lib/jxl/image.h"
#include "lib/jxl/image_bundle.h"

namespace jxl {

class Comparator {
 public:
  virtual ~Comparator() = default;

  // Sets the reference image, the first to compare
  // Image must be in linear sRGB (gamma expanded) in range 0.0f-1.0f as
  // the range from standard black point to standard white point, but values
  // outside permitted.
  virtual Status SetReferenceImage(const ImageBundle& ref) = 0;

  // Sets the actual image (with loss), the second to compare
  // Image must be in linear sRGB (gamma expanded) in range 0.0f-1.0f as
  // the range from standard black point to standard white point, but values
  // outside permitted.
  // In diffmap it outputs the local score per pixel, while in score it outputs
  // a single score. Any one may be set to nullptr to not compute it.
  virtual Status CompareWith(const ImageBundle& actual, ImageF* diffmap,
                             float* score) = 0;

  // Quality thresholds for diffmap and score values.
  // The good score must represent a value where the images are considered to
  // be perceptually indistinguishable (but not identical)
  // The bad value must be larger than good to indicate "lower means better"
  // and smaller than good to indicate "higher means better"
  virtual float GoodQualityScore() const = 0;
  virtual float BadQualityScore() const = 0;
};

// Computes the score given images in any RGB color model, optionally with
// alpha channel.
float ComputeScore(const ImageBundle& rgb0, const ImageBundle& rgb1,
                   Comparator* comparator, ImageF* diffmap = nullptr,
                   ThreadPool* pool = nullptr);

}  // namespace jxl

#endif  // LIB_JXL_ENC_COMPARATOR_H_
