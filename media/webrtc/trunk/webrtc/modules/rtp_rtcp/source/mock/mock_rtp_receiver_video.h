/*
 *  Copyright (c) 2012 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef WEBRTC_MODULES_RTP_RTCP_SOURCE_MOCK_MOCK_RTP_RECEIVER_VIDEO_H_
#define WEBRTC_MODULES_RTP_RTCP_SOURCE_MOCK_MOCK_RTP_RECEIVER_VIDEO_H_

#include "webrtc/modules/rtp_rtcp/source/rtp_receiver_video.h"

namespace webrtc {

class MockRTPReceiverVideo : public RTPReceiverVideo {
 public:
  MockRTPReceiverVideo() : RTPReceiverVideo(0, NULL, NULL) {}
  MOCK_METHOD1(ChangeUniqueId,
      void(const int32_t id));
  MOCK_METHOD3(ReceiveRecoveredPacketCallback,
      int32_t(WebRtcRTPHeader* rtpHeader,
              const uint8_t* payloadData,
              const uint16_t payloadDataLength));
  MOCK_METHOD3(CallbackOfReceivedPayloadData,
      int32_t(const uint8_t* payloadData,
              const uint16_t payloadSize,
              const WebRtcRTPHeader* rtpHeader));
  MOCK_CONST_METHOD0(TimeStamp,
      uint32_t());
  MOCK_CONST_METHOD0(SequenceNumber,
      uint16_t());
  MOCK_CONST_METHOD2(PayloadTypeToPayload,
      uint32_t(const uint8_t payloadType,
               ModuleRTPUtility::Payload*& payload));
  MOCK_CONST_METHOD2(RetransmitOfOldPacket,
      bool(const uint16_t sequenceNumber,
           const uint32_t rtpTimeStamp));
  MOCK_CONST_METHOD0(REDPayloadType,
      int8_t());
  MOCK_CONST_METHOD0(HaveNotReceivedPackets,
        bool());
};

}  // namespace webrtc

#endif  //WEBRTC_MODULES_RTP_RTCP_SOURCE_MOCK_MOCK_RTP_RECEIVER_VIDEO_H_
