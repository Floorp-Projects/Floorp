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

#ifndef LIB_JXL_CONVOLVE_H_
#define LIB_JXL_CONVOLVE_H_

// 2D convolution.

#include <stddef.h>
#include <stdint.h>

#include "lib/jxl/base/data_parallel.h"
#include "lib/jxl/image.h"

namespace jxl {

// No valid values outside [0, xsize), but the strategy may still safely load
// the preceding vector, and/or round xsize up to the vector lane count. This
// avoids needing PadImage.
// Requires xsize >= kConvolveLanes + kConvolveMaxRadius.
static constexpr size_t kConvolveMaxRadius = 3;

// Weights must already be normalized.

struct WeightsSymmetric3 {
  // d r d (each replicated 4x)
  // r c r
  // d r d
  float c[4];
  float r[4];
  float d[4];
};

struct WeightsSymmetric5 {
  // The lower-right quadrant is: c r R  (each replicated 4x)
  //                              r d L
  //                              R L D
  float c[4];
  float r[4];
  float R[4];
  float d[4];
  float D[4];
  float L[4];
};

// Weights for separable 5x5 filters (typically but not necessarily the same
// values for horizontal and vertical directions). The kernel must already be
// normalized, but note that values for negative offsets are omitted, so the
// given values do not sum to 1.
struct WeightsSeparable5 {
  // Horizontal 1D, distances 0..2 (each replicated 4x)
  float horz[3 * 4];
  float vert[3 * 4];
};

// Weights for separable 7x7 filters (typically but not necessarily the same
// values for horizontal and vertical directions). The kernel must already be
// normalized, but note that values for negative offsets are omitted, so the
// given values do not sum to 1.
//
// NOTE: for >= 7x7 Gaussian kernels, it is faster to use FastGaussian instead,
// at least when images exceed the L1 cache size.
struct WeightsSeparable7 {
  // Horizontal 1D, distances 0..3 (each replicated 4x)
  float horz[4 * 4];
  float vert[4 * 4];
};

const WeightsSymmetric3& WeightsSymmetric3Lowpass();
const WeightsSeparable5& WeightsSeparable5Lowpass();
const WeightsSymmetric5& WeightsSymmetric5Lowpass();

void SlowSymmetric3(const ImageF& in, const Rect& rect,
                    const WeightsSymmetric3& weights, ThreadPool* pool,
                    ImageF* JXL_RESTRICT out);
void SlowSymmetric3(const Image3F& in, const Rect& rect,
                    const WeightsSymmetric3& weights, ThreadPool* pool,
                    Image3F* JXL_RESTRICT out);

void SlowSeparable5(const ImageF& in, const Rect& rect,
                    const WeightsSeparable5& weights, ThreadPool* pool,
                    ImageF* out);
void SlowSeparable5(const Image3F& in, const Rect& rect,
                    const WeightsSeparable5& weights, ThreadPool* pool,
                    Image3F* out);

void SlowSeparable7(const ImageF& in, const Rect& rect,
                    const WeightsSeparable7& weights, ThreadPool* pool,
                    ImageF* out);
void SlowSeparable7(const Image3F& in, const Rect& rect,
                    const WeightsSeparable7& weights, ThreadPool* pool,
                    Image3F* out);

void SlowLaplacian5(const ImageF& in, const Rect& rect, ThreadPool* pool,
                    ImageF* out);
void SlowLaplacian5(const Image3F& in, const Rect& rect, ThreadPool* pool,
                    Image3F* out);

void Symmetric3(const ImageF& in, const Rect& rect,
                const WeightsSymmetric3& weights, ThreadPool* pool,
                ImageF* out);

void Symmetric5(const ImageF& in, const Rect& rect,
                const WeightsSymmetric5& weights, ThreadPool* pool,
                ImageF* JXL_RESTRICT out);

void Symmetric5_3(const Image3F& in, const Rect& rect,
                  const WeightsSymmetric5& weights, ThreadPool* pool,
                  Image3F* JXL_RESTRICT out);

void Separable5(const ImageF& in, const Rect& rect,
                const WeightsSeparable5& weights, ThreadPool* pool,
                ImageF* out);

void Separable5_3(const Image3F& in, const Rect& rect,
                  const WeightsSeparable5& weights, ThreadPool* pool,
                  Image3F* out);

void Separable7(const ImageF& in, const Rect& rect,
                const WeightsSeparable7& weights, ThreadPool* pool,
                ImageF* out);

void Separable7_3(const Image3F& in, const Rect& rect,
                  const WeightsSeparable7& weights, ThreadPool* pool,
                  Image3F* out);

}  // namespace jxl

#endif  // LIB_JXL_CONVOLVE_H_
