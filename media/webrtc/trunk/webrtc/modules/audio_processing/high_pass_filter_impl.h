/*
 *  Copyright (c) 2012 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef WEBRTC_MODULES_AUDIO_PROCESSING_HIGH_PASS_FILTER_IMPL_H_
#define WEBRTC_MODULES_AUDIO_PROCESSING_HIGH_PASS_FILTER_IMPL_H_

#include "webrtc/modules/audio_processing/include/audio_processing.h"
#include "webrtc/modules/audio_processing/processing_component.h"

namespace webrtc {
class AudioProcessingImpl;
class AudioBuffer;

class HighPassFilterImpl : public HighPassFilter,
                           public ProcessingComponent {
 public:
  explicit HighPassFilterImpl(const AudioProcessingImpl* apm);
  virtual ~HighPassFilterImpl();

  int ProcessCaptureAudio(AudioBuffer* audio);

  // HighPassFilter implementation.
  virtual bool is_enabled() const OVERRIDE;

 private:
  // HighPassFilter implementation.
  virtual int Enable(bool enable) OVERRIDE;

  // ProcessingComponent implementation.
  virtual void* CreateHandle() const OVERRIDE;
  virtual int InitializeHandle(void* handle) const OVERRIDE;
  virtual int ConfigureHandle(void* handle) const OVERRIDE;
  virtual int DestroyHandle(void* handle) const OVERRIDE;
  virtual int num_handles_required() const OVERRIDE;
  virtual int GetHandleError(void* handle) const OVERRIDE;

  const AudioProcessingImpl* apm_;
};
}  // namespace webrtc

#endif  // WEBRTC_MODULES_AUDIO_PROCESSING_HIGH_PASS_FILTER_IMPL_H_
