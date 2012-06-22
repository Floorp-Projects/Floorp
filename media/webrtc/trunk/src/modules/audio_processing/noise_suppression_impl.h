/*
 *  Copyright (c) 2012 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef WEBRTC_MODULES_AUDIO_PROCESSING_NOISE_SUPPRESSION_IMPL_H_
#define WEBRTC_MODULES_AUDIO_PROCESSING_NOISE_SUPPRESSION_IMPL_H_

#include "audio_processing.h"
#include "processing_component.h"

namespace webrtc {
class AudioProcessingImpl;
class AudioBuffer;

class NoiseSuppressionImpl : public NoiseSuppression,
                             public ProcessingComponent {
 public:
  explicit NoiseSuppressionImpl(const AudioProcessingImpl* apm);
  virtual ~NoiseSuppressionImpl();

  int ProcessCaptureAudio(AudioBuffer* audio);

  // NoiseSuppression implementation.
  virtual bool is_enabled() const;

 private:
  // NoiseSuppression implementation.
  virtual int Enable(bool enable);
  virtual int set_level(Level level);
  virtual Level level() const;

  // ProcessingComponent implementation.
  virtual void* CreateHandle() const;
  virtual int InitializeHandle(void* handle) const;
  virtual int ConfigureHandle(void* handle) const;
  virtual int DestroyHandle(void* handle) const;
  virtual int num_handles_required() const;
  virtual int GetHandleError(void* handle) const;

  const AudioProcessingImpl* apm_;
  Level level_;
};
}  // namespace webrtc

#endif  // WEBRTC_MODULES_AUDIO_PROCESSING_NOISE_SUPPRESSION_IMPL_H_
