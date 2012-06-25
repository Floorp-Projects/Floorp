/*
 *  Copyright (c) 2011 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef WEBRTC_MODULES_RTP_RTCP_TEST_BWESTANDALONE_TESTSENDERRECEIVER_H_
#define WEBRTC_MODULES_RTP_RTCP_TEST_BWESTANDALONE_TESTSENDERRECEIVER_H_

#include "typedefs.h"
#include "rtp_rtcp.h"
#include "rtp_rtcp_defines.h"
#include "udp_transport.h"

class TestLoadGenerator;
namespace webrtc {
class CriticalSectionWrapper;
class EventWrapper;
class ThreadWrapper;
}

using namespace webrtc;

#define MAX_BITRATE_KBPS 50000


class SendRecCB
{
public:
    virtual void OnOnNetworkChanged(const WebRtc_UWord32 bitrateTarget,
        const WebRtc_UWord8 fractionLost,
        const WebRtc_UWord16 roundTripTimeMs,
        const WebRtc_UWord16 bwEstimateKbitMin,
        const WebRtc_UWord16 bwEstimateKbitMax) = 0;

    virtual ~SendRecCB() {};
};


class TestSenderReceiver : public RtpFeedback, public RtpData, public UdpTransportData, public RtpVideoFeedback
{

public:
    TestSenderReceiver (void);

    ~TestSenderReceiver (void);

    void SetCallback (SendRecCB *cb) { _sendRecCB = cb; };

    WebRtc_Word32 Start();

    WebRtc_Word32 Stop();

    bool ProcLoop();

    /////////////////////////////////////////////
    // Receiver methods

    WebRtc_Word32 InitReceiver (const WebRtc_UWord16 rtpPort,
        const WebRtc_UWord16 rtcpPort = 0,
        const WebRtc_Word8 payloadType = 127);

    WebRtc_Word32 ReceiveBitrateKbps ();

    WebRtc_Word32 SetPacketTimeout(const WebRtc_UWord32 timeoutMS);

    bool timeOutTriggered () { return (_timeOut); };

    // Inherited from RtpFeedback
    virtual WebRtc_Word32 OnInitializeDecoder(const WebRtc_Word32 id,
                                            const WebRtc_Word8 payloadType,
                                            const WebRtc_Word8 payloadName[RTP_PAYLOAD_NAME_SIZE],
                                            const WebRtc_UWord32 frequency,
                                            const WebRtc_UWord8 channels,
                                            const WebRtc_UWord32 rate) { return(0);};

    virtual void OnPacketTimeout(const WebRtc_Word32 id);

    virtual void OnReceivedPacket(const WebRtc_Word32 id,
                                  const RtpRtcpPacketType packetType);

    virtual void OnPeriodicDeadOrAlive(const WebRtc_Word32 id,
                                       const RTPAliveType alive) {};

    virtual void OnIncomingSSRCChanged( const WebRtc_Word32 id,
                                        const WebRtc_UWord32 SSRC) {};

    virtual void OnIncomingCSRCChanged( const WebRtc_Word32 id,
                                        const WebRtc_UWord32 CSRC,
                                        const bool added) {};


    // Inherited from RtpData

    virtual WebRtc_Word32 OnReceivedPayloadData(const WebRtc_UWord8* payloadData,
                                                const WebRtc_UWord16 payloadSize,
                                                const webrtc::WebRtcRTPHeader* rtpHeader);


    // Inherited from UdpTransportData
    virtual void IncomingRTPPacket(const WebRtc_Word8* incomingRtpPacket,
        const WebRtc_Word32 rtpPacketLength,
        const WebRtc_Word8* fromIP,
        const WebRtc_UWord16 fromPort);

    virtual void IncomingRTCPPacket(const WebRtc_Word8* incomingRtcpPacket,
        const WebRtc_Word32 rtcpPacketLength,
        const WebRtc_Word8* fromIP,
        const WebRtc_UWord16 fromPort);



    /////////////////////////////////
    // Sender methods

    WebRtc_Word32 InitSender (const WebRtc_UWord32 startBitrateKbps,
        const WebRtc_Word8* ipAddr,
        const WebRtc_UWord16 rtpPort,
        const WebRtc_UWord16 rtcpPort = 0,
        const WebRtc_Word8 payloadType = 127);

    WebRtc_Word32 SendOutgoingData(const WebRtc_UWord32 timeStamp,
        const WebRtc_UWord8* payloadData,
        const WebRtc_UWord32 payloadSize,
        const webrtc::FrameType frameType = webrtc::kVideoFrameDelta);

    WebRtc_Word32 SetLoadGenerator(TestLoadGenerator *generator);

    WebRtc_UWord32 BitrateSent() { return (_rtp->BitrateSent()); };


    // Inherited from RtpVideoFeedback
    virtual void OnReceivedIntraFrameRequest(const WebRtc_Word32 id,
        const WebRtc_UWord8 message = 0) {};

    virtual void OnNetworkChanged(const WebRtc_Word32 id,
                                  const WebRtc_UWord32 minBitrateBps,
                                  const WebRtc_UWord32 maxBitrateBps,
                                  const WebRtc_UWord8 fractionLost,
                                  const WebRtc_UWord16 roundTripTimeMs,
                                  const WebRtc_UWord16 bwEstimateKbitMin,
                                  const WebRtc_UWord16 bwEstimateKbitMax);

private:
    RtpRtcp* _rtp;
    UdpTransport* _transport;
    webrtc::CriticalSectionWrapper* _critSect;
    webrtc::EventWrapper *_eventPtr;
    webrtc::ThreadWrapper* _procThread;
    bool _running;
    WebRtc_Word8 _payloadType;
    TestLoadGenerator* _loadGenerator;
    bool _isSender;
    bool _isReceiver;
    bool _timeOut;
    SendRecCB * _sendRecCB;
    WebRtc_UWord32 _lastBytesReceived;
    WebRtc_Word64 _lastTime;

};

#endif // WEBRTC_MODULES_RTP_RTCP_TEST_BWESTANDALONE_TESTSENDERRECEIVER_H_
