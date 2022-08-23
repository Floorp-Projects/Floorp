// Copyright (c) the JPEG XL Project Authors. All rights reserved.
//
// Use of this source code is governed by a BSD-style
// license that can be found in the LICENSE file.

#if defined(LIB_JXL_DEC_TONE_MAPPING_INL_H_) == defined(HWY_TARGET_TOGGLE)
#ifdef LIB_JXL_DEC_TONE_MAPPING_INL_H_
#undef LIB_JXL_DEC_TONE_MAPPING_INL_H_
#else
#define LIB_JXL_DEC_TONE_MAPPING_INL_H_
#endif

#include <hwy/highway.h>

#include "lib/jxl/transfer_functions-inl.h"

HWY_BEFORE_NAMESPACE();
namespace jxl {
namespace HWY_NAMESPACE {
namespace {

template <typename D>
class Rec2408ToneMapper {
 private:
  using V = hwy::HWY_NAMESPACE::Vec<D>;

 public:
  explicit Rec2408ToneMapper(std::pair<float, float> source_range,
                             std::pair<float, float> target_range,
                             const float primaries_luminances[3])
      : source_range_(source_range),
        target_range_(target_range),
        red_Y_(primaries_luminances[0]),
        green_Y_(primaries_luminances[1]),
        blue_Y_(primaries_luminances[2]) {}

  void ToneMap(V* red, V* green, V* blue) const {
    const V luminance =
        Set(df_, source_range_.second) *
        (MulAdd(Set(df_, red_Y_), *red,
                MulAdd(Set(df_, green_Y_), *green, Set(df_, blue_Y_) * *blue)));
    const V normalized_pq =
        Min(Set(df_, 1.f),
            (InvEOTF(luminance) - pq_mastering_min_) * inv_pq_mastering_range_);
    const V e2 =
        IfThenElse(normalized_pq < ks_, normalized_pq, P(normalized_pq));
    const V one_minus_e2 = Set(df_, 1) - e2;
    const V one_minus_e2_2 = one_minus_e2 * one_minus_e2;
    const V one_minus_e2_4 = one_minus_e2_2 * one_minus_e2_2;
    const V e3 = MulAdd(b_, one_minus_e2_4, e2);
    const V e4 = MulAdd(e3, pq_mastering_range_, pq_mastering_min_);
    const V new_luminance = Min(
        Set(df_, target_range_.second),
        ZeroIfNegative(Set(df_, 10000) * TF_PQ().DisplayFromEncoded(df_, e4)));

    const V ratio = new_luminance / luminance;

    for (V* const val : {red, green, blue}) {
      *val = IfThenElse(luminance <= Set(df_, 1e-6f), new_luminance,
                        *val * ratio) *
             normalizer_;
    }
  }

 private:
  V InvEOTF(const V luminance) const {
    return TF_PQ().EncodedFromDisplay(df_, luminance * Set(df_, 1. / 10000));
  }
  V T(const V a) const { return (a - ks_) * inv_one_minus_ks_; }
  V P(const V b) const {
    const V t_b = T(b);
    const V t_b_2 = t_b * t_b;
    const V t_b_3 = t_b_2 * t_b;
    return MulAdd(
        MulAdd(Set(df_, 2), t_b_3, MulAdd(Set(df_, -3), t_b_2, Set(df_, 1))),
        ks_,
        MulAdd(t_b_3 + MulAdd(Set(df_, -2), t_b_2, t_b), Set(df_, 1) - ks_,
               MulAdd(Set(df_, -2), t_b_3, Set(df_, 3) * t_b_2) * max_lum_));
  }

  D df_;
  const std::pair<float, float> source_range_;
  const std::pair<float, float> target_range_;
  const float red_Y_;
  const float green_Y_;
  const float blue_Y_;

  const V pq_mastering_min_ = InvEOTF(Set(df_, source_range_.first));
  const V pq_mastering_max_ = InvEOTF(Set(df_, source_range_.second));
  const V pq_mastering_range_ = pq_mastering_max_ - pq_mastering_min_;
  const V inv_pq_mastering_range_ =
      Set(df_, 1) / (pq_mastering_max_ - pq_mastering_min_);
  const V min_lum_ =
      (InvEOTF(Set(df_, target_range_.first)) - pq_mastering_min_) *
      inv_pq_mastering_range_;
  const V max_lum_ =
      (InvEOTF(Set(df_, target_range_.second)) - pq_mastering_min_) *
      inv_pq_mastering_range_;
  const V ks_ = MulAdd(Set(df_, 1.5f), max_lum_, Set(df_, -0.5f));
  const V b_ = min_lum_;

