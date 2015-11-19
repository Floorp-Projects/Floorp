/*
 *  Copyright (c) 2013 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */
#ifndef WEBRTC_TEST_FAKE_AUDIO_DEVICE_H_
#define WEBRTC_TEST_FAKE_AUDIO_DEVICE_H_

#include <string>

#include "webrtc/base/scoped_ptr.h"
#include "webrtc/modules/audio_device/include/fake_audio_device.h"
#include "webrtc/typedefs.h"

namespace webrtc {

class Clock;
class CriticalSectionWrapper;
class EventWrapper;
class FileWrapper;
class ModuleFileUtility;
class ThreadWrapper;

namespace test {

class FakeAudioDevice : public FakeAudioDeviceModule {
 public:
  FakeAudioDevice(Clock* clock, const std::string& filename);

  virtual ~FakeAudioDevice();

  int32_t Init() override;
  int32_t RegisterAudioCallback(AudioTransport* callback) override;

  bool Playing() const override;
  int32_t PlayoutDelay(uint16_t* delay_ms) const override;
  bool Recording() const override;

  void Start();
  void Stop();

 private:
  static bool Run(void* obj);
  void CaptureAudio();

  static const uint32_t kFrequencyHz = 16000;
  static const uint32_t kBufferSizeBytes = 2 * kFrequencyHz;

  AudioTransport* audio_callback_;
  bool capturing_;
  int8_t captured_audio_[kBufferSizeBytes];
  int8_t playout_buffer_[kBufferSizeBytes];
  int64_t last_playout_ms_;

  Clock* clock_;
  rtc::scoped_ptr<EventWrapper> tick_;
  rtc::scoped_ptr<CriticalSectionWrapper> lock_;
  rtc::scoped_ptr<ThreadWrapper> thread_;
  rtc::scoped_ptr<ModuleFileUtility> file_utility_;
  rtc::scoped_ptr<FileWrapper> input_stream_;
};
}  // namespace test
}  // namespace webrtc

#endif  // WEBRTC_TEST_FAKE_AUDIO_DEVICE_H_
