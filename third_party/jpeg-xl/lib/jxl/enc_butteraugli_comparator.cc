// Copyright (c) the JPEG XL Project Authors. All rights reserved.
//
// Use of this source code is governed by a BSD-style
// license that can be found in the LICENSE file.

#include "lib/jxl/enc_butteraugli_comparator.h"

#include "lib/jxl/base/status.h"
#include "lib/jxl/enc_image_bundle.h"

namespace jxl {

JxlButteraugliComparator::JxlButteraugliComparator(
    const ButteraugliParams& params, const JxlCmsInterface& cms)
    : params_(params), cms_(cms) {}

Status JxlButteraugliComparator::SetReferenceImage(const ImageBundle& ref) {
  const ImageBundle* ref_linear_srgb;
  ImageMetadata metadata = *ref.metadata();
  ImageBundle store(&metadata);
  if (!TransformIfNeeded(ref, ColorEncoding::LinearSRGB(ref.IsGray()), cms_,
                         /*pool=*/nullptr, &store, &ref_linear_srgb)) {
    return false;
  }
  JXL_ASSIGN_OR_RETURN(comparator_, ButteraugliComparator::Make(
                                        ref_linear_srgb->color(), params_));
  xsize_ = ref.xsize();
  ysize_ = ref.ysize();
  return true;
}

Status JxlButteraugliComparator::SetLinearReferenceImage(
    const Image3F& linear) {
  JXL_ASSIGN_OR_RETURN(comparator_,
                       ButteraugliComparator::Make(linear, params_));
  xsize_ = linear.xsize();
  ysize_ = linear.ysize();
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
                         cms_,
                         /*pool=*/nullptr, &store, &actual_linear_srgb)) {
    return false;
  }

  JXL_ASSIGN_OR_RETURN(ImageF temp_diffmap, ImageF::Create(xsize_, ysize_));
  JXL_RETURN_IF_ERROR(
      comparator_->Diffmap(actual_linear_srgb->color(), temp_diffmap));

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

}  // namespace jxl
