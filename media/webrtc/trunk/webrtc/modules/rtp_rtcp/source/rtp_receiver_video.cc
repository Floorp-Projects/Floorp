/*
 *  Copyright (c) 2012 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "webrtc/modules/rtp_rtcp/source/rtp_receiver_video.h"

#include <assert.h>
#include <string.h>

#include "webrtc/modules/rtp_rtcp/interface/rtp_cvo.h"
#include "webrtc/modules/rtp_rtcp/interface/rtp_payload_registry.h"
#include "webrtc/modules/rtp_rtcp/source/rtp_format.h"
#include "webrtc/modules/rtp_rtcp/source/rtp_format_video_generic.h"
#include "webrtc/modules/rtp_rtcp/source/rtp_utility.h"
#include "webrtc/system_wrappers/interface/critical_section_wrapper.h"
#include "webrtc/system_wrappers/interface/logging.h"
#include "webrtc/system_wrappers/interface/trace_event.h"

namespace webrtc {

RTPReceiverStrategy* RTPReceiverStrategy::CreateVideoStrategy(
    RtpData* data_callback) {
  return new RTPReceiverVideo(data_callback);
}

RTPReceiverVideo::RTPReceiverVideo(RtpData* data_callback)
    : RTPReceiverStrategy(data_callback) {
}

RTPReceiverVideo::~RTPReceiverVideo() {
}

bool RTPReceiverVideo::ShouldReportCsrcChanges(uint8_t payload_type) const {
  // Always do this for video packets.
  return true;
}

int32_t RTPReceiverVideo::OnNewPayloadTypeCreated(
    const char payload_name[RTP_PAYLOAD_NAME_SIZE],
    int8_t payload_type,
    uint32_t frequency) {
  return 0;
}

int32_t RTPReceiverVideo::ParseRtpPacket(WebRtcRTPHeader* rtp_header,
                                         const PayloadUnion& specific_payload,
                                         bool is_red,
                                         const uint8_t* payload,
                                         size_t payload_length,
                                         int64_t timestamp_ms,
                                         bool is_first_packet) {
  TRACE_EVENT2(TRACE_DISABLED_BY_DEFAULT("webrtc_rtp"), "Video::ParseRtp",
               "seqnum", rtp_header->header.sequenceNumber, "timestamp",
               rtp_header->header.timestamp);
  rtp_header->type.Video.codec = specific_payload.Video.videoCodecType;

  const size_t payload_data_length =
      payload_length - rtp_header->header.paddingLength;

  if (payload == NULL || payload_data_length == 0) {
    return data_callback_->OnReceivedPayloadData(NULL, 0, rtp_header) == 0 ? 0
                                                                           : -1;
  }

  // We are not allowed to hold a critical section when calling below functions.
  rtc::scoped_ptr<RtpDepacketizer> depacketizer(
      RtpDepacketizer::Create(rtp_header->type.Video.codec));
  if (depacketizer.get() == NULL) {
    LOG(LS_ERROR) << "Failed to create depacketizer.";
    return -1;
  }

  rtp_header->type.Video.isFirstPacket = is_first_packet;
  RtpDepacketizer::ParsedPayload parsed_payload;
  if (!depacketizer->Parse(&parsed_payload, payload, payload_data_length))
    return -1;

  rtp_header->frameType = parsed_payload.frame_type;
  rtp_header->type = parsed_payload.type;
  rtp_header->type.Video.rotation = kVideoRotation_0;

  // Retrieve the video rotation information.
  if (rtp_header->header.extension.hasVideoRotation) {
    rtp_header->type.Video.rotation = ConvertCVOByteToVideoRotation(
        rtp_header->header.extension.videoRotation);
  }

  return data_callback_->OnReceivedPayloadData(parsed_payload.payload,
                                               parsed_payload.payload_length,
                                               rtp_header) == 0
             ? 0
             : -1;
}

int RTPReceiverVideo::GetPayloadTypeFrequency() const {
  return kVideoPayloadTypeFrequency;
}

RTPAliveType RTPReceiverVideo::ProcessDeadOrAlive(
    uint16_t last_payload_length) const {
  return kRtpDead;
}

int32_t RTPReceiverVideo::InvokeOnInitializeDecoder(
    RtpFeedback* callback,
    int32_t id,
    int8_t payload_type,
    const char payload_name[RTP_PAYLOAD_NAME_SIZE],
    const PayloadUnion& specific_payload) const {
  // For video we just go with default values.
  if (-1 ==
      callback->OnInitializeDecoder(
          id, payload_type, payload_name, kVideoPayloadTypeFrequency, 1, 0)) {
    LOG(LS_ERROR) << "Failed to created decoder for payload type: "
                  << static_cast<int>(payload_type);
    return -1;
  }
  return 0;
}

}  // namespace webrtc
