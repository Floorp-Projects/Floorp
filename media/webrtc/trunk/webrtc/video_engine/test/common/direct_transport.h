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

#include "webrtc/system_wrappers/interface/critical_section_wrapper.h"
#include "webrtc/system_wrappers/interface/event_wrapper.h"
#include "webrtc/system_wrappers/interface/scoped_ptr.h"
#include "webrtc/system_wrappers/interface/thread_wrapper.h"
#include "webrtc/video_engine/new_include/transport.h"

namespace webrtc {

class PacketReceiver;

namespace test {

class DirectTransport : public newapi::Transport {
 public:
  DirectTransport();
  ~DirectTransport();

  virtual void StopSending();
  virtual void SetReceiver(PacketReceiver* receiver);

  virtual bool SendRTP(const uint8_t* data, size_t length) OVERRIDE;
  virtual bool SendRTCP(const uint8_t* data, size_t length) OVERRIDE;

 private:
  struct Packet {
    Packet();
    Packet(const uint8_t* data, size_t length);

    uint8_t data[1500];
    size_t length;
  };

  void QueuePacket(const uint8_t* data, size_t length);

  static bool NetworkProcess(void* transport);
  bool SendPackets();

  scoped_ptr<CriticalSectionWrapper> lock_;
  scoped_ptr<EventWrapper> packet_event_;
  scoped_ptr<ThreadWrapper> thread_;

  bool shutting_down_;

  std::deque<Packet> packet_queue_;
  PacketReceiver* receiver_;
};
}  // namespace test
}  // namespace webrtc

#endif  // WEBRTC_VIDEO_ENGINE_TEST_COMMON_DIRECT_TRANSPORT_H_
