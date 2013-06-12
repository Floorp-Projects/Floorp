/*
 *  Copyright (c) 2012 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef WEBRTC_MODULES_RTP_RTCP_SOURCE_RTP_RECEIVER_VIDEO_H_
#define WEBRTC_MODULES_RTP_RTCP_SOURCE_RTP_RECEIVER_VIDEO_H_

#include "bitrate.h"
#include "rtp_receiver.h"
#include "rtp_receiver_strategy.h"
#include "rtp_rtcp_defines.h"
#include "rtp_utility.h"
#include "scoped_ptr.h"
#include "typedefs.h"

namespace webrtc {
class CriticalSectionWrapper;
class ModuleRtpRtcpImpl;
class ReceiverFEC;

class RTPReceiverVideo : public RTPReceiverStrategy {
 public:
  RTPReceiverVideo(const WebRtc_Word32 id,
                   RTPReceiver* parent,
                   ModuleRtpRtcpImpl* owner);

  virtual ~RTPReceiverVideo();

  WebRtc_Word32 ParseRtpPacket(
      WebRtcRTPHeader* rtp_header,
      const ModuleRTPUtility::PayloadUnion& specificPayload,
      const bool is_red,
      const WebRtc_UWord8* packet,
      const WebRtc_UWord16 packet_length,
      const WebRtc_Word64 timestamp);

  WebRtc_Word32 GetFrequencyHz() const;

  RTPAliveType ProcessDeadOrAlive(WebRtc_UWord16 lastPayloadLength) const;

  bool PayloadIsCompatible(
      const ModuleRTPUtility::Payload& payload,
      const WebRtc_UWord32 frequency,
      const WebRtc_UWord8 channels,
      const WebRtc_UWord32 rate) const;

  void UpdatePayloadRate(
      ModuleRTPUtility::Payload* payload,
      const WebRtc_UWord32 rate) const;

  ModuleRTPUtility::Payload* CreatePayloadType(
      const char payloadName[RTP_PAYLOAD_NAME_SIZE],
      const WebRtc_Word8 payloadType,
      const WebRtc_UWord32 frequency,
      const WebRtc_UWord8 channels,
      const WebRtc_UWord32 rate);

  WebRtc_Word32 InvokeOnInitializeDecoder(
      RtpFeedback* callback,
      const WebRtc_Word32 id,
      const WebRtc_Word8 payloadType,
      const char payloadName[RTP_PAYLOAD_NAME_SIZE],
      const ModuleRTPUtility::PayloadUnion& specificPayload) const;

  virtual WebRtc_Word32 ReceiveRecoveredPacketCallback(
      WebRtcRTPHeader* rtpHeader,
      const WebRtc_UWord8* payloadData,
      const WebRtc_UWord16 payloadDataLength);

  void SetPacketOverHead(WebRtc_UWord16 packetOverHead);

 protected:
  WebRtc_Word32 SetCodecType(const RtpVideoCodecTypes videoType,
                             WebRtcRTPHeader* rtpHeader) const;

  WebRtc_Word32 ParseVideoCodecSpecificSwitch(
      WebRtcRTPHeader* rtpHeader,
      const WebRtc_UWord8* payloadData,
      const WebRtc_UWord16 payloadDataLength,
      const RtpVideoCodecTypes videoType);

  WebRtc_Word32 ReceiveGenericCodec(WebRtcRTPHeader *rtpHeader,
                                    const WebRtc_UWord8* payloadData,
                                    const WebRtc_UWord16 payloadDataLength);

  WebRtc_Word32 ReceiveVp8Codec(WebRtcRTPHeader *rtpHeader,
                                const WebRtc_UWord8* payloadData,
                                const WebRtc_UWord16 payloadDataLength);

  WebRtc_Word32 BuildRTPheader(const WebRtcRTPHeader* rtpHeader,
                               WebRtc_UWord8* dataBuffer) const;

 private:
  WebRtc_Word32 ParseVideoCodecSpecific(
      WebRtcRTPHeader* rtpHeader,
      const WebRtc_UWord8* payloadData,
      const WebRtc_UWord16 payloadDataLength,
      const RtpVideoCodecTypes videoType,
      const bool isRED,
      const WebRtc_UWord8* incomingRtpPacket,
      const WebRtc_UWord16 incomingRtpPacketSize,
      const WebRtc_Word64 nowMS);

  WebRtc_Word32             _id;
  RTPReceiver*              _parent;

  CriticalSectionWrapper*   _criticalSectionReceiverVideo;

  // FEC
  bool                      _currentFecFrameDecoded;
  ReceiverFEC*              _receiveFEC;
};
} // namespace webrtc
#endif // WEBRTC_MODULES_RTP_RTCP_SOURCE_RTP_RECEIVER_VIDEO_H_
