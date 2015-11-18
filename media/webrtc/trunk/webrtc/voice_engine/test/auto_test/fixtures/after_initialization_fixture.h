/*
 *  Copyright (c) 2011 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef SRC_VOICE_ENGINE_MAIN_TEST_AUTO_TEST_STANDARD_TEST_BASE_AFTER_INIT_H_
#define SRC_VOICE_ENGINE_MAIN_TEST_AUTO_TEST_STANDARD_TEST_BASE_AFTER_INIT_H_

#include <deque>

#include "webrtc/base/scoped_ptr.h"
#include "webrtc/common_types.h"
#include "webrtc/system_wrappers/interface/atomic32.h"
#include "webrtc/system_wrappers/interface/critical_section_wrapper.h"
#include "webrtc/system_wrappers/interface/event_wrapper.h"
#include "webrtc/system_wrappers/interface/sleep.h"
#include "webrtc/system_wrappers/interface/thread_wrapper.h"
#include "webrtc/voice_engine/test/auto_test/fixtures/before_initialization_fixture.h"

class TestErrorObserver;

class LoopBackTransport : public webrtc::Transport {
 public:
  LoopBackTransport(webrtc::VoENetwork* voe_network)
      : crit_(webrtc::CriticalSectionWrapper::CreateCriticalSection()),
        packet_event_(webrtc::EventWrapper::Create()),
        thread_(webrtc::ThreadWrapper::CreateThread(
            NetworkProcess, this, "LoopBackTransport")),
        voe_network_(voe_network), transmitted_packets_(0) {
    thread_->Start();
  }

  ~LoopBackTransport() { thread_->Stop(); }

  int SendPacket(int channel, const void* data, size_t len) override {
    StorePacket(Packet::Rtp, channel, data, len);
    return static_cast<int>(len);
  }

  int SendRTCPPacket(int channel, const void* data, size_t len) override {
    StorePacket(Packet::Rtcp, channel, data, len);
    return static_cast<int>(len);
  }

  void WaitForTransmittedPackets(int32_t packet_count) {
    enum {
      kSleepIntervalMs = 10
    };
    int32_t limit = transmitted_packets_.Value() + packet_count;
    while (transmitted_packets_.Value() < limit) {
      webrtc::SleepMs(kSleepIntervalMs);
    }
  }

 private:
  struct Packet {
    enum Type { Rtp, Rtcp, } type;

    Packet() : len(0) {}
    Packet(Type type, int channel, const void* data, size_t len)
        : type(type), channel(channel), len(len) {
      assert(len <= 1500);
      memcpy(this->data, data, len);
    }

    int channel;
    uint8_t data[1500];
    size_t len;
  };

  void StorePacket(Packet::Type type, int channel,
                   const void* data,
                   size_t len) {
    {
      webrtc::CriticalSectionScoped lock(crit_.get());
      packet_queue_.push_back(Packet(type, channel, data, len));
    }
    packet_event_->Set();
  }

  static bool NetworkProcess(void* transport) {
    return static_cast<LoopBackTransport*>(transport)->SendPackets();
  }

  bool SendPackets() {
    switch (packet_event_->Wait(10)) {
      case webrtc::kEventSignaled:
        break;
      case webrtc::kEventTimeout:
        break;
      case webrtc::kEventError:
        // TODO(pbos): Log a warning here?
        return true;
    }

    while (true) {
      Packet p;
      {
        webrtc::CriticalSectionScoped lock(crit_.get());
        if (packet_queue_.empty())
          break;
        p = packet_queue_.front();
        packet_queue_.pop_front();
      }

      switch (p.type) {
        case Packet::Rtp:
          voe_network_->ReceivedRTPPacket(p.channel, p.data, p.len,
                                          webrtc::PacketTime());
          break;
        case Packet::Rtcp:
          voe_network_->ReceivedRTCPPacket(p.channel, p.data, p.len);
          break;
      }
      ++transmitted_packets_;
    }
    return true;
  }

  const rtc::scoped_ptr<webrtc::CriticalSectionWrapper> crit_;
  const rtc::scoped_ptr<webrtc::EventWrapper> packet_event_;
  const rtc::scoped_ptr<webrtc::ThreadWrapper> thread_;
  std::deque<Packet> packet_queue_ GUARDED_BY(crit_.get());
  webrtc::VoENetwork* const voe_network_;
  webrtc::Atomic32 transmitted_packets_;
};

// This fixture initializes the voice engine in addition to the work
// done by the before-initialization fixture. It also registers an error
// observer which will fail tests on error callbacks. This fixture is
// useful to tests that want to run before we have started any form of
// streaming through the voice engine.
class AfterInitializationFixture : public BeforeInitializationFixture {
 public:
  AfterInitializationFixture();
  virtual ~AfterInitializationFixture();

 protected:
  rtc::scoped_ptr<TestErrorObserver> error_observer_;
};

#endif  // SRC_VOICE_ENGINE_MAIN_TEST_AUTO_TEST_STANDARD_TEST_BASE_AFTER_INIT_H_
