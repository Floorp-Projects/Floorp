/*
 *  Copyright (c) 2013 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */
#include "webrtc/video_engine/test/common/direct_transport.h"

#include "testing/gtest/include/gtest/gtest.h"
#include "webrtc/video_engine/new_include/call.h"

namespace webrtc {
namespace test {

DirectTransport::DirectTransport()
    : lock_(CriticalSectionWrapper::CreateCriticalSection()),
      packet_event_(EventWrapper::Create()),
      thread_(ThreadWrapper::CreateThread(NetworkProcess, this)),
      shutting_down_(false),
      receiver_(NULL) {
  unsigned int thread_id;
  EXPECT_TRUE(thread_->Start(thread_id));
}

DirectTransport::~DirectTransport() { StopSending(); }

void DirectTransport::StopSending() {
  {
    CriticalSectionScoped crit_(lock_.get());
    shutting_down_ = true;
  }

  packet_event_->Set();
  EXPECT_TRUE(thread_->Stop());
}

void DirectTransport::SetReceiver(PacketReceiver* receiver) {
  receiver_ = receiver;
}

bool DirectTransport::SendRTP(const uint8_t* data, size_t length) {
  QueuePacket(data, length);
  return true;
}

bool DirectTransport::SendRTCP(const uint8_t* data, size_t length) {
  QueuePacket(data, length);
  return true;
}

DirectTransport::Packet::Packet() : length(0) {}

DirectTransport::Packet::Packet(const uint8_t* data, size_t length)
    : length(length) {
  EXPECT_LE(length, sizeof(this->data));
  memcpy(this->data, data, length);
}

void DirectTransport::QueuePacket(const uint8_t* data, size_t length) {
  CriticalSectionScoped crit(lock_.get());
  EXPECT_TRUE(receiver_ != NULL);
  packet_queue_.push_back(Packet(data, length));
  packet_event_->Set();
}

bool DirectTransport::NetworkProcess(void* transport) {
  return static_cast<DirectTransport*>(transport)->SendPackets();
}

bool DirectTransport::SendPackets() {
  while (true) {
    Packet p;
    {
      CriticalSectionScoped crit(lock_.get());
      if (packet_queue_.empty())
        break;
      p = packet_queue_.front();
      packet_queue_.pop_front();
    }
    receiver_->DeliverPacket(p.data, p.length);
  }

  switch (packet_event_->Wait(WEBRTC_EVENT_INFINITE)) {
    case kEventSignaled:
      packet_event_->Reset();
      break;
    case kEventTimeout:
      break;
    case kEventError:
      // TODO(pbos): Log a warning here?
      return true;
  }

  CriticalSectionScoped crit(lock_.get());
  return shutting_down_ ? false : true;
}
}  // namespace test
}  // namespace webrtc
