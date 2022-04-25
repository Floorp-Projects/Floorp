/*
 *  Copyright (c) 2018 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "modules/audio_processing/agc2/rnn_vad/rnn.h"

// Defines WEBRTC_ARCH_X86_FAMILY, used below.
#include "rtc_base/system/arch.h"

#if defined(WEBRTC_HAS_NEON)
#include <arm_neon.h>
#endif
#if defined(WEBRTC_ARCH_X86_FAMILY)
#include <emmintrin.h>
#endif
#include <algorithm>
#include <array>
#include <cmath>
#include <numeric>

#include "rtc_base/checks.h"
#include "rtc_base/numerics/safe_conversions.h"
#include "third_party/rnnoise/src/rnn_activations.h"
#include "third_party/rnnoise/src/rnn_vad_weights.h"

namespace webrtc {
namespace rnn_vad {
namespace {

using rnnoise::kWeightsScale;

using rnnoise::kInputLayerInputSize;
static_assert(kFeatureVectorSize == kInputLayerInputSize, "");
using rnnoise::kInputDenseBias;
using rnnoise::kInputDenseWeights;
using rnnoise::kInputLayerOutputSize;
static_assert(kInputLayerOutputSize <= kFullyConnectedLayerMaxUnits, "");

using rnnoise::kHiddenGruBias;
using rnnoise::kHiddenGruRecurrentWeights;
using rnnoise::kHiddenGruWeights;
using rnnoise::kHiddenLayerOutputSize;
static_assert(kHiddenLayerOutputSize <= kGruLayerMaxUnits, "");

using rnnoise::kOutputDenseBias;
using rnnoise::kOutputDenseWeights;
using rnnoise::kOutputLayerOutputSize;
static_assert(kOutputLayerOutputSize <= kFullyConnectedLayerMaxUnits, "");

using rnnoise::SigmoidApproximated;
using rnnoise::TansigApproximated;

inline float RectifiedLinearUnit(float x) {
  return x < 0.f ? 0.f : x;
}

constexpr int kNumGruGates = 3;  // Update, reset, output.

// TODO(bugs.chromium.org/10480): Hard-coded optimized layout and remove this
// function to improve setup time.
// Casts and scales |tensor_src| for a GRU layer and re-arranges the layout.
// It works both for weights, recurrent weights and bias.
std::vector<float> GetPreprocessedGruTensor(
    rtc::ArrayView<const int8_t> tensor_src,
    int output_size) {
  // Transpose, cast and scale.
  // |n| is the size of the first dimension of the 3-dim tensor |weights|.
  const int n = rtc::CheckedDivExact(rtc::dchecked_cast<int>(tensor_src.size()),
                                     output_size * kNumGruGates);
  const int stride_src = kNumGruGates * output_size;
  const int stride_dst = n * output_size;
  std::vector<float> tensor_dst(tensor_src.size());
  for (int g = 0; g < kNumGruGates; ++g) {
    for (int o = 0; o < output_size; ++o) {
      for (int i = 0; i < n; ++i) {
        tensor_dst[g * stride_dst + o * n + i] =
            rnnoise::kWeightsScale *
            static_cast<float>(
                tensor_src[i * stride_src + g * output_size + o]);
      }
    }
  }
  return tensor_dst;
}

void ComputeGruUpdateResetGates(int input_size,
                                int output_size,
                                rtc::ArrayView<const float> weights,
                                rtc::ArrayView<const float> recurrent_weights,
                                rtc::ArrayView<const float> bias,
                                rtc::ArrayView<const float> input,
                                rtc::ArrayView<const float> state,
                                rtc::ArrayView<float> gate) {
  for (int o = 0; o < output_size; ++o) {
    gate[o] = bias[o];
    for (int i = 0; i < input_size; ++i) {
      gate[o] += input[i] * weights[o * input_size + i];
    }
    for (int s = 0; s < output_size; ++s) {
      gate[o] += state[s] * recurrent_weights[o * output_size + s];
    }
    gate[o] = SigmoidApproximated(gate[o]);
  }
}

void ComputeGruOutputGate(int input_size,
                          int output_size,
                          rtc::ArrayView<const float> weights,
                          rtc::ArrayView<const float> recurrent_weights,
                          rtc::ArrayView<const float> bias,
                          rtc::ArrayView<const float> input,
                          rtc::ArrayView<const float> state,
                          rtc::ArrayView<const float> reset,
                          rtc::ArrayView<float> gate) {
  for (int o = 0; o < output_size; ++o) {
    gate[o] = bias[o];
    for (int i = 0; i < input_size; ++i) {
      gate[o] += input[i] * weights[o * input_size + i];
    }
    for (int s = 0; s < output_size; ++s) {
      gate[o] += state[s] * recurrent_weights[o * output_size + s] * reset[s];
    }
    gate[o] = RectifiedLinearUnit(gate[o]);
  }
}

// Gated recurrent unit (GRU) layer un-optimized implementation.
void ComputeGruLayerOutput(int input_size,
                           int output_size,
                           rtc::ArrayView<const float> input,
                           rtc::ArrayView<const float> weights,
                           rtc::ArrayView<const float> recurrent_weights,
                           rtc::ArrayView<const float> bias,
                           rtc::ArrayView<float> state) {
  RTC_DCHECK_EQ(input_size, input.size());
  // Stride and offset used to read parameter arrays.
  const int stride_in = input_size * output_size;
  const int stride_out = output_size * output_size;

  // Update gate.
  std::array<float, kGruLayerMaxUnits> update;
  ComputeGruUpdateResetGates(
      input_size, output_size, weights.subview(0, stride_in),
      recurrent_weights.subview(0, stride_out), bias.subview(0, output_size),
      input, state, update);

  // Reset gate.
  std::array<float, kGruLayerMaxUnits> reset;
  ComputeGruUpdateResetGates(
      input_size, output_size, weights.subview(stride_in, stride_in),
      recurrent_weights.subview(stride_out, stride_out),
      bias.subview(output_size, output_size), input, state, reset);

  // Output gate.
  std::array<float, kGruLayerMaxUnits> output;
  ComputeGruOutputGate(
      input_size, output_size, weights.subview(2 * stride_in, stride_in),
      recurrent_weights.subview(2 * stride_out, stride_out),
      bias.subview(2 * output_size, output_size), input, state, reset, output);

  // Update output through the update gates and update the state.
  for (int o = 0; o < output_size; ++o) {
    output[o] = update[o] * state[o] + (1.f - update[o]) * output[o];
    state[o] = output[o];
  }
}

}  // namespace

GatedRecurrentLayer::GatedRecurrentLayer(
    const int input_size,
    const int output_size,
    const rtc::ArrayView<const int8_t> bias,
    const rtc::ArrayView<const int8_t> weights,
    const rtc::ArrayView<const int8_t> recurrent_weights)
    : input_size_(input_size),
      output_size_(output_size),
      bias_(GetPreprocessedGruTensor(bias, output_size)),
      weights_(GetPreprocessedGruTensor(weights, output_size)),
      recurrent_weights_(
          GetPreprocessedGruTensor(recurrent_weights, output_size)) {
  RTC_DCHECK_LE(output_size_, kGruLayerMaxUnits)
      << "Static over-allocation of recurrent layers state vectors is not "
         "sufficient.";
  RTC_DCHECK_EQ(kNumGruGates * output_size_, bias_.size())
      << "Mismatching output size and bias terms array size.";
  RTC_DCHECK_EQ(kNumGruGates * input_size_ * output_size_, weights_.size())
      << "Mismatching input-output size and weight coefficients array size.";
  RTC_DCHECK_EQ(kNumGruGates * output_size_ * output_size_,
                recurrent_weights_.size())
      << "Mismatching input-output size and recurrent weight coefficients array"
         " size.";
  Reset();
}

GatedRecurrentLayer::~GatedRecurrentLayer() = default;

void GatedRecurrentLayer::Reset() {
  state_.fill(0.f);
}

void GatedRecurrentLayer::ComputeOutput(rtc::ArrayView<const float> input) {
  // TODO(bugs.chromium.org/10480): Add AVX2.
  // TODO(bugs.chromium.org/10480): Add Neon.
  ComputeGruLayerOutput(input_size_, output_size_, input, weights_,
                        recurrent_weights_, bias_, state_);
}

RnnVad::RnnVad(const AvailableCpuFeatures& cpu_features)
    : input_(kInputLayerInputSize,
             kInputLayerOutputSize,
             kInputDenseBias,
             kInputDenseWeights,
             ActivationFunction::kTansigApproximated,
             cpu_features,
             /*layer_name=*/"FC1"),
      hidden_(kInputLayerOutputSize,
              kHiddenLayerOutputSize,
              kHiddenGruBias,
              kHiddenGruWeights,
              kHiddenGruRecurrentWeights),
      output_(kHiddenLayerOutputSize,
              kOutputLayerOutputSize,
              kOutputDenseBias,
              kOutputDenseWeights,
              ActivationFunction::kSigmoidApproximated,
              cpu_features,
              /*layer_name=*/"FC2") {
  // Input-output chaining size checks.
  RTC_DCHECK_EQ(input_.size(), hidden_.input_size())
      << "The input and the hidden layers sizes do not match.";
  RTC_DCHECK_EQ(hidden_.size(), output_.input_size())
      << "The hidden and the output layers sizes do not match.";
}

RnnVad::~RnnVad() = default;

void RnnVad::Reset() {
  hidden_.Reset();
}

float RnnVad::ComputeVadProbability(
    rtc::ArrayView<const float, kFeatureVectorSize> feature_vector,
    bool is_silence) {
  if (is_silence) {
    Reset();
    return 0.f;
  }
  input_.ComputeOutput(feature_vector);
  hidden_.ComputeOutput(input_);
  output_.ComputeOutput(hidden_);
  RTC_DCHECK_EQ(output_.size(), 1);
  return output_.data()[0];
}

}  // namespace rnn_vad
}  // namespace webrtc
