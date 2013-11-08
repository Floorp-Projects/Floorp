/*
 *  Copyright (c) 2012 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef WEBRTC_MODULES_VIDEO_CODING_TEST_TEST_CALLBACKS_H_
#define WEBRTC_MODULES_VIDEO_CODING_TEST_TEST_CALLBACKS_H_

/*
 * Declaration of general callbacks that are used throughout VCM's offline tests
 */


#include <stdlib.h>
#include <string.h>

#include <fstream>
#include <list>

#include "webrtc/modules/interface/module_common_types.h"
#include "webrtc/modules/rtp_rtcp/interface/rtp_rtcp.h"
#include "webrtc/modules/video_coding/main/interface/video_coding.h"
#include "webrtc/modules/video_coding/main/test/test_util.h"
#include "webrtc/system_wrappers/interface/trace.h"

namespace webrtc
{
class RTPPayloadRegistry;
class RtpDump;

// Send Side - Packetization callback - send an encoded frame to the VCMReceiver
class VCMEncodeCompleteCallback: public VCMPacketizationCallback
{
public:
    // Constructor input: file in which encoded data will be written
    VCMEncodeCompleteCallback(FILE* encodedFile);
    virtual ~VCMEncodeCompleteCallback();
    // Register transport callback
    void RegisterTransportCallback(VCMPacketizationCallback* transport);
    // Process encoded data received from the encoder, pass stream to the
    // VCMReceiver module
    int32_t SendData(const FrameType frameType,
                           const uint8_t payloadType,
                           const uint32_t timeStamp,
                           int64_t capture_time_ms,
                           const uint8_t* payloadData,
                           const uint32_t payloadSize,
                           const RTPFragmentationHeader& fragmentationHeader,
                           const RTPVideoHeader* videoHdr);
    // Register exisitng VCM. Currently - encode and decode under same module.
    void RegisterReceiverVCM(VideoCodingModule *vcm) {_VCMReceiver = vcm;}
    // Return size of last encoded frame data (all frames in the sequence)
    // Good for only one call - after which will reset value
    // (to allow detection of frame drop)
    float EncodedBytes();
    // Return encode complete (true/false)
    bool EncodeComplete();
    // Inform callback of codec used
    void SetCodecType(RtpVideoCodecTypes codecType)
    {_codecType = codecType;}
    // Inform callback of frame dimensions
    void SetFrameDimensions(int32_t width, int32_t height)
    {
        _width = width;
        _height = height;
    }
    // Initialize callback data
    void Initialize();
    void ResetByteCount();

    // Conversion function for payload type (needed for the callback function)

private:
    FILE*              _encodedFile;
    float              _encodedBytes;
    VideoCodingModule* _VCMReceiver;
    FrameType          _frameType;
    uint16_t     _seqNo;
    bool               _encodeComplete;
    int32_t      _width;
    int32_t      _height;
    RtpVideoCodecTypes _codecType;

}; // end of VCMEncodeCompleteCallback

// Send Side - Packetization callback - packetize an encoded frame via the
// RTP module
class VCMRTPEncodeCompleteCallback: public VCMPacketizationCallback
{
public:
    VCMRTPEncodeCompleteCallback(RtpRtcp* rtp) :
        _encodedBytes(0),
        _encodeComplete(false),
        _RTPModule(rtp) {}

    virtual ~VCMRTPEncodeCompleteCallback() {}
    // Process encoded data received from the encoder, pass stream to the
    // RTP module
    int32_t SendData(const FrameType frameType,
                           const uint8_t payloadType,
                           const uint32_t timeStamp,
                           int64_t capture_time_ms,
                           const uint8_t* payloadData,
                           const uint32_t payloadSize,
                           const RTPFragmentationHeader& fragmentationHeader,
                           const RTPVideoHeader* videoHdr);
    // Return size of last encoded frame. Value good for one call
    // (resets to zero after call to inform test of frame drop)
    float EncodedBytes();
    // Return encode complete (true/false)
    bool EncodeComplete();
    // Inform callback of codec used
    void SetCodecType(RtpVideoCodecTypes codecType)
    {_codecType = codecType;}

