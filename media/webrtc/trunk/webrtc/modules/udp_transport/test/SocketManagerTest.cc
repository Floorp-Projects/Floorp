/*
 *  Copyright (c) 2012 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include <cassert>
#include <iostream>

#ifdef _WIN32
#include <windows.h>
#include <tchar.h>
#else
#include <stdio.h>
#define Sleep(x) usleep(x*1000)
#endif

#include "udp_transport.h"
#include "common_types.h"
#include "trace.h"

//#define QOS_TEST
//#define QOS_TEST_WITH_OVERRIDE // require admin on Win7
//#define TOS_TEST               // require admin on Win7
//#define TOS_TEST_USING_SETSOCKOPT
//#define PCP_TEST

class UdpTransportDataA: public UdpTransportData
{
public:
    UdpTransportDataA() :
        _counterRTP(0),
        _counterRTCP(0)
    {
    };
    virtual void IncomingRTPPacket(const WebRtc_Word8* incommingRtpPacket,
                                   const WebRtc_Word32 rtpPacketLength,
                                   const char* fromIP,
                                   const WebRtc_UWord16 fromPort)
    {
        _counterRTP++;
    };

    virtual void IncomingRTCPPacket(const WebRtc_Word8* incommingRtcpPacket,
                                    const WebRtc_Word32 rtcpPacketLength,
                                    const char* fromIP,
                                    const WebRtc_UWord16 fromPort)
    {
        _counterRTCP++;
    };
    WebRtc_UWord32    _counterRTP;
    WebRtc_UWord32    _counterRTCP;
};

class UdpTransportDataB: public UdpTransportData
{
public:
    UdpTransportDataB() :
        _counterRTP(0),
        _counterRTCP(0)
    {
    };
    virtual void IncomingRTPPacket(const WebRtc_Word8* incommingRtpPacket,
                                   const WebRtc_Word32 rtpPacketLength,
                                   const char* fromIP,
                                   const WebRtc_UWord16 fromPort)
    {
        _counterRTP++;
    };

    virtual void IncomingRTCPPacket(const WebRtc_Word8* incommingRtcpPacket,
                                    const WebRtc_Word32 rtcpPacketLength,
                                    const char* fromIP,
                                    const WebRtc_UWord16 fromPort)
    {
        _counterRTCP++;
    };
    WebRtc_UWord32    _counterRTP;
    WebRtc_UWord32    _counterRTCP;
};

#ifdef _WIN32
int _tmain(int argc, _TCHAR* argv[])
#else
int main(int argc, char* argv[])
#endif
{
    Trace::CreateTrace();
    Trace::SetTraceFile("testTrace.txt");
    Trace::SetEncryptedTraceFile("testTraceDebug.txt");
    Trace::SetLevelFilter(webrtc::kTraceAll);

    printf("Start UdpTransport test\n");

    WebRtc_UWord8 numberOfSocketThreads = 5;
    UdpTransport* client1 = UdpTransport::Create(1,numberOfSocketThreads,NULL);
    numberOfSocketThreads = 0;
    UdpTransport* client2 = UdpTransport::Create(2,numberOfSocketThreads,NULL);
    assert(5 == numberOfSocketThreads);

    UdpTransportDataA* client1Callback = new UdpTransportDataA();
    UdpTransportDataB* client2Callback = new UdpTransportDataB();

    WebRtc_UWord32 localIP = 0;
    char localIPAddr[64];
    assert( 0 == client1->LocalHostAddress(localIP)); // network host order aka big-endian

    sprintf(localIPAddr,"%lu.%lu.%lu.%lu",(localIP>>24)& 0x0ff,(localIP>>16)& 0x0ff ,(localIP>>8)& 0x0ff, localIP & 0x0ff);
    printf("\tLocal IP:%s\n", localIPAddr);

    char localIPV6[16];
    char localIPAddrV6[128];
    if( 0 == client1->LocalHostAddressIPV6(localIPV6))
    {
        sprintf(localIPAddrV6,"%.2x%.2x:%.2x%.2x:%.2x%.2x:%.2x%.2x:%.2x%.2x:%.2x%.2x:%.2x%.2x:%.2x%.2x", localIPV6[0],localIPV6[1],localIPV6[2],localIPV6[3],localIPV6[4],localIPV6[5],localIPV6[6],localIPV6[7], localIPV6[8],localIPV6[9],localIPV6[10],localIPV6[11],localIPV6[12],localIPV6[13],localIPV6[14],localIPV6[15]);
        printf("\tLocal IPV6:%s\n", localIPAddrV6);
    }

    char test[9] = "testtest";
    assert( 0 == client1->InitializeReceiveSockets(client1Callback,1234, localIPAddr));

#if defined QOS_TEST_WITH_OVERRIDE || defined QOS_TEST || defined TOS_TEST || defined TOS_TEST_USING_SETSOCKOPT
    assert( -1 == client1->SetQoS(true, 3, 1000));  //  should fail
    assert( 0 == client1->InitializeSendSockets("192.168.200.1", 1236,1237));
#else
    assert( 0 == client1->InitializeSendSockets(localIPAddr, 1236,1237));
#endif
    assert( 0 == client1->StartReceiving(20));

    assert( 0 == client2->InitializeReceiveSockets(client2Callback,1236));
    assert( 0 == client2->InitializeSendSockets(localIPAddr, 1234,1235));
    assert( 0 == client2->StartReceiving(20));

    Sleep(10);

#ifdef TOS_TEST
    // note: you need to have QOS installed on your interface for this test
    // test by doing a ethereal sniff and filter out packets with ip.dst == 192.168.200.1
    assert( 0 == client1->SetToS(2));
    Sleep(10);
    assert( 9 == client1->SendPacket(-1, test, 9));
    Sleep(10);
    assert( 0 == client1->SetToS(3));
    Sleep(10);
    assert( 9 == client1->SendPacket(-1, test, 9));
    Sleep(10);
    assert( 0 == client1->SetToS(0));
    Sleep(10);
    assert( 9 == client1->SendPacket(-1, test, 9));

    printf("Tested TOS  \n");
    Sleep(5000);
    return 0;
#endif

#ifdef TOS_TEST_USING_SETSOCKOPT
    // note: you need to have QOS installed on your interface for this test
    // test by doing a ethereal sniff and filter out packets with ip.dst == 192.168.200.1
    assert( 0 == client1->SetToS(2, true));
    Sleep(10);
    assert( 9 == client1->SendPacket(-1, test, 9));
    Sleep(10);
    assert( 0 == client1->SetToS(3, true));
    Sleep(10);
    assert( 9 == client1->SendPacket(-1, test, 9));
    Sleep(10);
    assert( 0 == client1->SetToS(0, true));
    Sleep(10);
    assert( 9 == client1->SendPacket(-1, test, 9));

    printf("Tested TOS using setsockopt \n");
    Sleep(5000);
    return 0;
#endif

#ifdef QOS_TEST
    // note: you need to have QOS installed on your interface for this test
    // test by doing a ethereal sniff and filter out packets with ip.dst == 192.168.200.1
    assert( 0 == client1->SetQoS(true, 2, 1000));  // SERVICETYPE_CONTROLLEDLOAD 2
    Sleep(10);
    assert( 9 == client1->SendPacket(-1, test, 9));
    Sleep(10);
    assert( 0 == client1->SetQoS(true, 3, 1000));  // SERVICETYPE_GUARANTEED 3
    Sleep(10);
    assert( 9 == client1->SendPacket(-1, test, 9));
    Sleep(10);
    assert( 0 == client1->SetQoS(false, 0));  //
    Sleep(10);
    assert( 9 == client1->SendPacket(-1, test, 9));

    printf("Tested QOS  \n");
    Sleep(5000);
    return 0;
#endif

#ifdef QOS_TEST_WITH_OVERRIDE
    // note: you need to have QOS installed on your interface for this test
    // test by doing a ethereal sniff and filter out packets with ip.dst == 192.168.200.1
    assert( 0 == client1->SetQoS(true, 2, 1000, 1));  // SERVICETYPE_CONTROLLEDLOAD 2
    Sleep(10);
    assert( 9 == client1->SendPacket(-1, test, 9));
    Sleep(10);
    assert( 0 == client1->SetQoS(true, 2, 1000, 2));  // SERVICETYPE_GUARANTEED 3
    Sleep(10);
    assert( 9 == client1->SendPacket(-1, test, 9));
    Sleep(10);
    assert( 0 == client1->SetQoS(false, 0));  //
    Sleep(10);
    assert( 9 == client1->SendPacket(-1, test, 9));
    printf("Tested QOS with override \n");
    Sleep(5000);
    return 0;
#endif

#ifdef PCP_TEST
    // Note: We currently don't know how to test that the bits are actually set in the frame,
    // this test simply tests the API and that we can send a packet after setting PCP.
    assert( -1 == client1->SetPCP(-1)); // should fail
    assert( -1 == client1->SetPCP(8)); // should fail
    printf("Setting PCP to 7 returned %d \n", client1->SetPCP(7));
    printf("(Failing is normal, requires the CAP_NET_ADMIN capability to succeed.) \n");
    Sleep(10);
    for (int pcp = 6; pcp >= 0; --pcp)
    {
        assert( 0 == client1->SetPCP(pcp));
        Sleep(10);
        assert( 9 == client1->SendPacket(-1, test, 9));
    }
    printf("Tested PCP \n");
    Sleep(5000);
    return 0;
#endif

    Sleep(10);

    assert( 9 == client1->SendPacket(-1, test, 9));

    // test start rec after a socket has revceived data
    // result: packets received before first startReceive is saved by the OS
/*
    for(int i = 0; i < 100; i++)
    {
        assert( 9 == client1->SendPacket(-1, test, 9));
    }
    Sleep(10);
    assert( 0 == client2->StartReceiving(20));

//    assert( 0 == client2->StopReceiving());

    Sleep(10);
    for(int i = 0; i < 100; i++)
    {
        assert( 9 == client1->SendPacket(-1, test, 9));
    }

    assert( 0 == client2->StartReceiving(20));

    for(int i = 0; i < 100; i++)
    {
        assert( 9 == client1->SendPacket(-1, test, 9));
    }
*/
    Sleep(10);

    assert( 0 == client1Callback->_counterRTP);
    assert( 1 == client2Callback->_counterRTP);
    assert( 0 == client1Callback->_counterRTCP);
    assert( 0 == client2Callback->_counterRTCP);

    printf("Sent 1 packet on one socket \n");

    char ipAddr[64];
    char tempIpAddr[64];
    char ipMulticastAddr[64];
    WebRtc_UWord16 rtpPort = 0;
    WebRtc_UWord16 rtcpPort = 0;
    bool reusableSocket = true;
    assert( 0 == client2->RemoteSocketInformation(ipAddr, rtpPort, rtcpPort));
    assert( rtpPort == 1234);
    assert( strncmp(ipAddr, localIPAddr, 16) == 0);

    assert( 0 == client2->ReceiveSocketInformation(ipAddr, rtpPort, rtcpPort, ipMulticastAddr, reusableSocket));
    assert( rtpPort == 1236);
    assert( rtcpPort == 1237);
    assert( strncmp(ipAddr, "0.0.0.0", 16) == 0);
    assert( ipMulticastAddr[0] == 0);
    assert( reusableSocket == false);

    assert( 0 == client2->SendSocketInformation(ipAddr, rtpPort, rtcpPort));
    assert( rtpPort == 1234);
    assert( rtcpPort == 1235);
    assert( strncmp(ipAddr,localIPAddr, 16) == 0);

    const int numberOfPackets = 1000;
    int n = 0;
    while(n < numberOfPackets)
    {
        assert( 9 == client1->SendPacket(-1, test, 9));
        assert( 9 == client2->SendPacket(-1, test, 9));
        assert( 9 == client1->SendRTCPPacket(-1, test, 9));
        assert( 9 == client2->SendRTCPPacket(-1, test, 9));
        n++;
    }
    int loops = 0;
    for(; loops < 100 &&
        !(client1Callback->_counterRTP == numberOfPackets &&
        client1Callback->_counterRTCP == numberOfPackets &&
        client2Callback->_counterRTP == numberOfPackets+1 &&
        client2Callback->_counterRTCP == numberOfPackets);
        loops++)
    {
        Sleep(10);
    }
    printf("\tSent %d packets on 4 sockets in:%d ms\n", numberOfPackets, loops*10);

    assert( numberOfPackets == client1Callback->_counterRTP);
    assert( numberOfPackets+1 == client2Callback->_counterRTP);
    assert( numberOfPackets == client1Callback->_counterRTCP);
    assert( numberOfPackets == client2Callback->_counterRTCP);

    assert( 0 == client1->StopReceiving());
    assert( 0 == client2->StopReceiving());

    printf("Tear down client 2\n");

    // configure that fail
    assert( -1 == client2->InitializeReceiveSockets(client2Callback,1234, localIPAddr)); // port in use
    assert( !client2->ReceiveSocketsInitialized());
    assert( 0 == client2->InitializeReceiveSockets(client2Callback,1236));
    assert( 0 == client2->StartReceiving(20));

    printf("Client 2 re-configured\n");

    assert( client1->SendSocketsInitialized());
    assert( client1->ReceiveSocketsInitialized());
    assert( client2->SendSocketsInitialized());
    assert( client2->ReceiveSocketsInitialized());

    assert( 9 == client1->SendPacket(-1, test, 9));

    // this should not be received since we dont receive in client 1
    assert( 9 == client2->SendPacket(-1, test, 9));

    Sleep(10);

    assert( numberOfPackets == client1Callback->_counterRTP);
    assert( numberOfPackets+2 == client2Callback->_counterRTP);
    assert( numberOfPackets == client1Callback->_counterRTCP);
    assert( numberOfPackets == client2Callback->_counterRTCP);
    printf("\tSent 1 packet on one socket \n");

    printf("Start filter test\n");

    assert( 0 == client1->StartReceiving(20));

    assert( 0 == client1->SetFilterPorts(1234, 1235)); // should filter out what we send
    assert( 0 == client1->SetFilterIP(localIPAddr));

    assert( 0 == client1->FilterIP(tempIpAddr));
    assert( strncmp(tempIpAddr, localIPAddr, 16) == 0);

    assert( 9 == client2->SendPacket(-1, test, 9));
    assert( 9 == client2->SendRTCPPacket(-1, test, 9));

    Sleep(10);

    assert( numberOfPackets == client1Callback->_counterRTP);
    assert( numberOfPackets+2 == client2Callback->_counterRTP);
    assert( numberOfPackets == client1Callback->_counterRTCP);
    assert( numberOfPackets == client2Callback->_counterRTCP);

    assert( 0 == client1->SetFilterPorts(1236, 1237)); // should pass through

    assert( 9 == client2->SendPacket(-1, test, 9));
    assert( 9 == client2->SendRTCPPacket(-1, test, 9));
    printf("\tSent 1 packet on two sockets \n");

    Sleep(10);

    assert( numberOfPackets+1 == client1Callback->_counterRTP);
    assert( numberOfPackets+2 == client2Callback->_counterRTP);
    assert( numberOfPackets+1 == client1Callback->_counterRTCP);
    assert( numberOfPackets == client2Callback->_counterRTCP);

    assert( 0 == client1->SetFilterIP("127.0.0.2"));

    assert( 9 == client2->SendPacket(-1, test, 9));
    assert( 9 == client2->SendRTCPPacket(-1, test, 9));
    printf("\tSent 1 packet on two sockets \n");

    Sleep(10);

    assert( numberOfPackets+1 == client1Callback->_counterRTP);
    assert( numberOfPackets+2 == client2Callback->_counterRTP);
    assert( numberOfPackets+1 == client1Callback->_counterRTCP);
    assert( numberOfPackets == client2Callback->_counterRTCP);

    assert( 0 == client1->SetFilterIP(NULL));
    assert( 0 == client1->SetFilterPorts(0, 0));

    printf("Tested filter \n");

    assert( 0 == client2->InitializeSourcePorts(1238, 1239));
    assert( 9 == client2->SendPacket(-1, test, 9));
    assert( 9 == client2->SendRTCPPacket(-1, test, 9));
    printf("\tSent 1 packet on two sockets \n");

    Sleep(10);

    assert( numberOfPackets+2 == client1Callback->_counterRTP);
    assert( numberOfPackets+2 == client2Callback->_counterRTP);
    assert( numberOfPackets+2 == client1Callback->_counterRTCP);
    assert( numberOfPackets == client2Callback->_counterRTCP);

    assert( 0 == client1->RemoteSocketInformation(ipAddr, rtpPort, rtcpPort));
    assert( rtpPort == 1238);
    assert( rtcpPort == 1239);
    assert( strncmp(ipAddr, localIPAddr, 16) == 0);

    printf("Tested source port \n");

    assert( 0 == client2->InitializeSourcePorts(1240 ));
    assert( 9 == client2->SendPacket(-1, test, 9));
    assert( 9 == client2->SendRTCPPacket(-1, test, 9));
    printf("\tSent 1 packet on two sockets \n");

    Sleep(10);

    assert( 0 == client1->RemoteSocketInformation(ipAddr, rtpPort, rtcpPort));
    assert( rtpPort == 1240);
    assert( rtcpPort == 1241);

    printf("Tested SetSendPorts source port \n");

    UdpTransport::Destroy(client1);
    UdpTransport::Destroy(client2);

    printf("\n\nUdpTransport test done\n");

    delete client1Callback;
    delete client2Callback;

    Sleep(5000);
    Trace::ReturnTrace();
};
