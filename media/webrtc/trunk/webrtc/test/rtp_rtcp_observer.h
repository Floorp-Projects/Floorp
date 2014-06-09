/*
 *  Copyright (c) 2013 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */
#ifndef WEBRTC_VIDEO_ENGINE_TEST_COMMON_RTP_RTCP_OBSERVER_H_
#define WEBRTC_VIDEO_ENGINE_TEST_COMMON_RTP_RTCP_OBSERVER_H_

#include <map>
#include <vector>

#include "testing/gtest/include/gtest/gtest.h"

#include "webrtc/modules/rtp_rtcp/interface/rtp_header_parser.h"
#include "webrtc/typedefs.h"
#include "webrtc/video_send_stream.h"

namespace webrtc {
namespace test {

class RtpRtcpObserver {
 public:
  virtual ~RtpRtcpObserver() {}
  newapi::Transport* SendTransport() {
    return &send_transport_;
  }

  newapi::Transport* ReceiveTransport() {
    return &receive_transport_;
  }

  void SetReceivers(PacketReceiver* send_transport_receiver,
                    PacketReceiver* receive_transport_receiver) {
    send_transport_.SetReceiver(send_transport_receiver);
    receive_transport_.SetReceiver(receive_transport_receiver);
  }

  void StopSending() {
    send_transport_.StopSending();
    receive_transport_.StopSending();
  }

  virtual EventTypeWrapper Wait() {
    EventTypeWrapper result = observation_complete_->Wait(timeout_ms_);
    observation_complete_->Reset();
    return result;
  }

 protected:
  RtpRtcpObserver(unsigned int event_timeout_ms,
      const FakeNetworkPipe::Config& configuration)
      : lock_(CriticalSectionWrapper::CreateCriticalSection()),
        observation_complete_(EventWrapper::Create()),
        parser_(RtpHeaderParser::Create()),
        send_transport_(lock_.get(),
                        this,
                        &RtpRtcpObserver::OnSendRtp,
                        &RtpRtcpObserver::OnSendRtcp,
                        configuration),
        receive_transport_(lock_.get(),
                           this,
                           &RtpRtcpObserver::OnReceiveRtp,
                           &RtpRtcpObserver::OnReceiveRtcp,
                           configuration),
        timeout_ms_(event_timeout_ms) {}

  explicit RtpRtcpObserver(unsigned int event_timeout_ms)
      : lock_(CriticalSectionWrapper::CreateCriticalSection()),
        observation_complete_(EventWrapper::Create()),
        parser_(RtpHeaderParser::Create()),
        send_transport_(lock_.get(),
                        this,
                        &RtpRtcpObserver::OnSendRtp,
                        &RtpRtcpObserver::OnSendRtcp,
                        FakeNetworkPipe::Config()),
        receive_transport_(lock_.get(),
                           this,
                           &RtpRtcpObserver::OnReceiveRtp,
                           &RtpRtcpObserver::OnReceiveRtcp,
                           FakeNetworkPipe::Config()),
        timeout_ms_(event_timeout_ms) {}

  enum Action {
    SEND_PACKET,
    DROP_PACKET,
  };

  virtual Action OnSendRtp(const uint8_t* packet, size_t length) {
    return SEND_PACKET;
  }

  virtual Action OnSendRtcp(const uint8_t* packet, size_t length) {
    return SEND_PACKET;
  }

  virtual Action OnReceiveRtp(const uint8_t* packet, size_t length) {
    return SEND_PACKET;
  }

  virtual Action OnReceiveRtcp(const uint8_t* packet, size_t length) {
    return SEND_PACKET;
  }


 private:
  class PacketTransport : public test::DirectTransport {
   public:
    typedef Action (RtpRtcpObserver::*PacketTransportAction)(const uint8_t*,
                                                             size_t);

    PacketTransport(CriticalSectionWrapper* lock,
                    RtpRtcpObserver* observer,
                    PacketTransportAction on_rtp,
                    PacketTransportAction on_rtcp,
                    const FakeNetworkPipe::Config& configuration)
        : test::DirectTransport(configuration),
          lock_(lock),
          observer_(observer),
          on_rtp_(on_rtp),
          on_rtcp_(on_rtcp) {}

  private:
    virtual bool SendRtp(const uint8_t* packet, size_t length) OVERRIDE {
      EXPECT_FALSE(RtpHeaderParser::IsRtcp(packet, static_cast<int>(length)));
      Action action;
      {
        CriticalSectionScoped crit_(lock_);
        action = (observer_->*on_rtp_)(packet, length);
      }
      switch (action) {
        case DROP_PACKET:
          // Drop packet silently.
          return true;
        case SEND_PACKET:
          return test::DirectTransport::SendRtp(packet, length);
      }
      return true;  // Will never happen, makes compiler happy.
    }

    virtual bool SendRtcp(const uint8_t* packet, size_t length) OVERRIDE {
      EXPECT_TRUE(RtpHeaderParser::IsRtcp(packet, static_cast<int>(length)));
      Action action;
      {
        CriticalSectionScoped crit_(lock_);
        action = (observer_->*on_rtcp_)(packet, length);
      }
      switch (action) {
        case DROP_PACKET:
          // Drop packet silently.
          return true;
        case SEND_PACKET:
          return test::DirectTransport::SendRtcp(packet, length);
      }
      return true;  // Will never happen, makes compiler happy.
    }

    // Pointer to shared lock instance protecting on_rtp_/on_rtcp_ calls.
    CriticalSectionWrapper* lock_;

    RtpRtcpObserver* observer_;
    PacketTransportAction on_rtp_, on_rtcp_;
  };

 protected:
  scoped_ptr<CriticalSectionWrapper> lock_;
  scoped_ptr<EventWrapper> observation_complete_;
  scoped_ptr<RtpHeaderParser> parser_;

 private:
  PacketTransport send_transport_, receive_transport_;
  unsigned int timeout_ms_;
};
}  // namespace test
}  // namespace webrtc

#endif  // WEBRTC_VIDEO_ENGINE_TEST_COMMON_RTP_RTCP_OBSERVER_H_