    // Inform callback of frame dimensions
    void SetFrameDimensions(int16_t width, int16_t height)
    {
        _width = width;
        _height = height;
    }

private:
    float              _encodedBytes;
    FrameType          _frameType;
    bool               _encodeComplete;
    RtpRtcp*           _RTPModule;
    int16_t      _width;
    int16_t      _height;
    RtpVideoCodecTypes _codecType;
}; // end of VCMEncodeCompleteCallback

// Decode Complete callback
// Writes the decoded frames to a given file.
class VCMDecodeCompleteCallback: public VCMReceiveCallback
{
public:
    VCMDecodeCompleteCallback(FILE* decodedFile) :
        _decodedFile(decodedFile), _decodedBytes(0) {}
    virtual ~VCMDecodeCompleteCallback() {}
    // Write decoded frame into file
    int32_t FrameToRender(webrtc::I420VideoFrame& videoFrame);
    int32_t DecodedBytes();
private:
    FILE*               _decodedFile;
    uint32_t      _decodedBytes;
}; // end of VCMDecodeCompleCallback class

// Transport callback
// Called by the RTP Sender - simulates sending packets through a network to the
// RTP receiver. User can set network conditions as: RTT, packet loss,
// burst length and jitter.
class RTPSendCompleteCallback: public Transport
{
public:
    // Constructor input: (receive side) rtp module to send encoded data to
    RTPSendCompleteCallback(Clock* clock,
                            const char* filename = NULL);
    virtual ~RTPSendCompleteCallback();

    void SetRtpModule(RtpRtcp* rtp_module) { _rtp = rtp_module; }
    // Send Packet to receive side RTP module
    virtual int SendPacket(int channel, const void *data, int len);
    // Send RTCP Packet to receive side RTP module
    virtual int SendRTCPPacket(int channel, const void *data, int len);
    // Set percentage of channel loss in the network
    void SetLossPct(double lossPct);
    // Set average size of burst loss
    void SetBurstLength(double burstLength);
    // Set network delay in the network
    void SetNetworkDelay(uint32_t networkDelayMs)
                        {_networkDelayMs = networkDelayMs;};
    // Set Packet jitter delay
    void SetJitterVar(uint32_t jitterVar)
                      {_jitterVar = jitterVar;};
    // Return send count
    int SendCount() {return _sendCount; }
    // Return accumulated length in bytes of transmitted packets
    uint32_t TotalSentLength() {return _totalSentLength;}
protected:
    // Randomly decide whether to drop packets, based on the channel model
    bool PacketLoss();
    // Random uniform loss model
    bool UnifomLoss(double lossPct);

    Clock*                  _clock;
    uint32_t          _sendCount;
    RTPPayloadRegistry* rtp_payload_registry_;
    RtpReceiver* rtp_receiver_;
    RtpRtcp*                _rtp;
    double                  _lossPct;
    double                  _burstLength;
    uint32_t          _networkDelayMs;
    double                  _jitterVar;
    bool                    _prevLossState;
    uint32_t          _totalSentLength;
    std::list<RtpPacket*>   _rtpPackets;
    RtpDump*                _rtpDump;
};

// Request re-transmission of packets (NACK)
class PacketRequester: public VCMPacketRequestCallback
{
public:
    PacketRequester(RtpRtcp& rtp) :
        _rtp(rtp) {}
    int32_t ResendPackets(const uint16_t* sequenceNumbers,
            uint16_t length);
private:
    webrtc::RtpRtcp& _rtp;
};

// Key frame request
class KeyFrameReqTest: public VCMFrameTypeCallback
{
public:
    int32_t RequestKeyFrame();
};


// VCM statistics
class SendStatsTest: public webrtc::VCMSendStatisticsCallback
{
public:
    SendStatsTest() : _framerate(15), _bitrate(500) {}
    int32_t SendStatistics(const uint32_t bitRate,
            const uint32_t frameRate);
    void set_framerate(uint32_t frameRate) {_framerate = frameRate;}
    void set_bitrate(uint32_t bitrate) {_bitrate = bitrate;}
private:
    uint32_t _framerate;
    uint32_t _bitrate;
};

// Protection callback - allows the VCM (media optimization) to inform the RTP
// module of the required protection(FEC rates/settings and NACK mode).
class VideoProtectionCallback: public VCMProtectionCallback
{
public:
    VideoProtectionCallback();
    virtual ~VideoProtectionCallback();
    void RegisterRtpModule(RtpRtcp* rtp) {_rtp = rtp;}
    int32_t ProtectionRequest(
        const FecProtectionParams* delta_fec_params,
        const FecProtectionParams* key_fec_params,
        uint32_t* sent_video_rate_bps,
        uint32_t* sent_nack_rate_bps,
        uint32_t* sent_fec_rate_bps);
    FecProtectionParams DeltaFecParameters() const;
    FecProtectionParams KeyFecParameters() const;
private:
    RtpRtcp* _rtp;
    FecProtectionParams delta_fec_params_;
    FecProtectionParams key_fec_params_;
};
}  // namespace webrtc
#endif
