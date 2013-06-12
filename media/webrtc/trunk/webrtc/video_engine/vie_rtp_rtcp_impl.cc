/*
 *  Copyright (c) 2012 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "video_engine/vie_rtp_rtcp_impl.h"

#include "engine_configurations.h"  // NOLINT
#include "system_wrappers/interface/file_wrapper.h"
#include "system_wrappers/interface/trace.h"
#include "video_engine/include/vie_errors.h"
#include "video_engine/vie_channel.h"
#include "video_engine/vie_channel_manager.h"
#include "video_engine/vie_defines.h"
#include "video_engine/vie_encoder.h"
#include "video_engine/vie_impl.h"
#include "video_engine/vie_shared_data.h"

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
  VideoEngineImpl* vie_impl = reinterpret_cast<VideoEngineImpl*>(video_engine);
  ViERTP_RTCPImpl* vie_rtpimpl = vie_impl;
  // Increase ref count.
  (*vie_rtpimpl)++;
  return vie_rtpimpl;
#else
  return NULL;
#endif
}

int ViERTP_RTCPImpl::Release() {
  WEBRTC_TRACE(kTraceApiCall, kTraceVideo, shared_data_->instance_id(),
               "ViERTP_RTCP::Release()");
  // Decrease ref count.
  (*this)--;

  int32_t ref_count = GetCount();
  if (ref_count < 0) {
    WEBRTC_TRACE(kTraceWarning, kTraceVideo, shared_data_->instance_id(),
                 "ViERTP_RTCP release too many times");
    shared_data_->SetLastError(kViEAPIDoesNotExist);
    return -1;
  }
  WEBRTC_TRACE(kTraceInfo, kTraceVideo, shared_data_->instance_id(),
               "ViERTP_RTCP reference count: %d", ref_count);
  return ref_count;
}

ViERTP_RTCPImpl::ViERTP_RTCPImpl(ViESharedData* shared_data)
    : shared_data_(shared_data) {
  WEBRTC_TRACE(kTraceMemory, kTraceVideo, shared_data_->instance_id(),
               "ViERTP_RTCPImpl::ViERTP_RTCPImpl() Ctor");
}

ViERTP_RTCPImpl::~ViERTP_RTCPImpl() {
  WEBRTC_TRACE(kTraceMemory, kTraceVideo, shared_data_->instance_id(),
               "ViERTP_RTCPImpl::~ViERTP_RTCPImpl() Dtor");
}

int ViERTP_RTCPImpl::SetLocalSSRC(const int video_channel,
                                  const unsigned int SSRC,
                                  const StreamType usage,
                                  const unsigned char simulcast_idx) {
  WEBRTC_TRACE(kTraceApiCall, kTraceVideo,
               ViEId(shared_data_->instance_id(), video_channel),
               "%s(channel: %d, SSRC: %d)", __FUNCTION__, video_channel, SSRC);
  ViEChannelManagerScoped cs(*(shared_data_->channel_manager()));
  ViEChannel* vie_channel = cs.Channel(video_channel);
  if (!vie_channel) {
    // The channel doesn't exists
    WEBRTC_TRACE(kTraceError, kTraceVideo,
                 ViEId(shared_data_->instance_id(), video_channel),
                 "%s: Channel %d doesn't exist", __FUNCTION__, video_channel);
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
  WEBRTC_TRACE(webrtc::kTraceApiCall, webrtc::kTraceVideo,
               ViEId(shared_data_->instance_id(), videoChannel),
               "%s(channel: %d, usage:%d SSRC: 0x%x)",
               __FUNCTION__, usage, videoChannel, SSRC);

  // Get the channel
  ViEChannelManagerScoped cs(*(shared_data_->channel_manager()));
  ViEChannel* ptrViEChannel = cs.Channel(videoChannel);
  if (ptrViEChannel == NULL) {
    // The channel doesn't exists
    WEBRTC_TRACE(webrtc::kTraceError, webrtc::kTraceVideo,
                 ViEId(shared_data_->instance_id(), videoChannel),
                 "%s: Channel %d doesn't exist",
                 __FUNCTION__, videoChannel);
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
  WEBRTC_TRACE(kTraceApiCall, kTraceVideo,
               ViEId(shared_data_->instance_id(), video_channel),
               "%s(channel: %d, SSRC: %d)", __FUNCTION__, video_channel, SSRC);
  ViEChannelManagerScoped cs(*(shared_data_->channel_manager()));
  ViEChannel* vie_channel = cs.Channel(video_channel);
  if (!vie_channel) {
    WEBRTC_TRACE(kTraceError, kTraceVideo,
                 ViEId(shared_data_->instance_id(), video_channel),
                 "%s: Channel %d doesn't exist", __FUNCTION__, video_channel);
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
  WEBRTC_TRACE(kTraceApiCall, kTraceVideo,
               ViEId(shared_data_->instance_id(), video_channel),
               "%s(channel: %d)", __FUNCTION__, video_channel, SSRC);
  ViEChannelManagerScoped cs(*(shared_data_->channel_manager()));
  ViEChannel* vie_channel = cs.Channel(video_channel);
  if (!vie_channel) {
    WEBRTC_TRACE(kTraceError, kTraceVideo,
                 ViEId(shared_data_->instance_id(), video_channel),
                 "%s: Channel %d doesn't exist", __FUNCTION__, video_channel);
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
  WEBRTC_TRACE(kTraceApiCall, kTraceVideo,
               ViEId(shared_data_->instance_id(), video_channel),
               "%s(channel: %d)", __FUNCTION__, video_channel);
  ViEChannelManagerScoped cs(*(shared_data_->channel_manager()));
  ViEChannel* vie_channel = cs.Channel(video_channel);
  if (!vie_channel) {
    WEBRTC_TRACE(kTraceError, kTraceVideo,
                 ViEId(shared_data_->instance_id(), video_channel),
                 "%s: Channel %d doesn't exist", __FUNCTION__, video_channel);
    shared_data_->SetLastError(kViERtpRtcpInvalidChannelId);
    return -1;
  }
  if (vie_channel->GetRemoteCSRC(CSRCs) != 0) {
    shared_data_->SetLastError(kViERtpRtcpUnknownError);
    return -1;
  }
  return 0;
}

int ViERTP_RTCPImpl::SetRtxSendPayloadType(const int video_channel,
                                           const uint8_t payload_type) {
  WEBRTC_TRACE(kTraceApiCall, kTraceVideo,
               ViEId(shared_data_->instance_id(), video_channel),
               "%s(channel: %d)", __FUNCTION__, video_channel);
  ViEChannelManagerScoped cs(*(shared_data_->channel_manager()));
  ViEChannel* vie_channel = cs.Channel(video_channel);
  if (!vie_channel) {
    WEBRTC_TRACE(kTraceError, kTraceVideo,
                 ViEId(shared_data_->instance_id(), video_channel),
                 "%s: Channel %d doesn't exist", __FUNCTION__, video_channel);
    shared_data_->SetLastError(kViERtpRtcpInvalidChannelId);
    return -1;
  }
  if (!vie_channel->SetRtxSendPayloadType(payload_type)) {
    return -1;
  }
  return 0;
}

int ViERTP_RTCPImpl::SetRtxReceivePayloadType(const int video_channel,
                                              const uint8_t payload_type) {
  WEBRTC_TRACE(kTraceApiCall, kTraceVideo,
               ViEId(shared_data_->instance_id(), video_channel),
               "%s(channel: %d)", __FUNCTION__, video_channel);
  ViEChannelManagerScoped cs(*(shared_data_->channel_manager()));
  ViEChannel* vie_channel = cs.Channel(video_channel);
  if (!vie_channel) {
    WEBRTC_TRACE(kTraceError, kTraceVideo,
                 ViEId(shared_data_->instance_id(), video_channel),
                 "%s: Channel %d doesn't exist", __FUNCTION__, video_channel);
    shared_data_->SetLastError(kViERtpRtcpInvalidChannelId);
    return -1;
  }
  vie_channel->SetRtxReceivePayloadType(payload_type);
  return 0;
}

int ViERTP_RTCPImpl::SetStartSequenceNumber(const int video_channel,
                                            uint16_t sequence_number) {
  WEBRTC_TRACE(kTraceApiCall, kTraceVideo,
               ViEId(shared_data_->instance_id(), video_channel),
               "%s(channel: %d, sequence_number: %u)", __FUNCTION__,
               video_channel, sequence_number);
  ViEChannelManagerScoped cs(*(shared_data_->channel_manager()));
  ViEChannel* vie_channel = cs.Channel(video_channel);
  if (!vie_channel) {
    WEBRTC_TRACE(kTraceError, kTraceVideo,
                 ViEId(shared_data_->instance_id(), video_channel),
                 "%s: Channel %d doesn't exist", __FUNCTION__, video_channel);
    shared_data_->SetLastError(kViERtpRtcpInvalidChannelId);
    return -1;
  }
  if (vie_channel->Sending()) {
    WEBRTC_TRACE(kTraceError, kTraceVideo,
                 ViEId(shared_data_->instance_id(), video_channel),
                 "%s: Channel %d already sending.", __FUNCTION__,
                 video_channel);
    shared_data_->SetLastError(kViERtpRtcpAlreadySending);
    return -1;
  }
  if (vie_channel->SetStartSequenceNumber(sequence_number) != 0) {
    shared_data_->SetLastError(kViERtpRtcpUnknownError);
    return -1;
  }
  return 0;
}

int ViERTP_RTCPImpl::SetRTCPStatus(const int video_channel,
                                   const ViERTCPMode rtcp_mode) {
  WEBRTC_TRACE(kTraceApiCall, kTraceVideo,
               ViEId(shared_data_->instance_id(), video_channel),
               "%s(channel: %d, mode: %d)", __FUNCTION__, video_channel,
               rtcp_mode);
  ViEChannelManagerScoped cs(*(shared_data_->channel_manager()));
  ViEChannel* vie_channel = cs.Channel(video_channel);
  if (!vie_channel) {
    WEBRTC_TRACE(kTraceError, kTraceVideo,
                 ViEId(shared_data_->instance_id(), video_channel),
                 "%s: Channel %d doesn't exist", __FUNCTION__, video_channel);
    shared_data_->SetLastError(kViERtpRtcpInvalidChannelId);
    return -1;
  }

  RTCPMethod module_mode = ViERTCPModeToRTCPMethod(rtcp_mode);
  if (vie_channel->SetRTCPMode(module_mode) != 0) {
    shared_data_->SetLastError(kViERtpRtcpUnknownError);
    return -1;
  }
  return 0;
}

int ViERTP_RTCPImpl::GetRTCPStatus(const int video_channel,
                                   ViERTCPMode& rtcp_mode) const {
  WEBRTC_TRACE(kTraceApiCall, kTraceVideo,
               ViEId(shared_data_->instance_id(), video_channel),
               "%s(channel: %d)", __FUNCTION__, video_channel, rtcp_mode);
  ViEChannelManagerScoped cs(*(shared_data_->channel_manager()));
  ViEChannel* vie_channel = cs.Channel(video_channel);
  if (!vie_channel) {
    WEBRTC_TRACE(kTraceError, kTraceVideo,
                 ViEId(shared_data_->instance_id(), video_channel),
                 "%s: Channel %d doesn't exist", __FUNCTION__, video_channel);
    shared_data_->SetLastError(kViERtpRtcpInvalidChannelId);
    return -1;
  }
  RTCPMethod module_mode = kRtcpOff;
  if (vie_channel->GetRTCPMode(&module_mode) != 0) {
    WEBRTC_TRACE(kTraceError, kTraceVideo,
                 ViEId(shared_data_->instance_id(), video_channel),
                 "%s: could not get current RTCP mode", __FUNCTION__);
    shared_data_->SetLastError(kViERtpRtcpUnknownError);
    return -1;
  }
  rtcp_mode = RTCPMethodToViERTCPMode(module_mode);
  return 0;
}

int ViERTP_RTCPImpl::SetRTCPCName(const int video_channel,
                                  const char rtcp_cname[KMaxRTCPCNameLength]) {
  WEBRTC_TRACE(kTraceApiCall, kTraceVideo,
               ViEId(shared_data_->instance_id(), video_channel),
               "%s(channel: %d, name: %s)", __FUNCTION__, video_channel,
               rtcp_cname);
  ViEChannelManagerScoped cs(*(shared_data_->channel_manager()));
  ViEChannel* vie_channel = cs.Channel(video_channel);
  if (!vie_channel) {
    WEBRTC_TRACE(kTraceError, kTraceVideo,
                 ViEId(shared_data_->instance_id(), video_channel),
                 "%s: Channel %d doesn't exist", __FUNCTION__, video_channel);
    shared_data_->SetLastError(kViERtpRtcpInvalidChannelId);
    return -1;
  }
  if (vie_channel->Sending()) {
    WEBRTC_TRACE(kTraceError, kTraceVideo,
                 ViEId(shared_data_->instance_id(), video_channel),
                 "%s: Channel %d already sending.", __FUNCTION__,
                 video_channel);
    shared_data_->SetLastError(kViERtpRtcpAlreadySending);
    return -1;
  }
  if (vie_channel->SetRTCPCName(rtcp_cname) != 0) {
    shared_data_->SetLastError(kViERtpRtcpUnknownError);
    return -1;
  }
  return 0;
}

int ViERTP_RTCPImpl::GetRTCPCName(const int video_channel,
                                  char rtcp_cname[KMaxRTCPCNameLength]) const {
  WEBRTC_TRACE(kTraceApiCall, kTraceVideo,
               ViEId(shared_data_->instance_id(), video_channel),
               "%s(channel: %d)", __FUNCTION__, video_channel);
  ViEChannelManagerScoped cs(*(shared_data_->channel_manager()));
  ViEChannel* vie_channel = cs.Channel(video_channel);
  if (!vie_channel) {
    WEBRTC_TRACE(kTraceError, kTraceVideo,
                 ViEId(shared_data_->instance_id(), video_channel),
                 "%s: Channel %d doesn't exist", __FUNCTION__, video_channel);
    shared_data_->SetLastError(kViERtpRtcpInvalidChannelId);
    return -1;
  }
  if (vie_channel->GetRTCPCName(rtcp_cname) != 0) {
    shared_data_->SetLastError(kViERtpRtcpUnknownError);
    return -1;
  }
  return 0;
}

int ViERTP_RTCPImpl::GetRemoteRTCPCName(
    const int video_channel,
    char rtcp_cname[KMaxRTCPCNameLength]) const {
  WEBRTC_TRACE(kTraceApiCall, kTraceVideo,
               ViEId(shared_data_->instance_id(), video_channel),
               "%s(channel: %d)", __FUNCTION__, video_channel);
  ViEChannelManagerScoped cs(*(shared_data_->channel_manager()));
  ViEChannel* vie_channel = cs.Channel(video_channel);
  if (!vie_channel) {
    WEBRTC_TRACE(kTraceError, kTraceVideo,
                 ViEId(shared_data_->instance_id(), video_channel),
                 "%s: Channel %d doesn't exist", __FUNCTION__, video_channel);
    shared_data_->SetLastError(kViERtpRtcpInvalidChannelId);
    return -1;
  }
  if (vie_channel->GetRemoteRTCPCName(rtcp_cname) != 0) {
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
  WEBRTC_TRACE(kTraceApiCall, kTraceVideo,
               ViEId(shared_data_->instance_id(), video_channel),
               "%s(channel: %d, sub_type: %c, name: %d, data: x, length: %u)",
               __FUNCTION__, video_channel, sub_type, name,
               data_length_in_bytes);
  ViEChannelManagerScoped cs(*(shared_data_->channel_manager()));
  ViEChannel* vie_channel = cs.Channel(video_channel);
  if (!vie_channel) {
    WEBRTC_TRACE(kTraceError, kTraceVideo,
                 ViEId(shared_data_->instance_id(), video_channel),
                 "%s: Channel %d doesn't exist", __FUNCTION__, video_channel);
    shared_data_->SetLastError(kViERtpRtcpInvalidChannelId);
    return -1;
  }
  if (!vie_channel->Sending()) {
    WEBRTC_TRACE(kTraceError, kTraceVideo,
                 ViEId(shared_data_->instance_id(), video_channel),
                 "%s: Channel %d not sending", __FUNCTION__, video_channel);
    shared_data_->SetLastError(kViERtpRtcpNotSending);
    return -1;
  }
  RTCPMethod method;
  if (vie_channel->GetRTCPMode(&method) != 0 || method == kRtcpOff) {
    WEBRTC_TRACE(kTraceError, kTraceVideo,
                 ViEId(shared_data_->instance_id(), video_channel),
                 "%s: RTCP disabled on channel %d.", __FUNCTION__,
                 video_channel);
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
  WEBRTC_TRACE(kTraceApiCall, kTraceVideo,
               ViEId(shared_data_->instance_id(), video_channel),
               "%s(channel: %d, enable: %d)", __FUNCTION__, video_channel,
               enable);
  ViEChannelManagerScoped cs(*(shared_data_->channel_manager()));
  ViEChannel* vie_channel = cs.Channel(video_channel);
  if (!vie_channel) {
    WEBRTC_TRACE(kTraceError, kTraceVideo,
                 ViEId(shared_data_->instance_id(), video_channel),
                 "%s: Channel %d doesn't exist", __FUNCTION__, video_channel);
    shared_data_->SetLastError(kViERtpRtcpInvalidChannelId);
    return -1;
  }
  if (vie_channel->SetNACKStatus(enable) != 0) {
    WEBRTC_TRACE(kTraceError, kTraceVideo,
                 ViEId(shared_data_->instance_id(), video_channel),
                 "%s: failed for channel %d", __FUNCTION__, video_channel);
    shared_data_->SetLastError(kViERtpRtcpUnknownError);
    return -1;
  }

  // Update the encoder
  ViEEncoder* vie_encoder = cs.Encoder(video_channel);
  if (!vie_encoder) {
    WEBRTC_TRACE(kTraceError, kTraceVideo,
                 ViEId(shared_data_->instance_id(), video_channel),
                 "%s: Could not get encoder for channel %d", __FUNCTION__,
                 video_channel);
    shared_data_->SetLastError(kViERtpRtcpUnknownError);
    return -1;
  }
  vie_encoder->UpdateProtectionMethod();
  return 0;
}

int ViERTP_RTCPImpl::SetFECStatus(const int video_channel, const bool enable,
                                  const unsigned char payload_typeRED,
                                  const unsigned char payload_typeFEC) {
  WEBRTC_TRACE(kTraceApiCall, kTraceVideo,
               ViEId(shared_data_->instance_id(), video_channel),
               "%s(channel: %d, enable: %d, payload_typeRED: %u, "
               "payloadTypeFEC: %u)",
               __FUNCTION__, video_channel, enable, payload_typeRED,
               payload_typeFEC);
  ViEChannelManagerScoped cs(*(shared_data_->channel_manager()));
  ViEChannel* vie_channel = cs.Channel(video_channel);
  if (!vie_channel) {
    WEBRTC_TRACE(kTraceError, kTraceVideo,
                 ViEId(shared_data_->instance_id(), video_channel),
                 "%s: Channel %d doesn't exist", __FUNCTION__,
                 video_channel);
    shared_data_->SetLastError(kViERtpRtcpInvalidChannelId);
    return -1;
  }
  if (vie_channel->SetFECStatus(enable, payload_typeRED,
                                payload_typeFEC) != 0) {
    WEBRTC_TRACE(kTraceError, kTraceVideo,
                 ViEId(shared_data_->instance_id(), video_channel),
                 "%s: failed for channel %d", __FUNCTION__, video_channel);
    shared_data_->SetLastError(kViERtpRtcpUnknownError);
    return -1;
  }
  // Update the encoder.
  ViEEncoder* vie_encoder = cs.Encoder(video_channel);
  if (!vie_encoder) {
    WEBRTC_TRACE(kTraceError, kTraceVideo,
                 ViEId(shared_data_->instance_id(), video_channel),
                 "%s: Could not get encoder for channel %d", __FUNCTION__,
                 video_channel);
    shared_data_->SetLastError(kViERtpRtcpUnknownError);
    return -1;
  }
  vie_encoder->UpdateProtectionMethod();
  return 0;
}

int ViERTP_RTCPImpl::SetHybridNACKFECStatus(
    const int video_channel,
    const bool enable,
    const unsigned char payload_typeRED,
    const unsigned char payload_typeFEC) {
  WEBRTC_TRACE(kTraceApiCall, kTraceVideo,
               ViEId(shared_data_->instance_id(), video_channel),
               "%s(channel: %d, enable: %d, payload_typeRED: %u, "
               "payloadTypeFEC: %u)",
               __FUNCTION__, video_channel, enable, payload_typeRED,
               payload_typeFEC);
  ViEChannelManagerScoped cs(*(shared_data_->channel_manager()));
  ViEChannel* vie_channel = cs.Channel(video_channel);
  if (!vie_channel) {
    WEBRTC_TRACE(kTraceError, kTraceVideo,
                 ViEId(shared_data_->instance_id(), video_channel),
                 "%s: Channel %d doesn't exist", __FUNCTION__, video_channel);
    shared_data_->SetLastError(kViERtpRtcpInvalidChannelId);
    return -1;
  }

  // Update the channel status with hybrid NACK FEC mode.
  if (vie_channel->SetHybridNACKFECStatus(enable, payload_typeRED,
                                          payload_typeFEC) != 0) {
    WEBRTC_TRACE(kTraceError, kTraceVideo,
                 ViEId(shared_data_->instance_id(), video_channel),
                 "%s: failed for channel %d", __FUNCTION__, video_channel);
    shared_data_->SetLastError(kViERtpRtcpUnknownError);
    return -1;
  }

  // Update the encoder.
  ViEEncoder* vie_encoder = cs.Encoder(video_channel);
  if (!vie_encoder) {
    WEBRTC_TRACE(kTraceError, kTraceVideo,
                 ViEId(shared_data_->instance_id(), video_channel),
                 "%s: Could not get encoder for channel %d", __FUNCTION__,
                 video_channel);
    shared_data_->SetLastError(kViERtpRtcpUnknownError);
    return -1;
  }
  vie_encoder->UpdateProtectionMethod();
  return 0;
}

int ViERTP_RTCPImpl::SetSenderBufferingMode(int video_channel,
                                               int target_delay_ms) {
  WEBRTC_TRACE(kTraceApiCall, kTraceVideo,
               ViEId(shared_data_->instance_id(), video_channel),
               "%s(channel: %d, sender target_delay: %d)",
               __FUNCTION__, video_channel, target_delay_ms);
  ViEChannelManagerScoped cs(*(shared_data_->channel_manager()));
  ViEChannel* vie_channel = cs.Channel(video_channel);
  if (!vie_channel) {
    WEBRTC_TRACE(kTraceError, kTraceVideo,
                 ViEId(shared_data_->instance_id(), video_channel),
                 "%s: Channel %d doesn't exist", __FUNCTION__, video_channel);
    shared_data_->SetLastError(kViERtpRtcpInvalidChannelId);
    return -1;
  }
  ViEEncoder* vie_encoder = cs.Encoder(video_channel);
  if (!vie_encoder) {
    WEBRTC_TRACE(kTraceError, kTraceVideo,
                 ViEId(shared_data_->instance_id(), video_channel),
                 "%s: Could not get encoder for channel %d", __FUNCTION__,
                 video_channel);
    shared_data_->SetLastError(kViERtpRtcpInvalidChannelId);
    return -1;
  }

  // Update the channel with buffering mode settings.
  if (vie_channel->SetSenderBufferingMode(target_delay_ms) != 0) {
    WEBRTC_TRACE(kTraceError, kTraceVideo,
                 ViEId(shared_data_->instance_id(), video_channel),
                 "%s: failed for channel %d", __FUNCTION__, video_channel);
    shared_data_->SetLastError(kViERtpRtcpUnknownError);
    return -1;
  }

  // Update the encoder's buffering mode settings.
  vie_encoder->SetSenderBufferingMode(target_delay_ms);
  return 0;
}

int ViERTP_RTCPImpl::SetReceiverBufferingMode(int video_channel,
                                                 int target_delay_ms) {
  WEBRTC_TRACE(kTraceApiCall, kTraceVideo,
               ViEId(shared_data_->instance_id(), video_channel),
               "%s(channel: %d, receiver target_delay: %d)",
               __FUNCTION__, video_channel, target_delay_ms);
  ViEChannelManagerScoped cs(*(shared_data_->channel_manager()));
  ViEChannel* vie_channel = cs.Channel(video_channel);
  if (!vie_channel) {
    WEBRTC_TRACE(kTraceError, kTraceVideo,
                 ViEId(shared_data_->instance_id(), video_channel),
                 "%s: Channel %d doesn't exist", __FUNCTION__, video_channel);
    shared_data_->SetLastError(kViERtpRtcpInvalidChannelId);
    return -1;
  }

  // Update the channel with buffering mode settings.
  if (vie_channel->SetReceiverBufferingMode(target_delay_ms) != 0) {
    WEBRTC_TRACE(kTraceError, kTraceVideo,
                 ViEId(shared_data_->instance_id(), video_channel),
                 "%s: failed for channel %d", __FUNCTION__, video_channel);
    shared_data_->SetLastError(kViERtpRtcpUnknownError);
    return -1;
  }
  return 0;
}

int ViERTP_RTCPImpl::SetKeyFrameRequestMethod(
  const int video_channel,
  const ViEKeyFrameRequestMethod method) {
  WEBRTC_TRACE(kTraceApiCall, kTraceVideo,
               ViEId(shared_data_->instance_id(), video_channel),
               "%s(channel: %d, method: %d)", __FUNCTION__, video_channel,
               method);

  // Get the channel.
  ViEChannelManagerScoped cs(*(shared_data_->channel_manager()));
  ViEChannel* vie_channel = cs.Channel(video_channel);
  if (!vie_channel) {
    WEBRTC_TRACE(kTraceError, kTraceVideo,
                 ViEId(shared_data_->instance_id(), video_channel),
                 "%s: Channel %d doesn't exist", __FUNCTION__, video_channel);
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
  WEBRTC_TRACE(kTraceApiCall, kTraceVideo,
               ViEId(shared_data_->instance_id(), video_channel),
               "%s(channel: %d, enable: %d)", __FUNCTION__, video_channel,
               enable);
  ViEChannelManagerScoped cs(*(shared_data_->channel_manager()));
  ViEChannel* vie_channel = cs.Channel(video_channel);
  if (!vie_channel) {
    WEBRTC_TRACE(kTraceError, kTraceVideo,
                 ViEId(shared_data_->instance_id(), video_channel),
                 "%s: Channel %d doesn't exist", __FUNCTION__, video_channel);
    shared_data_->SetLastError(kViERtpRtcpInvalidChannelId);
    return -1;
  }
  if (vie_channel->EnableTMMBR(enable) != 0) {
    shared_data_->SetLastError(kViERtpRtcpUnknownError);
    return -1;
  }
  return 0;
}

int ViERTP_RTCPImpl::SetRembStatus(int video_channel, bool sender,
                                   bool receiver) {
  WEBRTC_TRACE(kTraceApiCall, kTraceVideo,
               ViEId(shared_data_->instance_id(), video_channel),
               "ViERTP_RTCPImpl::SetRembStatus(%d, %d, %d)", video_channel,
               sender, receiver);
  if (!shared_data_->channel_manager()->SetRembStatus(video_channel, sender,
                                                      receiver)) {
    return -1;
  }
  return 0;
}

int ViERTP_RTCPImpl::SetBandwidthEstimationMode(BandwidthEstimationMode mode) {
  WEBRTC_TRACE(kTraceApiCall, kTraceVideo, shared_data_->instance_id(),
               "ViERTP_RTCPImpl::SetBandwidthEstimationMode(%d)", mode);
  if (!shared_data_->channel_manager()->SetBandwidthEstimationMode(mode)) {
    return -1;
  }
  return 0;
}

int ViERTP_RTCPImpl::SetSendTimestampOffsetStatus(int video_channel,
                                                  bool enable,
                                                  int id) {
  WEBRTC_TRACE(kTraceApiCall, kTraceVideo,
               ViEId(shared_data_->instance_id(), video_channel),
               "ViERTP_RTCPImpl::SetSendTimestampOffsetStatus(%d, %d, %d)",
               video_channel, enable, id);

  ViEChannelManagerScoped cs(*(shared_data_->channel_manager()));
  ViEChannel* vie_channel = cs.Channel(video_channel);
  if (!vie_channel) {
    WEBRTC_TRACE(kTraceError, kTraceVideo,
                 ViEId(shared_data_->instance_id(), video_channel),
                 "%s: Channel %d doesn't exist", __FUNCTION__, video_channel);
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
  WEBRTC_TRACE(kTraceApiCall, kTraceVideo,
               ViEId(shared_data_->instance_id(), video_channel),
               "ViERTP_RTCPImpl::SetReceiveTimestampOffsetStatus(%d, %d, %d)",
               video_channel, enable, id);
  ViEChannelManagerScoped cs(*(shared_data_->channel_manager()));
  ViEChannel* vie_channel = cs.Channel(video_channel);
  if (!vie_channel) {
    WEBRTC_TRACE(kTraceError, kTraceVideo,
                 ViEId(shared_data_->instance_id(), video_channel),
                 "%s: Channel %d doesn't exist", __FUNCTION__, video_channel);
    shared_data_->SetLastError(kViERtpRtcpInvalidChannelId);
    return -1;
  }
  if (vie_channel->SetReceiveTimestampOffsetStatus(enable, id) != 0) {
    shared_data_->SetLastError(kViERtpRtcpUnknownError);
    return -1;
  }
  return 0;
}

int ViERTP_RTCPImpl::SetTransmissionSmoothingStatus(int video_channel,
                                                    bool enable) {
  WEBRTC_TRACE(kTraceApiCall, kTraceVideo,
               ViEId(shared_data_->instance_id(), video_channel),
               "%s(channel: %d, enble: %d)", __FUNCTION__, video_channel,
               enable);
  ViEChannelManagerScoped cs(*(shared_data_->channel_manager()));
  ViEChannel* vie_channel = cs.Channel(video_channel);
  if (!vie_channel) {
    WEBRTC_TRACE(kTraceError, kTraceVideo,
                 ViEId(shared_data_->instance_id(), video_channel),
                 "%s: Channel %d doesn't exist", __FUNCTION__, video_channel);
    shared_data_->SetLastError(kViERtpRtcpInvalidChannelId);
    return -1;
  }
  vie_channel->SetTransmissionSmoothingStatus(enable);
  return 0;
}

int ViERTP_RTCPImpl::GetReceivedRTCPStatistics(const int video_channel,
                                               uint16_t& fraction_lost,
                                               unsigned int& cumulative_lost,
                                               unsigned int& extended_max,
                                               unsigned int& jitter,
                                               int& rtt_ms) const {
  WEBRTC_TRACE(kTraceApiCall, kTraceVideo,
               ViEId(shared_data_->instance_id(), video_channel),
               "%s(channel: %d)", __FUNCTION__, video_channel);
  ViEChannelManagerScoped cs(*(shared_data_->channel_manager()));
  ViEChannel* vie_channel = cs.Channel(video_channel);
  if (!vie_channel) {
    WEBRTC_TRACE(kTraceError, kTraceVideo,
                 ViEId(shared_data_->instance_id(), video_channel),
                 "%s: Channel %d doesn't exist", __FUNCTION__, video_channel);
    shared_data_->SetLastError(kViERtpRtcpInvalidChannelId);
    return -1;
  }
  if (vie_channel->GetReceivedRtcpStatistics(&fraction_lost,
                                             &cumulative_lost,
                                             &extended_max,
                                             &jitter,
                                             &rtt_ms) != 0) {
    shared_data_->SetLastError(kViERtpRtcpUnknownError);
    return -1;
  }
  return 0;
}

int ViERTP_RTCPImpl::GetSentRTCPStatistics(const int video_channel,
                                           uint16_t& fraction_lost,
                                           unsigned int& cumulative_lost,
                                           unsigned int& extended_max,
                                           unsigned int& jitter,
                                           int& rtt_ms) const {
  WEBRTC_TRACE(kTraceApiCall, kTraceVideo,
               ViEId(shared_data_->instance_id(), video_channel),
               "%s(channel: %d)", __FUNCTION__, video_channel);
  ViEChannelManagerScoped cs(*(shared_data_->channel_manager()));
  ViEChannel* vie_channel = cs.Channel(video_channel);
  if (!vie_channel) {
    WEBRTC_TRACE(kTraceError, kTraceVideo,
                 ViEId(shared_data_->instance_id(), video_channel),
                 "%s: Channel %d doesn't exist", __FUNCTION__, video_channel);
    shared_data_->SetLastError(kViERtpRtcpInvalidChannelId);
    return -1;
  }

  if (vie_channel->GetSendRtcpStatistics(&fraction_lost, &cumulative_lost,
                                         &extended_max, &jitter,
                                         &rtt_ms) != 0) {
    shared_data_->SetLastError(kViERtpRtcpUnknownError);
    return -1;
  }
  return 0;
}

int ViERTP_RTCPImpl::GetRTPStatistics(const int video_channel,
                                      unsigned int& bytes_sent,
                                      unsigned int& packets_sent,
                                      unsigned int& bytes_received,
                                      unsigned int& packets_received) const {
  WEBRTC_TRACE(kTraceApiCall, kTraceVideo,
               ViEId(shared_data_->instance_id(), video_channel),
               "%s(channel: %d)", __FUNCTION__, video_channel);
  ViEChannelManagerScoped cs(*(shared_data_->channel_manager()));
  ViEChannel* vie_channel = cs.Channel(video_channel);
  if (!vie_channel) {
    WEBRTC_TRACE(kTraceError, kTraceVideo,
                 ViEId(shared_data_->instance_id(), video_channel),
                 "%s: Channel %d doesn't exist", __FUNCTION__, video_channel);
    shared_data_->SetLastError(kViERtpRtcpInvalidChannelId);
    return -1;
  }
  if (vie_channel->GetRtpStatistics(&bytes_sent,
                                    &packets_sent,
                                    &bytes_received,
                                    &packets_received) != 0) {
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
  WEBRTC_TRACE(kTraceApiCall, kTraceVideo,
               ViEId(shared_data_->instance_id(), video_channel),
               "%s(channel: %d)", __FUNCTION__, video_channel);
  ViEChannelManagerScoped cs(*(shared_data_->channel_manager()));
  ViEChannel* vie_channel = cs.Channel(video_channel);
  if (!vie_channel) {
    WEBRTC_TRACE(kTraceError, kTraceVideo,
                 ViEId(shared_data_->instance_id(), video_channel),
                 "%s: Channel %d doesn't exist", __FUNCTION__, video_channel);
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
  WEBRTC_TRACE(kTraceApiCall, kTraceVideo,
               ViEId(shared_data_->instance_id(), video_channel),
               "%s(channel: %d)", __FUNCTION__, video_channel);
  ViEChannelManagerScoped cs(*(shared_data_->channel_manager()));
  ViEEncoder* vie_encoder = cs.Encoder(video_channel);
  if (!vie_encoder) {
    WEBRTC_TRACE(kTraceError, kTraceVideo,
                 ViEId(shared_data_->instance_id(), video_channel),
                 "%s: Could not get encoder for channel %d", __FUNCTION__,
                 video_channel);
    shared_data_->SetLastError(kViERtpRtcpInvalidChannelId);
    return -1;
  }
  return vie_encoder->EstimatedSendBandwidth(
      static_cast<uint32_t*>(estimated_bandwidth));
}

int ViERTP_RTCPImpl::GetEstimatedReceiveBandwidth(
    const int video_channel,
    unsigned int* estimated_bandwidth) const {
  WEBRTC_TRACE(kTraceApiCall, kTraceVideo,
               ViEId(shared_data_->instance_id(), video_channel),
               "%s(channel: %d)", __FUNCTION__, video_channel);
  ViEChannelManagerScoped cs(*(shared_data_->channel_manager()));
  ViEChannel* vie_channel = cs.Channel(video_channel);
  if (!vie_channel) {
    WEBRTC_TRACE(kTraceError, kTraceVideo,
                 ViEId(shared_data_->instance_id(), video_channel),
                 "%s: Could not get channel %d", __FUNCTION__,
                 video_channel);
    shared_data_->SetLastError(kViERtpRtcpInvalidChannelId);
    return -1;
  }
  vie_channel->GetEstimatedReceiveBandwidth(
      static_cast<uint32_t*>(estimated_bandwidth));
  return 0;
}

int ViERTP_RTCPImpl::SetOverUseDetectorOptions(
    const OverUseDetectorOptions& options) const {
  if (!shared_data_->Initialized()) {
    shared_data_->SetLastError(kViENotInitialized);
    WEBRTC_TRACE(kTraceError, kTraceVideo, ViEId(shared_data_->instance_id()),
                 "%s - ViE instance %d not initialized", __FUNCTION__,
                 shared_data_->instance_id());
    return -1;
  }
  // Lock the channel manager to avoid creating a channel with
  // "undefined" bwe settings (atomic copy).
  ViEChannelManagerScoped cs(*(shared_data_->channel_manager()));
  shared_data_->SetOverUseDetectorOptions(options);
  return 0;
}

int ViERTP_RTCPImpl::StartRTPDump(const int video_channel,
                                  const char file_nameUTF8[1024],
                                  RTPDirections direction) {
  WEBRTC_TRACE(kTraceApiCall, kTraceVideo,
               ViEId(shared_data_->instance_id(), video_channel),
               "%s(channel: %d, file_name: %s, direction: %d)", __FUNCTION__,
               video_channel, file_nameUTF8, direction);
  assert(FileWrapper::kMaxFileNameSize == 1024);
  ViEChannelManagerScoped cs(*(shared_data_->channel_manager()));
  ViEChannel* vie_channel = cs.Channel(video_channel);
  if (!vie_channel) {
    WEBRTC_TRACE(kTraceError, kTraceVideo,
                 ViEId(shared_data_->instance_id(), video_channel),
                 "%s: Channel %d doesn't exist", __FUNCTION__, video_channel);
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
  WEBRTC_TRACE(kTraceApiCall, kTraceVideo,
               ViEId(shared_data_->instance_id(), video_channel),
               "%s(channel: %d, direction: %d)", __FUNCTION__, video_channel,
               direction);
  ViEChannelManagerScoped cs(*(shared_data_->channel_manager()));
  ViEChannel* vie_channel = cs.Channel(video_channel);
  if (!vie_channel) {
    WEBRTC_TRACE(kTraceError, kTraceVideo,
                 ViEId(shared_data_->instance_id(), video_channel),
                 "%s: Channel %d doesn't exist", __FUNCTION__, video_channel);
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
  WEBRTC_TRACE(kTraceApiCall, kTraceVideo,
               ViEId(shared_data_->instance_id(), video_channel),
               "%s(channel: %d)", __FUNCTION__, video_channel);
  ViEChannelManagerScoped cs(*(shared_data_->channel_manager()));
  ViEChannel* vie_channel = cs.Channel(video_channel);
  if (!vie_channel) {
    WEBRTC_TRACE(kTraceError, kTraceVideo,
                 ViEId(shared_data_->instance_id(), video_channel),
                 "%s: Channel %d doesn't exist", __FUNCTION__, video_channel);
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
  WEBRTC_TRACE(kTraceApiCall, kTraceVideo,
               ViEId(shared_data_->instance_id(), video_channel),
               "%s(channel: %d)", __FUNCTION__, video_channel);
  ViEChannelManagerScoped cs(*(shared_data_->channel_manager()));
  ViEChannel* vie_channel = cs.Channel(video_channel);
  if (!vie_channel) {
    WEBRTC_TRACE(kTraceError, kTraceVideo,
                 ViEId(shared_data_->instance_id(), video_channel),
                 "%s: Channel %d doesn't exist", __FUNCTION__, video_channel);
    shared_data_->SetLastError(kViERtpRtcpInvalidChannelId);
    return -1;
  }
  if (vie_channel->RegisterRtpObserver(NULL) != 0) {
    shared_data_->SetLastError(kViERtpRtcpObserverNotRegistered);
    return -1;
  }
  return 0;
}

int ViERTP_RTCPImpl::RegisterRTCPObserver(const int video_channel,
                                          ViERTCPObserver& observer) {
  WEBRTC_TRACE(kTraceApiCall, kTraceVideo,
               ViEId(shared_data_->instance_id(), video_channel),
               "%s(channel: %d)", __FUNCTION__, video_channel);
  ViEChannelManagerScoped cs(*(shared_data_->channel_manager()));
  ViEChannel* vie_channel = cs.Channel(video_channel);
  if (!vie_channel) {
    WEBRTC_TRACE(kTraceError, kTraceVideo,
                 ViEId(shared_data_->instance_id(), video_channel),
                 "%s: Channel %d doesn't exist", __FUNCTION__, video_channel);
    shared_data_->SetLastError(kViERtpRtcpInvalidChannelId);
    return -1;
  }
  if (vie_channel->RegisterRtcpObserver(&observer) != 0) {
    shared_data_->SetLastError(kViERtpRtcpObserverAlreadyRegistered);
    return -1;
  }
  return 0;
}

int ViERTP_RTCPImpl::DeregisterRTCPObserver(const int video_channel) {
  WEBRTC_TRACE(kTraceApiCall, kTraceVideo,
               ViEId(shared_data_->instance_id(), video_channel),
               "%s(channel: %d)", __FUNCTION__, video_channel);
  ViEChannelManagerScoped cs(*(shared_data_->channel_manager()));
  ViEChannel* vie_channel = cs.Channel(video_channel);
  if (!vie_channel) {
    WEBRTC_TRACE(kTraceError, kTraceVideo,
                 ViEId(shared_data_->instance_id(), video_channel),
                 "%s: Channel %d doesn't exist", __FUNCTION__, video_channel);
    shared_data_->SetLastError(kViERtpRtcpInvalidChannelId);
    return -1;
  }
  if (vie_channel->RegisterRtcpObserver(NULL) != 0) {
    shared_data_->SetLastError(kViERtpRtcpObserverNotRegistered);
    return -1;
  }
  return 0;
}

}  // namespace webrtc
