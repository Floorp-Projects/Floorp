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
class AudioProcessingImpl;
class AudioBuffer;

class VoiceDetectionImpl : public VoiceDetection,
                           public ProcessingComponent {
 public:
  explicit VoiceDetectionImpl(const AudioProcessingImpl* apm);
  virtual ~VoiceDetectionImpl();

  int ProcessCaptureAudio(AudioBuffer* audio);

  // VoiceDetection implementation.
  virtual bool is_enabled() const;

  // ProcessingComponent implementation.
  virtual int Initialize();

 private:
  // VoiceDetection implementation.
  virtual int Enable(bool enable);
  virtual int set_stream_has_voice(bool has_voice);
  virtual bool stream_has_voice() const;
  virtual int set_likelihood(Likelihood likelihood);
  virtual Likelihood likelihood() const;
  virtual int set_frame_size_ms(int size);
  virtual int frame_size_ms() const;

  // ProcessingComponent implementation.
  virtual void* CreateHandle() const;
  virtual int InitializeHandle(void* handle) const;
  virtual int ConfigureHandle(void* handle) const;
  virtual int DestroyHandle(void* handle) const;
  virtual int num_handles_required() const;
  virtual int GetHandleError(void* handle) const;

  const AudioProcessingImpl* apm_;
  bool stream_has_voice_;
  bool using_external_vad_;
  Likelihood likelihood_;
  int frame_size_ms_;
  int frame_size_samples_;
};
}  // namespace webrtc

#endif  // WEBRTC_MODULES_AUDIO_PROCESSING_VOICE_DETECTION_IMPL_H_
