/*
 *  Copyright (c) 2012 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */
#ifndef VOICE_ENGINE_MAIN_TEST_AUTO_TEST_FAKES_FAKE_EXTERNAL_TRANSPORT_H_
#define VOICE_ENGINE_MAIN_TEST_AUTO_TEST_FAKES_FAKE_EXTERNAL_TRANSPORT_H_

#include "webrtc/common_types.h"

namespace webrtc {
class CriticalSectionWrapper;
class EventWrapper;
class ThreadWrapper;
class VoENetwork;
}

class FakeExternalTransport : public webrtc::Transport {
 public:
  explicit FakeExternalTransport(webrtc::VoENetwork* ptr);
  virtual ~FakeExternalTransport();
  int SendPacket(int channel, const void* data, size_t len) override;
  int SendRTCPPacket(int channel, const void* data, size_t len) override;
  void SetDelayStatus(bool enabled, unsigned int delayInMs = 100);

  webrtc::VoENetwork* my_network_;
 private:
  static bool Run(void* ptr);
  bool Process();
 private:
  rtc::scoped_ptr<webrtc::ThreadWrapper> thread_;
  webrtc::CriticalSectionWrapper* lock_;
  webrtc::EventWrapper* event_;
 private:
  unsigned char packet_buffer_[1612];
  size_t length_;
  int channel_;
  bool delay_is_enabled_;
  int delay_time_in_ms_;
};

#endif  // VOICE_ENGINE_MAIN_TEST_AUTO_TEST_FAKES_FAKE_EXTERNAL_TRANSPORT_H_
