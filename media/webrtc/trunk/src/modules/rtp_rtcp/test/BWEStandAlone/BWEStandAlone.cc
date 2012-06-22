/*
 *  Copyright (c) 2011 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

// BWEStandAlone.cpp : Defines the entry point for the console application.
//

#include <string>
#include <stdio.h>

#include "event_wrapper.h"
#include "udp_transport.h"
#include "rtp_rtcp.h"
#include "trace.h"

#include "TestSenderReceiver.h"
#include "TestLoadGenerator.h"

#include "MatlabPlot.h"

//#include "vld.h"

class myTransportCB: public UdpTransportData
{
public:
    myTransportCB (RtpRtcp *rtpMod) : _rtpMod(rtpMod) {};
protected:
    // Inherited from UdpTransportData
    virtual void IncomingRTPPacket(const WebRtc_Word8* incomingRtpPacket,
        const WebRtc_Word32 rtpPacketLength,
        const WebRtc_Word8* fromIP,
        const WebRtc_UWord16 fromPort);

    virtual void IncomingRTCPPacket(const WebRtc_Word8* incomingRtcpPacket,
        const WebRtc_Word32 rtcpPacketLength,
        const WebRtc_Word8* fromIP,
        const WebRtc_UWord16 fromPort);

private:
    RtpRtcp *_rtpMod;
};

void myTransportCB::IncomingRTPPacket(const WebRtc_Word8* incomingRtpPacket,
                                      const WebRtc_Word32 rtpPacketLength,
                                      const WebRtc_Word8* fromIP,
                                      const WebRtc_UWord16 fromPort)
{
    printf("Receiving RTP from IP %s, port %u\n", fromIP, fromPort);
    _rtpMod->IncomingPacket((WebRtc_UWord8 *) incomingRtpPacket, static_cast<WebRtc_UWord16>(rtpPacketLength));
}

void myTransportCB::IncomingRTCPPacket(const WebRtc_Word8* incomingRtcpPacket,
                                       const WebRtc_Word32 rtcpPacketLength,
                                       const WebRtc_Word8* fromIP,
                                       const WebRtc_UWord16 fromPort)
{
    printf("Receiving RTCP from IP %s, port %u\n", fromIP, fromPort);
    _rtpMod->IncomingPacket((WebRtc_UWord8 *) incomingRtcpPacket, static_cast<WebRtc_UWord16>(rtcpPacketLength));
}


int main(int argc, char* argv[])
{
    bool isSender = false;
    bool isReceiver = false;
    WebRtc_UWord16 port;
    std::string ip;
    TestSenderReceiver *sendrec = new TestSenderReceiver();
    TestLoadGenerator *gen;

    if (argc == 2)
    {
        // receiver only
        isReceiver = true;

        // read port
        port = atoi(argv[1]);
    }
    else if (argc == 3)
    {
        // sender and receiver
        isSender = true;
        isReceiver = true;

        // read IP
        ip = argv[1];

        // read port
        port = atoi(argv[2]);
    }

    Trace::CreateTrace();
    Trace::SetTraceFile("BWEStandAloneTrace.txt");
    Trace::SetLevelFilter(webrtc::kTraceAll);

    sendrec->InitReceiver(port);

    sendrec->Start();

    if (isSender)
    {
        const WebRtc_UWord32 startRateKbps = 1000;
        //gen = new CBRGenerator(sendrec, 1000, 500);
        gen = new CBRFixFRGenerator(sendrec, startRateKbps, 90000, 30, 0.2);
        //gen = new PeriodicKeyFixFRGenerator(sendrec, startRateKbps, 90000, 30, 0.2, 7, 300);
        //const WebRtc_UWord16 numFrameRates = 5;
        //const WebRtc_UWord8 frameRates[numFrameRates] = {30, 15, 20, 23, 25};
        //gen = new CBRVarFRGenerator(sendrec, 1000, frameRates, numFrameRates, 90000, 4.0, 0.1, 0.2);
        //gen = new CBRFrameDropGenerator(sendrec, startRateKbps, 90000, 0.2);
        sendrec->SetLoadGenerator(gen);
        sendrec->InitSender(startRateKbps, ip.c_str(), port);
        gen->Start();
    }

    while (1)
    {
    }

    if (isSender)
    {
        gen->Stop();
        delete gen;
    }

    delete sendrec;

    //WebRtc_UWord8 numberOfSocketThreads = 1;
    //UdpTransport* transport = UdpTransport::Create(0, numberOfSocketThreads);

    //RtpRtcp* rtp = RtpRtcp::CreateRtpRtcp(1, false);
    //if (rtp->InitSender() != 0)
    //{
    //    exit(1);
    //}
    //if (rtp->RegisterSendTransport(transport) != 0)
    //{
    //    exit(1);
    //}

//    transport->InitializeSendSockets("192.168.200.39", 8000);
    //transport->InitializeSendSockets("127.0.0.1", 10000);
    //transport->InitializeSourcePorts(8000);


    return(0);
 //   myTransportCB *tp = new myTransportCB(rtp);
 //   transport->InitializeReceiveSockets(tp, 10000, "0.0.0.0");
 //   transport->StartReceiving(500);

 //   WebRtc_Word8 data[100];
 //   for (int i = 0; i < 100; data[i] = i++);

 //   for (int i = 0; i < 100; i++)
 //   {
 //       transport->SendRaw(data, 100, false);
 //   }



 //   WebRtc_Word32 totTime = 0;
 //   while (totTime < 10000)
 //   {
 //       transport->Process();
 //       WebRtc_Word32 wTime = transport->TimeUntilNextProcess();
 //       totTime += wTime;
 //       Sleep(wTime);
 //   }


    //if (transport)
    //{
    //    // Destroy the Socket Transport module
    //    transport->StopReceiving();
    //    transport->InitializeReceiveSockets(NULL,0);// deregister callback
 //       UdpTransport::Destroy(transport);
    //    transport = NULL;
 //   }

 //   if (tp)
 //   {
 //       delete tp;
 //       tp = NULL;
 //   }

 //   if (rtp)
 //   {
 //       RtpRtcp::DestroyRtpRtcp(rtp);
 //       rtp = NULL;
 //   }


    //return 0;
}

