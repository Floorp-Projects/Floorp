/*
 *  Copyright (c) 2012 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef WEBRTC_MODULES_AUDIO_PROCESSING_VOICE_DETECTION_IMPL_H_
#define WEBRTC_MODULES_AUDIO_PROCESSING_VOICE_DETECTION_IMPL_H_

#include "webrtc/modules/audio_processing/include/audio_processing.h"
#include "webrtc/modules/audio_processing/processing_component.h"

namespace webrtc {

class AudioBuffer;
class CriticalSectionWrapper;

class VoiceDetectionImpl : public VoiceDetection,
                           public ProcessingComponent {
 public:
  VoiceDetectionImpl(const AudioProcessing* apm, CriticalSectionWrapper* crit);
  virtual ~VoiceDetectionImpl();

  int ProcessCaptureAudio(AudioBuffer* audio);

  // VoiceDetection implementation.
  virtual bool is_enabled() const OVERRIDE;

  // ProcessingComponent implementation.
  virtual int Initialize() OVERRIDE;

 private:
  // VoiceDetection implementation.
  virtual int Enable(bool enable) OVERRIDE;
  virtual int set_stream_has_voice(bool has_voice) OVERRIDE;
  virtual bool stream_has_voice() const OVERRIDE;
  virtual int set_likelihood(Likelihood likelihood) OVERRIDE;
  virtual Likelihood likelihood() const OVERRIDE;
  virtual int set_frame_size_ms(int size) OVERRIDE;
  virtual int frame_size_ms() const OVERRIDE;

  // ProcessingComponent implementation.
  virtual void* CreateHandle() const OVERRIDE;
  virtual int InitializeHandle(void* handle) const OVERRIDE;
  virtual int ConfigureHandle(void* handle) const OVERRIDE;
  virtual void DestroyHandle(void* handle) const OVERRIDE;
  virtual int num_handles_required() const OVERRIDE;
  virtual int GetHandleError(void* handle) const OVERRIDE;

  const AudioProcessing* apm_;
  CriticalSectionWrapper* crit_;
  bool stream_has_voice_;
  bool using_external_vad_;
  Likelihood likelihood_;
  int frame_size_ms_;
  int frame_size_samples_;
};
}  // namespace webrtc

#endif  // WEBRTC_MODULES_AUDIO_PROCESSING_VOICE_DETECTION_IMPL_H_
