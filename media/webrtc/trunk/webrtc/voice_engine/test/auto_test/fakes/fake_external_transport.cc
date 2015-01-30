/*
 *  Copyright (c) 2012 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "webrtc/system_wrappers/interface/critical_section_wrapper.h"
#include "webrtc/system_wrappers/interface/event_wrapper.h"
#include "webrtc/system_wrappers/interface/sleep.h"
#include "webrtc/system_wrappers/interface/thread_wrapper.h"
#include "webrtc/voice_engine/include/voe_network.h"
#include "webrtc/voice_engine/test/auto_test/fakes/fake_external_transport.h"
#include "webrtc/voice_engine/voice_engine_defines.h"

FakeExternalTransport::FakeExternalTransport(webrtc::VoENetwork* ptr)
    : my_network_(ptr),
      thread_(NULL),
      lock_(NULL),
      event_(NULL),
      length_(0),
      channel_(0),
      delay_is_enabled_(0),
      delay_time_in_ms_(0) {
  const char* thread_name = "external_thread";
  lock_ = webrtc::CriticalSectionWrapper::CreateCriticalSection();
  event_ = webrtc::EventWrapper::Create();
  thread_ = webrtc::ThreadWrapper::CreateThread(
      Run, this, webrtc::kHighPriority, thread_name);
  if (thread_) {
    unsigned int id;
    thread_->Start(id);
  }
}

FakeExternalTransport::~FakeExternalTransport() {
  if (thread_) {
    thread_->SetNotAlive();
    event_->Set();
    if (thread_->Stop()) {
      delete thread_;
      thread_ = NULL;
      delete event_;
      event_ = NULL;
      delete lock_;
      lock_ = NULL;
    }
  }
}

bool FakeExternalTransport::Run(void* ptr) {
  return static_cast<FakeExternalTransport*> (ptr)->Process();
}

bool FakeExternalTransport::Process() {
  switch (event_->Wait(500)) {
    case webrtc::kEventSignaled:
      lock_->Enter();
      my_network_->ReceivedRTPPacket(channel_, packet_buffer_, length_,
                                     webrtc::PacketTime());
      lock_->Leave();
      return true;
    case webrtc::kEventTimeout:
      return true;
    case webrtc::kEventError:
      break;
  }
  return true;
}

int FakeExternalTransport::SendPacket(int channel, const void *data, int len) {
  lock_->Enter();
  if (len < 1612) {
    memcpy(packet_buffer_, (const unsigned char*) data, len);
    length_ = len;
    channel_ = channel;
  }
  lock_->Leave();
  event_->Set();  // Triggers ReceivedRTPPacket() from worker thread.
  return len;
}

int FakeExternalTransport::SendRTCPPacket(int channel,
                                          const void *data,
                                          int len) {
  if (delay_is_enabled_) {
    webrtc::SleepMs(delay_time_in_ms_);
  }
  my_network_->ReceivedRTCPPacket(channel, data, len);
  return len;
}

void FakeExternalTransport::SetDelayStatus(bool enable,
                                           unsigned int delayInMs) {
  delay_is_enabled_ = enable;
  delay_time_in_ms_ = delayInMs;
}
