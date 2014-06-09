/*
 *  Copyright (c) 2013 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef WEBRTC_MODULES_AUDIO_DEVICE_ANDROID_AUDIO_COMMON_H_
#define WEBRTC_MODULES_AUDIO_DEVICE_ANDROID_AUDIO_COMMON_H_

namespace webrtc {

enum {
  kDefaultSampleRate = 44100,
  kNumChannels = 1,
  kDefaultBufSizeInSamples = kDefaultSampleRate * 10 / 1000,
};

class PlayoutDelayProvider {
 public:
  virtual int PlayoutDelayMs() = 0;

 protected:
  PlayoutDelayProvider() {}
  virtual ~PlayoutDelayProvider() {}
};

}  // namespace webrtc

#endif  // WEBRTC_MODULES_AUDIO_DEVICE_ANDROID_AUDIO_COMMON_H_
