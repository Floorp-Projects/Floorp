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

namespace webrtc {
class AudioProcessingImpl;
class AudioBuffer;

class LevelEstimatorImpl : public LevelEstimator,
                           public ProcessingComponent {
 public:
  explicit LevelEstimatorImpl(const AudioProcessingImpl* apm);
  virtual ~LevelEstimatorImpl();

  int ProcessStream(AudioBuffer* audio);

  // LevelEstimator implementation.
  virtual bool is_enabled() const;

 private:
  // LevelEstimator implementation.
  virtual int Enable(bool enable);
  virtual int RMS();

  // ProcessingComponent implementation.
  virtual void* CreateHandle() const;
  virtual int InitializeHandle(void* handle) const;
  virtual int ConfigureHandle(void* handle) const;
  virtual int DestroyHandle(void* handle) const;
  virtual int num_handles_required() const;
  virtual int GetHandleError(void* handle) const;

  const AudioProcessingImpl* apm_;
};
}  // namespace webrtc

#endif  // WEBRTC_MODULES_AUDIO_PROCESSING_LEVEL_ESTIMATOR_IMPL_H_
