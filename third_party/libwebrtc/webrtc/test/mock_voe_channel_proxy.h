/*
 *  Copyright (c) 2015 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef TEST_MOCK_VOE_CHANNEL_PROXY_H_
#define TEST_MOCK_VOE_CHANNEL_PROXY_H_

#include <string>

#include "modules/rtp_rtcp/source/rtp_packet_received.h"
#include "test/gmock.h"
#include "voice_engine/channel_proxy.h"

namespace webrtc {
namespace test {

class MockVoEChannelProxy : public voe::ChannelProxy {
 public:
  // GTest doesn't like move-only types, like std::unique_ptr
  bool SetEncoder(int payload_type,
                  std::unique_ptr<AudioEncoder> encoder) {
    return SetEncoderForMock(payload_type, &encoder);
  }
  MOCK_METHOD2(SetEncoderForMock,
               bool(int payload_type,
                    std::unique_ptr<AudioEncoder>* encoder));
  MOCK_METHOD1(
      ModifyEncoder,
      void(rtc::FunctionView<void(std::unique_ptr<AudioEncoder>*)> modifier));
  MOCK_METHOD1(SetRTCPStatus, void(bool enable));
  MOCK_METHOD1(SetLocalSSRC, void(uint32_t ssrc));
  MOCK_METHOD1(SetRTCP_CNAME, void(const std::string& c_name));
  MOCK_METHOD2(SetNACKStatus, void(bool enable, int max_packets));
  MOCK_METHOD2(SetSendAudioLevelIndicationStatus, void(bool enable, int id));
  MOCK_METHOD2(SetReceiveAudioLevelIndicationStatus, void(bool enable, int id));
  MOCK_METHOD1(EnableSendTransportSequenceNumber, void(int id));
  MOCK_METHOD1(EnableReceiveTransportSequenceNumber, void(int id));
  MOCK_METHOD2(RegisterSenderCongestionControlObjects,
               void(RtpTransportControllerSendInterface* transport,
                    RtcpBandwidthObserver* bandwidth_observer));
  MOCK_METHOD1(RegisterReceiverCongestionControlObjects,
               void(PacketRouter* packet_router));
  MOCK_METHOD0(ResetSenderCongestionControlObjects, void());
  MOCK_METHOD0(ResetReceiverCongestionControlObjects, void());
  MOCK_CONST_METHOD0(GetRTCPStatistics, CallStatistics());
  MOCK_CONST_METHOD0(GetRemoteRTCPReportBlocks, std::vector<ReportBlock>());
  MOCK_CONST_METHOD0(GetNetworkStatistics, NetworkStatistics());
  MOCK_CONST_METHOD0(GetDecodingCallStatistics, AudioDecodingCallStats());
  MOCK_CONST_METHOD0(GetANAStatistics, ANAStats());
  MOCK_CONST_METHOD0(GetSpeechOutputLevel, int());
  MOCK_CONST_METHOD0(GetSpeechOutputLevelFullRange, int());
  MOCK_CONST_METHOD0(GetTotalOutputEnergy, double());
  MOCK_CONST_METHOD0(GetTotalOutputDuration, double());
  MOCK_CONST_METHOD0(GetDelayEstimate, uint32_t());
  MOCK_METHOD2(SetSendTelephoneEventPayloadType, bool(int payload_type,
                                                      int payload_frequency));
  MOCK_METHOD2(SendTelephoneEventOutband, bool(int event, int duration_ms));
  MOCK_METHOD2(SetBitrate, void(int bitrate_bps, int64_t probing_interval_ms));
  // TODO(solenberg): Talk the compiler into accepting this mock method:
  // MOCK_METHOD1(SetSink, void(std::unique_ptr<AudioSinkInterface> sink));
  MOCK_METHOD1(SetInputMute, void(bool muted));
  MOCK_METHOD1(RegisterTransport, void(Transport* transport));
  MOCK_METHOD1(OnRtpPacket, void(const RtpPacketReceived& packet));
  MOCK_METHOD2(ReceivedRTCPPacket, bool(const uint8_t* packet, size_t length));
  MOCK_CONST_METHOD0(GetAudioDecoderFactory,
                     const rtc::scoped_refptr<AudioDecoderFactory>&());
  MOCK_METHOD1(SetChannelOutputVolumeScaling, void(float scaling));
  MOCK_METHOD1(SetRtcEventLog, void(RtcEventLog* event_log));
  MOCK_METHOD1(SetRtcpRttStats, void(RtcpRttStats* rtcp_rtt_stats));
  MOCK_METHOD2(GetAudioFrameWithInfo,
      AudioMixer::Source::AudioFrameInfo(int sample_rate_hz,
                                         AudioFrame* audio_frame));
  MOCK_CONST_METHOD0(PreferredSampleRate, int());
  MOCK_METHOD1(SetTransportOverhead, void(int transport_overhead_per_packet));
  MOCK_METHOD1(AssociateSendChannel,
               void(const ChannelProxy& send_channel_proxy));
  MOCK_METHOD0(DisassociateSendChannel, void());
  MOCK_CONST_METHOD2(GetRtpRtcp, void(RtpRtcp** rtp_rtcp,
                                      RtpReceiver** rtp_receiver));
  MOCK_CONST_METHOD0(GetPlayoutTimestamp, uint32_t());
  MOCK_METHOD1(SetMinimumPlayoutDelay, void(int delay_ms));
  MOCK_CONST_METHOD1(GetRecCodec, bool(CodecInst* codec_inst));
  MOCK_METHOD1(SetReceiveCodecs,
               void(const std::map<int, SdpAudioFormat>& codecs));
  MOCK_METHOD1(OnTwccBasedUplinkPacketLossRate, void(float packet_loss_rate));
  MOCK_METHOD1(OnRecoverableUplinkPacketLossRate,
               void(float recoverable_packet_loss_rate));
  MOCK_CONST_METHOD0(GetSources, std::vector<RtpSource>());
};
}  // namespace test
}  // namespace webrtc

#endif  // TEST_MOCK_VOE_CHANNEL_PROXY_H_
