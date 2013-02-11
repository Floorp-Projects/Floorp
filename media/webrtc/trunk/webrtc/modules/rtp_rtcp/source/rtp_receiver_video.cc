/*
 *  Copyright (c) 2012 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "rtp_receiver_video.h"

#include <cassert> //assert
#include <cstring>  // memcpy()
#include <math.h>

#include "critical_section_wrapper.h"
#include "receiver_fec.h"
#include "rtp_receiver.h"
#include "rtp_rtcp_impl.h"
#include "rtp_utility.h"
#include "trace.h"

namespace webrtc {
WebRtc_UWord32 BitRateBPS(WebRtc_UWord16 x )
{
    return (x & 0x3fff) * WebRtc_UWord32(pow(10.0f,(2 + (x >> 14))));
}

RTPReceiverVideo::RTPReceiverVideo(const WebRtc_Word32 id,
                                   RTPReceiver* parent,
                                   ModuleRtpRtcpImpl* owner)
    : _id(id),
      _parent(parent),
      _criticalSectionReceiverVideo(
          CriticalSectionWrapper::CreateCriticalSection()),
      _currentFecFrameDecoded(false),
      _receiveFEC(NULL) {
}

RTPReceiverVideo::~RTPReceiverVideo() {
    delete _criticalSectionReceiverVideo;
    delete _receiveFEC;
}

ModuleRTPUtility::Payload* RTPReceiverVideo::CreatePayloadType(
    const char payloadName[RTP_PAYLOAD_NAME_SIZE],
    const WebRtc_Word8 payloadType,
    const WebRtc_UWord32 frequency,
    const WebRtc_UWord8 channels,
    const WebRtc_UWord32 rate) {
  RtpVideoCodecTypes videoType = kRtpNoVideo;
  if (ModuleRTPUtility::StringCompare(payloadName, "VP8", 3)) {
    videoType = kRtpVp8Video;
  } else if (ModuleRTPUtility::StringCompare(payloadName, "I420", 4)) {
    videoType = kRtpNoVideo;
  } else if (ModuleRTPUtility::StringCompare(payloadName, "ULPFEC", 6)) {
    // store this
    if (_receiveFEC == NULL) {
      _receiveFEC = new ReceiverFEC(_id, this);
    }
    _receiveFEC->SetPayloadTypeFEC(payloadType);
    videoType = kRtpFecVideo;
  } else {
    return NULL;
  }
  ModuleRTPUtility::Payload* payload =  new ModuleRTPUtility::Payload;

  payload->name[RTP_PAYLOAD_NAME_SIZE - 1] = 0;
  strncpy(payload->name, payloadName, RTP_PAYLOAD_NAME_SIZE - 1);
  payload->typeSpecific.Video.videoCodecType = videoType;
  payload->typeSpecific.Video.maxRate = rate;
  payload->audio = false;
  return payload;
}

WebRtc_Word32 RTPReceiverVideo::ParseRtpPacket(
    WebRtcRTPHeader* rtpHeader,
    const ModuleRTPUtility::PayloadUnion& specificPayload,
    const bool isRed,
    const WebRtc_UWord8* packet,
    const WebRtc_UWord16 packetLength,
    const WebRtc_Word64 timestampMs) {
  const WebRtc_UWord8* payloadData =
      ModuleRTPUtility::GetPayloadData(rtpHeader, packet);
  const WebRtc_UWord16 payloadDataLength =
      ModuleRTPUtility::GetPayloadDataLength(rtpHeader, packetLength);
  return ParseVideoCodecSpecific(
      rtpHeader, payloadData, payloadDataLength,
      specificPayload.Video.videoCodecType, isRed, packet, packetLength,
      timestampMs);
}

WebRtc_Word32 RTPReceiverVideo::GetFrequencyHz() const {
  return kDefaultVideoFrequency;
}

RTPAliveType RTPReceiverVideo::ProcessDeadOrAlive(
      WebRtc_UWord16 lastPayloadLength) const {
  return kRtpDead;
}

bool RTPReceiverVideo::PayloadIsCompatible(
    const ModuleRTPUtility::Payload& payload,
    const WebRtc_UWord32 frequency,
    const WebRtc_UWord8 channels,
    const WebRtc_UWord32 rate) const {
  return !payload.audio;
}

void RTPReceiverVideo::UpdatePayloadRate(
    ModuleRTPUtility::Payload* payload,
    const WebRtc_UWord32 rate) const {
  payload->typeSpecific.Video.maxRate = rate;
}

WebRtc_Word32 RTPReceiverVideo::InvokeOnInitializeDecoder(
    RtpFeedback* callback,
    const WebRtc_Word32 id,
    const WebRtc_Word8 payloadType,
    const char payloadName[RTP_PAYLOAD_NAME_SIZE],
    const ModuleRTPUtility::PayloadUnion& specificPayload) const {
  // For video we just go with default values.
  if (-1 == callback->OnInitializeDecoder(
      id, payloadType, payloadName, kDefaultVideoFrequency, 1, 0)) {
    WEBRTC_TRACE(kTraceError, kTraceRtpRtcp, id,
                 "Failed to create video decoder for payload type:%d",
                 payloadType);
    return -1;
  }
  return 0;
}

// we have no critext when calling this
// we are not allowed to have any critsects when calling
// CallbackOfReceivedPayloadData
WebRtc_Word32 RTPReceiverVideo::ParseVideoCodecSpecific(
    WebRtcRTPHeader* rtpHeader,
    const WebRtc_UWord8* payloadData,
    const WebRtc_UWord16 payloadDataLength,
    const RtpVideoCodecTypes videoType,
    const bool isRED,
    const WebRtc_UWord8* incomingRtpPacket,
    const WebRtc_UWord16 incomingRtpPacketSize,
    const WebRtc_Word64 nowMS) {
  WebRtc_Word32 retVal = 0;

  _criticalSectionReceiverVideo->Enter();

  if (isRED) {
    if(_receiveFEC == NULL) {
      _criticalSectionReceiverVideo->Leave();
      return -1;
    }
    bool FECpacket = false;
    retVal = _receiveFEC->AddReceivedFECPacket(
        rtpHeader,
        incomingRtpPacket,
        payloadDataLength,
        FECpacket);
    if (retVal != -1) {
      retVal = _receiveFEC->ProcessReceivedFEC();
    }
    _criticalSectionReceiverVideo->Leave();

    if(retVal == 0 && FECpacket) {
      // Callback with the received FEC packet.
      // The normal packets are delivered after parsing.
      // This contains the original RTP packet header but with
      // empty payload and data length.
      rtpHeader->frameType = kFrameEmpty;
      // We need this for the routing.
      WebRtc_Word32 retVal = SetCodecType(videoType, rtpHeader);
      if(retVal != 0) {
        return retVal;
      }
      // Pass the length of FEC packets so that they can be accounted for in
      // the bandwidth estimator.
      retVal = _parent->CallbackOfReceivedPayloadData(NULL, payloadDataLength,
                                                      rtpHeader);
    }
  } else {
    // will leave the _criticalSectionReceiverVideo critsect
    retVal = ParseVideoCodecSpecificSwitch(rtpHeader,
                                           payloadData,
                                           payloadDataLength,
                                           videoType);
  }
  return retVal;
}

WebRtc_Word32 RTPReceiverVideo::BuildRTPheader(
    const WebRtcRTPHeader* rtpHeader,
    WebRtc_UWord8* dataBuffer) const {
  dataBuffer[0] = static_cast<WebRtc_UWord8>(0x80);  // version 2
  dataBuffer[1] = static_cast<WebRtc_UWord8>(rtpHeader->header.payloadType);
  if (rtpHeader->header.markerBit) {
    dataBuffer[1] |= kRtpMarkerBitMask;  // MarkerBit is 1
  }
  ModuleRTPUtility::AssignUWord16ToBuffer(dataBuffer + 2,
                                          rtpHeader->header.sequenceNumber);
  ModuleRTPUtility::AssignUWord32ToBuffer(dataBuffer + 4,
                                          rtpHeader->header.timestamp);
  ModuleRTPUtility::AssignUWord32ToBuffer(dataBuffer + 8,
                                          rtpHeader->header.ssrc);

  WebRtc_Word32 rtpHeaderLength = 12;

  // Add the CSRCs if any
  if (rtpHeader->header.numCSRCs > 0) {
    if (rtpHeader->header.numCSRCs > 16) {
      // error
      assert(false);
    }
    WebRtc_UWord8* ptr = &dataBuffer[rtpHeaderLength];
    for (WebRtc_UWord32 i = 0; i < rtpHeader->header.numCSRCs; ++i) {
      ModuleRTPUtility::AssignUWord32ToBuffer(ptr,
                                              rtpHeader->header.arrOfCSRCs[i]);
      ptr +=4;
    }
    dataBuffer[0] = (dataBuffer[0]&0xf0) | rtpHeader->header.numCSRCs;
    // Update length of header
    rtpHeaderLength += sizeof(WebRtc_UWord32)*rtpHeader->header.numCSRCs;
  }
  return rtpHeaderLength;
}

WebRtc_Word32 RTPReceiverVideo::ReceiveRecoveredPacketCallback(
    WebRtcRTPHeader* rtpHeader,
    const WebRtc_UWord8* payloadData,
    const WebRtc_UWord16 payloadDataLength) {
  // TODO(pwestin) Re-factor this to avoid the messy critsect handling.
  _criticalSectionReceiverVideo->Enter();

  _currentFecFrameDecoded = true;

  ModuleRTPUtility::Payload* payload = NULL;
  if (_parent->PayloadTypeToPayload(
      rtpHeader->header.payloadType, payload) != 0) {
    _criticalSectionReceiverVideo->Leave();
    return -1;
  }
  // here we can re-create the original lost packet so that we can use it for
  // the relay we need to re-create the RED header too
  WebRtc_UWord8 recoveredPacket[IP_PACKET_SIZE];
  WebRtc_UWord16 rtpHeaderLength = (WebRtc_UWord16)BuildRTPheader(
      rtpHeader, recoveredPacket);

  const WebRtc_UWord8 REDForFECHeaderLength = 1;

  // replace pltype
  recoveredPacket[1] &= 0x80;  // Reset.
  recoveredPacket[1] += _parent->REDPayloadType();

  // add RED header
  recoveredPacket[rtpHeaderLength] = rtpHeader->header.payloadType;
  // f-bit always 0

  memcpy(recoveredPacket + rtpHeaderLength + REDForFECHeaderLength, payloadData,
         payloadDataLength);

  return ParseVideoCodecSpecificSwitch(
      rtpHeader,
      payloadData,
      payloadDataLength,
      payload->typeSpecific.Video.videoCodecType);
}

WebRtc_Word32 RTPReceiverVideo::SetCodecType(const RtpVideoCodecTypes videoType,
                                             WebRtcRTPHeader* rtpHeader) const {
  switch (videoType) {
    case kRtpNoVideo:
      rtpHeader->type.Video.codec = kRTPVideoGeneric;
      break;
    case kRtpVp8Video:
      rtpHeader->type.Video.codec = kRTPVideoVP8;
      break;
    case kRtpFecVideo:
      rtpHeader->type.Video.codec = kRTPVideoFEC;
      break;
  }
  return 0;
}

WebRtc_Word32 RTPReceiverVideo::ParseVideoCodecSpecificSwitch(
    WebRtcRTPHeader* rtpHeader,
    const WebRtc_UWord8* payloadData,
    const WebRtc_UWord16 payloadDataLength,
    const RtpVideoCodecTypes videoType) {
  WebRtc_Word32 retVal = SetCodecType(videoType, rtpHeader);
  if (retVal != 0) {
    _criticalSectionReceiverVideo->Leave();
    return retVal;
  }
  WEBRTC_TRACE(kTraceStream, kTraceRtpRtcp, _id, "%s(timestamp:%u)",
               __FUNCTION__, rtpHeader->header.timestamp);

  // All receive functions release _criticalSectionReceiverVideo before
  // returning.
  switch (videoType) {
    case kRtpNoVideo:
      return ReceiveGenericCodec(rtpHeader, payloadData, payloadDataLength);
    case kRtpVp8Video:
      return ReceiveVp8Codec(rtpHeader, payloadData, payloadDataLength);
    case kRtpFecVideo:
      break;
  }
  _criticalSectionReceiverVideo->Leave();
  return -1;
}

WebRtc_Word32 RTPReceiverVideo::ReceiveVp8Codec(
    WebRtcRTPHeader* rtpHeader,
    const WebRtc_UWord8* payloadData,
    const WebRtc_UWord16 payloadDataLength) {
  bool success;
  ModuleRTPUtility::RTPPayload parsedPacket;
  if (payloadDataLength == 0) {
    success = true;
    parsedPacket.info.VP8.dataLength = 0;
  } else {
    ModuleRTPUtility::RTPPayloadParser rtpPayloadParser(kRtpVp8Video,
                                                        payloadData,
                                                        payloadDataLength,
                                                        _id);

    success = rtpPayloadParser.Parse(parsedPacket);
  }
  // from here down we only work on local data
  _criticalSectionReceiverVideo->Leave();

  if (!success) {
    return -1;
  }
  if (parsedPacket.info.VP8.dataLength == 0) {
    // we have an "empty" VP8 packet, it's ok, could be one way video
    // Inform the jitter buffer about this packet.
    rtpHeader->frameType = kFrameEmpty;
    if (_parent->CallbackOfReceivedPayloadData(NULL, 0, rtpHeader) != 0) {
      return -1;
    }
    return 0;
  }
  rtpHeader->frameType = (parsedPacket.frameType == ModuleRTPUtility::kIFrame) ?
      kVideoFrameKey : kVideoFrameDelta;

  RTPVideoHeaderVP8 *toHeader = &rtpHeader->type.Video.codecHeader.VP8;
  ModuleRTPUtility::RTPPayloadVP8 *fromHeader = &parsedPacket.info.VP8;

  rtpHeader->type.Video.isFirstPacket = fromHeader->beginningOfPartition
      && (fromHeader->partitionID == 0);
  toHeader->nonReference = fromHeader->nonReferenceFrame;
  toHeader->pictureId = fromHeader->hasPictureID ? fromHeader->pictureID :
      kNoPictureId;
  toHeader->tl0PicIdx = fromHeader->hasTl0PicIdx ? fromHeader->tl0PicIdx :
      kNoTl0PicIdx;
  if (fromHeader->hasTID) {
    toHeader->temporalIdx = fromHeader->tID;
    toHeader->layerSync = fromHeader->layerSync;
  } else {
    toHeader->temporalIdx = kNoTemporalIdx;
    toHeader->layerSync = false;
  }
  toHeader->keyIdx = fromHeader->hasKeyIdx ? fromHeader->keyIdx : kNoKeyIdx;

  toHeader->frameWidth = fromHeader->frameWidth;
  toHeader->frameHeight = fromHeader->frameHeight;

  toHeader->partitionId = fromHeader->partitionID;
  toHeader->beginningOfPartition = fromHeader->beginningOfPartition;

  if(_parent->CallbackOfReceivedPayloadData(parsedPacket.info.VP8.data,
                                            parsedPacket.info.VP8.dataLength,
                                            rtpHeader) != 0) {
    return -1;
  }
  return 0;
}


WebRtc_Word32 RTPReceiverVideo::ReceiveGenericCodec(
    WebRtcRTPHeader* rtpHeader,
    const WebRtc_UWord8* payloadData,
    const WebRtc_UWord16 payloadDataLength) {
  rtpHeader->frameType = kVideoFrameKey;

  bool isFirstPacketInFrame =
      (_parent->SequenceNumber() + 1) == rtpHeader->header.sequenceNumber &&
      (_parent->TimeStamp() != rtpHeader->header.timestamp);

  if (isFirstPacketInFrame || _parent->HaveNotReceivedPackets()) {
    rtpHeader->type.Video.isFirstPacket = true;
  }
  _criticalSectionReceiverVideo->Leave();

  if (_parent->CallbackOfReceivedPayloadData(payloadData, payloadDataLength,
                                             rtpHeader) != 0) {
    return -1;
  }
  return 0;
}
} // namespace webrtc
