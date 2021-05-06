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

#include "lib/extras/tone_mapping.h"

#undef HWY_TARGET_INCLUDE
#define HWY_TARGET_INCLUDE "lib/extras/tone_mapping.cc"
#include <hwy/foreach_target.h>
#include <hwy/highway.h>

#include "lib/jxl/transfer_functions-inl.h"

HWY_BEFORE_NAMESPACE();
namespace jxl {
namespace HWY_NAMESPACE {

Status ToneMapFrame(const std::pair<float, float> display_nits,
                    ImageBundle* const ib, ThreadPool* const pool) {
  // Perform tone mapping as described in Report ITU-R BT.2390-8, section 5.4
  // (pp. 23-25).
  // https://www.itu.int/pub/R-REP-BT.2390-8-2020

  HWY_FULL(float) df;
  using V = decltype(Zero(df));

  ColorEncoding linear_rec2020;
  linear_rec2020.SetColorSpace(ColorSpace::kRGB);
  linear_rec2020.primaries = Primaries::k2100;
  linear_rec2020.white_point = WhitePoint::kD65;
  linear_rec2020.tf.SetTransferFunction(TransferFunction::kLinear);
  JXL_RETURN_IF_ERROR(linear_rec2020.CreateICC());
  JXL_RETURN_IF_ERROR(ib->TransformTo(linear_rec2020, pool));

  const auto eotf_inv = [&df](const V luminance) -> V {
    return TF_PQ().EncodedFromDisplay(df, luminance * Set(df, 1. / 10000));
  };

  const V pq_mastering_min =
      eotf_inv(Set(df, ib->metadata()->tone_mapping.min_nits));
  const V pq_mastering_max =
      eotf_inv(Set(df, ib->metadata()->tone_mapping.intensity_target));
  const V pq_mastering_range = pq_mastering_max - pq_mastering_min;
  const V inv_pq_mastering_range =
      Set(df, 1) / (pq_mastering_max - pq_mastering_min);
  const V min_lum = (eotf_inv(Set(df, display_nits.first)) - pq_mastering_min) *
                    inv_pq_mastering_range;
  const V max_lum =
      (eotf_inv(Set(df, display_nits.second)) - pq_mastering_min) *
      inv_pq_mastering_range;
  const V ks = MulAdd(Set(df, 1.5f), max_lum, Set(df, -0.5f));
  const V b = min_lum;

  const V inv_one_minus_ks = Set(df, 1) / Max(Set(df, 1e-6f), Set(df, 1) - ks);
  const auto T = [ks, inv_one_minus_ks](const V a) {
    return (a - ks) * inv_one_minus_ks;
  };
  const auto P = [&T, &df, ks, max_lum](const V b) {
    const V t_b = T(b);
    const V t_b_2 = t_b * t_b;
    const V t_b_3 = t_b_2 * t_b;
    return MulAdd(
        MulAdd(Set(df, 2), t_b_3, MulAdd(Set(df, -3), t_b_2, Set(df, 1))), ks,
        MulAdd(t_b_3 + MulAdd(Set(df, -2), t_b_2, t_b), Set(df, 1) - ks,
               MulAdd(Set(df, -2), t_b_3, Set(df, 3) * t_b_2) * max_lum));
  };

  const V inv_max_display_nits = Set(df, 1 / display_nits.second);

  JXL_RETURN_IF_ERROR(RunOnPool(
      pool, 0, ib->ysize(), ThreadPool::SkipInit(),
      [&](const int y, const int thread) {
        float* const JXL_RESTRICT row_r = ib->color()->PlaneRow(0, y);
        float* const JXL_RESTRICT row_g = ib->color()->PlaneRow(1, y);
        float* const JXL_RESTRICT row_b = ib->color()->PlaneRow(2, y);
        for (size_t x = 0; x < ib->xsize(); x += Lanes(df)) {
          V red = Load(df, row_r + x);
          V green = Load(df, row_g + x);
          V blue = Load(df, row_b + x);
          const V luminance = Set(df, ib->metadata()->IntensityTarget()) *
                              (MulAdd(Set(df, 0.2627f), red,
                                      MulAdd(Set(df, 0.6780f), green,
                                             Set(df, 0.0593f) * blue)));
          const V normalized_pq =
              Min(Set(df, 1.f), (eotf_inv(luminance) - pq_mastering_min) *
                                    inv_pq_mastering_range);
          const V e2 =
              IfThenElse(normalized_pq < ks, normalized_pq, P(normalized_pq));
          const V one_minus_e2 = Set(df, 1) - e2;
          const V one_minus_e2_2 = one_minus_e2 * one_minus_e2;
          const V one_minus_e2_4 = one_minus_e2_2 * one_minus_e2_2;
          const V e3 = MulAdd(b, one_minus_e2_4, e2);
          const V e4 = MulAdd(e3, pq_mastering_range, pq_mastering_min);
          const V new_luminance =
              Min(Set(df, display_nits.second),
                  ZeroIfNegative(Set(df, 10000) *
                                 TF_PQ().DisplayFromEncoded(df, e4)));

          const V ratio = new_luminance / luminance;
          const V multiplier = ratio *
                               Set(df, ib->metadata()->IntensityTarget()) *
                               inv_max_display_nits;

          red *= multiplier;
          green *= multiplier;
          blue *= multiplier;

          const V gray = new_luminance * inv_max_display_nits;

          // Desaturate out-of-gamut pixels.
          V gray_mix = Zero(df);
          for (const V val : {red, green, blue}) {
            const V inv_val_minus_gray = Set(df, 1) / (val - gray);
            const V bound1 = val * inv_val_minus_gray;
            const V bound2 = bound1 - inv_val_minus_gray;
            const V min_bound = Min(bound1, bound2);
            const V max_bound = Max(bound1, bound2);
            gray_mix = Clamp(gray_mix, min_bound, max_bound);
          }
          gray_mix = Clamp(gray_mix, Zero(df), Set(df, 1));
          for (V* const val : {&red, &green, &blue}) {
            *val = IfThenElse(luminance < Set(df, 1e-6), gray,
                              MulAdd(gray_mix, gray - *val, *val));
          }

          Store(red, df, row_r + x);
          Store(green, df, row_g + x);
          Store(blue, df, row_b + x);
        }
      },
      "ToneMap"));

  return true;
}

// NOLINTNEXTLINE(google-readability-namespace-comments)
}  // namespace HWY_NAMESPACE
}  // namespace jxl
HWY_AFTER_NAMESPACE();

#if HWY_ONCE
namespace jxl {

namespace {
HWY_EXPORT(ToneMapFrame);
}

Status ToneMapTo(const std::pair<float, float> display_nits,
                 CodecInOut* const io, ThreadPool* const pool) {
  const auto tone_map_frame = HWY_DYNAMIC_DISPATCH(ToneMapFrame);
  for (ImageBundle& ib : io->frames) {
    JXL_RETURN_IF_ERROR(tone_map_frame(display_nits, &ib, pool));
  }
  io->metadata.m.SetIntensityTarget(display_nits.second);
  return true;
}

}  // namespace jxl
#endif
