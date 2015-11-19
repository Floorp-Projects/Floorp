/*
 *  Copyright (c) 2012 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "webrtc/tools/agc/test_utils.h"

#include <cmath>

#include <algorithm>

#include "webrtc/modules/interface/module_common_types.h"

namespace webrtc {

float MicLevel2Gain(int gain_range_db, int level) {
  return (level - 127.0f) / 128.0f * gain_range_db / 2;
}

float Db2Linear(float db) {
  return powf(10.0f, db / 20.0f);
}

void ApplyGainLinear(float gain, float last_gain, AudioFrame* frame) {
  const int frame_length = frame->samples_per_channel_ * frame->num_channels_;
  // Smooth the transition between gain levels across the frame.
  float smoothed_gain = last_gain;
  float gain_step = (gain - last_gain) / (frame_length - 1);
  for (int i = 0; i < frame_length; ++i) {
    smoothed_gain += gain_step;
    float sample = std::floor(frame->data_[i] * smoothed_gain + 0.5);
    sample = std::max(std::min(32767.0f, sample), -32768.0f);
    frame->data_[i] = static_cast<int16_t>(sample);
  }
}

void ApplyGain(float gain_db, float last_gain_db, AudioFrame* frame) {
  ApplyGainLinear(Db2Linear(gain_db), Db2Linear(last_gain_db), frame);
}

void SimulateMic(int gain_range_db, int mic_level, int last_mic_level,
                 AudioFrame* frame) {
  assert(mic_level >= 0 && mic_level <= 255);
  assert(last_mic_level >= 0 && last_mic_level <= 255);
  ApplyGain(MicLevel2Gain(gain_range_db, mic_level),
            MicLevel2Gain(gain_range_db, last_mic_level),
            frame);
}

void SimulateMic(int gain_map[255], int mic_level, int last_mic_level,
                 AudioFrame* frame) {
  assert(mic_level >= 0 && mic_level <= 255);
  assert(last_mic_level >= 0 && last_mic_level <= 255);
  ApplyGain(gain_map[mic_level], gain_map[last_mic_level], frame);
}

}  // namespace webrtc

