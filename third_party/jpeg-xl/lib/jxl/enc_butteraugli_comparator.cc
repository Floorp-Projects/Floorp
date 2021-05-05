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

#include "lib/jxl/enc_butteraugli_comparator.h"

#include <algorithm>
#include <vector>

#include "lib/jxl/color_management.h"

namespace jxl {

JxlButteraugliComparator::JxlButteraugliComparator(
    const ButteraugliParams& params)
    : params_(params) {}

Status JxlButteraugliComparator::SetReferenceImage(const ImageBundle& ref) {
  const ImageBundle* ref_linear_srgb;
  ImageMetadata metadata = *ref.metadata();
  ImageBundle store(&metadata);
  if (!TransformIfNeeded(ref, ColorEncoding::LinearSRGB(ref.IsGray()),
                         /*pool=*/nullptr, &store, &ref_linear_srgb)) {
    return false;
  }

  comparator_.reset(
      new ButteraugliComparator(ref_linear_srgb->color(), params_));
  xsize_ = ref.xsize();
  ysize_ = ref.ysize();
  return true;
}

Status JxlButteraugliComparator::CompareWith(const ImageBundle& actual,
                                             ImageF* diffmap, float* score) {
  if (!comparator_) {
    return JXL_FAILURE("Must set reference image first");
  }
  if (xsize_ != actual.xsize() || ysize_ != actual.ysize()) {
    return JXL_FAILURE("Images must have same size");
  }

  const ImageBundle* actual_linear_srgb;
  ImageMetadata metadata = *actual.metadata();
  ImageBundle store(&metadata);
  if (!TransformIfNeeded(actual, ColorEncoding::LinearSRGB(actual.IsGray()),
                         /*pool=*/nullptr, &store, &actual_linear_srgb)) {
    return false;
  }

  ImageF temp_diffmap(xsize_, ysize_);
  comparator_->Diffmap(actual_linear_srgb->color(), temp_diffmap);

  if (score != nullptr) {
    *score = ButteraugliScoreFromDiffmap(temp_diffmap, &params_);
  }
  if (diffmap != nullptr) {
    diffmap->Swap(temp_diffmap);
  }

  return true;
}

float JxlButteraugliComparator::GoodQualityScore() const {
  return ButteraugliFuzzyInverse(1.5);
}

float JxlButteraugliComparator::BadQualityScore() const {
  return ButteraugliFuzzyInverse(0.5);
}

float ButteraugliDistance(const ImageBundle& rgb0, const ImageBundle& rgb1,
                          const ButteraugliParams& params, ImageF* distmap,
                          ThreadPool* pool) {
  JxlButteraugliComparator comparator(params);
  return ComputeScore(rgb0, rgb1, &comparator, distmap, pool);
}

float ButteraugliDistance(const CodecInOut& rgb0, const CodecInOut& rgb1,
                          const ButteraugliParams& params, ImageF* distmap,
                          ThreadPool* pool) {
  JxlButteraugliComparator comparator(params);
  JXL_ASSERT(rgb0.frames.size() == rgb1.frames.size());
  float max_dist = 0.0f;
  for (size_t i = 0; i < rgb0.frames.size(); ++i) {
    max_dist = std::max(max_dist, ComputeScore(rgb0.frames[i], rgb1.frames[i],
                                               &comparator, distmap, pool));
  }
  return max_dist;
}

}  // namespace jxl
