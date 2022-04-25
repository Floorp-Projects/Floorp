/*
 *  Copyright (c) 2020 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "modules/audio_processing/agc2/rnn_vad/rnn_gru.h"

#include "rtc_base/checks.h"
#include "rtc_base/numerics/safe_conversions.h"
#include "third_party/rnnoise/src/rnn_activations.h"
#include "third_party/rnnoise/src/rnn_vad_weights.h"

namespace webrtc {
namespace rnn_vad {
namespace {

constexpr int kNumGruGates = 3;  // Update, reset, output.

std::vector<float> PreprocessGruTensor(rtc::ArrayView<const int8_t> tensor_src,
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
            ::rnnoise::kWeightsScale *
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
    gate[o] = ::rnnoise::SigmoidApproximated(gate[o]);
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
    // Rectified linear unit.
    if (gate[o] < 0.f) {
      gate[o] = 0.f;
    }
  }
}

}  // namespace

GatedRecurrentLayer::GatedRecurrentLayer(
    const int input_size,
    const int output_size,
    const rtc::ArrayView<const int8_t> bias,
    const rtc::ArrayView<const int8_t> weights,
    const rtc::ArrayView<const int8_t> recurrent_weights,
    absl::string_view layer_name)
    : input_size_(input_size),
      output_size_(output_size),
      bias_(PreprocessGruTensor(bias, output_size)),
      weights_(PreprocessGruTensor(weights, output_size)),
      recurrent_weights_(PreprocessGruTensor(recurrent_weights, output_size)) {
  RTC_DCHECK_LE(output_size_, kGruLayerMaxUnits)
      << "Insufficient GRU layer over-allocation (" << layer_name << ").";
  RTC_DCHECK_EQ(kNumGruGates * output_size_, bias_.size())
      << "Mismatching output size and bias terms array size (" << layer_name
      << ").";
  RTC_DCHECK_EQ(kNumGruGates * input_size_ * output_size_, weights_.size())
      << "Mismatching input-output size and weight coefficients array size ("
      << layer_name << ").";
  RTC_DCHECK_EQ(kNumGruGates * output_size_ * output_size_,
                recurrent_weights_.size())
      << "Mismatching input-output size and recurrent weight coefficients array"
         " size ("
      << layer_name << ").";
  Reset();
}

GatedRecurrentLayer::~GatedRecurrentLayer() = default;

void GatedRecurrentLayer::Reset() {
  state_.fill(0.f);
}

void GatedRecurrentLayer::ComputeOutput(rtc::ArrayView<const float> input) {
  RTC_DCHECK_EQ(input.size(), input_size_);

  // TODO(bugs.chromium.org/10480): Add AVX2.
  // TODO(bugs.chromium.org/10480): Add Neon.

  // Stride and offset used to read parameter arrays.
  const int stride_in = input_size_ * output_size_;
  const int stride_out = output_size_ * output_size_;

  rtc::ArrayView<const float> bias(bias_);
  rtc::ArrayView<const float> weights(weights_);
  rtc::ArrayView<const float> recurrent_weights(recurrent_weights_);

  // Update gate.
  std::array<float, kGruLayerMaxUnits> update;
  ComputeGruUpdateResetGates(
      input_size_, output_size_, weights.subview(0, stride_in),
      recurrent_weights.subview(0, stride_out), bias.subview(0, output_size_),
      input, state_, update);

  // Reset gate.
  std::array<float, kGruLayerMaxUnits> reset;
  ComputeGruUpdateResetGates(
      input_size_, output_size_, weights.subview(stride_in, stride_in),
      recurrent_weights.subview(stride_out, stride_out),
      bias.subview(output_size_, output_size_), input, state_, reset);

  // Output gate.
  std::array<float, kGruLayerMaxUnits> output;
  ComputeGruOutputGate(input_size_, output_size_,
                       weights.subview(2 * stride_in, stride_in),
                       recurrent_weights.subview(2 * stride_out, stride_out),
                       bias.subview(2 * output_size_, output_size_), input,
                       state_, reset, output);

  // Update output through the update gates and update the state.
  for (int o = 0; o < output_size_; ++o) {
    output[o] = update[o] * state_[o] + (1.f - update[o]) * output[o];
    state_[o] = output[o];
  }
}

}  // namespace rnn_vad
}  // namespace webrtc
