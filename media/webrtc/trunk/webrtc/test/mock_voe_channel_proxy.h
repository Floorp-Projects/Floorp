/*
 *  Copyright (c) 2015 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef WEBRTC_TEST_MOCK_VOE_CHANNEL_PROXY_H_
#define WEBRTC_TEST_MOCK_VOE_CHANNEL_PROXY_H_

#include <string>
#include "testing/gmock/include/gmock/gmock.h"
#include "webrtc/voice_engine/channel_proxy.h"

namespace webrtc {
namespace test {

class MockVoEChannelProxy : public voe::ChannelProxy {
 public:
  MOCK_METHOD1(SetRTCPStatus, void(bool enable));
  MOCK_METHOD1(SetLocalSSRC, void(uint32_t ssrc));
  MOCK_METHOD1(SetRTCP_CNAME, void(const std::string& c_name));
  MOCK_METHOD2(SetSendAbsoluteSenderTimeStatus, void(bool enable, int id));
  MOCK_METHOD2(SetSendAudioLevelIndicationStatus, void(bool enable, int id));
  MOCK_METHOD1(EnableSendTransportSequenceNumber, void(int id));
  MOCK_METHOD2(SetReceiveAbsoluteSenderTimeStatus, void(bool enable, int id));
  MOCK_METHOD2(SetReceiveAudioLevelIndicationStatus, void(bool enable, int id));
  MOCK_METHOD3(SetCongestionControlObjects,
               void(RtpPacketSender* rtp_packet_sender,
                    TransportFeedbackObserver* transport_feedback_observer,
                    PacketRouter* seq_num_allocator));
  MOCK_CONST_METHOD0(GetRTCPStatistics, CallStatistics());
  MOCK_CONST_METHOD0(GetRemoteRTCPReportBlocks, std::vector<ReportBlock>());
  MOCK_CONST_METHOD0(GetNetworkStatistics, NetworkStatistics());
  MOCK_CONST_METHOD0(GetDecodingCallStatistics, AudioDecodingCallStats());
  MOCK_CONST_METHOD0(GetSpeechOutputLevelFullRange, int32_t());
  MOCK_CONST_METHOD0(GetDelayEstimate, uint32_t());
  MOCK_METHOD1(SetSendTelephoneEventPayloadType, bool(int payload_type));
  MOCK_METHOD2(SendTelephoneEventOutband, bool(uint8_t event,
                                               uint32_t duration_ms));
};
}  // namespace test
}  // namespace webrtc

#endif  // WEBRTC_TEST_MOCK_VOE_CHANNEL_PROXY_H_
