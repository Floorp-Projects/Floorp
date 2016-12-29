/*
 *  Copyright (c) 2015 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "webrtc/voice_engine/channel_proxy.h"

#include <utility>

#include "webrtc/audio/audio_sink.h"
#include "webrtc/base/checks.h"
#include "webrtc/voice_engine/channel.h"

namespace webrtc {
namespace voe {
ChannelProxy::ChannelProxy() : channel_owner_(nullptr) {}

ChannelProxy::ChannelProxy(const ChannelOwner& channel_owner) :
    channel_owner_(channel_owner) {
  RTC_CHECK(channel_owner_.channel());
}

ChannelProxy::~ChannelProxy() {}

void ChannelProxy::SetRTCPStatus(bool enable) {
  channel()->SetRTCPStatus(enable);
}

void ChannelProxy::SetLocalSSRC(uint32_t ssrc) {
  RTC_DCHECK(thread_checker_.CalledOnValidThread());
  int error = channel()->SetLocalSSRC(ssrc);
  RTC_DCHECK_EQ(0, error);
}

void ChannelProxy::SetRTCP_CNAME(const std::string& c_name) {
  RTC_DCHECK(thread_checker_.CalledOnValidThread());
  // Note: VoERTP_RTCP::SetRTCP_CNAME() accepts a char[256] array.
  std::string c_name_limited = c_name.substr(0, 255);
  int error = channel()->SetRTCP_CNAME(c_name_limited.c_str());
  RTC_DCHECK_EQ(0, error);
}

void ChannelProxy::SetSendAbsoluteSenderTimeStatus(bool enable, int id) {
  RTC_DCHECK(thread_checker_.CalledOnValidThread());
  int error = channel()->SetSendAbsoluteSenderTimeStatus(enable, id);
  RTC_DCHECK_EQ(0, error);
}

void ChannelProxy::SetSendAudioLevelIndicationStatus(bool enable, int id) {
  RTC_DCHECK(thread_checker_.CalledOnValidThread());
  int error = channel()->SetSendAudioLevelIndicationStatus(enable, id);
  RTC_DCHECK_EQ(0, error);
}

void ChannelProxy::EnableSendTransportSequenceNumber(int id) {
  RTC_DCHECK(thread_checker_.CalledOnValidThread());
  channel()->EnableSendTransportSequenceNumber(id);
}

void ChannelProxy::SetReceiveAbsoluteSenderTimeStatus(bool enable, int id) {
  RTC_DCHECK(thread_checker_.CalledOnValidThread());
  int error = channel()->SetReceiveAbsoluteSenderTimeStatus(enable, id);
  RTC_DCHECK_EQ(0, error);
}

void ChannelProxy::SetReceiveAudioLevelIndicationStatus(bool enable, int id) {
  RTC_DCHECK(thread_checker_.CalledOnValidThread());
  int error = channel()->SetReceiveAudioLevelIndicationStatus(enable, id);
  RTC_DCHECK_EQ(0, error);
}

void ChannelProxy::SetCongestionControlObjects(
    RtpPacketSender* rtp_packet_sender,
    TransportFeedbackObserver* transport_feedback_observer,
    PacketRouter* packet_router) {
  RTC_DCHECK(thread_checker_.CalledOnValidThread());
  channel()->SetCongestionControlObjects(
      rtp_packet_sender, transport_feedback_observer, packet_router);
}

CallStatistics ChannelProxy::GetRTCPStatistics() const {
  RTC_DCHECK(thread_checker_.CalledOnValidThread());
  CallStatistics stats = {0};
  int error = channel()->GetRTPStatistics(stats);
  RTC_DCHECK_EQ(0, error);
  return stats;
}

std::vector<ReportBlock> ChannelProxy::GetRemoteRTCPReportBlocks() const {
  RTC_DCHECK(thread_checker_.CalledOnValidThread());
  std::vector<webrtc::ReportBlock> blocks;
  int error = channel()->GetRemoteRTCPReportBlocks(&blocks);
  RTC_DCHECK_EQ(0, error);
  return blocks;
}

NetworkStatistics ChannelProxy::GetNetworkStatistics() const {
  RTC_DCHECK(thread_checker_.CalledOnValidThread());
  NetworkStatistics stats = {0};
  int error = channel()->GetNetworkStatistics(stats);
  RTC_DCHECK_EQ(0, error);
  return stats;
}

AudioDecodingCallStats ChannelProxy::GetDecodingCallStatistics() const {
  RTC_DCHECK(thread_checker_.CalledOnValidThread());
  AudioDecodingCallStats stats;
  channel()->GetDecodingCallStatistics(&stats);
  return stats;
}

int32_t ChannelProxy::GetSpeechOutputLevelFullRange() const {
  RTC_DCHECK(thread_checker_.CalledOnValidThread());
  uint32_t level = 0;
  int error = channel()->GetSpeechOutputLevelFullRange(level);
  RTC_DCHECK_EQ(0, error);
  return static_cast<int32_t>(level);
}

uint32_t ChannelProxy::GetDelayEstimate() const {
  RTC_DCHECK(thread_checker_.CalledOnValidThread());
  return channel()->GetDelayEstimate();
}

bool ChannelProxy::SetSendTelephoneEventPayloadType(int payload_type) {
  RTC_DCHECK(thread_checker_.CalledOnValidThread());
  return channel()->SetSendTelephoneEventPayloadType(payload_type) == 0;
}

bool ChannelProxy::SendTelephoneEventOutband(uint8_t event,
                                             uint32_t duration_ms) {
  RTC_DCHECK(thread_checker_.CalledOnValidThread());
  return
      channel()->SendTelephoneEventOutband(event, duration_ms, 10, false) == 0;
}

void ChannelProxy::SetSink(rtc::scoped_ptr<AudioSinkInterface> sink) {
  RTC_DCHECK(thread_checker_.CalledOnValidThread());
  channel()->SetSink(std::move(sink));
}

Channel* ChannelProxy::channel() const {
  RTC_DCHECK(channel_owner_.channel());
  return channel_owner_.channel();
}

}  // namespace voe
}  // namespace webrtc