  const V inv_one_minus_ks_ =
      Set(df_, 1) / Max(Set(df_, 1e-6f), Set(df_, 1) - ks_);

  const V normalizer_ = Set(df_, source_range_.second / target_range_.second);
};

class HlgOOTF {
 public:
  explicit HlgOOTF(float source_luminance, float target_luminance,
                   const float primaries_luminances[3])
      : HlgOOTF(/*gamma=*/std::pow(
                    1.111f, std::log2(target_luminance / source_luminance)),
                primaries_luminances) {}

  static HlgOOTF FromSceneLight(float display_luminance,
                                const float primaries_luminances[3]) {
    return HlgOOTF(/*gamma=*/1.2f *
                       std::pow(1.111f, std::log2(display_luminance / 1000.f)),
                   primaries_luminances);
  }

  static HlgOOTF ToSceneLight(float display_luminance,
                              const float primaries_luminances[3]) {
    return HlgOOTF(
        /*gamma=*/(1 / 1.2f) *
            std::pow(1.111f, -std::log2(display_luminance / 1000.f)),
        primaries_luminances);
  }

  template <typename V>
  void Apply(V* red, V* green, V* blue) const {
    hwy::HWY_NAMESPACE::DFromV<V> df;
    if (!apply_ootf_) return;
    const V luminance =
        MulAdd(Set(df, red_Y_), *red,
               MulAdd(Set(df, green_Y_), *green, Set(df, blue_Y_) * *blue));
    const V ratio =
        Min(FastPowf(df, luminance, Set(df, exponent_)), Set(df, 1e9));
    *red *= ratio;
    *green *= ratio;
    *blue *= ratio;
  }

  bool WarrantsGamutMapping() const { return apply_ootf_ && exponent_ < 0; }

 private:
  explicit HlgOOTF(float gamma, const float luminances[3])
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

template <typename V>
void GamutMap(V* red, V* green, V* blue, const float primaries_luminances[3],
              float preserve_saturation = 0.1f) {
  hwy::HWY_NAMESPACE::DFromV<V> df;
  const V luminance = MulAdd(Set(df, primaries_luminances[0]), *red,
                             MulAdd(Set(df, primaries_luminances[1]), *green,
                                    Set(df, primaries_luminances[2]) * *blue));

  // Desaturate out-of-gamut pixels. This is done by mixing each pixel
  // with just enough gray of the target luminance to make all
  // components non-negative.
  // - For saturation preservation, if a component is still larger than
  // 1 then the pixel is normalized to have a maximum component of 1.
  // That will reduce its luminance.
  // - For luminance preservation, getting all components below 1 is
  // done by mixing in yet more gray. That will desaturate it further.
  V gray_mix_saturation = Zero(df);
  V gray_mix_luminance = Zero(df);
  for (const V val : {*red, *green, *blue}) {
    const V inv_val_minus_gray = Set(df, 1) / (val - luminance);
    gray_mix_saturation =
        IfThenElse(val >= luminance, gray_mix_saturation,
                   Max(gray_mix_saturation, val * inv_val_minus_gray));
    gray_mix_luminance =
        Max(gray_mix_luminance,
            IfThenElse(val <= luminance, gray_mix_saturation,
                       (val - Set(df, 1)) * inv_val_minus_gray));
  }
  const V gray_mix = Clamp(Set(df, preserve_saturation) *
                                   (gray_mix_saturation - gray_mix_luminance) +
                               gray_mix_luminance,
                           Zero(df), Set(df, 1));
  for (V* const val : {red, green, blue}) {
    *val = MulAdd(gray_mix, luminance - *val, *val);
  }
  const V normalizer =
      Set(df, 1) / Max(Set(df, 1), Max(*red, Max(*green, *blue)));
  for (V* const val : {red, green, blue}) {
    *val *= normalizer;
  }
}

}  // namespace
// NOLINTNEXTLINE(google-readability-namespace-comments)
}  // namespace HWY_NAMESPACE
}  // namespace jxl
HWY_AFTER_NAMESPACE();

#endif  // LIB_JXL_DEC_TONE_MAPPING_INL_H_
