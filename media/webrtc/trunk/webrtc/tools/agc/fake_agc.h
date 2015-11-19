/*
 *  Copyright (c) 2012 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef WEBRTC_TOOLS_AGC_FAKE_AGC_H_
#define WEBRTC_TOOLS_AGC_FAKE_AGC_H_

#include "webrtc/modules/audio_processing/agc/agc.h"

namespace webrtc {

class FakeAgc : public Agc {
 public:
  FakeAgc()
      : counter_(0),
        volume_(kMaxVolume / 2) {
  }

  virtual int Process(const AudioFrame& audio_frame) {
    const int kUpdateIntervalFrames = 10;
    const int kMaxVolume = 255;
    if (counter_ % kUpdateIntervalFrames == 0) {
      volume_ = (++volume_) % kMaxVolume;
    }
    counter_++;
    return 0;
  }

  virtual int FakeAgc::MicVolume() {
    return volume_;
  }

 private:
  int counter_;
  int volume_;
};

}  // namespace webrtc

#endif  // WEBRTC_TOOLS_AGC_FAKE_AGC_H_
