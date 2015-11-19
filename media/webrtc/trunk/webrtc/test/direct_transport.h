/*
 *  Copyright (c) 2013 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */
#ifndef WEBRTC_VIDEO_ENGINE_TEST_COMMON_DIRECT_TRANSPORT_H_
#define WEBRTC_VIDEO_ENGINE_TEST_COMMON_DIRECT_TRANSPORT_H_

#include <assert.h>

#include <deque>

#include "webrtc/base/scoped_ptr.h"
#include "webrtc/system_wrappers/interface/critical_section_wrapper.h"
#include "webrtc/system_wrappers/interface/event_wrapper.h"
#include "webrtc/system_wrappers/interface/thread_wrapper.h"
#include "webrtc/test/fake_network_pipe.h"
#include "webrtc/transport.h"

namespace webrtc {

class Clock;
class PacketReceiver;

namespace test {

class DirectTransport : public newapi::Transport {
 public:
  DirectTransport();
  explicit DirectTransport(const FakeNetworkPipe::Config& config);
  ~DirectTransport();

  void SetConfig(const FakeNetworkPipe::Config& config);

  virtual void StopSending();
  virtual void SetReceiver(PacketReceiver* receiver);

  bool SendRtp(const uint8_t* data, size_t length) override;
  bool SendRtcp(const uint8_t* data, size_t length) override;

 private:
  static bool NetworkProcess(void* transport);
  bool SendPackets();

  rtc::scoped_ptr<CriticalSectionWrapper> lock_;
  rtc::scoped_ptr<EventWrapper> packet_event_;
  rtc::scoped_ptr<ThreadWrapper> thread_;
  Clock* const clock_;

  bool shutting_down_;

  FakeNetworkPipe fake_network_;
};
}  // namespace test
}  // namespace webrtc

#endif  // WEBRTC_VIDEO_ENGINE_TEST_COMMON_DIRECT_TRANSPORT_H_
