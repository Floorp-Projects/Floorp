/*
 *  Copyright (c) 2012 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "webrtc/modules/video_coding/main/test/test_callbacks.h"

#include <math.h>

#include "webrtc/common_video/libyuv/include/webrtc_libyuv.h"
#include "webrtc/modules/rtp_rtcp/interface/rtp_header_parser.h"
#include "webrtc/modules/rtp_rtcp/interface/rtp_payload_registry.h"
#include "webrtc/modules/rtp_rtcp/interface/rtp_receiver.h"
#include "webrtc/modules/utility/interface/rtp_dump.h"
#include "webrtc/modules/video_coding/main/test/test_macros.h"
#include "webrtc/system_wrappers/interface/clock.h"

namespace webrtc {

/******************************
 *  VCMEncodeCompleteCallback
 *****************************/
// Basic callback implementation
// passes the encoded frame directly to the encoder
// Packetization callback implementation
VCMEncodeCompleteCallback::VCMEncodeCompleteCallback(FILE* encodedFile):
    _encodedFile(encodedFile),
    _encodedBytes(0),
    _VCMReceiver(NULL),
    _seqNo(0),
    _encodeComplete(false),
    _width(0),
    _height(0),
    _codecType(kRtpVideoNone)
{
    //
}
VCMEncodeCompleteCallback::~VCMEncodeCompleteCallback()
{
}

void
VCMEncodeCompleteCallback::RegisterTransportCallback(
                                            VCMPacketizationCallback* transport)
{
}

int32_t
VCMEncodeCompleteCallback::SendData(
        const FrameType frameType,
        const uint8_t  payloadType,
        const uint32_t timeStamp,
        int64_t capture_time_ms,
        const uint8_t* payloadData,
        const uint32_t payloadSize,
        const RTPFragmentationHeader& fragmentationHeader,
        const RTPVideoHeader* videoHdr)
{
    // will call the VCMReceiver input packet
    _frameType = frameType;
    // writing encodedData into file
    if (fwrite(payloadData, 1, payloadSize, _encodedFile) !=  payloadSize) {
      return -1;
    }
    WebRtcRTPHeader rtpInfo;
    rtpInfo.header.markerBit = true; // end of frame
    rtpInfo.type.Video.isFirstPacket = true;
    rtpInfo.type.Video.codec = _codecType;
    rtpInfo.type.Video.height = (uint16_t)_height;
    rtpInfo.type.Video.width = (uint16_t)_width;
    switch (_codecType)
    {
    case webrtc::kRtpVideoVp8:
        rtpInfo.type.Video.codecHeader.VP8.InitRTPVideoHeaderVP8();
        rtpInfo.type.Video.codecHeader.VP8.nonReference =
            videoHdr->codecHeader.VP8.nonReference;
        rtpInfo.type.Video.codecHeader.VP8.pictureId =
            videoHdr->codecHeader.VP8.pictureId;
        break;
    case webrtc::kRtpVideoGeneric:
      // Leave for now, until we add kRtpVideoVp9 to RTP.
      break;
    default:
        assert(false);
        return -1;
    }

    rtpInfo.header.payloadType = payloadType;
    rtpInfo.header.sequenceNumber = _seqNo++;
    rtpInfo.header.ssrc = 0;
    rtpInfo.header.timestamp = timeStamp;
    rtpInfo.frameType = frameType;
    // Size should also be received from that table, since the payload type
    // defines the size.

    _encodedBytes += payloadSize;
    // directly to receiver
    int ret = _VCMReceiver->IncomingPacket(payloadData, payloadSize, rtpInfo);
    _encodeComplete = true;

    return ret;
}

float
VCMEncodeCompleteCallback::EncodedBytes()
{
    return _encodedBytes;
}

bool
VCMEncodeCompleteCallback::EncodeComplete()
{
    if (_encodeComplete)
    {
        _encodeComplete = false;
        return true;
    }
    return false;
}

void
VCMEncodeCompleteCallback::Initialize()
{
    _encodeComplete = false;
    _encodedBytes = 0;
    _seqNo = 0;
    return;
}

void
VCMEncodeCompleteCallback::ResetByteCount()
{
    _encodedBytes = 0;
}

/***********************************/
/*   VCMRTPEncodeCompleteCallback  */
/***********************************/
// Encode Complete callback implementation
// passes the encoded frame via the RTP module to the decoder
// Packetization callback implementation

