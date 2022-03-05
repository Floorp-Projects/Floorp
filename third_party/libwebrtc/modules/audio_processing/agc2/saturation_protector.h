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

namespace webrtc {

class ApmDataDumper;

class SaturationProtector {
 public:
  explicit SaturationProtector(ApmDataDumper* apm_data_dumper);
  SaturationProtector(ApmDataDumper* apm_data_dumper,
                      float initial_saturation_margin_db);

  void Reset();

  // Updates the margin by analyzing the estimated speech level
  // `speech_level_dbfs` and the peak power `speech_peak_dbfs` for an observed
  // frame which is reliably classified as "speech".
  void UpdateMargin(float speech_peak_dbfs, float speech_level_dbfs);

  // Returns latest computed margin.
  float margin_db() const { return margin_db_; }

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

  float GetDelayedPeakDbfs() const;

  ApmDataDumper* apm_data_dumper_;
  // Parameters.
  const float initial_saturation_margin_db_;
  // State.
  float margin_db_;
  RingBuffer peak_delay_buffer_;
  float max_peaks_dbfs_;
  int time_since_push_ms_;
};

}  // namespace webrtc

#endif  // MODULES_AUDIO_PROCESSING_AGC2_SATURATION_PROTECTOR_H_
