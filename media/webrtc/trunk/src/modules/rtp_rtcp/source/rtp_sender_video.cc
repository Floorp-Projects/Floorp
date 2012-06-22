/*
 *  Copyright (c) 2012 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "rtp_sender_video.h"

#include "critical_section_wrapper.h"
#include "trace.h"

#include "rtp_utility.h"

#include <string.h> // memcpy
#include <cassert>  // assert
#include <cstdlib>  // srand

#include "producer_fec.h"
#include "rtp_format_vp8.h"

namespace webrtc {
enum { REDForFECHeaderLength = 1 };

struct RtpPacket {
  WebRtc_UWord16 rtpHeaderLength;
  ForwardErrorCorrection::Packet* pkt;
};

RTPSenderVideo::RTPSenderVideo(const WebRtc_Word32 id,
                               RtpRtcpClock* clock,
                               RTPSenderInterface* rtpSender) :
    _id(id),
    _rtpSender(*rtpSender),
    _sendVideoCritsect(CriticalSectionWrapper::CreateCriticalSection()),

    _videoType(kRtpNoVideo),
    _videoCodecInformation(NULL),
    _maxBitrate(0),
    _retransmissionSettings(kRetransmitBaseLayer),

    // Generic FEC
    _fec(id),
    _fecEnabled(false),
    _payloadTypeRED(-1),
    _payloadTypeFEC(-1),
    _numberFirstPartition(0),
    delta_fec_params_(),
    key_fec_params_(),
    producer_fec_(&_fec),
    _fecOverheadRate(clock),
    _videoBitrate(clock) {
  memset(&delta_fec_params_, 0, sizeof(delta_fec_params_));
  memset(&key_fec_params_, 0, sizeof(key_fec_params_));
  delta_fec_params_.max_fec_frames = key_fec_params_.max_fec_frames = 1;
}

RTPSenderVideo::~RTPSenderVideo()
{
    if(_videoCodecInformation)
    {
        delete _videoCodecInformation;
    }
    delete _sendVideoCritsect;
}

WebRtc_Word32
RTPSenderVideo::Init()
{
    CriticalSectionScoped cs(_sendVideoCritsect);

    _retransmissionSettings = kRetransmitBaseLayer;
    _fecEnabled = false;
    _payloadTypeRED = -1;
    _payloadTypeFEC = -1;
    _numberFirstPartition = 0;
    memset(&delta_fec_params_, 0, sizeof(delta_fec_params_));
    memset(&key_fec_params_, 0, sizeof(key_fec_params_));
    delta_fec_params_.max_fec_frames = key_fec_params_.max_fec_frames = 1;
    _fecOverheadRate.Init();
    return 0;
}

void
RTPSenderVideo::ChangeUniqueId(const WebRtc_Word32 id)
{
    _id = id;
}

void
RTPSenderVideo::SetVideoCodecType(RtpVideoCodecTypes videoType)
{
    CriticalSectionScoped cs(_sendVideoCritsect);
    _videoType = videoType;
}

RtpVideoCodecTypes
RTPSenderVideo::VideoCodecType() const
{
    return _videoType;
}

WebRtc_Word32 RTPSenderVideo::RegisterVideoPayload(
    const char payloadName[RTP_PAYLOAD_NAME_SIZE],
    const WebRtc_Word8 payloadType,
    const WebRtc_UWord32 maxBitRate,
    ModuleRTPUtility::Payload*& payload) {
  CriticalSectionScoped cs(_sendVideoCritsect);

  RtpVideoCodecTypes videoType = kRtpNoVideo;
  if (ModuleRTPUtility::StringCompare(payloadName, "VP8",3)) {
    videoType = kRtpVp8Video;
  } else if (ModuleRTPUtility::StringCompare(payloadName, "I420", 4)) {
    videoType = kRtpNoVideo;
  } else {
    return -1;
  }
  payload = new ModuleRTPUtility::Payload;
  payload->name[RTP_PAYLOAD_NAME_SIZE - 1] = 0;
  strncpy(payload->name, payloadName, RTP_PAYLOAD_NAME_SIZE - 1);
  payload->typeSpecific.Video.videoCodecType = videoType;
  payload->typeSpecific.Video.maxRate = maxBitRate;
  payload->audio = false;
  return 0;
}

WebRtc_Word32
RTPSenderVideo::SendVideoPacket(const WebRtc_UWord8* data_buffer,
                                const WebRtc_UWord16 payload_length,
                                const WebRtc_UWord16 rtp_header_length,
                                StorageType storage,
                                bool protect) {
  if(_fecEnabled) {
    int ret = 0;
    int fec_overhead_sent = 0;
    int video_sent = 0;

    RedPacket* red_packet = producer_fec_.BuildRedPacket(data_buffer,
                                                         payload_length,
                                                         rtp_header_length,
                                                         _payloadTypeRED);
    // Sending the media packet with RED header.
    int packet_success = _rtpSender.SendToNetwork(
        red_packet->data(),
        red_packet->length() - rtp_header_length,
        rtp_header_length,
        storage);

    ret |= packet_success;

    if (packet_success == 0) {
      video_sent += red_packet->length();
    }
    delete red_packet;
    red_packet = NULL;

    if (protect) {
      ret = producer_fec_.AddRtpPacketAndGenerateFec(data_buffer,
                                                     payload_length,
                                                     rtp_header_length);
      if (ret != 0)
        return ret;
    }

    while (producer_fec_.FecAvailable()) {
      red_packet = producer_fec_.GetFecPacket(
          _payloadTypeRED,
          _payloadTypeFEC,
          _rtpSender.IncrementSequenceNumber());
      StorageType storage = kDontRetransmit;
      if (_retransmissionSettings & kRetransmitFECPackets) {
        storage = kAllowRetransmission;
      }
      // Sending FEC packet with RED header.
      int packet_success = _rtpSender.SendToNetwork(
          red_packet->data(),
          red_packet->length() - rtp_header_length,
          rtp_header_length,
          storage);

      ret |= packet_success;

      if (packet_success == 0) {
        fec_overhead_sent += red_packet->length();
      }
      delete red_packet;
      red_packet = NULL;
    }
    _videoBitrate.Update(video_sent);
    _fecOverheadRate.Update(fec_overhead_sent);
    return ret;
  }
  int ret = _rtpSender.SendToNetwork(data_buffer,
                                     payload_length,
                                     rtp_header_length,
                                     storage);
  if (ret == 0) {
    _videoBitrate.Update(payload_length + rtp_header_length);
  }
  return ret;
}

WebRtc_Word32
RTPSenderVideo::SendRTPIntraRequest()
{
    // RFC 2032
    // 5.2.1.  Full intra-frame Request (FIR) packet

    WebRtc_UWord16 length = 8;
    WebRtc_UWord8 data[8];
    data[0] = 0x80;
    data[1] = 192;
    data[2] = 0;
    data[3] = 1; // length

    ModuleRTPUtility::AssignUWord32ToBuffer(data+4, _rtpSender.SSRC());

    return _rtpSender.SendToNetwork(data, 0, length, kAllowRetransmission);
}

WebRtc_Word32
RTPSenderVideo::SetGenericFECStatus(const bool enable,
                                    const WebRtc_UWord8 payloadTypeRED,
                                    const WebRtc_UWord8 payloadTypeFEC)
{
    _fecEnabled = enable;
    _payloadTypeRED = payloadTypeRED;
    _payloadTypeFEC = payloadTypeFEC;
    memset(&delta_fec_params_, 0, sizeof(delta_fec_params_));
    memset(&key_fec_params_, 0, sizeof(key_fec_params_));
    delta_fec_params_.max_fec_frames = key_fec_params_.max_fec_frames = 1;
    return 0;
}

WebRtc_Word32
RTPSenderVideo::GenericFECStatus(bool& enable,
                                 WebRtc_UWord8& payloadTypeRED,
                                 WebRtc_UWord8& payloadTypeFEC) const
{
    enable = _fecEnabled;
    payloadTypeRED = _payloadTypeRED;
    payloadTypeFEC = _payloadTypeFEC;
    return 0;
}

WebRtc_UWord16
RTPSenderVideo::FECPacketOverhead() const
{
    if (_fecEnabled)
    {
        return ForwardErrorCorrection::PacketOverhead() +
            REDForFECHeaderLength;
    }
    return 0;
}

WebRtc_Word32 RTPSenderVideo::SetFecParameters(
    const FecProtectionParams* delta_params,
    const FecProtectionParams* key_params) {
  assert(delta_params);
  assert(key_params);
  delta_fec_params_ = *delta_params;
  key_fec_params_ = *key_params;
  return 0;
}

WebRtc_Word32
RTPSenderVideo::SendVideo(const RtpVideoCodecTypes videoType,
                          const FrameType frameType,
                          const WebRtc_Word8 payloadType,
                          const WebRtc_UWord32 captureTimeStamp,
                          const WebRtc_UWord8* payloadData,
                          const WebRtc_UWord32 payloadSize,
                          const RTPFragmentationHeader* fragmentation,
                          VideoCodecInformation* codecInfo,
                          const RTPVideoTypeHeader* rtpTypeHdr)
{
    if( payloadSize == 0)
    {
        return -1;
    }

    if (frameType == kVideoFrameKey) {
      producer_fec_.SetFecParameters(&key_fec_params_,
                                     _numberFirstPartition);
    } else {
      producer_fec_.SetFecParameters(&delta_fec_params_,
                                     _numberFirstPartition);
    }

    // Default setting for number of first partition packets:
    // Will be extracted in SendVP8 for VP8 codec; other codecs use 0
    _numberFirstPartition = 0;

    WebRtc_Word32 retVal = -1;
    switch(videoType)
    {
    case kRtpNoVideo:
        retVal = SendGeneric(payloadType,captureTimeStamp, payloadData,
                             payloadSize);
        break;
    case kRtpVp8Video:
        retVal = SendVP8(frameType, payloadType, captureTimeStamp,
                payloadData, payloadSize, fragmentation, rtpTypeHdr);
        break;
    default:
        assert(false);
        break;
    }
    if(retVal <= 0)
    {
        return retVal;
    }
    WEBRTC_TRACE(kTraceStream, kTraceRtpRtcp, _id, "%s(timestamp:%u)",
                 __FUNCTION__, captureTimeStamp);
    return 0;
}

WebRtc_Word32
RTPSenderVideo::SendGeneric(const WebRtc_Word8 payloadType,
                            const WebRtc_UWord32 captureTimeStamp,
                            const WebRtc_UWord8* payloadData,
                            const WebRtc_UWord32 payloadSize)
{
    WebRtc_UWord16 payloadBytesInPacket = 0;
    WebRtc_UWord32 bytesSent = 0;
    WebRtc_Word32 payloadBytesToSend = payloadSize;

    const WebRtc_UWord8* data = payloadData;
    WebRtc_UWord16 rtpHeaderLength = _rtpSender.RTPHeaderLength();
    WebRtc_UWord16 maxLength = _rtpSender.MaxPayloadLength() -
        FECPacketOverhead() - rtpHeaderLength;
    WebRtc_UWord8 dataBuffer[IP_PACKET_SIZE];

    // Fragment packet into packets of max MaxPayloadLength bytes payload.
    while (payloadBytesToSend > 0)
    {
        if (payloadBytesToSend > maxLength)
        {
            payloadBytesInPacket = maxLength;
            payloadBytesToSend -= payloadBytesInPacket;
            // MarkerBit is 0
            if(_rtpSender.BuildRTPheader(dataBuffer,
                                         payloadType,
                                         false,
                                         captureTimeStamp) != rtpHeaderLength)
            {
                return -1;
           }
        }
        else
        {
            payloadBytesInPacket = (WebRtc_UWord16)payloadBytesToSend;
            payloadBytesToSend = 0;
            // MarkerBit is 1
            if(_rtpSender.BuildRTPheader(dataBuffer, payloadType, true,
                                         captureTimeStamp) != rtpHeaderLength)
            {
                return -1;
            }
        }

        // Put payload in packet
        memcpy(&dataBuffer[rtpHeaderLength], &data[bytesSent],
               payloadBytesInPacket);
        bytesSent += payloadBytesInPacket;

        if(-1 == SendVideoPacket(dataBuffer,
                                 payloadBytesInPacket,
                                 rtpHeaderLength,
                                 kAllowRetransmission,
                                 true))
        {
            return -1;
        }
    }
    return 0;
}

VideoCodecInformation*
RTPSenderVideo::CodecInformationVideo()
{
    return _videoCodecInformation;
}

void
RTPSenderVideo::SetMaxConfiguredBitrateVideo(const WebRtc_UWord32 maxBitrate)
{
    _maxBitrate = maxBitrate;
}

WebRtc_UWord32
RTPSenderVideo::MaxConfiguredBitrateVideo() const
{
    return _maxBitrate;
}

WebRtc_Word32
RTPSenderVideo::SendVP8(const FrameType frameType,
                        const WebRtc_Word8 payloadType,
                        const WebRtc_UWord32 captureTimeStamp,
                        const WebRtc_UWord8* payloadData,
                        const WebRtc_UWord32 payloadSize,
                        const RTPFragmentationHeader* fragmentation,
                        const RTPVideoTypeHeader* rtpTypeHdr)
{
    const WebRtc_UWord16 rtpHeaderLength = _rtpSender.RTPHeaderLength();

    WebRtc_Word32 payloadBytesToSend = payloadSize;
    const WebRtc_UWord8* data = payloadData;

    WebRtc_UWord16 maxPayloadLengthVP8 = _rtpSender.MaxDataPayloadLength();

    assert(rtpTypeHdr);
    // Initialize disregarding partition boundaries: this will use kEqualSize
    // packetization mode, which produces ~equal size packets for each frame.
    RtpFormatVp8 packetizer(data, payloadBytesToSend, rtpTypeHdr->VP8,
                            maxPayloadLengthVP8);

    StorageType storage = kAllowRetransmission;
    if (rtpTypeHdr->VP8.temporalIdx == 0 &&
        !(_retransmissionSettings & kRetransmitBaseLayer)) {
      storage = kDontRetransmit;
    }
    if (rtpTypeHdr->VP8.temporalIdx > 0 &&
        !(_retransmissionSettings & kRetransmitHigherLayers)) {
      storage = kDontRetransmit;
    }

    bool last = false;
    _numberFirstPartition = 0;
    // |rtpTypeHdr->VP8.temporalIdx| is zero for base layers, or -1 if the field
    // isn't used. We currently only protect base layers.
    bool protect = (rtpTypeHdr->VP8.temporalIdx < 1);
    while (!last)
    {
        // Write VP8 Payload Descriptor and VP8 payload.
        WebRtc_UWord8 dataBuffer[IP_PACKET_SIZE] = {0};
        int payloadBytesInPacket = 0;
        int packetStartPartition =
            packetizer.NextPacket(&dataBuffer[rtpHeaderLength],
                                  &payloadBytesInPacket, &last);
        if (packetStartPartition == 0)
        {
            ++_numberFirstPartition;
        }
        else if (packetStartPartition < 0)
        {
            return -1;
        }

        // Write RTP header.
        // Set marker bit true if this is the last packet in frame.
        _rtpSender.BuildRTPheader(dataBuffer, payloadType, last,
            captureTimeStamp);
        if (-1 == SendVideoPacket(dataBuffer, payloadBytesInPacket,
            rtpHeaderLength, storage, protect))
        {
          WEBRTC_TRACE(kTraceError, kTraceRtpRtcp, _id,
                       "RTPSenderVideo::SendVP8 failed to send packet number"
                       " %d", _rtpSender.SequenceNumber());
        }
    }
    return 0;
}

void RTPSenderVideo::ProcessBitrate() {
  _videoBitrate.Process();
  _fecOverheadRate.Process();
}

WebRtc_UWord32 RTPSenderVideo::VideoBitrateSent() const {
  return _videoBitrate.BitrateLast();
}

WebRtc_UWord32 RTPSenderVideo::FecOverheadRate() const {
  return _fecOverheadRate.BitrateLast();
}

int RTPSenderVideo::SelectiveRetransmissions() const {
  return _retransmissionSettings;
}

int RTPSenderVideo::SetSelectiveRetransmissions(uint8_t settings) {
  _retransmissionSettings = settings;
  return 0;
}

} // namespace webrtc
