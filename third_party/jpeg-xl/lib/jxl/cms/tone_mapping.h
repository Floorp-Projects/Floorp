// Copyright (c) the JPEG XL Project Authors. All rights reserved.
//
// Use of this source code is governed by a BSD-style
// license that can be found in the LICENSE file.

#ifndef LIB_JXL_CMS_TONE_MAPPING_H_
#define LIB_JXL_CMS_TONE_MAPPING_H_

#include <algorithm>
#include <cmath>
#include <utility>

#include "lib/jxl/base/common.h"
#include "lib/jxl/base/compiler_specific.h"
#include "lib/jxl/cms/transfer_functions.h"

namespace jxl {

class Rec2408ToneMapperBase {
 public:
  explicit Rec2408ToneMapperBase(std::pair<float, float> source_range,
                                 std::pair<float, float> target_range,
                                 const float primaries_luminances[3])
      : source_range_(source_range),
        target_range_(target_range),
        red_Y_(primaries_luminances[0]),
        green_Y_(primaries_luminances[1]),
        blue_Y_(primaries_luminances[2]) {}

  // TODO(eustas): test me
  void ToneMap(float* red, float* green, float* blue) const {
    const float luminance =
        source_range_.second *
        (red_Y_ * *red + green_Y_ * *green + blue_Y_ * *blue);
    const float normalized_pq =
        std::min(1.f, (InvEOTF(luminance) - pq_mastering_min_) *
                          inv_pq_mastering_range_);
    const float e2 = (normalized_pq < ks_) ? normalized_pq : P(normalized_pq);
    const float one_minus_e2 = 1 - e2;
    const float one_minus_e2_2 = one_minus_e2 * one_minus_e2;
    const float one_minus_e2_4 = one_minus_e2_2 * one_minus_e2_2;
    const float e3 = min_lum_ * one_minus_e2_4 + e2;
    const float e4 = e3 * pq_mastering_range_ + pq_mastering_min_;
    const float d4 =
        TF_PQ_Base::DisplayFromEncoded(/*display_intensity_target=*/1.0, e4);
    const float new_luminance = Clamp1(d4, 0.f, target_range_.second);
    const float min_luminance = 1e-6f;
    const bool use_cap = (luminance <= min_luminance);
    const float ratio = new_luminance / std::max(luminance, min_luminance);
    const float cap = new_luminance * inv_target_peak_;
    const float multiplier = ratio * normalizer_;
    for (float* const val : {red, green, blue}) {
      *val = use_cap ? cap : *val * multiplier;
    }
  }

 protected:
  float InvEOTF(const float luminance) const {
    return TF_PQ_Base::EncodedFromDisplay(/*display_intensity_target=*/1.0,
                                          luminance);
  }
  float T(const float a) const { return (a - ks_) * inv_one_minus_ks_; }
  float P(const float b) const {
    const float t_b = T(b);
    const float t_b_2 = t_b * t_b;
    const float t_b_3 = t_b_2 * t_b;
    return (2 * t_b_3 - 3 * t_b_2 + 1) * ks_ +
           (t_b_3 - 2 * t_b_2 + t_b) * (1 - ks_) +
           (-2 * t_b_3 + 3 * t_b_2) * max_lum_;
  }

  const std::pair<float, float> source_range_;
  const std::pair<float, float> target_range_;
  const float red_Y_;
  const float green_Y_;
  const float blue_Y_;

  const float pq_mastering_min_ = InvEOTF(source_range_.first);
  const float pq_mastering_max_ = InvEOTF(source_range_.second);
  const float pq_mastering_range_ = pq_mastering_max_ - pq_mastering_min_;
  const float inv_pq_mastering_range_ = 1.0f / pq_mastering_range_;
  // TODO(eustas): divide instead of inverse-multiply?
  const float min_lum_ = (InvEOTF(target_range_.first) - pq_mastering_min_) *
                         inv_pq_mastering_range_;
  // TODO(eustas): divide instead of inverse-multiply?
  const float max_lum_ = (InvEOTF(target_range_.second) - pq_mastering_min_) *
                         inv_pq_mastering_range_;
  const float ks_ = 1.5f * max_lum_ - 0.5f;