int32_t
VCMRTPEncodeCompleteCallback::SendData(
        const FrameType frameType,
        const uint8_t  payloadType,
        const uint32_t timeStamp,
        int64_t capture_time_ms,
        const uint8_t* payloadData,
        const uint32_t payloadSize,
        const RTPFragmentationHeader& fragmentationHeader,
        const RTPVideoHeader* videoHdr)
{
    _frameType = frameType;
    _encodedBytes+= payloadSize;
    _encodeComplete = true;
    return _RTPModule->SendOutgoingData(frameType,
                                        payloadType,
                                        timeStamp,
                                        capture_time_ms,
                                        payloadData,
                                        payloadSize,
                                        &fragmentationHeader,
                                        videoHdr);
}

float
VCMRTPEncodeCompleteCallback::EncodedBytes()
{
    // only good for one call  - after which will reset value;
    float tmp = _encodedBytes;
    _encodedBytes = 0;
    return tmp;
 }

bool
VCMRTPEncodeCompleteCallback::EncodeComplete()
{
    if (_encodeComplete)
    {
        _encodeComplete = false;
        return true;
    }
    return false;
}

// Decoded Frame Callback Implementation

int32_t
VCMDecodeCompleteCallback::FrameToRender(I420VideoFrame& videoFrame)
{
  if (PrintI420VideoFrame(videoFrame, _decodedFile) < 0) {
    return -1;
  }
  _decodedBytes+= CalcBufferSize(kI420, videoFrame.width(),
                                 videoFrame.height());
  return VCM_OK;
 }

int32_t
VCMDecodeCompleteCallback::DecodedBytes()
{
    return _decodedBytes;
}

RTPSendCompleteCallback::RTPSendCompleteCallback(Clock* clock,
                                                 const char* filename):
    _clock(clock),
    _sendCount(0),
    rtp_payload_registry_(NULL),
    rtp_receiver_(NULL),
    _rtp(NULL),
    _lossPct(0),
    _burstLength(0),
    _networkDelayMs(0),
    _jitterVar(0),
    _prevLossState(0),
    _totalSentLength(0),
    _rtpPackets(),
    _rtpDump(NULL)
{
    if (filename != NULL)
    {
        _rtpDump = RtpDump::CreateRtpDump();
        _rtpDump->Start(filename);
    }
}

RTPSendCompleteCallback::~RTPSendCompleteCallback()
{
    if (_rtpDump != NULL)
    {
        _rtpDump->Stop();
        RtpDump::DestroyRtpDump(_rtpDump);
    }
    // Delete remaining packets
    while (!_rtpPackets.empty())
    {
        // Take first packet in list
        delete _rtpPackets.front();
        _rtpPackets.pop_front();
    }
}

int
RTPSendCompleteCallback::SendPacket(int channel, const void *data, int len)
{
    _sendCount++;
    _totalSentLength += len;

    if (_rtpDump != NULL)
    {
        if (_rtpDump->DumpPacket((const uint8_t*)data, len) != 0)
        {
            return -1;
        }
    }

    bool transmitPacket = true;
    transmitPacket = PacketLoss();

    int64_t now = _clock->TimeInMilliseconds();
    // Insert outgoing packet into list
    if (transmitPacket)
    {
        RtpPacket* newPacket = new RtpPacket();
        memcpy(newPacket->data, data, len);
        newPacket->length = len;
        // Simulate receive time = network delay + packet jitter
        // simulated as a Normal distribution random variable with
        // mean = networkDelay and variance = jitterVar
        int32_t
        simulatedDelay = (int32_t)NormalDist(_networkDelayMs,
                                                   sqrt(_jitterVar));
        newPacket->receiveTime = now + simulatedDelay;
        _rtpPackets.push_back(newPacket);
    }

    // Are we ready to send packets to the receiver?
    RtpPacket* packet = NULL;

    while (!_rtpPackets.empty())
    {
        // Take first packet in list
        packet = _rtpPackets.front();
        int64_t timeToReceive = packet->receiveTime - now;
        if (timeToReceive > 0)
        {
            // No available packets to send
            break;
        }

        _rtpPackets.pop_front();
        assert(_rtp);  // We must have a configured RTP module for this test.
        // Send to receive side
        RTPHeader header;
        scoped_ptr<RtpHeaderParser> parser(RtpHeaderParser::Create());
        if (!parser->Parse(packet->data, packet->length, &header)) {
          delete packet;
          return -1;
        }
        PayloadUnion payload_specific;
        if (!rtp_payload_registry_->GetPayloadSpecifics(
            header.payloadType, &payload_specific)) {
          return -1;
        }
        if (!rtp_receiver_->IncomingRtpPacket(header, packet->data,
                                              packet->length, payload_specific,
                                              true))
        {
            delete packet;
            return -1;
        }
        delete packet;
        packet = NULL;
    }
    return len; // OK
}

