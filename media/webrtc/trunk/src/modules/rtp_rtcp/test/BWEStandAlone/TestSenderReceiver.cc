/*
 *  Copyright (c) 2011 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include <stdio.h>

#include "rtp_rtcp.h"
#include "udp_transport.h"
#include "event_wrapper.h"
#include "thread_wrapper.h"
#include "tick_util.h"
#include "critical_section_wrapper.h"
#include "TestSenderReceiver.h"
#include "TestLoadGenerator.h"
#include <stdlib.h>

#define NR_OF_SOCKET_BUFFERS 500


bool ProcThreadFunction(void *obj)
{
    if (obj == NULL)
    {
        return false;
    }
    TestSenderReceiver *theObj = static_cast<TestSenderReceiver *>(obj);

    return theObj->ProcLoop();
}


TestSenderReceiver::TestSenderReceiver (void)
:
_critSect(CriticalSectionWrapper::CreateCriticalSection()),
_eventPtr(NULL),
_procThread(NULL),
_running(false),
_payloadType(0),
_loadGenerator(NULL),
_isSender(false),
_isReceiver(false),
_timeOut(false),
_sendRecCB(NULL),
_lastBytesReceived(0),
_lastTime(-1)
{
    // RTP/RTCP module
    _rtp = RtpRtcp::CreateRtpRtcp(0, false);
    if (!_rtp)
    {
        throw "Could not create RTP/RTCP module";
        exit(1);
    }

    if (_rtp->InitReceiver() != 0)
    {
        throw "_rtp->InitReceiver()";
        exit(1);
    }

    if (_rtp->InitSender() != 0)
    {
        throw "_rtp->InitSender()";
        exit(1);
    }

    // SocketTransport module
    WebRtc_UWord8 numberOfThreads = 1;
    _transport = UdpTransport::Create(0, numberOfThreads);
    if (!_transport)
    {
        throw "Could not create transport module";
        exit(1);
    }
}

TestSenderReceiver::~TestSenderReceiver (void)
{

    Stop(); // N.B. without critSect

    _critSect->Enter();

    if (_rtp)
    {
        RtpRtcp::DestroyRtpRtcp(_rtp);
        _rtp = NULL;
    }

    if (_transport)
    {
        UdpTransport::Destroy(_transport);
        _transport = NULL;
    }

    delete _critSect;

}


WebRtc_Word32 TestSenderReceiver::InitReceiver (const WebRtc_UWord16 rtpPort,
                                              const WebRtc_UWord16 rtcpPort,
                                              const WebRtc_Word8 payloadType /*= 127*/)
{
    CriticalSectionScoped cs(_critSect);

    // init transport
    if (_transport->InitializeReceiveSockets(this, rtpPort/*, 0, NULL, 0, true*/) != 0)
    {
        throw "_transport->InitializeReceiveSockets";
        exit(1);
    }

    if (_rtp->RegisterIncomingRTPCallback(this) != 0)
    {
        throw "_rtp->RegisterIncomingRTPCallback";
        exit(1);
    }

    if (_rtp->RegisterIncomingDataCallback(this) != 0)
    {
        throw "_rtp->RegisterIncomingRTPCallback";
        exit(1);
    }

    if (_rtp->SetRTCPStatus(kRtcpNonCompound) != 0)
    {
        throw "_rtp->SetRTCPStatus";
        exit(1);
    }

    if (_rtp->SetTMMBRStatus(true) != 0)
    {
        throw "_rtp->SetTMMBRStatus";
        exit(1);
    }

    if (_rtp->RegisterReceivePayload("I420", payloadType, 90000) != 0)
    {
        throw "_rtp->RegisterReceivePayload";
        exit(1);
    }

    _isReceiver = true;

    return (0);
}


