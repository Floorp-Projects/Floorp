/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et cindent: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef MEDIA_WEBRTC_SIGNALING_GTEST_MOCKCONDUIT_H_
#define MEDIA_WEBRTC_SIGNALING_GTEST_MOCKCONDUIT_H_

#include "gmock/gmock.h"
#include "MediaConduitInterface.h"
#include "libwebrtcglue/FrameTransformer.h"

namespace webrtc {
std::ostream& operator<<(std::ostream& aStream,
                         const webrtc::Call::Stats& aObj) {
  aStream << aObj.ToString(0);
  return aStream;
}
}  // namespace webrtc

namespace mozilla {
class MockConduit : public MediaSessionConduit {
 public:
  MockConduit() = default;

  MOCK_CONST_METHOD0(type, Type());
  MOCK_CONST_METHOD0(ActiveSendPayloadType, Maybe<int>());
  MOCK_CONST_METHOD0(ActiveRecvPayloadType, Maybe<int>());
  MOCK_METHOD1(SetTransportActive, void(bool));
  MOCK_METHOD0(SenderRtpSendEvent, MediaEventSourceExc<MediaPacket>&());
  MOCK_METHOD0(SenderRtcpSendEvent, MediaEventSourceExc<MediaPacket>&());
  MOCK_METHOD0(ReceiverRtcpSendEvent, MediaEventSourceExc<MediaPacket>&());
  MOCK_METHOD1(
      ConnectReceiverRtpEvent,
      void(MediaEventSourceExc<webrtc::RtpPacketReceived, webrtc::RTPHeader>&));
  MOCK_METHOD1(ConnectReceiverRtcpEvent,
               void(MediaEventSourceExc<MediaPacket>&));
  MOCK_METHOD1(ConnectSenderRtcpEvent, void(MediaEventSourceExc<MediaPacket>&));
  MOCK_CONST_METHOD0(LastRtcpReceived, Maybe<DOMHighResTimeStamp>());
  MOCK_CONST_METHOD1(RtpSendBaseSeqFor, Maybe<uint16_t>(uint32_t));
  MOCK_CONST_METHOD0(GetNow, DOMHighResTimeStamp());
  MOCK_CONST_METHOD0(GetTimestampMaker, dom::RTCStatsTimestampMaker&());
  MOCK_CONST_METHOD0(GetLocalSSRCs, Ssrcs());
  MOCK_CONST_METHOD0(GetRemoteSSRC, Maybe<Ssrc>());
  MOCK_METHOD1(UnsetRemoteSSRC, void(Ssrc));
  MOCK_METHOD0(DisableSsrcChanges, void());
  MOCK_CONST_METHOD1(HasCodecPluginID, bool(uint64_t));
  MOCK_METHOD0(RtcpByeEvent, MediaEventSource<void>&());
  MOCK_METHOD0(RtcpTimeoutEvent, MediaEventSource<void>&());
  MOCK_METHOD0(RtpPacketEvent, MediaEventSource<void>&());
  MOCK_METHOD3(SendRtp,
               bool(const uint8_t*, size_t, const webrtc::PacketOptions&));
  MOCK_METHOD2(SendSenderRtcp, bool(const uint8_t*, size_t));
  MOCK_METHOD2(SendReceiverRtcp, bool(const uint8_t*, size_t));
  MOCK_METHOD2(DeliverPacket, void(rtc::CopyOnWriteBuffer, PacketType));
  MOCK_METHOD0(Shutdown, RefPtr<GenericPromise>());
  MOCK_METHOD0(AsAudioSessionConduit, Maybe<RefPtr<AudioSessionConduit>>());
  MOCK_METHOD0(AsVideoSessionConduit, Maybe<RefPtr<VideoSessionConduit>>());
  MOCK_CONST_METHOD0(GetCallStats, Maybe<webrtc::Call::Stats>());
  MOCK_METHOD1(SetJitterBufferTarget, void(DOMHighResTimeStamp));
  MOCK_CONST_METHOD0(GetUpstreamRtpSources, std::vector<webrtc::RtpSource>());
};
}  // namespace mozilla

#endif