  const float inv_one_minus_ks_ = 1.0f / std::max(1e-6f, 1.0f - ks_);

  const float normalizer_ = source_range_.second / target_range_.second;
  const float inv_target_peak_ = 1.f / target_range_.second;
};

class HlgOOTF_Base {
 public:
  explicit HlgOOTF_Base(float source_luminance, float target_luminance,
                        const float primaries_luminances[3])
      : HlgOOTF_Base(/*gamma=*/std::pow(1.111f, std::log2(target_luminance /
                                                          source_luminance)),
                     primaries_luminances) {}

  // TODO(eustas): test me
  void Apply(float* red, float* green, float* blue) const {
    if (!apply_ootf_) return;
    const float luminance = red_Y_ * *red + green_Y_ * *green + blue_Y_ * *blue;
    const float ratio = std::min<float>(powf(luminance, exponent_), 1e9);
    *red *= ratio;
    *green *= ratio;
    *blue *= ratio;
  }

 protected:
  explicit HlgOOTF_Base(float gamma, const float luminances[3])
      : exponent_(gamma - 1),
        red_Y_(luminances[0]),
        green_Y_(luminances[1]),
        blue_Y_(luminances[2]) {}
  const float exponent_;
  const bool apply_ootf_ = exponent_ < -0.01f || 0.01f < exponent_;
  const float red_Y_;
  const float green_Y_;
  const float blue_Y_;
};

static JXL_MAYBE_UNUSED void GamutMapScalar(float* red, float* green,
                                            float* blue,
                                            const float primaries_luminances[3],
                                            float preserve_saturation = 0.1f) {
  const float luminance = primaries_luminances[0] * *red +
                          primaries_luminances[1] * *green +
                          primaries_luminances[2] * *blue;

  // Desaturate out-of-gamut pixels. This is done by mixing each pixel
  // with just enough gray of the target luminance to make all
  // components non-negative.
  // - For saturation preservation, if a component is still larger than
  // 1 then the pixel is normalized to have a maximum component of 1.
  // That will reduce its luminance.
  // - For luminance preservation, getting all components below 1 is
  // done by mixing in yet more gray. That will desaturate it further.
  float gray_mix_saturation = 0.0f;
  float gray_mix_luminance = 0.0f;
  for (const float* ch : {red, green, blue}) {
    const float& val = *ch;
    const float val_minus_gray = val - luminance;
    const float inv_val_minus_gray =
        1.0f / ((val_minus_gray == 0.0f) ? 1.0f : val_minus_gray);
    const float val_over_val_minus_gray = val * inv_val_minus_gray;
    gray_mix_saturation =
        (val_minus_gray >= 0.0f)
            ? gray_mix_saturation
            : std::max(gray_mix_saturation, val_over_val_minus_gray);
    gray_mix_luminance =
        std::max(gray_mix_luminance,
                 (val_minus_gray <= 0.0f)
                     ? gray_mix_saturation
                     : (val_over_val_minus_gray - inv_val_minus_gray));
  }
  const float gray_mix =
      Clamp1((preserve_saturation * (gray_mix_saturation - gray_mix_luminance) +
              gray_mix_luminance),
             0.0f, 1.0f);
  for (float* const ch : {red, green, blue}) {
    float& val = *ch;
    val = gray_mix * (luminance - val) + val;
  }
  const float max_clr = std::max({1.0f, *red, *green, *blue});
  const float normalizer = 1.0f / max_clr;
  for (float* const ch : {red, green, blue}) {
    float& val = *ch;
    val *= normalizer;
  }
}

}  // namespace jxl

#endif  // LIB_JXL_CMS_TONE_MAPPING_H_
