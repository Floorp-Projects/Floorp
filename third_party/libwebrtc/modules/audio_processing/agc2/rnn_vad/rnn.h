/*
 *  Copyright (c) 2018 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef MODULES_AUDIO_PROCESSING_AGC2_RNN_VAD_RNN_H_
#define MODULES_AUDIO_PROCESSING_AGC2_RNN_VAD_RNN_H_

#include <stddef.h>
#include <sys/types.h>

#include <array>
#include <vector>

#include "api/array_view.h"
#include "api/function_view.h"
#include "modules/audio_processing/agc2/cpu_features.h"
#include "modules/audio_processing/agc2/rnn_vad/common.h"
#include "rtc_base/system/arch.h"

namespace webrtc {
namespace rnn_vad {

// Maximum number of units for an FC layer.
constexpr int kFullyConnectedLayerMaxUnits = 24;

// Maximum number of units for a GRU layer.
constexpr int kGruLayerMaxUnits = 24;

// Fully-connected layer with a custom activation function which owns the output
// buffer.
class FullyConnectedLayer {
 public:
  // Ctor. `output_size` cannot be greater than `kFullyConnectedLayerMaxUnits`.
  FullyConnectedLayer(int input_size,
                      int output_size,
                      rtc::ArrayView<const int8_t> bias,
                      rtc::ArrayView<const int8_t> weights,
                      rtc::FunctionView<float(float)> activation_function,
                      const AvailableCpuFeatures& cpu_features);
  FullyConnectedLayer(const FullyConnectedLayer&) = delete;
  FullyConnectedLayer& operator=(const FullyConnectedLayer&) = delete;
  ~FullyConnectedLayer();

  // Returns the size of the input vector.
  int input_size() const { return input_size_; }
  // Returns the pointer to the first element of the output buffer.
  const float* data() const { return output_.data(); }
  // Returns the size of the output buffer.
  int size() const { return output_size_; }

  // Computes the fully-connected layer output.
  void ComputeOutput(rtc::ArrayView<const float> input);

 private:
  const int input_size_;
  const int output_size_;
  const std::vector<float> bias_;
  const std::vector<float> weights_;
  rtc::FunctionView<float(float)> activation_function_;
  // The output vector of a recurrent layer has length equal to |output_size_|.
  // However, for efficiency, over-allocation is used.
  std::array<float, kFullyConnectedLayerMaxUnits> output_;
  const AvailableCpuFeatures cpu_features_;
};

// Recurrent layer with gated recurrent units (GRUs) with sigmoid and ReLU as
// activation functions for the update/reset and output gates respectively. It
// owns the output buffer.
class GatedRecurrentLayer {
 public:
  // Ctor. `output_size` cannot be greater than `kGruLayerMaxUnits`.
  GatedRecurrentLayer(int input_size,
                      int output_size,
                      rtc::ArrayView<const int8_t> bias,
                      rtc::ArrayView<const int8_t> weights,
                      rtc::ArrayView<const int8_t> recurrent_weights);
  GatedRecurrentLayer(const GatedRecurrentLayer&) = delete;
  GatedRecurrentLayer& operator=(const GatedRecurrentLayer&) = delete;
  ~GatedRecurrentLayer();

  // Returns the size of the input vector.
  int input_size() const { return input_size_; }
  // Returns the pointer to the first element of the output buffer.
  const float* data() const { return state_.data(); }
  // Returns the size of the output buffer.
  int size() const { return output_size_; }

  // Resets the GRU state.
  void Reset();
  // Computes the recurrent layer output and updates the status.
  void ComputeOutput(rtc::ArrayView<const float> input);

 private:
  const int input_size_;
  const int output_size_;
  const std::vector<float> bias_;
  const std::vector<float> weights_;
  const std::vector<float> recurrent_weights_;
  // The state vector of a recurrent layer has length equal to |output_size_|.
  // However, to avoid dynamic allocation, over-allocation is used.
  std::array<float, kGruLayerMaxUnits> state_;
};

// Recurrent network with hard-coded architecture and weights for voice activity
// detection.
class RnnVad {
 public:
  explicit RnnVad(const AvailableCpuFeatures& cpu_features);
  RnnVad(const RnnVad&) = delete;
  RnnVad& operator=(const RnnVad&) = delete;
  ~RnnVad();
  void Reset();
  // Observes `feature_vector` and `is_silence`, updates the RNN and returns the
  // current voice probability.
  float ComputeVadProbability(
      rtc::ArrayView<const float, kFeatureVectorSize> feature_vector,
      bool is_silence);

 private:
  FullyConnectedLayer input_;
  GatedRecurrentLayer hidden_;
  FullyConnectedLayer output_;
};

}  // namespace rnn_vad
}  // namespace webrtc

#endif  // MODULES_AUDIO_PROCESSING_AGC2_RNN_VAD_RNN_H_
