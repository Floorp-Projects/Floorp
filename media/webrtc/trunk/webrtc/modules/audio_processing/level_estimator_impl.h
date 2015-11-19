/*
 *  Copyright (c) 2012 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef WEBRTC_MODULES_AUDIO_PROCESSING_LEVEL_ESTIMATOR_IMPL_H_
#define WEBRTC_MODULES_AUDIO_PROCESSING_LEVEL_ESTIMATOR_IMPL_H_

#include "webrtc/modules/audio_processing/include/audio_processing.h"
#include "webrtc/modules/audio_processing/processing_component.h"
#include "webrtc/modules/audio_processing/rms_level.h"

namespace webrtc {

class AudioBuffer;
class CriticalSectionWrapper;

class LevelEstimatorImpl : public LevelEstimator,
                           public ProcessingComponent {
 public:
  LevelEstimatorImpl(const AudioProcessing* apm,
                     CriticalSectionWrapper* crit);
  virtual ~LevelEstimatorImpl();

  int ProcessStream(AudioBuffer* audio);

  // LevelEstimator implementation.
  bool is_enabled() const override;

 private:
  // LevelEstimator implementation.
  int Enable(bool enable) override;
  int RMS() override;

  // ProcessingComponent implementation.
  void* CreateHandle() const override;
  int InitializeHandle(void* handle) const override;
  int ConfigureHandle(void* handle) const override;
  void DestroyHandle(void* handle) const override;
  int num_handles_required() const override;
  int GetHandleError(void* handle) const override;

  CriticalSectionWrapper* crit_;
};

}  // namespace webrtc

#endif  // WEBRTC_MODULES_AUDIO_PROCESSING_LEVEL_ESTIMATOR_IMPL_H_
