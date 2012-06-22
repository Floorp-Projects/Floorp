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
#include "rtp_rtcp_impl.h"
#include "rtp_utility.h"
#include "trace.h"

namespace webrtc {
WebRtc_UWord32 BitRateBPS(WebRtc_UWord16 x )
{
    return (x & 0x3fff) * WebRtc_UWord32(pow(10.0f,(2 + (x >> 14))));
}

RTPReceiverVideo::RTPReceiverVideo():
    _id(0),
    _rtpRtcp(NULL),
    _criticalSectionFeedback(CriticalSectionWrapper::CreateCriticalSection()),
    _cbVideoFeedback(NULL),
    _criticalSectionReceiverVideo(
        CriticalSectionWrapper::CreateCriticalSection()),
    _completeFrame(false),
    _packetStartTimeMs(0),
    _receivedBW(),
    _estimatedBW(0),
    _currentFecFrameDecoded(false),
    _receiveFEC(NULL),
    _overUseDetector(),
    _videoBitRate(),
    _lastBitRateChange(0),
    _packetOverHead(28)
{
    memset(_receivedBW, 0,sizeof(_receivedBW));
}

RTPReceiverVideo::RTPReceiverVideo(const WebRtc_Word32 id,
                                   ModuleRtpRtcpImpl* owner):
    _id(id),
    _rtpRtcp(owner),
    _criticalSectionFeedback(CriticalSectionWrapper::CreateCriticalSection()),
    _cbVideoFeedback(NULL),
    _criticalSectionReceiverVideo(
        CriticalSectionWrapper::CreateCriticalSection()),
    _completeFrame(false),
    _packetStartTimeMs(0),
    _receivedBW(),
    _estimatedBW(0),
    _currentFecFrameDecoded(false),
    _receiveFEC(NULL),
    _overUseDetector(),
    _videoBitRate(),
    _lastBitRateChange(0),
    _packetOverHead(28)
{
    memset(_receivedBW, 0,sizeof(_receivedBW));
}

RTPReceiverVideo::~RTPReceiverVideo()
{
    delete _criticalSectionFeedback;
    delete _criticalSectionReceiverVideo;
    delete _receiveFEC;
}

WebRtc_Word32
RTPReceiverVideo::Init()
{
    _completeFrame = false;
    _packetStartTimeMs = 0;
    _estimatedBW = 0;
    _currentFecFrameDecoded = false;
    _packetOverHead = 28;
    for (int i = 0; i < BW_HISTORY_SIZE; i++)
    {
        _receivedBW[i] = 0;
    }
    ResetOverUseDetector();
    return 0;
}

void
RTPReceiverVideo::ChangeUniqueId(const WebRtc_Word32 id)
{
    _id = id;
}

WebRtc_Word32
RTPReceiverVideo::RegisterIncomingVideoCallback(RtpVideoFeedback* incomingMessagesCallback)
{
    CriticalSectionScoped lock(_criticalSectionFeedback);
    _cbVideoFeedback = incomingMessagesCallback;
    return 0;
}

void
RTPReceiverVideo::UpdateBandwidthManagement(const WebRtc_UWord32 bitrateBps,
                                            const WebRtc_UWord8 fractionLost,
                                            const WebRtc_UWord16 roundTripTimeMs)
{
    CriticalSectionScoped lock(_criticalSectionFeedback);
    if(_cbVideoFeedback)
    {
        _cbVideoFeedback->OnNetworkChanged(_id,
                                           bitrateBps,
                                           fractionLost,
                                           roundTripTimeMs);
    }
}

ModuleRTPUtility::Payload* RTPReceiverVideo::RegisterReceiveVideoPayload(
    const char payloadName[RTP_PAYLOAD_NAME_SIZE],
    const WebRtc_Word8 payloadType,
    const WebRtc_UWord32 maxRate) {
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
  payload->typeSpecific.Video.maxRate = maxRate;
  payload->audio = false;
  return payload;
}

void RTPReceiverVideo::ResetOverUseDetector()
{
    _overUseDetector.Reset();
    _videoBitRate.Init();
    _lastBitRateChange = 0;
}

// called under _criticalSectionReceiverVideo
WebRtc_UWord16
RTPReceiverVideo::EstimateBandwidth(const WebRtc_UWord16 bandwidth)
{
    // received fragments
    // estimate BW

    WebRtc_UWord16 bwSort[BW_HISTORY_SIZE];
    for(int i = 0; i < BW_HISTORY_SIZE-1; i++)
    {
        _receivedBW[i] = _receivedBW[i+1];
        bwSort[i] = _receivedBW[i+1];
    }
    _receivedBW[BW_HISTORY_SIZE-1] = bandwidth;
    bwSort[BW_HISTORY_SIZE-1] = bandwidth;

    WebRtc_UWord16 temp;
    for (int i = BW_HISTORY_SIZE-1; i >= 0; i--)
    {
        for (int j = 1; j <= i; j++)
        {
            if (bwSort[j-1] > bwSort[j])
            {
                temp = bwSort[j-1];
                bwSort[j-1] = bwSort[j];
                bwSort[j] = temp;
            }
        }
    }
    int zeroCount = 0;
    for (; zeroCount < BW_HISTORY_SIZE; zeroCount++)
    {
        if (bwSort[zeroCount]!= 0)
        {
            break;
        }
    }
    WebRtc_UWord32 indexMedian = (BW_HISTORY_SIZE -1) - (BW_HISTORY_SIZE-zeroCount)/2;
    WebRtc_UWord16 bandwidthMedian = bwSort[indexMedian];

    if (bandwidthMedian > 0)
    {
        if (_estimatedBW == bandwidth)
        {
            // don't trigger a callback
            bandwidthMedian = 0;
        } else
        {
            _estimatedBW = bandwidthMedian;
        }
    } else
    {
        // can't be negative
        bandwidthMedian = 0;
    }

    return bandwidthMedian;
}

// we have no critext when calling this
// we are not allowed to have any critsects when calling CallbackOfReceivedPayloadData
WebRtc_Word32
RTPReceiverVideo::ParseVideoCodecSpecific(WebRtcRTPHeader* rtpHeader,
                                          const WebRtc_UWord8* payloadData,
                                          const WebRtc_UWord16 payloadDataLength,
                                          const RtpVideoCodecTypes videoType,
                                          const bool isRED,
                                          const WebRtc_UWord8* incomingRtpPacket,
                                          const WebRtc_UWord16 incomingRtpPacketSize,
                                          const WebRtc_Word64 nowMS)
{
    WebRtc_Word32 retVal = 0;

    _criticalSectionReceiverVideo->Enter();

    _videoBitRate.Update(payloadDataLength + rtpHeader->header.paddingLength,
                         nowMS);

    // Add headers, ideally we would like to include for instance
    // Ethernet header here as well.
    const WebRtc_UWord16 packetSize = payloadDataLength + _packetOverHead +
        rtpHeader->header.headerLength + rtpHeader->header.paddingLength;
    _overUseDetector.Update(*rtpHeader, packetSize, nowMS);

    if (isRED)
    {
        if(_receiveFEC == NULL)
        {
            _criticalSectionReceiverVideo->Leave();
            return -1;
        }
        bool FECpacket = false;
        retVal = _receiveFEC->AddReceivedFECPacket(
            rtpHeader,
            incomingRtpPacket,
            payloadDataLength,
            FECpacket);
        if (retVal != -1)
          retVal = _receiveFEC->ProcessReceivedFEC();
        _criticalSectionReceiverVideo->Leave();

        if(retVal == 0 && FECpacket)
        {
            // Callback with the received FEC packet.
            // The normal packets are delivered after parsing.
            // This contains the original RTP packet header but with
            // empty payload and data length.
            rtpHeader->frameType = kFrameEmpty;
            // We need this for the routing.
            WebRtc_Word32 retVal = SetCodecType(videoType, rtpHeader);
            if(retVal != 0)
            {
                return retVal;
            }
            retVal = CallbackOfReceivedPayloadData(NULL, 0, rtpHeader);
        }
    }else
    {
        // will leave the _criticalSectionReceiverVideo critsect
        retVal = ParseVideoCodecSpecificSwitch(rtpHeader,
                                               payloadData,
                                               payloadDataLength,
                                               videoType);
    }

    // Update the remote rate control object and update the overuse
    // detector with the current rate control region.
    _criticalSectionReceiverVideo->Enter();
    const RateControlInput input(_overUseDetector.State(),
                                 _videoBitRate.BitRate(nowMS),
                                 _overUseDetector.NoiseVar());
    _criticalSectionReceiverVideo->Leave();

    // Call the callback outside critical section
    if (_rtpRtcp) {
      const RateControlRegion region = _rtpRtcp->OnOverUseStateUpdate(input);

      _criticalSectionReceiverVideo->Enter();
      _overUseDetector.SetRateControlRegion(region);
      _criticalSectionReceiverVideo->Leave();
    }

    return retVal;
}

WebRtc_Word32
RTPReceiverVideo::BuildRTPheader(const WebRtcRTPHeader* rtpHeader,
                                 WebRtc_UWord8* dataBuffer) const
{
    dataBuffer[0] = static_cast<WebRtc_UWord8>(0x80);            // version 2
    dataBuffer[1] = static_cast<WebRtc_UWord8>(rtpHeader->header.payloadType);
    if (rtpHeader->header.markerBit)
    {
        dataBuffer[1] |= kRtpMarkerBitMask;  // MarkerBit is 1
    }

    ModuleRTPUtility::AssignUWord16ToBuffer(dataBuffer+2, rtpHeader->header.sequenceNumber);
    ModuleRTPUtility::AssignUWord32ToBuffer(dataBuffer+4, rtpHeader->header.timestamp);
    ModuleRTPUtility::AssignUWord32ToBuffer(dataBuffer+8, rtpHeader->header.ssrc);

    WebRtc_Word32 rtpHeaderLength = 12;

    // Add the CSRCs if any
    if (rtpHeader->header.numCSRCs > 0)
    {
        if(rtpHeader->header.numCSRCs > 16)
        {
            // error
            assert(false);
        }
        WebRtc_UWord8* ptr = &dataBuffer[rtpHeaderLength];
        for (WebRtc_UWord32 i = 0; i < rtpHeader->header.numCSRCs; ++i)
        {
            ModuleRTPUtility::AssignUWord32ToBuffer(ptr, rtpHeader->header.arrOfCSRCs[i]);
            ptr +=4;
        }
        dataBuffer[0] = (dataBuffer[0]&0xf0) | rtpHeader->header.numCSRCs;

        // Update length of header
        rtpHeaderLength += sizeof(WebRtc_UWord32)*rtpHeader->header.numCSRCs;
    }
    return rtpHeaderLength;
}

WebRtc_Word32
RTPReceiverVideo::ReceiveRecoveredPacketCallback(WebRtcRTPHeader* rtpHeader,
                                                 const WebRtc_UWord8* payloadData,
                                                 const WebRtc_UWord16 payloadDataLength)
{
     _criticalSectionReceiverVideo->Enter();

    _currentFecFrameDecoded = true;

    ModuleRTPUtility::Payload* payload = NULL;
    if (PayloadTypeToPayload(rtpHeader->header.payloadType, payload) != 0)
    {
        return -1;
    }
    // here we can re-create the original lost packet so that we can use it for the relay
    // we need to re-create the RED header too
    WebRtc_UWord8 recoveredPacket[IP_PACKET_SIZE];
    WebRtc_UWord16 rtpHeaderLength = (WebRtc_UWord16)BuildRTPheader(rtpHeader, recoveredPacket);

    const WebRtc_UWord8 REDForFECHeaderLength = 1;

    // replace pltype
    recoveredPacket[1] &= 0x80;             // reset
    recoveredPacket[1] += REDPayloadType(); // replace with RED payload type

    // add RED header
    recoveredPacket[rtpHeaderLength] = rtpHeader->header.payloadType; // f-bit always 0

    memcpy(recoveredPacket + rtpHeaderLength + REDForFECHeaderLength, payloadData, payloadDataLength);

    return ParseVideoCodecSpecificSwitch(rtpHeader,
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

WebRtc_Word32
RTPReceiverVideo::ReceiveVp8Codec(WebRtcRTPHeader* rtpHeader,
                                  const WebRtc_UWord8* payloadData,
                                  const WebRtc_UWord16 payloadDataLength)
{
    bool success;
    ModuleRTPUtility::RTPPayload parsedPacket;
    if (payloadDataLength == 0)
    {
        success = true;
        parsedPacket.info.VP8.dataLength = 0;
    } else
    {
        ModuleRTPUtility::RTPPayloadParser rtpPayloadParser(kRtpVp8Video,
                                                            payloadData,
                                                            payloadDataLength,
                                                           _id);
 
        success = rtpPayloadParser.Parse(parsedPacket);
    }
    // from here down we only work on local data
    _criticalSectionReceiverVideo->Leave();

    if (!success)
    {
        return -1;
    }
    if (parsedPacket.info.VP8.dataLength == 0)
    {
        // we have an "empty" VP8 packet, it's ok, could be one way video
        // Inform the jitter buffer about this packet.
        rtpHeader->frameType = kFrameEmpty;
        if (CallbackOfReceivedPayloadData(NULL, 0, rtpHeader) != 0)
        {
            return -1;
        }
        return 0;
    }
    rtpHeader->frameType = (parsedPacket.frameType == ModuleRTPUtility::kIFrame) ? kVideoFrameKey : kVideoFrameDelta;

    RTPVideoHeaderVP8 *toHeader = &rtpHeader->type.Video.codecHeader.VP8;
    ModuleRTPUtility::RTPPayloadVP8 *fromHeader = &parsedPacket.info.VP8;

    rtpHeader->type.Video.isFirstPacket = fromHeader->beginningOfPartition
        && (fromHeader->partitionID == 0);
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

    if(CallbackOfReceivedPayloadData(parsedPacket.info.VP8.data,
                                     parsedPacket.info.VP8.dataLength,
                                     rtpHeader) != 0)
    {
        return -1;
    }
    return 0;
}


WebRtc_Word32
RTPReceiverVideo::ReceiveGenericCodec(WebRtcRTPHeader* rtpHeader,
                                      const WebRtc_UWord8* payloadData,
                                      const WebRtc_UWord16 payloadDataLength)
{
    rtpHeader->frameType = kVideoFrameKey;

    if(((SequenceNumber() + 1) == rtpHeader->header.sequenceNumber) &&
        (TimeStamp() != rtpHeader->header.timestamp))
    {
        rtpHeader->type.Video.isFirstPacket = true;
    }
    _criticalSectionReceiverVideo->Leave();

    if(CallbackOfReceivedPayloadData(payloadData, payloadDataLength, rtpHeader) != 0)
    {
        return -1;
    }
    return 0;
}

void RTPReceiverVideo::SetPacketOverHead(WebRtc_UWord16 packetOverHead)
{
    _packetOverHead = packetOverHead;
}
} // namespace webrtc
