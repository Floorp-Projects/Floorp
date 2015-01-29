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

#include <stdio.h>
#include <string>

#include "webrtc/modules/rtp_rtcp/interface/rtp_rtcp.h"
#include "webrtc/system_wrappers/interface/event_wrapper.h"
#include "webrtc/system_wrappers/interface/trace.h"
#include "webrtc/test/channel_transport/udp_transport.h"

#include "webrtc/modules/rtp_rtcp/test/BWEStandAlone/TestLoadGenerator.h"
#include "webrtc/modules/rtp_rtcp/test/BWEStandAlone/TestSenderReceiver.h"

#include "webrtc/modules/rtp_rtcp/test/BWEStandAlone/MatlabPlot.h"

//#include "vld.h"

class myTransportCB: public UdpTransportData
{
public:
    myTransportCB (RtpRtcp *rtpMod) : _rtpMod(rtpMod) {};
protected:
    // Inherited from UdpTransportData
    virtual void IncomingRTPPacket(const int8_t* incomingRtpPacket,
                                   const int32_t rtpPacketLength,
                                   const int8_t* fromIP,
                                   const uint16_t fromPort) OVERRIDE;

    virtual void IncomingRTCPPacket(const int8_t* incomingRtcpPacket,
                                    const int32_t rtcpPacketLength,
                                    const int8_t* fromIP,
                                    const uint16_t fromPort) OVERRIDE;

private:
    RtpRtcp *_rtpMod;
};

void myTransportCB::IncomingRTPPacket(const int8_t* incomingRtpPacket,
                                      const int32_t rtpPacketLength,
                                      const int8_t* fromIP,
                                      const uint16_t fromPort)
{
    printf("Receiving RTP from IP %s, port %u\n", fromIP, fromPort);
    _rtpMod->IncomingPacket((uint8_t *) incomingRtpPacket, static_cast<uint16_t>(rtpPacketLength));
}

void myTransportCB::IncomingRTCPPacket(const int8_t* incomingRtcpPacket,
                                       const int32_t rtcpPacketLength,
                                       const int8_t* fromIP,
                                       const uint16_t fromPort)
{
    printf("Receiving RTCP from IP %s, port %u\n", fromIP, fromPort);
    _rtpMod->IncomingPacket((uint8_t *) incomingRtcpPacket, static_cast<uint16_t>(rtcpPacketLength));
}


int main(int argc, char* argv[])
{
    bool isSender = false;
    bool isReceiver = false;
    uint16_t port;
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
    Trace::set_level_filter(webrtc::kTraceAll);

    sendrec->InitReceiver(port);

    sendrec->Start();

    if (isSender)
    {
        const uint32_t startRateKbps = 1000;
        //gen = new CBRGenerator(sendrec, 1000, 500);
        gen = new CBRFixFRGenerator(sendrec, startRateKbps, 90000, 30, 0.2);
        //gen = new PeriodicKeyFixFRGenerator(sendrec, startRateKbps, 90000, 30, 0.2, 7, 300);
        //const uint16_t numFrameRates = 5;
        //const uint8_t frameRates[numFrameRates] = {30, 15, 20, 23, 25};
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

    //uint8_t numberOfSocketThreads = 1;
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

 //   int8_t data[100];
 //   for (int i = 0; i < 100; data[i] = i++);

 //   for (int i = 0; i < 100; i++)
 //   {
 //       transport->SendRaw(data, 100, false);
 //   }



 //   int32_t totTime = 0;
 //   while (totTime < 10000)
 //   {
 //       transport->Process();
 //       int32_t wTime = transport->TimeUntilNextProcess();
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
