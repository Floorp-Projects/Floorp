/*
 *  Copyright (c) 2018 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef MODULES_AUDIO_PROCESSING_AGC2_SATURATION_PROTECTOR_H_
#define MODULES_AUDIO_PROCESSING_AGC2_SATURATION_PROTECTOR_H_

#include <array>

#include "absl/types/optional.h"
#include "modules/audio_processing/agc2/agc2_common.h"
#include "modules/audio_processing/agc2/vad_with_level.h"

namespace webrtc {

class ApmDataDumper;

class SaturationProtector {
 public:
  explicit SaturationProtector(ApmDataDumper* apm_data_dumper);

  SaturationProtector(ApmDataDumper* apm_data_dumper,
                      float extra_saturation_margin_db);

  // Updates the margin estimate. This method should be called whenever a frame
  // is reliably classified as 'speech'.
  void UpdateMargin(const VadWithLevel::LevelAndProbability& vad_data,
                    float last_speech_level_estimate);

  // Returns latest computed margin.
  float LastMargin() const;

  void Reset();

  void DebugDumpEstimate() const;

 private:
  // Ring buffer which only supports (i) push back and (ii) read oldest item.
  class RingBuffer {
   public:
    void Reset();
    // Pushes back `v`. If the buffer is full, the oldest item is replaced.
    void PushBack(float v);
    // Returns the oldest item in the buffer. Returns an empty value if the
    // buffer is empty.
    absl::optional<float> Front() const;

   private:
    std::array<float, kPeakEnveloperBufferSize> buffer_;
    int next_ = 0;
    int size_ = 0;
  };

  // Computes a delayed envelope of peaks.
  class PeakEnveloper {
   public:
    PeakEnveloper();
    void Reset();
    void Process(float frame_peak_dbfs);
    float Query() const;

   private:
    size_t speech_time_in_estimate_ms_;
    float current_superframe_peak_dbfs_;
    RingBuffer peak_delay_buffer_;
  };

  ApmDataDumper* apm_data_dumper_;
  PeakEnveloper peak_enveloper_;

  const float extra_saturation_margin_db_;
  float last_margin_;
};

}  // namespace webrtc

#endif  // MODULES_AUDIO_PROCESSING_AGC2_SATURATION_PROTECTOR_H_
