/*
 *  Copyright (c) 2020 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

// Defines WEBRTC_ARCH_X86_FAMILY, used below.
#include "rtc_base/system/arch.h"

#if defined(WEBRTC_ARCH_X86_FAMILY)
#include <emmintrin.h>
#endif

#include <algorithm>
#include <numeric>

#include "modules/audio_processing/agc2/rnn_vad/rnn_fc.h"
#include "rtc_base/checks.h"
#include "rtc_base/numerics/safe_conversions.h"
#include "third_party/rnnoise/src/rnn_activations.h"
#include "third_party/rnnoise/src/rnn_vad_weights.h"

namespace webrtc {
namespace rnn_vad {
namespace {

std::vector<float> GetScaledParams(rtc::ArrayView<const int8_t> params) {
  std::vector<float> scaled_params(params.size());
  std::transform(params.begin(), params.end(), scaled_params.begin(),
                 [](int8_t x) -> float {
                   return ::rnnoise::kWeightsScale * static_cast<float>(x);
                 });
  return scaled_params;
}

// TODO(bugs.chromium.org/10480): Hard-code optimized layout and remove this
// function to improve setup time.
// Casts and scales |weights| and re-arranges the layout.
std::vector<float> PreprocessWeights(rtc::ArrayView<const int8_t> weights,
                                     int output_size) {
  if (output_size == 1) {
    return GetScaledParams(weights);
  }
  // Transpose, scale and cast.
  const int input_size = rtc::CheckedDivExact(
      rtc::dchecked_cast<int>(weights.size()), output_size);
  std::vector<float> w(weights.size());
  for (int o = 0; o < output_size; ++o) {
    for (int i = 0; i < input_size; ++i) {
      w[o * input_size + i] = rnnoise::kWeightsScale *
                              static_cast<float>(weights[i * output_size + o]);
    }
  }
  return w;
}

rtc::FunctionView<float(float)> GetActivationFunction(
    ActivationFunction activation_function) {
  switch (activation_function) {
    case ActivationFunction::kTansigApproximated:
      return ::rnnoise::TansigApproximated;
      break;
    case ActivationFunction::kSigmoidApproximated:
      return ::rnnoise::SigmoidApproximated;
      break;
  }
}

}  // namespace

FullyConnectedLayer::FullyConnectedLayer(
    const int input_size,
    const int output_size,
    const rtc::ArrayView<const int8_t> bias,
    const rtc::ArrayView<const int8_t> weights,
    ActivationFunction activation_function,
    const AvailableCpuFeatures& cpu_features,
    absl::string_view layer_name)
    : input_size_(input_size),
      output_size_(output_size),
      bias_(GetScaledParams(bias)),
      weights_(PreprocessWeights(weights, output_size)),
      cpu_features_(cpu_features),
      activation_function_(GetActivationFunction(activation_function)) {
  RTC_DCHECK_LE(output_size_, kFullyConnectedLayerMaxUnits)
      << "Insufficient FC layer over-allocation (" << layer_name << ").";
  RTC_DCHECK_EQ(output_size_, bias_.size())
      << "Mismatching output size and bias terms array size (" << layer_name
      << ").";
  RTC_DCHECK_EQ(input_size_ * output_size_, weights_.size())
      << "Mismatching input-output size and weight coefficients array size ("
      << layer_name << ").";
}

FullyConnectedLayer::~FullyConnectedLayer() = default;

void FullyConnectedLayer::ComputeOutput(rtc::ArrayView<const float> input) {
  RTC_DCHECK_EQ(input.size(), input_size_);
#if defined(WEBRTC_ARCH_X86_FAMILY)
  // TODO(bugs.chromium.org/10480): Add AVX2.
  if (cpu_features_.sse2) {
    ComputeOutputSse2(input);
    return;
  }
#endif
  // TODO(bugs.chromium.org/10480): Add Neon.

  // Un-optimized implementation.
  for (int o = 0; o < output_size_; ++o) {
    output_[o] = bias_[o];
    // TODO(bugs.chromium.org/9076): Benchmark how different layouts for
    // |weights_| change the performance across different platforms.
    for (int i = 0; i < input_size_; ++i) {
      output_[o] += input[i] * weights_[o * input_size_ + i];
    }
    output_[o] = activation_function_(output_[o]);
  }
}

#if defined(WEBRTC_ARCH_X86_FAMILY)
void FullyConnectedLayer::ComputeOutputSse2(rtc::ArrayView<const float> input) {
  const int input_size_by_4 = input_size_ >> 2;
  const int offset = input_size_ & ~3;
  // TODO(bugs.chromium.org/10480): Check if reinterpret_cast below is ok.
  __m128 sum_wx_128;
  const float* v = reinterpret_cast<const float*>(&sum_wx_128);
  for (int o = 0; o < output_size_; ++o) {
    // Perform 128 bit vector operations.
    sum_wx_128 = _mm_set1_ps(0);
    const float* x_p = input.data();
    const float* w_p = weights_.data() + o * input.size();
    for (int i = 0; i < input_size_by_4; ++i, x_p += 4, w_p += 4) {
      sum_wx_128 = _mm_add_ps(sum_wx_128,
                              _mm_mul_ps(_mm_loadu_ps(x_p), _mm_loadu_ps(w_p)));
    }
    // Perform non-vector operations for any remaining items, sum up bias term
    // and results from the vectorized code, and apply the activation function.
    output_[o] = activation_function_(
        std::inner_product(input.begin() + offset, input.end(),
                           weights_.begin() + o * input.size() + offset,
                           bias_[o] + v[0] + v[1] + v[2] + v[3]));
  }
}
#endif  // defined(WEBRTC_ARCH_X86_FAMILY)

}  // namespace rnn_vad
}  // namespace webrtc