WebRtc_Word32 TestSenderReceiver::Start()
{
    CriticalSectionScoped cs(_critSect);

    _eventPtr = EventWrapper::Create();

    if (_rtp->SetSendingStatus(true) != 0)
    {
        throw "_rtp->SetSendingStatus";
        exit(1);
    }

    _procThread = ThreadWrapper::CreateThread(ProcThreadFunction, this, kRealtimePriority, "TestSenderReceiver");
    if (_procThread == NULL)
    {
        throw "Unable to create process thread";
        exit(1);
    }

    _running = true;

    if (_isReceiver)
    {
        if (_transport->StartReceiving(NR_OF_SOCKET_BUFFERS) != 0)
        {
            throw "_transport->StartReceiving";
            exit(1);
        }
    }

    unsigned int tid;
    _procThread->Start(tid);

    return 0;

}


WebRtc_Word32 TestSenderReceiver::Stop ()
{
    CriticalSectionScoped cs(_critSect);

    _transport->StopReceiving();

    if (_procThread)
    {
        _procThread->SetNotAlive();
        _running = false;
        _eventPtr->Set();

        while (!_procThread->Stop())
        {
            ;
        }

        delete _eventPtr;

        delete _procThread;
    }

    _procThread = NULL;

    return (0);
}


bool TestSenderReceiver::ProcLoop(void)
{

    // process RTP/RTCP module
    _rtp->Process();

    // process SocketTransport module
    _transport->Process();

    // no critSect
    while (_running)
    {
        // ask RTP/RTCP module for wait time
        WebRtc_Word32 rtpWait = _rtp->TimeUntilNextProcess();

        // ask SocketTransport module for wait time
        WebRtc_Word32 tpWait = _transport->TimeUntilNextProcess();

        WebRtc_Word32 minWait = (rtpWait < tpWait) ? rtpWait: tpWait;
        minWait = (minWait > 0) ? minWait : 0;
        // wait
        _eventPtr->Wait(minWait);

        // process RTP/RTCP module
        _rtp->Process();

        // process SocketTransport module
        _transport->Process();

    }

    return true;
}


WebRtc_Word32 TestSenderReceiver::ReceiveBitrateKbps ()
{
    WebRtc_UWord32 bytesSent;
    WebRtc_UWord32 packetsSent;
    WebRtc_UWord32 bytesReceived;
    WebRtc_UWord32 packetsReceived;

    if (_rtp->DataCountersRTP(&bytesSent, &packetsSent, &bytesReceived, &packetsReceived) == 0)
    {
        WebRtc_Word64 now = TickTime::MillisecondTimestamp();
        WebRtc_Word32 kbps = 0;
        if (now > _lastTime)
        {
            if (_lastTime > 0)
            {
                // 8 * bytes / ms = kbps
                kbps = static_cast<WebRtc_Word32>(
                    (8 * (bytesReceived - _lastBytesReceived)) / (now - _lastTime));
            }
            _lastTime = now;
            _lastBytesReceived = bytesReceived;
        }
        return (kbps);
    }

    return (-1);
}


WebRtc_Word32 TestSenderReceiver::SetPacketTimeout(const WebRtc_UWord32 timeoutMS)
{
    return (_rtp->SetPacketTimeout(timeoutMS, 0 /* RTCP timeout */));
}


void TestSenderReceiver::OnPacketTimeout(const WebRtc_Word32 id)
{
    CriticalSectionScoped lock(_critSect);

    _timeOut = true;
}


void TestSenderReceiver::OnReceivedPacket(const WebRtc_Word32 id,
                                    const RtpRtcpPacketType packetType)
{
    // do nothing
    //printf("OnReceivedPacket\n");

}

WebRtc_Word32 TestSenderReceiver::OnReceivedPayloadData(const WebRtc_UWord8* payloadData,
                                          const WebRtc_UWord16 payloadSize,
                                          const webrtc::WebRtcRTPHeader* rtpHeader)
{
    //printf("OnReceivedPayloadData\n");
    return (0);
}


