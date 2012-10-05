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


#include <string.h>
#include <cstdlib>
#include <fstream>
#include <list>

#include "module_common_types.h"
#include "rtp_rtcp.h"
#include "test_util.h"
#include "trace.h"
#include "video_coding.h"

namespace webrtc
{
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
    WebRtc_Word32 SendData(const FrameType frameType,
                           const WebRtc_UWord8 payloadType,
                           const WebRtc_UWord32 timeStamp,
                           int64_t capture_time_ms,
                           const WebRtc_UWord8* payloadData,
                           const WebRtc_UWord32 payloadSize,
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
    void SetCodecType(RTPVideoCodecTypes codecType)
    {_codecType = codecType;}
    // Inform callback of frame dimensions
    void SetFrameDimensions(WebRtc_Word32 width, WebRtc_Word32 height)
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
    WebRtc_UWord16     _seqNo;
    bool               _encodeComplete;
    WebRtc_Word32      _width;
    WebRtc_Word32      _height;
    RTPVideoCodecTypes _codecType;

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
    WebRtc_Word32 SendData(const FrameType frameType,
                           const WebRtc_UWord8 payloadType,
                           const WebRtc_UWord32 timeStamp,
                           int64_t capture_time_ms,
                           const WebRtc_UWord8* payloadData,
                           const WebRtc_UWord32 payloadSize,
                           const RTPFragmentationHeader& fragmentationHeader,
                           const RTPVideoHeader* videoHdr);
    // Return size of last encoded frame. Value good for one call
    // (resets to zero after call to inform test of frame drop)
    float EncodedBytes();
    // Return encode complete (true/false)
    bool EncodeComplete();
    // Inform callback of codec used
    void SetCodecType(RTPVideoCodecTypes codecType)
    {_codecType = codecType;}

    // Inform callback of frame dimensions
    void SetFrameDimensions(WebRtc_Word16 width, WebRtc_Word16 height)
    {
        _width = width;
        _height = height;
    }

private:
    float              _encodedBytes;
    FrameType          _frameType;
    bool               _encodeComplete;
    RtpRtcp*           _RTPModule;
    WebRtc_Word16      _width;
    WebRtc_Word16      _height;
    RTPVideoCodecTypes _codecType;
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
    WebRtc_Word32 FrameToRender(webrtc::VideoFrame& videoFrame);
    WebRtc_Word32 DecodedBytes();
private:
    FILE*               _decodedFile;
    WebRtc_UWord32      _decodedBytes;
}; // end of VCMDecodeCompleCallback class

// Transport callback
// Called by the RTP Sender - simulates sending packets through a network to the
// RTP receiver. User can set network conditions as: RTT, packet loss,
// burst length and jitter.
class RTPSendCompleteCallback: public Transport
{
public:
    // Constructor input: (receive side) rtp module to send encoded data to
    RTPSendCompleteCallback(TickTimeBase* clock,
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
    void SetNetworkDelay(WebRtc_UWord32 networkDelayMs)
                        {_networkDelayMs = networkDelayMs;};
    // Set Packet jitter delay
    void SetJitterVar(WebRtc_UWord32 jitterVar)
                      {_jitterVar = jitterVar;};
    // Return send count
    int SendCount() {return _sendCount; }
    // Return accumulated length in bytes of transmitted packets
    WebRtc_UWord32 TotalSentLength() {return _totalSentLength;}
protected:
    // Randomly decide whether to drop packets, based on the channel model
    bool PacketLoss();
    // Random uniform loss model
    bool UnifomLoss(double lossPct);

    TickTimeBase*           _clock;
    WebRtc_UWord32          _sendCount;
    RtpRtcp*                _rtp;
    double                  _lossPct;
    double                  _burstLength;
    WebRtc_UWord32          _networkDelayMs;
    double                  _jitterVar;
    bool                    _prevLossState;
    WebRtc_UWord32          _totalSentLength;
    std::list<RtpPacket*>   _rtpPackets;
    RtpDump*                _rtpDump;
};

// Request re-transmission of packets (NACK)
class PacketRequester: public VCMPacketRequestCallback
{
public:
    PacketRequester(RtpRtcp& rtp) :
        _rtp(rtp) {}
    WebRtc_Word32 ResendPackets(const WebRtc_UWord16* sequenceNumbers,
            WebRtc_UWord16 length);
private:
    webrtc::RtpRtcp& _rtp;
};

// Key frame request
class KeyFrameReqTest: public VCMFrameTypeCallback
{
public:
    WebRtc_Word32 RequestKeyFrame();
};


// VCM statistics
class SendStatsTest: public webrtc::VCMSendStatisticsCallback
{
public:
    SendStatsTest() : _frameRate(15) {}
    WebRtc_Word32 SendStatistics(const WebRtc_UWord32 bitRate,
            const WebRtc_UWord32 frameRate);
    void SetTargetFrameRate(WebRtc_UWord32 frameRate) {_frameRate = frameRate;}
private:
    WebRtc_UWord32 _frameRate;
};

// Protection callback - allows the VCM (media optimization) to inform the RTP
// module of the required protection(FEC rates/settings and NACK mode).
class VideoProtectionCallback: public VCMProtectionCallback
{
public:
    VideoProtectionCallback();
    virtual ~VideoProtectionCallback();
    void RegisterRtpModule(RtpRtcp* rtp) {_rtp = rtp;}
    WebRtc_Word32 ProtectionRequest(
        const FecProtectionParams* delta_fec_params,
        const FecProtectionParams* key_fec_params,
        WebRtc_UWord32* sent_video_rate_bps,
        WebRtc_UWord32* sent_nack_rate_bps,
        WebRtc_UWord32* sent_fec_rate_bps);
    FecProtectionParams DeltaFecParameters() const;
    FecProtectionParams KeyFecParameters() const;
private:
    RtpRtcp* _rtp;
    FecProtectionParams delta_fec_params_;
    FecProtectionParams key_fec_params_;
};
}  // namespace webrtc
#endif
