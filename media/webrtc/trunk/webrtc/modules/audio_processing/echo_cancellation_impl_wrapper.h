/*
 *  Copyright (c) 2013 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef WEBRTC_MODULES_AUDIO_PROCESSING_ECHO_CANCELLATION_IMPL_WRAPPER_H_
#define WEBRTC_MODULES_AUDIO_PROCESSING_ECHO_CANCELLATION_IMPL_WRAPPER_H_

#include "webrtc/modules/audio_processing/include/audio_processing.h"
#include "webrtc/modules/audio_processing/processing_component.h"

namespace webrtc {

class AudioProcessingImpl;
class AudioBuffer;

class EchoCancellationImplWrapper : public virtual EchoCancellation,
                                    public virtual ProcessingComponent {
 public:
  static EchoCancellationImplWrapper* Create(
      const AudioProcessingImpl* audioproc);
  virtual ~EchoCancellationImplWrapper() {}

  virtual int ProcessRenderAudio(const AudioBuffer* audio) = 0;
  virtual int ProcessCaptureAudio(AudioBuffer* audio) = 0;
};

}  // namespace webrtc

#endif  // WEBRTC_MODULES_AUDIO_PROCESSING_ECHO_CANCELLATION_IMPL_WRAPPER_H_
