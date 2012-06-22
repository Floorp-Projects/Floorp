/*
 *  Copyright (c) 2012 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

//
// tb_external_transport.h
//

#ifndef WEBRTC_VIDEO_ENGINE_TEST_AUTOTEST_INTERFACE_TB_EXTERNAL_TRANSPORT_H_
#define WEBRTC_VIDEO_ENGINE_TEST_AUTOTEST_INTERFACE_TB_EXTERNAL_TRANSPORT_H_

#include <list>

#include "common_types.h"

namespace webrtc
{
class CriticalSectionWrapper;
class EventWrapper;
class ThreadWrapper;
class ViENetwork;
}

// Allows to subscribe for callback when a frame is started being sent.
class SendFrameCallback
{
public:
    // Called once per frame (when a new RTP timestamp is detected) when the
    // first data packet of the frame is being sent using the
    // TbExternalTransport.SendPacket method.
    virtual void FrameSent(unsigned int rtp_timestamp) = 0;
protected:
    SendFrameCallback() {}
    virtual ~SendFrameCallback() {}
};

// Allows to subscribe for callback when the first packet of a frame is
// received.
class ReceiveFrameCallback
{
public:
    // Called once per frame (when a new RTP timestamp is detected)
    // during the processing of the RTP packet queue in
    // TbExternalTransport::ViEExternalTransportProcess.
    virtual void FrameReceived(unsigned int rtp_timestamp) = 0;
protected:
    ReceiveFrameCallback() {}
    virtual ~ReceiveFrameCallback() {}
};

// External transport implementation for testing purposes.
// A packet loss probability must be set in order to drop packets from the data
// being sent to this class.
// Will never drop packets from the first frame of a video sequence.
class TbExternalTransport : public webrtc::Transport
{
public:
    TbExternalTransport(webrtc::ViENetwork& vieNetwork);
    ~TbExternalTransport(void);

    virtual int SendPacket(int channel, const void *data, int len);
    virtual int SendRTCPPacket(int channel, const void *data, int len);

    // Should only be called before/after traffic is being processed.
    // Only one observer can be set (multiple calls will overwrite each other).
    virtual void RegisterSendFrameCallback(SendFrameCallback* callback);

    // Should only be called before/after traffic is being processed.
    // Only one observer can be set (multiple calls will overwrite each other).
    virtual void RegisterReceiveFrameCallback(ReceiveFrameCallback* callback);

    // The probability of a packet of being dropped. Packets belonging to the
    // first packet (same RTP timestamp) will never be dropped.
    WebRtc_Word32 SetPacketLoss(WebRtc_Word32 lossRate);  // Rate in %
    void SetNetworkDelay(WebRtc_Word64 delayMs);
    void SetSSRCFilter(WebRtc_UWord32 SSRC);

    void ClearStats();
    void GetStats(WebRtc_Word32& numRtpPackets,
                  WebRtc_Word32& numDroppedPackets,
                  WebRtc_Word32& numRtcpPackets);

    void SetTemporalToggle(unsigned char layers);
    void EnableSSRCCheck();
    unsigned int ReceivedSSRC();

    void EnableSequenceNumberCheck();
    unsigned short GetFirstSequenceNumber();

protected:
    static bool ViEExternalTransportRun(void* object);
    bool ViEExternalTransportProcess();
private:
    WebRtc_Word64 NowMs();

    enum
    {
        KMaxPacketSize = 1650
    };
    enum
    {
        KMaxWaitTimeMs = 100
    };
    typedef struct
    {
        WebRtc_Word8 packetBuffer[KMaxPacketSize];
        WebRtc_Word32 length;
        WebRtc_Word32 channel;
        WebRtc_Word64 receiveTime;
    } VideoPacket;

    webrtc::ViENetwork& _vieNetwork;
    webrtc::ThreadWrapper& _thread;
    webrtc::EventWrapper& _event;
    webrtc::CriticalSectionWrapper& _crit;
    webrtc::CriticalSectionWrapper& _statCrit;

    WebRtc_Word32 _lossRate;
    WebRtc_Word64 _networkDelayMs;
    WebRtc_Word32 _rtpCount;
    WebRtc_Word32 _rtcpCount;
    WebRtc_Word32 _dropCount;

    std::list<VideoPacket*> _rtpPackets;
    std::list<VideoPacket*> _rtcpPackets;

    SendFrameCallback* _send_frame_callback;
    ReceiveFrameCallback* _receive_frame_callback;

    unsigned char _temporalLayers;
    unsigned short _seqNum;
    unsigned short _sendPID;
    unsigned char _receivedPID;
    bool _switchLayer;
    unsigned char _currentRelayLayer;
    unsigned int _lastTimeMs;

    bool _checkSSRC;
    WebRtc_UWord32 _lastSSRC;
    bool _filterSSRC;
    WebRtc_UWord32 _SSRC;
    bool _checkSequenceNumber;
    WebRtc_UWord16 _firstSequenceNumber;

    // Keep track of the first RTP timestamp so we don't do packet loss on
    // the first frame.
    WebRtc_UWord32 _firstRTPTimestamp;
    // Track RTP timestamps so we invoke callbacks properly (if registered).
    WebRtc_UWord32 _lastSendRTPTimestamp;
    WebRtc_UWord32 _lastReceiveRTPTimestamp;
};

#endif  // WEBRTC_VIDEO_ENGINE_TEST_AUTOTEST_INTERFACE_TB_EXTERNAL_TRANSPORT_H_