void TestSenderReceiver::IncomingRTPPacket(const WebRtc_Word8* incomingRtpPacket,
                                      const WebRtc_Word32 rtpPacketLength,
                                      const WebRtc_Word8* fromIP,
                                      const WebRtc_UWord16 fromPort)
{
    _rtp->IncomingPacket((WebRtc_UWord8 *) incomingRtpPacket, static_cast<WebRtc_UWord16>(rtpPacketLength));
}



void TestSenderReceiver::IncomingRTCPPacket(const WebRtc_Word8* incomingRtcpPacket,
                                       const WebRtc_Word32 rtcpPacketLength,
                                       const WebRtc_Word8* fromIP,
                                       const WebRtc_UWord16 fromPort)
{
    _rtp->IncomingPacket((WebRtc_UWord8 *) incomingRtcpPacket, static_cast<WebRtc_UWord16>(rtcpPacketLength));
}





///////////////////


WebRtc_Word32 TestSenderReceiver::InitSender (const WebRtc_UWord32 startBitrateKbps,
                                            const WebRtc_Word8* ipAddr,
                                            const WebRtc_UWord16 rtpPort,
                                            const WebRtc_UWord16 rtcpPort /*= 0*/,
                                            const WebRtc_Word8 payloadType /*= 127*/)
{
    CriticalSectionScoped cs(_critSect);

    _payloadType = payloadType;

    // check load generator valid
    if (_loadGenerator)
    {
        _loadGenerator->SetBitrate(startBitrateKbps);
    }

    if (_rtp->RegisterSendTransport(_transport) != 0)
    {
        throw "_rtp->RegisterSendTransport";
        exit(1);
    }
    if (_rtp->RegisterSendPayload("I420", _payloadType, 90000) != 0)
    {
        throw "_rtp->RegisterSendPayload";
        exit(1);
    }

    if (_rtp->RegisterIncomingVideoCallback(this) != 0)
    {
        throw "_rtp->RegisterIncomingVideoCallback";
        exit(1);
    }

    if (_rtp->SetRTCPStatus(kRtcpNonCompound) != 0)
    {
        throw "_rtp->SetRTCPStatus";
        exit(1);
    }

    if (_rtp->SetSendBitrate(startBitrateKbps*1000, 0, MAX_BITRATE_KBPS) != 0)
    {
        throw "_rtp->SetSendBitrate";
        exit(1);
    }


    // SocketTransport
    if (_transport->InitializeSendSockets(ipAddr, rtpPort, rtcpPort))
    {
        throw "_transport->InitializeSendSockets";
        exit(1);
    }

    _isSender = true;

    return (0);
}



WebRtc_Word32
TestSenderReceiver::SendOutgoingData(const WebRtc_UWord32 timeStamp,
                                     const WebRtc_UWord8* payloadData,
                                     const WebRtc_UWord32 payloadSize,
                                     const webrtc::FrameType frameType /*= webrtc::kVideoFrameDelta*/)
{
    return (_rtp->SendOutgoingData(frameType, _payloadType, timeStamp, payloadData, payloadSize));
}


WebRtc_Word32 TestSenderReceiver::SetLoadGenerator(TestLoadGenerator *generator)
{
    CriticalSectionScoped cs(_critSect);

    _loadGenerator = generator;
    return(0);

}

void TestSenderReceiver::OnNetworkChanged(const WebRtc_Word32 id,
                                  const WebRtc_UWord32 minBitrateBps,
                                  const WebRtc_UWord32 maxBitrateBps,
                                  const WebRtc_UWord8 fractionLost,
                                  const WebRtc_UWord16 roundTripTimeMs,
                                  const WebRtc_UWord16 bwEstimateKbitMin,
                                  const WebRtc_UWord16 bwEstimateKbitMax)
{
    if (_loadGenerator)
    {
        _loadGenerator->SetBitrate(maxBitrateBps/1000);
    }

    if (_sendRecCB)
    {
        _sendRecCB->OnOnNetworkChanged(maxBitrateBps,
            fractionLost,
            roundTripTimeMs,
            bwEstimateKbitMin,
            bwEstimateKbitMax);
    }
}
