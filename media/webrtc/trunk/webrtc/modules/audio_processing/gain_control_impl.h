/*
 *  Copyright (c) 2012 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef WEBRTC_MODULES_AUDIO_PROCESSING_GAIN_CONTROL_IMPL_H_
#define WEBRTC_MODULES_AUDIO_PROCESSING_GAIN_CONTROL_IMPL_H_

#include <vector>

#include "webrtc/modules/audio_processing/include/audio_processing.h"
#include "webrtc/modules/audio_processing/processing_component.h"

namespace webrtc {

class AudioBuffer;
class CriticalSectionWrapper;

class GainControlImpl : public GainControl,
                        public ProcessingComponent {
 public:
  GainControlImpl(const AudioProcessing* apm,
                  CriticalSectionWrapper* crit);
  virtual ~GainControlImpl();

  int ProcessRenderAudio(AudioBuffer* audio);
  int AnalyzeCaptureAudio(AudioBuffer* audio);
  int ProcessCaptureAudio(AudioBuffer* audio);

  // ProcessingComponent implementation.
  virtual int Initialize() OVERRIDE;

  // GainControl implementation.
  virtual bool is_enabled() const OVERRIDE;
  virtual int stream_analog_level() OVERRIDE;

 private:
  // GainControl implementation.
  virtual int Enable(bool enable) OVERRIDE;
  virtual int set_stream_analog_level(int level) OVERRIDE;
  virtual int set_mode(Mode mode) OVERRIDE;
  virtual Mode mode() const OVERRIDE;
  virtual int set_target_level_dbfs(int level) OVERRIDE;
  virtual int target_level_dbfs() const OVERRIDE;
  virtual int set_compression_gain_db(int gain) OVERRIDE;
  virtual int compression_gain_db() const OVERRIDE;
  virtual int enable_limiter(bool enable) OVERRIDE;
  virtual bool is_limiter_enabled() const OVERRIDE;
  virtual int set_analog_level_limits(int minimum, int maximum) OVERRIDE;
  virtual int analog_level_minimum() const OVERRIDE;
  virtual int analog_level_maximum() const OVERRIDE;
  virtual bool stream_is_saturated() const OVERRIDE;

  // ProcessingComponent implementation.
  virtual void* CreateHandle() const OVERRIDE;
  virtual int InitializeHandle(void* handle) const OVERRIDE;
  virtual int ConfigureHandle(void* handle) const OVERRIDE;
  virtual void DestroyHandle(void* handle) const OVERRIDE;
  virtual int num_handles_required() const OVERRIDE;
  virtual int GetHandleError(void* handle) const OVERRIDE;

  const AudioProcessing* apm_;
  CriticalSectionWrapper* crit_;
  Mode mode_;
  int minimum_capture_level_;
  int maximum_capture_level_;
  bool limiter_enabled_;
  int target_level_dbfs_;
  int compression_gain_db_;
  std::vector<int> capture_levels_;
  int analog_capture_level_;
  bool was_analog_level_set_;
  bool stream_is_saturated_;
};
}  // namespace webrtc

#endif  // WEBRTC_MODULES_AUDIO_PROCESSING_GAIN_CONTROL_IMPL_H_
