/*
 *  Copyright (c) 2013 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef WEBRTC_MODULES_AUDIO_DEVICE_MOCK_AUDIO_DEVICE_BUFFER_H_
#define WEBRTC_MODULES_AUDIO_DEVICE_MOCK_AUDIO_DEVICE_BUFFER_H_

#include "testing/gmock/include/gmock/gmock.h"
#include "webrtc/modules/audio_device/audio_device_buffer.h"

namespace webrtc {

class MockAudioDeviceBuffer : public AudioDeviceBuffer {
 public:
  MockAudioDeviceBuffer() {}
  virtual ~MockAudioDeviceBuffer() {}

  MOCK_METHOD1(RequestPlayoutData, int32_t(uint32_t nSamples));
  MOCK_METHOD1(GetPlayoutData, int32_t(void* audioBuffer));
};

}  // namespace webrtc

#endif  // WEBRTC_MODULES_AUDIO_DEVICE_MOCK_AUDIO_DEVICE_BUFFER_H_