int
RTPSendCompleteCallback::SendRTCPPacket(int channel, const void *data, int len)
{
    // Incorporate network conditions
    return SendPacket(channel, data, len);
}

void
RTPSendCompleteCallback::SetLossPct(double lossPct)
{
    _lossPct = lossPct;
    return;
}

void
RTPSendCompleteCallback::SetBurstLength(double burstLength)
{
    _burstLength = burstLength;
    return;
}

bool
RTPSendCompleteCallback::PacketLoss()
{
    bool transmitPacket = true;
    if (_burstLength <= 1.0)
    {
        // Random loss: if _burstLength parameter is not set, or <=1
        if (UnifomLoss(_lossPct))
        {
            // drop
            transmitPacket = false;
        }
    }
    else
    {
        // Simulate bursty channel (Gilbert model)
        // (1st order) Markov chain model with memory of the previous/last
        // packet state (loss or received)

        // 0 = received state
        // 1 = loss state

        // probTrans10: if previous packet is lost, prob. to -> received state
        // probTrans11: if previous packet is lost, prob. to -> loss state

        // probTrans01: if previous packet is received, prob. to -> loss state
        // probTrans00: if previous packet is received, prob. to -> received

        // Map the two channel parameters (average loss rate and burst length)
        // to the transition probabilities:
        double probTrans10 = 100 * (1.0 / _burstLength);
        double probTrans11 = (100.0 - probTrans10);
        double probTrans01 = (probTrans10 * ( _lossPct / (100.0 - _lossPct)));

        // Note: Random loss (Bernoulli) model is a special case where:
        // burstLength = 100.0 / (100.0 - _lossPct) (i.e., p10 + p01 = 100)

        if (_prevLossState == 0 )
        {
            // previous packet was received
            if (UnifomLoss(probTrans01))
            {
                // drop, update previous state to loss
                _prevLossState = 1;
                transmitPacket = false;
            }
        }
        else if (_prevLossState == 1)
        {
            _prevLossState = 0;
            // previous packet was lost
            if (UnifomLoss(probTrans11))
            {
                // drop, update previous state to loss
                _prevLossState = 1;
                transmitPacket = false;
             }
        }
    }
    return transmitPacket;
}


bool
RTPSendCompleteCallback::UnifomLoss(double lossPct)
{
    double randVal = (rand() + 1.0) / (RAND_MAX + 1.0);
    return randVal < lossPct/100;
}

int32_t
PacketRequester::ResendPackets(const uint16_t* sequenceNumbers,
                               uint16_t length)
{
    return _rtp.SendNACK(sequenceNumbers, length);
}

int32_t
SendStatsTest::SendStatistics(const uint32_t bitRate,
                              const uint32_t frameRate)
{
    TEST(frameRate <= _framerate);
    TEST(bitRate > _bitrate / 2 && bitRate < 3 * _bitrate / 2);
    printf("VCM 1 sec: Bit rate: %u\tFrame rate: %u\n", bitRate, frameRate);
    return 0;
}

int32_t KeyFrameReqTest::RequestKeyFrame() {
  printf("Key frame requested\n");
  return 0;
}


VideoProtectionCallback::VideoProtectionCallback():
delta_fec_params_(),
key_fec_params_()
{
    memset(&delta_fec_params_, 0, sizeof(delta_fec_params_));
    memset(&key_fec_params_, 0, sizeof(key_fec_params_));
}

VideoProtectionCallback::~VideoProtectionCallback()
{
    //
}

int32_t
VideoProtectionCallback::ProtectionRequest(
    const FecProtectionParams* delta_fec_params,
    const FecProtectionParams* key_fec_params,
    uint32_t* sent_video_rate_bps,
    uint32_t* sent_nack_rate_bps,
    uint32_t* sent_fec_rate_bps)
{
    key_fec_params_ = *key_fec_params;
    delta_fec_params_ = *delta_fec_params;

    // Update RTP
    if (_rtp->SetFecParameters(&delta_fec_params_,
                               &key_fec_params_) != 0)
    {
        printf("Error in Setting FEC rate\n");
        return -1;

    }
    return 0;

}

FecProtectionParams VideoProtectionCallback::DeltaFecParameters() const
{
    return delta_fec_params_;
}

FecProtectionParams VideoProtectionCallback::KeyFecParameters() const
{
    return key_fec_params_;
}
}  // namespace webrtc
