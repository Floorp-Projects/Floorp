/*
 *  Copyright (c) 2012 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "webrtc/video_engine/vie_rtp_rtcp_impl.h"

#include "webrtc/engine_configurations.h"
#include "webrtc/system_wrappers/interface/file_wrapper.h"
#include "webrtc/system_wrappers/interface/logging.h"
#include "webrtc/video_engine/include/vie_errors.h"
#include "webrtc/video_engine/vie_channel.h"
#include "webrtc/video_engine/vie_channel_manager.h"
#include "webrtc/video_engine/vie_defines.h"
#include "webrtc/video_engine/vie_encoder.h"
#include "webrtc/video_engine/vie_impl.h"
#include "webrtc/video_engine/vie_shared_data.h"

namespace webrtc {

// Helper methods for converting between module format and ViE API format.

static RTCPMethod ViERTCPModeToRTCPMethod(ViERTCPMode api_mode) {
  switch (api_mode) {
    case kRtcpNone:
      return kRtcpOff;

    case kRtcpCompound_RFC4585:
      return kRtcpCompound;

    case kRtcpNonCompound_RFC5506:
      return kRtcpNonCompound;
  }
  assert(false);
  return kRtcpOff;
}

static ViERTCPMode RTCPMethodToViERTCPMode(RTCPMethod module_method) {
  switch (module_method) {
    case kRtcpOff:
      return kRtcpNone;

    case kRtcpCompound:
      return kRtcpCompound_RFC4585;

    case kRtcpNonCompound:
      return kRtcpNonCompound_RFC5506;
  }
  assert(false);
  return kRtcpNone;
}

static KeyFrameRequestMethod APIRequestToModuleRequest(
  ViEKeyFrameRequestMethod api_method) {
  switch (api_method) {
    case kViEKeyFrameRequestNone:
      return kKeyFrameReqFirRtp;

    case kViEKeyFrameRequestPliRtcp:
      return kKeyFrameReqPliRtcp;

    case kViEKeyFrameRequestFirRtp:
      return kKeyFrameReqFirRtp;

    case kViEKeyFrameRequestFirRtcp:
      return kKeyFrameReqFirRtcp;
  }
  assert(false);
  return kKeyFrameReqFirRtp;
}

ViERTP_RTCP* ViERTP_RTCP::GetInterface(VideoEngine* video_engine) {
#ifdef WEBRTC_VIDEO_ENGINE_RTP_RTCP_API
  if (!video_engine) {
    return NULL;
  }
  VideoEngineImpl* vie_impl = static_cast<VideoEngineImpl*>(video_engine);
  ViERTP_RTCPImpl* vie_rtpimpl = vie_impl;
  // Increase ref count.
  (*vie_rtpimpl)++;
  return vie_rtpimpl;
#else
  return NULL;
#endif
}

int ViERTP_RTCPImpl::Release() {
  // Decrease ref count.
  (*this)--;

  int32_t ref_count = GetCount();
  if (ref_count < 0) {
    LOG(LS_ERROR) << "ViERTP_RTCP released too many times.";
    shared_data_->SetLastError(kViEAPIDoesNotExist);
    return -1;
  }
  return ref_count;
}

ViERTP_RTCPImpl::ViERTP_RTCPImpl(ViESharedData* shared_data)
    : shared_data_(shared_data) {}

ViERTP_RTCPImpl::~ViERTP_RTCPImpl() {}

int ViERTP_RTCPImpl::SetLocalSSRC(const int video_channel,
                                  const unsigned int SSRC,
                                  const StreamType usage,
                                  const unsigned char simulcast_idx) {
  LOG_F(LS_INFO) << "channel: " << video_channel << " ssrc: " << SSRC << "";
  ViEChannelManagerScoped cs(*(shared_data_->channel_manager()));
  ViEChannel* vie_channel = cs.Channel(video_channel);
  if (!vie_channel) {
    shared_data_->SetLastError(kViERtpRtcpInvalidChannelId);
    return -1;
  }
  if (vie_channel->SetSSRC(SSRC, usage, simulcast_idx) != 0) {
    shared_data_->SetLastError(kViERtpRtcpUnknownError);
    return -1;
  }
  return 0;
}

int ViERTP_RTCPImpl::SetRemoteSSRCType(const int videoChannel,
                                       const StreamType usage,
                                       const unsigned int SSRC) const {
  LOG_F(LS_INFO) << "channel: " << videoChannel
                 << " usage: " << static_cast<int>(usage) << " ssrc: " << SSRC;

  // Get the channel
  ViEChannelManagerScoped cs(*(shared_data_->channel_manager()));
  ViEChannel* ptrViEChannel = cs.Channel(videoChannel);
  if (ptrViEChannel == NULL) {
    shared_data_->SetLastError(kViERtpRtcpInvalidChannelId);
    return -1;
  }
  if (ptrViEChannel->SetRemoteSSRCType(usage, SSRC) != 0) {
    shared_data_->SetLastError(kViERtpRtcpUnknownError);
    return -1;
  }
  return 0;
}

int ViERTP_RTCPImpl::GetLocalSSRC(const int video_channel,
                                  unsigned int& SSRC) const {
  ViEChannelManagerScoped cs(*(shared_data_->channel_manager()));
  ViEChannel* vie_channel = cs.Channel(video_channel);
  if (!vie_channel) {
    shared_data_->SetLastError(kViERtpRtcpInvalidChannelId);
    return -1;
  }
  uint8_t idx = 0;
  if (vie_channel->GetLocalSSRC(idx, &SSRC) != 0) {
    shared_data_->SetLastError(kViERtpRtcpUnknownError);
    return -1;
  }
  return 0;
}

int ViERTP_RTCPImpl::GetRemoteSSRC(const int video_channel,
                                   unsigned int& SSRC) const {
  ViEChannelManagerScoped cs(*(shared_data_->channel_manager()));
  ViEChannel* vie_channel = cs.Channel(video_channel);
  if (!vie_channel) {
    shared_data_->SetLastError(kViERtpRtcpInvalidChannelId);
    return -1;
  }
  if (vie_channel->GetRemoteSSRC(&SSRC) != 0) {
    shared_data_->SetLastError(kViERtpRtcpUnknownError);
    return -1;
  }
  return 0;
}

int ViERTP_RTCPImpl::GetRemoteCSRCs(const int video_channel,
                                    unsigned int CSRCs[kRtpCsrcSize]) const {
  ViEChannelManagerScoped cs(*(shared_data_->channel_manager()));
  ViEChannel* vie_channel = cs.Channel(video_channel);
  if (!vie_channel) {
    shared_data_->SetLastError(kViERtpRtcpInvalidChannelId);
    return -1;
  }
  if (vie_channel->GetRemoteCSRC(CSRCs) != 0) {
    shared_data_->SetLastError(kViERtpRtcpUnknownError);
    return -1;
  }
  return 0;
}

int ViERTP_RTCPImpl::GetRemoteRID(const int video_channel,
                                  char rid[256]) const {
  ViEChannelManagerScoped cs(*(shared_data_->channel_manager()));
  ViEChannel* vie_channel = cs.Channel(video_channel);
  if (!vie_channel) {
    shared_data_->SetLastError(kViERtpRtcpInvalidChannelId);
    return -1;
  }
  if (vie_channel->GetRemoteRID(rid) != 0) {
    shared_data_->SetLastError(kViERtpRtcpUnknownError);
    return -1;
  }
  return 0;
}

int ViERTP_RTCPImpl::SetRtxSendPayloadType(const int video_channel,
                                           const uint8_t payload_type) {
  LOG_F(LS_INFO) << "channel: " << video_channel
                 << " payload_type: " << static_cast<int>(payload_type);
  ViEChannelManagerScoped cs(*(shared_data_->channel_manager()));
  ViEChannel* vie_channel = cs.Channel(video_channel);
  if (!vie_channel) {
    shared_data_->SetLastError(kViERtpRtcpInvalidChannelId);
    return -1;
  }
  if (vie_channel->SetRtxSendPayloadType(payload_type) != 0) {
    return -1;
  }
  return 0;
}

int ViERTP_RTCPImpl::SetRtxReceivePayloadType(const int video_channel,
                                              const uint8_t payload_type) {
  LOG_F(LS_INFO) << "channel: " << video_channel
                 << " payload_type: " << static_cast<int>(payload_type);
  ViEChannelManagerScoped cs(*(shared_data_->channel_manager()));
  ViEChannel* vie_channel = cs.Channel(video_channel);
  if (!vie_channel) {
    shared_data_->SetLastError(kViERtpRtcpInvalidChannelId);
    return -1;
  }
  vie_channel->SetRtxReceivePayloadType(payload_type);
  return 0;
}

int ViERTP_RTCPImpl::SetStartSequenceNumber(const int video_channel,
                                            uint16_t sequence_number) {
  ViEChannelManagerScoped cs(*(shared_data_->channel_manager()));
  ViEChannel* vie_channel = cs.Channel(video_channel);
  if (!vie_channel) {
    shared_data_->SetLastError(kViERtpRtcpInvalidChannelId);
    return -1;
  }
  if (vie_channel->Sending()) {
    LOG_F(LS_ERROR) << "channel " << video_channel << " is already sending.";
    shared_data_->SetLastError(kViERtpRtcpAlreadySending);
    return -1;
  }
  if (vie_channel->SetStartSequenceNumber(sequence_number) != 0) {
    shared_data_->SetLastError(kViERtpRtcpUnknownError);
    return -1;
  }
  return 0;
}

void ViERTP_RTCPImpl::SetRtpStateForSsrc(int video_channel,
                                         uint32_t ssrc,
                                         const RtpState& rtp_state) {
  ViEChannelManagerScoped cs(*(shared_data_->channel_manager()));
  ViEChannel* vie_channel = cs.Channel(video_channel);
  if (!vie_channel)
    return;

  if (vie_channel->Sending()) {
    LOG_F(LS_ERROR) << "channel " << video_channel << " is already sending.";
    return;
  }
  vie_channel->SetRtpStateForSsrc(ssrc, rtp_state);
}

RtpState ViERTP_RTCPImpl::GetRtpStateForSsrc(int video_channel, uint32_t ssrc) {
  ViEChannelManagerScoped cs(*(shared_data_->channel_manager()));
  ViEChannel* vie_channel = cs.Channel(video_channel);
  if (!vie_channel)
    return RtpState();

  return vie_channel->GetRtpStateForSsrc(ssrc);
}

int ViERTP_RTCPImpl::SetRTCPStatus(const int video_channel,
                                   const ViERTCPMode rtcp_mode) {
  LOG_F(LS_INFO) << "channel: " << video_channel
                 << " mode: " << static_cast<int>(rtcp_mode);
  ViEChannelManagerScoped cs(*(shared_data_->channel_manager()));
  ViEChannel* vie_channel = cs.Channel(video_channel);
  if (!vie_channel) {
    shared_data_->SetLastError(kViERtpRtcpInvalidChannelId);
    return -1;
  }

  RTCPMethod module_mode = ViERTCPModeToRTCPMethod(rtcp_mode);
  vie_channel->SetRTCPMode(module_mode);
  return 0;
}

int ViERTP_RTCPImpl::GetRTCPStatus(const int video_channel,
                                   ViERTCPMode& rtcp_mode) const {
  ViEChannelManagerScoped cs(*(shared_data_->channel_manager()));
  ViEChannel* vie_channel = cs.Channel(video_channel);
  if (!vie_channel) {
    shared_data_->SetLastError(kViERtpRtcpInvalidChannelId);
    return -1;
  }
  RTCPMethod module_mode = vie_channel->GetRTCPMode();
  rtcp_mode = RTCPMethodToViERTCPMode(module_mode);
  return 0;
}

int ViERTP_RTCPImpl::SetRTCPCName(const int video_channel,
                                  const char rtcp_cname[KMaxRTCPCNameLength]) {
  LOG_F(LS_INFO) << "channel: " << video_channel
                 << " rtcp_cname: " << rtcp_cname;
  ViEChannelManagerScoped cs(*(shared_data_->channel_manager()));
  ViEChannel* vie_channel = cs.Channel(video_channel);
  if (!vie_channel) {
    shared_data_->SetLastError(kViERtpRtcpInvalidChannelId);
    return -1;
  }
  if (vie_channel->Sending()) {
    LOG_F(LS_ERROR) << "channel " << video_channel << " is already sending.";
    shared_data_->SetLastError(kViERtpRtcpAlreadySending);
    return -1;
  }
  if (vie_channel->SetRTCPCName(rtcp_cname) != 0) {
    shared_data_->SetLastError(kViERtpRtcpUnknownError);
    return -1;
  }
  return 0;
}

int ViERTP_RTCPImpl::GetRemoteRTCPCName(
    const int video_channel,
    char rtcp_cname[KMaxRTCPCNameLength]) const {
  ViEChannelManagerScoped cs(*(shared_data_->channel_manager()));
  ViEChannel* vie_channel = cs.Channel(video_channel);
  if (!vie_channel) {
    shared_data_->SetLastError(kViERtpRtcpInvalidChannelId);
    return -1;
  }
  if (vie_channel->GetRemoteRTCPCName(rtcp_cname) != 0) {
    shared_data_->SetLastError(kViERtpRtcpUnknownError);
    return -1;
  }
  return 0;
}

int ViERTP_RTCPImpl::GetRemoteRTCPReceiverInfo(const int video_channel,
                                               uint32_t& NTPHigh,
                                               uint32_t& NTPLow,
                                               uint32_t& receivedPacketCount,
                                               uint64_t& receivedOctetCount,
                                               uint32_t* jitter,
                                               uint16_t* fractionLost,
                                               uint32_t* cumulativeLost,
                                               int32_t* rttMs) const {
  LOG_F(LS_INFO) << "channel:" << video_channel;
  ViEChannelManagerScoped cs(*(shared_data_->channel_manager()));
  ViEChannel* vie_channel = cs.Channel(video_channel);
  if (!vie_channel) {
    LOG(LS_ERROR) << "Channel " << video_channel << " doesn't exist";
    shared_data_->SetLastError(kViERtpRtcpInvalidChannelId);
    return -1;
  }
  if (vie_channel->GetRemoteRTCPReceiverInfo(NTPHigh,
                                             NTPLow,
                                             receivedPacketCount,
                                             receivedOctetCount,
                                             jitter,
                                             fractionLost,
                                             cumulativeLost,
                                             rttMs) != 0) {
    shared_data_->SetLastError(kViERtpRtcpUnknownError);
    return -1;
  }
  return 0;
}

int ViERTP_RTCPImpl::SendApplicationDefinedRTCPPacket(
  const int video_channel,
  const unsigned char sub_type,
  unsigned int name,
  const char* data,
  uint16_t data_length_in_bytes) {
  ViEChannelManagerScoped cs(*(shared_data_->channel_manager()));
  ViEChannel* vie_channel = cs.Channel(video_channel);
  if (!vie_channel) {
    shared_data_->SetLastError(kViERtpRtcpInvalidChannelId);
    return -1;
  }
  if (!vie_channel->Sending()) {
    shared_data_->SetLastError(kViERtpRtcpNotSending);
    return -1;
  }
  RTCPMethod method = vie_channel->GetRTCPMode();
  if (method == kRtcpOff) {
    shared_data_->SetLastError(kViERtpRtcpRtcpDisabled);
    return -1;
  }
  if (vie_channel->SendApplicationDefinedRTCPPacket(
        sub_type, name, reinterpret_cast<const uint8_t*>(data),
        data_length_in_bytes) != 0) {
    shared_data_->SetLastError(kViERtpRtcpUnknownError);
    return -1;
  }
  return 0;
}

int ViERTP_RTCPImpl::SetNACKStatus(const int video_channel, const bool enable) {
  LOG_F(LS_INFO) << "channel: " << video_channel << " "
                 << (enable ? "on" : "off");
  ViEChannelManagerScoped cs(*(shared_data_->channel_manager()));
  ViEChannel* vie_channel = cs.Channel(video_channel);
  if (!vie_channel) {
    shared_data_->SetLastError(kViERtpRtcpInvalidChannelId);
    return -1;
  }
  if (vie_channel->SetNACKStatus(enable) != 0) {
    shared_data_->SetLastError(kViERtpRtcpUnknownError);
    return -1;
  }

  // Update the encoder
  ViEEncoder* vie_encoder = cs.Encoder(video_channel);
  if (!vie_encoder) {
    shared_data_->SetLastError(kViERtpRtcpUnknownError);
    return -1;
  }
  vie_encoder->UpdateProtectionMethod(enable,
                                      vie_channel->IsSendingFecEnabled());
  return 0;
}

int ViERTP_RTCPImpl::SetFECStatus(const int video_channel, const bool enable,
                                  const unsigned char payload_typeRED,
                                  const unsigned char payload_typeFEC) {
  LOG_F(LS_INFO) << "channel: " << video_channel
                 << " enable: " << (enable ? "on" : "off")
                 << " payload_typeRED: " << static_cast<int>(payload_typeRED)
                 << " payload_typeFEC: " << static_cast<int>(payload_typeFEC);
  ViEChannelManagerScoped cs(*(shared_data_->channel_manager()));
  ViEChannel* vie_channel = cs.Channel(video_channel);
  if (!vie_channel) {
    shared_data_->SetLastError(kViERtpRtcpInvalidChannelId);
    return -1;
  }
  if (vie_channel->SetFECStatus(enable, payload_typeRED,
                                payload_typeFEC) != 0) {
    shared_data_->SetLastError(kViERtpRtcpUnknownError);
    return -1;
  }
  // Update the encoder.
  ViEEncoder* vie_encoder = cs.Encoder(video_channel);
  if (!vie_encoder) {
    shared_data_->SetLastError(kViERtpRtcpUnknownError);
    return -1;
  }
  vie_encoder->UpdateProtectionMethod(false, true);
  return 0;
}

int ViERTP_RTCPImpl::SetHybridNACKFECStatus(
    const int video_channel,
    const bool enable,
    const unsigned char payload_typeRED,
    const unsigned char payload_typeFEC) {
  LOG_F(LS_INFO) << "channel: " << video_channel
                 << " enable: " << (enable ? "on" : "off")
                 << " payload_typeRED: " << static_cast<int>(payload_typeRED)
                 << " payload_typeFEC: " << static_cast<int>(payload_typeFEC);
  ViEChannelManagerScoped cs(*(shared_data_->channel_manager()));
  ViEChannel* vie_channel = cs.Channel(video_channel);
  if (!vie_channel) {
    shared_data_->SetLastError(kViERtpRtcpInvalidChannelId);
    return -1;
  }

  // Update the channel status with hybrid NACK FEC mode.
  if (vie_channel->SetHybridNACKFECStatus(enable, payload_typeRED,
                                          payload_typeFEC) != 0) {
    shared_data_->SetLastError(kViERtpRtcpUnknownError);
    return -1;
  }

  // Update the encoder.
  ViEEncoder* vie_encoder = cs.Encoder(video_channel);
  if (!vie_encoder) {
    shared_data_->SetLastError(kViERtpRtcpUnknownError);
    return -1;
  }
  vie_encoder->UpdateProtectionMethod(enable, enable);
  return 0;
}

int ViERTP_RTCPImpl::SetSenderBufferingMode(int video_channel,
                                            int target_delay_ms) {
  LOG_F(LS_INFO) << "channel: " << video_channel
                 << " target_delay_ms: " << target_delay_ms;
  ViEChannelManagerScoped cs(*(shared_data_->channel_manager()));
  ViEChannel* vie_channel = cs.Channel(video_channel);
  if (!vie_channel) {
    shared_data_->SetLastError(kViERtpRtcpInvalidChannelId);
    return -1;
  }
  ViEEncoder* vie_encoder = cs.Encoder(video_channel);
  if (!vie_encoder) {
    shared_data_->SetLastError(kViERtpRtcpInvalidChannelId);
    return -1;
  }

  // Update the channel with buffering mode settings.
  if (vie_channel->SetSenderBufferingMode(target_delay_ms) != 0) {
    shared_data_->SetLastError(kViERtpRtcpUnknownError);
    return -1;
  }

  // Update the encoder's buffering mode settings.
  vie_encoder->SetSenderBufferingMode(target_delay_ms);
  return 0;
}

int ViERTP_RTCPImpl::SetReceiverBufferingMode(int video_channel,
                                              int target_delay_ms) {
  LOG_F(LS_INFO) << "channel: " << video_channel
                 << " target_delay_ms: " << target_delay_ms;
  ViEChannelManagerScoped cs(*(shared_data_->channel_manager()));
  ViEChannel* vie_channel = cs.Channel(video_channel);
  if (!vie_channel) {
    shared_data_->SetLastError(kViERtpRtcpInvalidChannelId);
    return -1;
  }

  // Update the channel with buffering mode settings.
  if (vie_channel->SetReceiverBufferingMode(target_delay_ms) != 0) {
    shared_data_->SetLastError(kViERtpRtcpUnknownError);
    return -1;
  }
  return 0;
}

int ViERTP_RTCPImpl::SetKeyFrameRequestMethod(
  const int video_channel,
  const ViEKeyFrameRequestMethod method) {
  LOG_F(LS_INFO) << "channel: " << video_channel
                 << " method: " << static_cast<int>(method);

  ViEChannelManagerScoped cs(*(shared_data_->channel_manager()));
  ViEChannel* vie_channel = cs.Channel(video_channel);
  if (!vie_channel) {
    shared_data_->SetLastError(kViERtpRtcpInvalidChannelId);
    return -1;
  }
  KeyFrameRequestMethod module_method = APIRequestToModuleRequest(method);
  if (vie_channel->SetKeyFrameRequestMethod(module_method) != 0) {
    shared_data_->SetLastError(kViERtpRtcpUnknownError);
    return -1;
  }
  return 0;
}

int ViERTP_RTCPImpl::SetTMMBRStatus(const int video_channel,
                                    const bool enable) {
  LOG_F(LS_INFO) << "channel: " << video_channel
                 << "enable: " << (enable ? "on" : "off");
  ViEChannelManagerScoped cs(*(shared_data_->channel_manager()));
  ViEChannel* vie_channel = cs.Channel(video_channel);
  if (!vie_channel) {
    shared_data_->SetLastError(kViERtpRtcpInvalidChannelId);
    return -1;
  }
  vie_channel->EnableTMMBR(enable);
  return 0;
}

int ViERTP_RTCPImpl::SetRembStatus(int video_channel,
                                   bool sender,
                                   bool receiver) {
  LOG_F(LS_INFO) << "channel: " << video_channel
                 << " sender: " << (sender ? "on" : "off")
                 << " receiver: " << (receiver ? "on" : "off");
  if (!shared_data_->channel_manager()->SetRembStatus(video_channel, sender,
                                                      receiver)) {
    return -1;
  }
  return 0;
}

int ViERTP_RTCPImpl::SetSendTimestampOffsetStatus(int video_channel,
                                                  bool enable,
                                                  int id) {
  LOG_F(LS_INFO) << "channel: " << video_channel
                 << "enable: " << (enable ? "on" : "off") << " id: " << id;

  ViEChannelManagerScoped cs(*(shared_data_->channel_manager()));
  ViEChannel* vie_channel = cs.Channel(video_channel);
  if (!vie_channel) {
    shared_data_->SetLastError(kViERtpRtcpInvalidChannelId);
    return -1;
  }
  if (vie_channel->SetSendTimestampOffsetStatus(enable, id) != 0) {
    shared_data_->SetLastError(kViERtpRtcpUnknownError);
    return -1;
  }
  return 0;
}

int ViERTP_RTCPImpl::SetReceiveTimestampOffsetStatus(int video_channel,
                                                     bool enable,
                                                     int id) {
  LOG_F(LS_INFO) << "channel: " << video_channel
                 << " enable: " << (enable ? "on" : "off") << " id: " << id;
  ViEChannelManagerScoped cs(*(shared_data_->channel_manager()));
  ViEChannel* vie_channel = cs.Channel(video_channel);
  if (!vie_channel) {
    shared_data_->SetLastError(kViERtpRtcpInvalidChannelId);
    return -1;
  }
  if (vie_channel->SetReceiveTimestampOffsetStatus(enable, id) != 0) {
    shared_data_->SetLastError(kViERtpRtcpUnknownError);
    return -1;
  }
  return 0;
}

int ViERTP_RTCPImpl::SetSendAbsoluteSendTimeStatus(int video_channel,
                                                   bool enable,
                                                   int id) {
  LOG_F(LS_INFO) << "channel: " << video_channel
                 << " enable: " << (enable ? "on" : "off") << " id: " << id;

  ViEChannelManagerScoped cs(*(shared_data_->channel_manager()));
  ViEChannel* vie_channel = cs.Channel(video_channel);
  if (!vie_channel) {
    shared_data_->SetLastError(kViERtpRtcpInvalidChannelId);
    return -1;
  }
  if (vie_channel->SetSendAbsoluteSendTimeStatus(enable, id) != 0) {
    shared_data_->SetLastError(kViERtpRtcpUnknownError);
    return -1;
  }
  return 0;
}

int ViERTP_RTCPImpl::SetReceiveAbsoluteSendTimeStatus(int video_channel,
                                                      bool enable,
                                                      int id) {
  LOG_F(LS_INFO) << "channel: " << video_channel
                 << " enable: " << (enable ? "on" : "off") << " id: " << id;
  ViEChannelManagerScoped cs(*(shared_data_->channel_manager()));
  ViEChannel* vie_channel = cs.Channel(video_channel);
  if (!vie_channel) {
    shared_data_->SetLastError(kViERtpRtcpInvalidChannelId);
    return -1;
  }
  if (vie_channel->SetReceiveAbsoluteSendTimeStatus(enable, id) != 0) {
    shared_data_->SetLastError(kViERtpRtcpUnknownError);
    return -1;
  }
  return 0;
}

int ViERTP_RTCPImpl::SetSendVideoRotationStatus(int video_channel,
                                                bool enable,
                                                int id) {
  LOG_F(LS_INFO) << "channel: " << video_channel
                 << " enable: " << (enable ? "on" : "off") << " id: " << id;

  ViEChannelManagerScoped cs(*(shared_data_->channel_manager()));
  ViEChannel* vie_channel = cs.Channel(video_channel);
  if (!vie_channel) {
    shared_data_->SetLastError(kViERtpRtcpInvalidChannelId);
    return -1;
  }
  if (vie_channel->SetSendVideoRotationStatus(enable, id) != 0) {
    shared_data_->SetLastError(kViERtpRtcpUnknownError);
    return -1;
  }
  return 0;
}

int ViERTP_RTCPImpl::SetReceiveVideoRotationStatus(int video_channel,
                                                   bool enable,
                                                   int id) {
  LOG_F(LS_INFO) << "channel: " << video_channel
                 << " enable: " << (enable ? "on" : "off") << " id: " << id;
  ViEChannelManagerScoped cs(*(shared_data_->channel_manager()));
  ViEChannel* vie_channel = cs.Channel(video_channel);
  if (!vie_channel) {
    shared_data_->SetLastError(kViERtpRtcpInvalidChannelId);
    return -1;
  }
  if (vie_channel->SetReceiveVideoRotationStatus(enable, id) != 0) {
    shared_data_->SetLastError(kViERtpRtcpUnknownError);
    return -1;
  }
  return 0;
}

int ViERTP_RTCPImpl::SetSendRIDStatus(int video_channel,
                                      bool enable,
                                      int id,
                                      const char *rid) {
  LOG_F(LS_INFO) << "channel: " << video_channel
                 << " enable: " << (enable ? "on" : "off") << " id: " << id << " RID: " << rid;

  ViEChannelManagerScoped cs(*(shared_data_->channel_manager()));
  ViEChannel* vie_channel = cs.Channel(video_channel);
  if (!vie_channel) {
    shared_data_->SetLastError(kViERtpRtcpInvalidChannelId);
    return -1;
  }
  if (vie_channel->SetSendRIDStatus(enable, id, rid) != 0) {
    shared_data_->SetLastError(kViERtpRtcpUnknownError);
    return -1;
  }
  return 0;
}

int ViERTP_RTCPImpl::SetReceiveRIDStatus(int video_channel,
                                         bool enable,
                                         int id) {
  LOG_F(LS_INFO) << "channel: " << video_channel
                 << " enable: " << (enable ? "on" : "off") << " id: " << id;
  ViEChannelManagerScoped cs(*(shared_data_->channel_manager()));
  ViEChannel* vie_channel = cs.Channel(video_channel);
  if (!vie_channel) {
    shared_data_->SetLastError(kViERtpRtcpInvalidChannelId);
    return -1;
  }
  if (vie_channel->SetReceiveRIDStatus(enable, id) != 0) {
    shared_data_->SetLastError(kViERtpRtcpUnknownError);
    return -1;
  }
  return 0;
}

int ViERTP_RTCPImpl::SetRtcpXrRrtrStatus(int video_channel, bool enable) {
  LOG_F(LS_INFO) << "channel: " << video_channel
                 << " enable: " << (enable ? "on" : "off");

  ViEChannelManagerScoped cs(*(shared_data_->channel_manager()));
  ViEChannel* vie_channel = cs.Channel(video_channel);
  if (!vie_channel) {
    shared_data_->SetLastError(kViERtpRtcpInvalidChannelId);
    return -1;
  }
  vie_channel->SetRtcpXrRrtrStatus(enable);
  return 0;
}

int ViERTP_RTCPImpl::SetTransmissionSmoothingStatus(int video_channel,
                                                    bool enable) {
  LOG_F(LS_INFO) << "channel: " << video_channel
                 << " enable: " << (enable ? "on" : "off");
  ViEChannelManagerScoped cs(*(shared_data_->channel_manager()));
  ViEChannel* vie_channel = cs.Channel(video_channel);
  if (!vie_channel) {
    shared_data_->SetLastError(kViERtpRtcpInvalidChannelId);
    return -1;
  }
  vie_channel->SetTransmissionSmoothingStatus(enable);
  return 0;
}

int ViERTP_RTCPImpl::SetMinTransmitBitrate(int video_channel,
                                           int min_transmit_bitrate_kbps) {
  LOG_F(LS_INFO) << "channel: " << video_channel
                 << " min_transmit_bitrate_kbps: " << min_transmit_bitrate_kbps;
  ViEChannelManagerScoped cs(*(shared_data_->channel_manager()));
  ViEEncoder* vie_encoder = cs.Encoder(video_channel);
  if (vie_encoder == NULL)
    return -1;
  vie_encoder->SetMinTransmitBitrate(min_transmit_bitrate_kbps);
  return 0;
}

int ViERTP_RTCPImpl::SetReservedTransmitBitrate(
    int video_channel, unsigned int reserved_transmit_bitrate_bps) {
  LOG_F(LS_INFO) << "channel: " << video_channel
                 << " reserved_transmit_bitrate_bps: "
                 << reserved_transmit_bitrate_bps;
  if (!shared_data_->channel_manager()->SetReservedTransmitBitrate(
      video_channel, reserved_transmit_bitrate_bps)) {
    return -1;
  }
  return 0;
}

int ViERTP_RTCPImpl::GetReceiveChannelRtcpStatistics(
    const int video_channel,
    RtcpStatistics& basic_stats,
    int64_t& rtt_ms) const {
  ViEChannelManagerScoped cs(*(shared_data_->channel_manager()));
  ViEChannel* vie_channel = cs.Channel(video_channel);
  if (!vie_channel) {
    shared_data_->SetLastError(kViERtpRtcpInvalidChannelId);
    return -1;
  }

  // TODO(sprang): Clean this up when stats struct is propagated all the way.
  uint16_t frac_lost;
  if (vie_channel->GetReceivedRtcpStatistics(
          &frac_lost,
          &basic_stats.cumulative_lost,
          &basic_stats.extended_max_sequence_number,
          &basic_stats.jitter,
          &rtt_ms) != 0) {
    shared_data_->SetLastError(kViERtpRtcpUnknownError);
    return -1;
  }
  basic_stats.fraction_lost = frac_lost;
  return 0;
}

int ViERTP_RTCPImpl::GetSendChannelRtcpStatistics(const int video_channel,
                                                  RtcpStatistics& basic_stats,
                                                  int64_t& rtt_ms) const {
  ViEChannelManagerScoped cs(*(shared_data_->channel_manager()));
  ViEChannel* vie_channel = cs.Channel(video_channel);
  if (!vie_channel) {
    shared_data_->SetLastError(kViERtpRtcpInvalidChannelId);
    return -1;
  }

  // TODO(sprang): Clean this up when stats struct is propagated all the way.
  uint16_t frac_lost;
  if (vie_channel->GetSendRtcpStatistics(
          &frac_lost,
          &basic_stats.cumulative_lost,
          &basic_stats.extended_max_sequence_number,
          &basic_stats.jitter,
          &rtt_ms) != 0) {
    shared_data_->SetLastError(kViERtpRtcpUnknownError);
    return -1;
  }
  basic_stats.fraction_lost = frac_lost;
  return 0;
}

int ViERTP_RTCPImpl::GetRtpStatistics(const int video_channel,
                                      StreamDataCounters& sent,
                                      StreamDataCounters& received) const {
  ViEChannelManagerScoped cs(*(shared_data_->channel_manager()));
  ViEChannel* vie_channel = cs.Channel(video_channel);
  if (!vie_channel) {
    shared_data_->SetLastError(kViERtpRtcpInvalidChannelId);
    return -1;
  }
  if (vie_channel->GetRtpStatistics(&sent.transmitted.payload_bytes,
                                    &sent.transmitted.packets,
                                    &received.transmitted.payload_bytes,
                                    &received.transmitted.packets) != 0) {
    shared_data_->SetLastError(kViERtpRtcpUnknownError);
    return -1;
  }
  return 0;
}

int ViERTP_RTCPImpl::GetSendRtcpPacketTypeCounter(
    int video_channel,
    RtcpPacketTypeCounter* packet_counter) const {
  ViEChannelManagerScoped cs(*(shared_data_->channel_manager()));
  ViEChannel* vie_channel = cs.Channel(video_channel);
  if (!vie_channel) {
    shared_data_->SetLastError(kViERtpRtcpInvalidChannelId);
    return -1;
  }
  vie_channel->GetSendRtcpPacketTypeCounter(packet_counter);
  return 0;
}

int ViERTP_RTCPImpl::GetReceiveRtcpPacketTypeCounter(
    int video_channel,
    RtcpPacketTypeCounter* packet_counter) const {
  ViEChannelManagerScoped cs(*(shared_data_->channel_manager()));
  ViEChannel* vie_channel = cs.Channel(video_channel);
  if (!vie_channel) {
    shared_data_->SetLastError(kViERtpRtcpInvalidChannelId);
    return -1;
  }
  vie_channel->GetReceiveRtcpPacketTypeCounter(packet_counter);
  return 0;
}

int ViERTP_RTCPImpl::GetRemoteRTCPSenderInfo(const int video_channel,
                                             SenderInfo* sender_info) const {
  LOG_F(LS_INFO) << "channel:" << video_channel;
  ViEChannelManagerScoped cs(*(shared_data_->channel_manager()));
  ViEChannel* vie_channel = cs.Channel(video_channel);
  if (!vie_channel) {
    LOG(LS_ERROR) << "Channel " << video_channel << " doesn't exist";
    shared_data_->SetLastError(kViERtpRtcpInvalidChannelId);
    return -1;
  }
  if (vie_channel->GetRemoteRTCPSenderInfo(sender_info) != 0) {
    shared_data_->SetLastError(kViERtpRtcpUnknownError);
    return -1;
  }
  return 0;
}

int ViERTP_RTCPImpl::GetBandwidthUsage(const int video_channel,
                                       unsigned int& total_bitrate_sent,
                                       unsigned int& video_bitrate_sent,
                                       unsigned int& fec_bitrate_sent,
                                       unsigned int& nackBitrateSent) const {
  ViEChannelManagerScoped cs(*(shared_data_->channel_manager()));
  ViEChannel* vie_channel = cs.Channel(video_channel);
  if (!vie_channel) {
    shared_data_->SetLastError(kViERtpRtcpInvalidChannelId);
    return -1;
  }
  vie_channel->GetBandwidthUsage(&total_bitrate_sent,
                                 &video_bitrate_sent,
                                 &fec_bitrate_sent,
                                 &nackBitrateSent);
  return 0;
}

int ViERTP_RTCPImpl::GetEstimatedSendBandwidth(
    const int video_channel,
    unsigned int* estimated_bandwidth) const {
  if (!shared_data_->channel_manager()->GetEstimatedSendBandwidth(
      video_channel, estimated_bandwidth)) {
    return -1;
  }
  return 0;
}

int ViERTP_RTCPImpl::GetEstimatedReceiveBandwidth(
    const int video_channel,
    unsigned int* estimated_bandwidth) const {
  if (!shared_data_->channel_manager()->GetEstimatedReceiveBandwidth(
      video_channel, estimated_bandwidth)) {
    return -1;
  }
  return 0;
}

int ViERTP_RTCPImpl::GetPacerQueuingDelayMs(
    const int video_channel, int64_t* delay_ms) const {
  if (!shared_data_->channel_manager()->GetPacerQueuingDelayMs(video_channel,
                                                               delay_ms)) {
    return -1;
  }
  return 0;
}

int ViERTP_RTCPImpl::StartRTPDump(const int video_channel,
                                  const char file_nameUTF8[1024],
                                  RTPDirections direction) {
  LOG_F(LS_INFO) << "channel: " << video_channel
                 << " filename: " << file_nameUTF8
                 << " direction: " << static_cast<int>(direction);
  assert(FileWrapper::kMaxFileNameSize == 1024);
  ViEChannelManagerScoped cs(*(shared_data_->channel_manager()));
  ViEChannel* vie_channel = cs.Channel(video_channel);
  if (!vie_channel) {
    shared_data_->SetLastError(kViERtpRtcpInvalidChannelId);
    return -1;
  }
  if (vie_channel->StartRTPDump(file_nameUTF8, direction) != 0) {
    shared_data_->SetLastError(kViERtpRtcpUnknownError);
    return -1;
  }
  return 0;
}

int ViERTP_RTCPImpl::StopRTPDump(const int video_channel,
                                 RTPDirections direction) {
  LOG_F(LS_INFO) << "channel: " << video_channel
                 << " direction: " << static_cast<int>(direction);
  ViEChannelManagerScoped cs(*(shared_data_->channel_manager()));
  ViEChannel* vie_channel = cs.Channel(video_channel);
  if (!vie_channel) {
    shared_data_->SetLastError(kViERtpRtcpInvalidChannelId);
    return -1;
  }
  if (vie_channel->StopRTPDump(direction) != 0) {
    shared_data_->SetLastError(kViERtpRtcpUnknownError);
    return -1;
  }
  return 0;
}

int ViERTP_RTCPImpl::RegisterRTPObserver(const int video_channel,
                                         ViERTPObserver& observer) {
  LOG_F(LS_INFO) << "channel " << video_channel;
  ViEChannelManagerScoped cs(*(shared_data_->channel_manager()));
  ViEChannel* vie_channel = cs.Channel(video_channel);
  if (!vie_channel) {
    shared_data_->SetLastError(kViERtpRtcpInvalidChannelId);
    return -1;
  }
  if (vie_channel->RegisterRtpObserver(&observer) != 0) {
    shared_data_->SetLastError(kViERtpRtcpObserverAlreadyRegistered);
    return -1;
  }
  return 0;
}

int ViERTP_RTCPImpl::DeregisterRTPObserver(const int video_channel) {
  LOG_F(LS_INFO) << "channel " << video_channel;
  ViEChannelManagerScoped cs(*(shared_data_->channel_manager()));
  ViEChannel* vie_channel = cs.Channel(video_channel);
  if (!vie_channel) {
    shared_data_->SetLastError(kViERtpRtcpInvalidChannelId);
    return -1;
  }
  if (vie_channel->RegisterRtpObserver(NULL) != 0) {
    shared_data_->SetLastError(kViERtpRtcpObserverNotRegistered);
    return -1;
  }
  return 0;
}

int ViERTP_RTCPImpl::RegisterSendChannelRtcpStatisticsCallback(
  int video_channel, RtcpStatisticsCallback* callback) {
  LOG_F(LS_INFO) << "channel " << video_channel;
  ViEChannelManagerScoped cs(*(shared_data_->channel_manager()));
  ViEChannel* vie_channel = cs.Channel(video_channel);
  if (!vie_channel) {
    shared_data_->SetLastError(kViERtpRtcpInvalidChannelId);
    return -1;
  }
  vie_channel->RegisterSendChannelRtcpStatisticsCallback(callback);
  return 0;
}

int ViERTP_RTCPImpl::DeregisterSendChannelRtcpStatisticsCallback(
    int video_channel, RtcpStatisticsCallback* callback) {
  LOG_F(LS_INFO) << "channel " << video_channel;
  ViEChannelManagerScoped cs(*(shared_data_->channel_manager()));
  ViEChannel* vie_channel = cs.Channel(video_channel);
  if (!vie_channel) {
    shared_data_->SetLastError(kViERtpRtcpInvalidChannelId);
    return -1;
  }
  vie_channel->RegisterSendChannelRtcpStatisticsCallback(NULL);
  return 0;
}

int ViERTP_RTCPImpl::RegisterReceiveChannelRtcpStatisticsCallback(
    const int video_channel,
    RtcpStatisticsCallback* callback) {
  LOG_F(LS_INFO) << "channel " << video_channel;
  ViEChannelManagerScoped cs(*(shared_data_->channel_manager()));
  ViEChannel* vie_channel = cs.Channel(video_channel);
  assert(vie_channel != NULL);
  vie_channel->RegisterReceiveChannelRtcpStatisticsCallback(callback);
  return 0;
}

int ViERTP_RTCPImpl::DeregisterReceiveChannelRtcpStatisticsCallback(
    const int video_channel,
    RtcpStatisticsCallback* callback) {
  LOG_F(LS_INFO) << "channel " << video_channel;
  ViEChannelManagerScoped cs(*(shared_data_->channel_manager()));
  ViEChannel* vie_channel = cs.Channel(video_channel);
  assert(vie_channel != NULL);
  vie_channel->RegisterReceiveChannelRtcpStatisticsCallback(NULL);
  return 0;
}

int ViERTP_RTCPImpl::RegisterSendChannelRtpStatisticsCallback(
    int video_channel, StreamDataCountersCallback* callback) {
  LOG_F(LS_INFO) << "channel " << video_channel;
  ViEChannelManagerScoped cs(*(shared_data_->channel_manager()));
  ViEChannel* vie_channel = cs.Channel(video_channel);
  assert(vie_channel != NULL);
  vie_channel->RegisterSendChannelRtpStatisticsCallback(callback);
  return 0;
}

int ViERTP_RTCPImpl::DeregisterSendChannelRtpStatisticsCallback(
    int video_channel, StreamDataCountersCallback* callback) {
  LOG_F(LS_INFO) << "channel " << video_channel;
  ViEChannelManagerScoped cs(*(shared_data_->channel_manager()));
  ViEChannel* vie_channel = cs.Channel(video_channel);
  assert(vie_channel != NULL);
  vie_channel->RegisterSendChannelRtpStatisticsCallback(NULL);
  return 0;
}

int ViERTP_RTCPImpl::RegisterReceiveChannelRtpStatisticsCallback(
    const int video_channel,
    StreamDataCountersCallback* callback) {
  LOG_F(LS_INFO) << "channel " << video_channel;
  ViEChannelManagerScoped cs(*(shared_data_->channel_manager()));
  ViEChannel* vie_channel = cs.Channel(video_channel);
  assert(vie_channel != NULL);
  vie_channel->RegisterReceiveChannelRtpStatisticsCallback(callback);
  return 0;
}

int ViERTP_RTCPImpl::DeregisterReceiveChannelRtpStatisticsCallback(
    const int video_channel,
    StreamDataCountersCallback* callback) {
  LOG_F(LS_INFO) << "channel " << video_channel;
  ViEChannelManagerScoped cs(*(shared_data_->channel_manager()));
  ViEChannel* vie_channel = cs.Channel(video_channel);
  assert(vie_channel != NULL);
  vie_channel->RegisterReceiveChannelRtpStatisticsCallback(NULL);
  return 0;
}

// Called whenever the send bitrate is updated.
int ViERTP_RTCPImpl::RegisterSendBitrateObserver(
    const int video_channel,
    BitrateStatisticsObserver* observer) {
  LOG_F(LS_INFO) << "channel " << video_channel;
  ViEChannelManagerScoped cs(*(shared_data_->channel_manager()));
  ViEChannel* vie_channel = cs.Channel(video_channel);
  assert(vie_channel != NULL);
  vie_channel->RegisterSendBitrateObserver(observer);
  return 0;
}

int ViERTP_RTCPImpl::DeregisterSendBitrateObserver(
    const int video_channel,
    BitrateStatisticsObserver* observer) {
  LOG_F(LS_INFO) << "channel " << video_channel;
  ViEChannelManagerScoped cs(*(shared_data_->channel_manager()));
  ViEChannel* vie_channel = cs.Channel(video_channel);
  assert(vie_channel != NULL);
  vie_channel->RegisterSendBitrateObserver(NULL);
  return 0;
}

int ViERTP_RTCPImpl::RegisterSendFrameCountObserver(
    int video_channel, FrameCountObserver* callback) {
  LOG_F(LS_INFO) << "channel " << video_channel;
  ViEChannelManagerScoped cs(*(shared_data_->channel_manager()));
  ViEChannel* vie_channel = cs.Channel(video_channel);
  if (!vie_channel) {
    shared_data_->SetLastError(kViERtpRtcpInvalidChannelId);
    return -1;
  }
  vie_channel->RegisterSendFrameCountObserver(callback);
  return 0;
}

int ViERTP_RTCPImpl::DeregisterSendFrameCountObserver(
    int video_channel, FrameCountObserver* callback) {
  LOG_F(LS_INFO) << "channel " << video_channel;
  ViEChannelManagerScoped cs(*(shared_data_->channel_manager()));
  ViEChannel* vie_channel = cs.Channel(video_channel);
  if (!vie_channel) {
    shared_data_->SetLastError(kViERtpRtcpInvalidChannelId);
    return -1;
  }
  vie_channel->RegisterSendFrameCountObserver(NULL);
  return 0;
}

int ViERTP_RTCPImpl::RegisterRtcpPacketTypeCounterObserver(
    int video_channel,
    RtcpPacketTypeCounterObserver* observer) {
  ViEChannelManagerScoped cs(*(shared_data_->channel_manager()));
  ViEChannel* vie_channel = cs.Channel(video_channel);
  if (!vie_channel) {
    shared_data_->SetLastError(kViERtpRtcpInvalidChannelId);
    return -1;
  }
  vie_channel->RegisterRtcpPacketTypeCounterObserver(observer);
  return 0;
}

}  // namespace webrtc
