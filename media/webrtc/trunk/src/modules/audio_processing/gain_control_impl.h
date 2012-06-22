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

#include "audio_processing.h"
#include "processing_component.h"

namespace webrtc {
class AudioProcessingImpl;
class AudioBuffer;

class GainControlImpl : public GainControl,
                        public ProcessingComponent {
 public:
  explicit GainControlImpl(const AudioProcessingImpl* apm);
  virtual ~GainControlImpl();

  int ProcessRenderAudio(AudioBuffer* audio);
  int AnalyzeCaptureAudio(AudioBuffer* audio);
  int ProcessCaptureAudio(AudioBuffer* audio);

  // ProcessingComponent implementation.
  virtual int Initialize();

  // GainControl implementation.
  virtual bool is_enabled() const;
  virtual int stream_analog_level();

 private:
  // GainControl implementation.
  virtual int Enable(bool enable);
  virtual int set_stream_analog_level(int level);
  virtual int set_mode(Mode mode);
  virtual Mode mode() const;
  virtual int set_target_level_dbfs(int level);
  virtual int target_level_dbfs() const;
  virtual int set_compression_gain_db(int gain);
  virtual int compression_gain_db() const;
  virtual int enable_limiter(bool enable);
  virtual bool is_limiter_enabled() const;
  virtual int set_analog_level_limits(int minimum, int maximum);
  virtual int analog_level_minimum() const;
  virtual int analog_level_maximum() const;
  virtual bool stream_is_saturated() const;

  // ProcessingComponent implementation.
  virtual void* CreateHandle() const;
  virtual int InitializeHandle(void* handle) const;
  virtual int ConfigureHandle(void* handle) const;
  virtual int DestroyHandle(void* handle) const;
  virtual int num_handles_required() const;
  virtual int GetHandleError(void* handle) const;

  const AudioProcessingImpl* apm_;
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
