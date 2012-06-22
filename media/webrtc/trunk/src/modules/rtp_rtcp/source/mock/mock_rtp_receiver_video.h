/*
 *  Copyright (c) 2012 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "modules/rtp_rtcp/source/rtp_receiver_video.h"

namespace webrtc {

class MockRTPReceiverVideo : public RTPReceiverVideo {
 public:
  MOCK_METHOD1(ChangeUniqueId,
      void(const WebRtc_Word32 id));
  MOCK_METHOD3(ReceiveRecoveredPacketCallback,
      WebRtc_Word32(WebRtcRTPHeader* rtpHeader,
                    const WebRtc_UWord8* payloadData,
                    const WebRtc_UWord16 payloadDataLength));
  MOCK_METHOD3(CallbackOfReceivedPayloadData,
      WebRtc_Word32(const WebRtc_UWord8* payloadData,
                    const WebRtc_UWord16 payloadSize,
                    const WebRtcRTPHeader* rtpHeader));
  MOCK_CONST_METHOD0(TimeStamp,
      WebRtc_UWord32());
  MOCK_CONST_METHOD0(SequenceNumber,
      WebRtc_UWord16());
  MOCK_CONST_METHOD2(PayloadTypeToPayload,
      WebRtc_UWord32(const WebRtc_UWord8 payloadType,
                     ModuleRTPUtility::Payload*& payload));
  MOCK_CONST_METHOD2(RetransmitOfOldPacket,
      bool(const WebRtc_UWord16 sequenceNumber,
           const WebRtc_UWord32 rtpTimeStamp));
  MOCK_CONST_METHOD0(REDPayloadType,
      WebRtc_Word8());
};

}  // namespace webrtc
