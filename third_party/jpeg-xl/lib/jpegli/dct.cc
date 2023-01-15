// Copyright (c) the JPEG XL Project Authors. All rights reserved.
//
// Use of this source code is governed by a BSD-style
// license that can be found in the LICENSE file.

#include "lib/jpegli/dct.h"

#undef HWY_TARGET_INCLUDE
#define HWY_TARGET_INCLUDE "lib/jpegli/dct.cc"
#include <hwy/foreach_target.h>
#include <hwy/highway.h>

#include "lib/jxl/common.h"
#include "lib/jxl/enc_transforms.h"
#include "lib/jxl/image.h"
HWY_BEFORE_NAMESPACE();
namespace jpegli {
namespace HWY_NAMESPACE {

void ComputeDCTCoefficients(const jxl::Image3F& opsin, const bool xyb,
                            const jxl::ImageF& qf,
                            const float* qm,
                            std::vector<jxl::jpeg::JPEGComponent>* components) {
  int max_samp_factor = 1;
  for (const auto& c : *components) {
    JXL_DASSERT(c.h_samp_factor == c.v_samp_factor);
    max_samp_factor = std::max(c.h_samp_factor, max_samp_factor);
  }
  float qfmin, qfmax;
  ImageMinMax(qf, &qfmin, &qfmax);
  HWY_ALIGN float scratch_space[2 * jxl::kDCTBlockSize];
  jxl::ImageF tmp;
  for (size_t c = 0; c < 3; c++) {
    auto& comp = (*components)[c];
    const size_t xsize_blocks = comp.width_in_blocks;
    const size_t ysize_blocks = comp.height_in_blocks;
    JXL_DASSERT(max_samp_factor % comp.h_samp_factor == 0);
    const int factor = max_samp_factor / comp.h_samp_factor;
    const jxl::ImageF* plane = &opsin.Plane(c);
    if (factor > 1) {
      tmp = CopyImage(*plane);
      DownsampleImage(&tmp, factor);
      plane = &tmp;
    }
    std::vector<jxl::jpeg::coeff_t>& coeffs = comp.coeffs;
    coeffs.resize(xsize_blocks * ysize_blocks * jxl::kDCTBlockSize);
    const float* qmc = &qm[c * jxl::kDCTBlockSize];
    for (size_t by = 0, bix = 0; by < ysize_blocks; by++) {
      for (size_t bx = 0; bx < xsize_blocks; bx++, bix++) {
        jxl::jpeg::coeff_t* block = &coeffs[bix * jxl::kDCTBlockSize];
        HWY_ALIGN float dct[jxl::kDCTBlockSize];
        TransformFromPixels(jxl::AcStrategy::Type::DCT,
                            plane->Row(8 * by) + 8 * bx, plane->PixelsPerRow(),
                            dct, scratch_space);
        for (size_t iy = 0, i = 0; iy < 8; iy++) {
          for (size_t ix = 0; ix < 8; ix++, i++) {
            float coeff = 2040 * dct[i] * qmc[i];
            // Create more zeros in areas where jpeg xl would have used a lower
            // quantization multiplier.
            float zero_bias = 0.5f * qfmax / qf.Row(by * factor)[bx * factor];
            int cc = std::abs(coeff) < zero_bias ? 0 : std::round(coeff);
            // If the relative value of the adaptive quantization field is less
            // than 0.5, we drop the least significant bit.
            if (zero_bias > 1) {
              cc = cc / 2 * 2;
            }
            block[ix * 8 + iy] = cc;
          }
        }
        if (xyb) {
          // ToXYB does not create zero-centered sample values like RgbToYcbcr
          // does, so we apply an offset to the DC values instead.
          block[0] = std::round((2040 * dct[0] - 1024) * qmc[0]);
        }
      }
    }
  }
}

// NOLINTNEXTLINE(google-readability-namespace-comments)
}  // namespace HWY_NAMESPACE
}  // namespace jpegli
HWY_AFTER_NAMESPACE();

#if HWY_ONCE
namespace jpegli {

HWY_EXPORT(ComputeDCTCoefficients);

void ComputeDCTCoefficients(const jxl::Image3F& opsin, const bool xyb,
                            const jxl::ImageF& qf,
                            const float* qm,
                            std::vector<jxl::jpeg::JPEGComponent>* components) {
  HWY_DYNAMIC_DISPATCH(ComputeDCTCoefficients)
  (opsin, xyb, qf, qm, components);
}

}  // namespace jpegli
#endif  // HWY_ONCE
