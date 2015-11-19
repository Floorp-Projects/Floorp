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

class AudioBuffer;
class CriticalSectionWrapper;

class HighPassFilterImpl : public HighPassFilter,
                           public ProcessingComponent {
 public:
  HighPassFilterImpl(const AudioProcessing* apm, CriticalSectionWrapper* crit);
  virtual ~HighPassFilterImpl();

  int ProcessCaptureAudio(AudioBuffer* audio);

  // HighPassFilter implementation.
  bool is_enabled() const override;

 private:
  // HighPassFilter implementation.
  int Enable(bool enable) override;

  // ProcessingComponent implementation.
  void* CreateHandle() const override;
  int InitializeHandle(void* handle) const override;
  int ConfigureHandle(void* handle) const override;
  void DestroyHandle(void* handle) const override;
  int num_handles_required() const override;
  int GetHandleError(void* handle) const override;

  const AudioProcessing* apm_;
  CriticalSectionWrapper* crit_;
};
}  // namespace webrtc

#endif  // WEBRTC_MODULES_AUDIO_PROCESSING_HIGH_PASS_FILTER_IMPL_H_
