/*
 *  Copyright (c) 2011 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

//
// tbExternalTransport.h
//

#ifndef WEBRTC_VIDEO_ENGINE_MAIN_TEST_WINDOWSTEST_TBEXTERNALTRANSPORT_H_
#define WEBRTC_VIDEO_ENGINE_MAIN_TEST_WINDOWSTEST_TBEXTERNALTRANSPORT_H_

#include "common_types.h"
#include <queue>

namespace webrtc
{
class CriticalSectionWrapper;
class EventWrapper;
class ThreadWrapper;
class ViENetwork;
}

class TbExternalTransport :  public webrtc::Transport
{
public:
    TbExternalTransport(webrtc::ViENetwork& vieNetwork);
    ~TbExternalTransport(void);

    virtual int SendPacket(int channel, const void *data, int len);
    virtual int SendRTCPPacket(int channel, const void *data, int len);

    WebRtc_Word32 SetPacketLoss(WebRtc_Word32 lossRate);  // Rate in %
    void SetNetworkDelay(WebRtc_Word64 delayMs);

    void ClearStats();
    void GetStats(WebRtc_Word32& numRtpPackets, WebRtc_Word32& numDroppedPackets, WebRtc_Word32& numRtcpPackets);

    void EnableSSRCCheck();
    unsigned int ReceivedSSRC();

    void EnableSequenceNumberCheck();
    unsigned short GetFirstSequenceNumber();

    
protected:
    static bool ViEExternalTransportRun(void* object);
    bool ViEExternalTransportProcess();
private:
    WebRtc_Word64 NowMs();

    enum { KMaxPacketSize = 1650};
    enum { KMaxWaitTimeMs = 100};
    typedef struct
    {
        WebRtc_Word8  packetBuffer[KMaxPacketSize];
        WebRtc_Word32 length;
        WebRtc_Word32 channel;
        WebRtc_Word64 receiveTime;
    } VideoPacket;

    typedef std::queue<VideoPacket*>  VideoPacketQueue;


    webrtc::ViENetwork&      _vieNetwork;
    webrtc::ThreadWrapper&   _thread;
    webrtc::EventWrapper&           _event;
    webrtc::CriticalSectionWrapper& _crit;
    webrtc::CriticalSectionWrapper& _statCrit;

    WebRtc_Word32          _lossRate;
    WebRtc_Word64          _networkDelayMs;
    WebRtc_Word32          _rtpCount;
    WebRtc_Word32          _rtcpCount;
    WebRtc_Word32          _dropCount;

    VideoPacketQueue     _rtpPackets;
    VideoPacketQueue     _rtcpPackets;

    bool                 _checkSSRC;
    WebRtc_UWord32         _lastSSRC;
    bool                 _checkSequenceNumber;
    WebRtc_UWord16         _firstSequenceNumber;
    WebRtc_Word32          _lastSeq;

    //int& numberOfErrors;

    //int _bits;
    //int _lastTicks;            
    //int _dropCnt;
    //int _sentCount;
    //int _frameCount;
    //int _packetLoss;

    //VideoEngine* _video;

    //ReceiveBufferQueue _videoBufferQueue;
    //ReceiveBufferQueue _rtcpBufferQueue;
};

#endif // WEBRTC_VIDEO_ENGINE_MAIN_TEST_WINDOWSTEST_TBEXTERNALTRANSPORT_H_
