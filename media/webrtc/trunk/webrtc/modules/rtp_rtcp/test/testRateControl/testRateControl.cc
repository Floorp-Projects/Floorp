/*
 *  Copyright (c) 2011 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include <cassert>
#include <windows.h>
#include <iostream>
#include <tchar.h>

#include "rtp_rtcp.h"
#include "common_types.h"
#include "RateControlDetector.h"
/*#include "rtcp_utility.h"
#include "tmmbr_help.h"*/

#define TEST_STR "Test RateControl."
#define TEST_PASSED() std::cerr << TEST_STR << " : [OK]" << std::endl
#define PRINT_LINE std::cout << "------------------------------------------" << std::endl;


const int maxFileLen = 200;
WebRtc_UWord8* dataFile[maxFileLen];


struct InputSet
{
    WebRtc_UWord32 TMMBR;
    WebRtc_UWord32 packetOH;
    WebRtc_UWord32 SSRC;
};

const InputSet set0   = {220,  80, 11111};  // bitRate, packetOH, ssrc
const InputSet set1   = {180,  90, 22222};
const InputSet set2   = {100, 210, 33333};
const InputSet set3   = { 35,  40, 44444};
const InputSet set4   = { 40,  60, 55555};
const InputSet set4_1 = {100,  60, 55555};
const InputSet set4_2 = { 10,  60, 55555};
const InputSet set5   = {200,  40, 66666};
const InputSet set00  = {  0,  40, 66666};




WebRtc_Word32 GetFile(char* fileName)
{
    if (!fileName[0])
    {
        return 0;
    }

    FILE* openFile = fopen(fileName, "rb");
    assert(openFile != NULL);
    fseek(openFile, 0, SEEK_END);
    int len = (WebRtc_Word16)(ftell(openFile));
    rewind(openFile);
    assert(len > 0 && len < maxFileLen);
    fread(dataFile, 1, len, openFile);
    fclose(openFile);
    return len;
};


class LoopBackTransport2 : public webrtc::Transport
{
public:
    LoopBackTransport2(RtpRtcp* rtpRtcpModule)  :
      _rtpRtcpModule(rtpRtcpModule),
      _cnt(0)
    {
    }
    virtual int SendPacket(int channel, const void *data, int len)
    {
        return _rtpRtcpModule->IncomingPacket((const WebRtc_UWord8*)data, len);
    }
    virtual int SendRTCPPacket(int channel, const void *data, int len)
    {
        char fileName[256] = {0};


        // Get stored rtcp packet w/ TMMBR
        len = GetFile(fileName);
        if (len == 0)
        {
            return 0;
        }

        // Send in bitrate request
        return _rtpRtcpModule->IncomingPacket((const WebRtc_UWord8*)dataFile, len);
    }
    RtpRtcp* _rtpRtcpModule;
    WebRtc_UWord32       _cnt;
};


class LoopBackTransportVideo : public webrtc::Transport
{
public:
    LoopBackTransportVideo(RtpRtcp* rtpRtcpModule)  :
      _rtpRtcpModule(rtpRtcpModule),
      _cnt(0)
    {
    }
    virtual int SendPacket(int channel, const void *data, int len)
    {
        return _rtpRtcpModule->IncomingPacket((const WebRtc_UWord8*)data, len);
    }
    virtual int SendRTCPPacket(int channel, const void *data, int len)
    {
        char fileName[256] = {0};

        strcpy(fileName, "RTCPPacketTMMBR0.bin");

        ++_cnt;

        // Get stored rtcp packet w/ TMMBR
        len = GetFile(fileName);
        if (len == 0)
        {
            return 0;
        }

        // Send in bitrate request*/
        return _rtpRtcpModule->IncomingPacket((const WebRtc_UWord8*)dataFile, len);
    }

    RtpRtcp* _rtpRtcpModule;
    WebRtc_UWord32       _cnt;
};

class TestRateControl : private RateControlDetector
{
public:
    TestRateControl():RateControlDetector(0)
    {
    }
    ~TestRateControl()
    {
    }
    void Start()
    {
        //Test perfect conditions
        // But only one packet per frame
        SetLastUsedBitRate(500);
        WebRtc_UWord32 rtpTs=1234*90;
        WebRtc_UWord32 framePeriod=33; // In Ms
        WebRtc_UWord32 rtpDelta=framePeriod*90;
        WebRtc_UWord32 netWorkDelay=10;
        WebRtc_UWord32 arrivalTime=rtpTs/90+netWorkDelay;
        WebRtc_UWord32 newBitRate=0;
        for(WebRtc_UWord32 k=0;k<10;k++)
        {
            // Receive 10 packets
            for(WebRtc_UWord32 i=0;i<10;i++)
            {
                NotifyNewArrivedPacket(rtpTs,arrivalTime);
                rtpTs+=rtpDelta;
                arrivalTime=rtpTs/90+netWorkDelay;
            }
            newBitRate=RateControl(2*netWorkDelay);
            SetLastUsedBitRate(newBitRate);
            Sleep(10*framePeriod);
            std::cout << "RTCP Packet " << k << " new bitrate " << newBitRate << std::endl;
        }
        Reset();


        //Test increasing RTT
        std::cout << "Test increasing RTT - No Receive timing changes" << std::endl;
        SetLastUsedBitRate(500);

        for(WebRtc_UWord32 k=0;k<10;k++)
        {
            // Receive 10 packets
            for(WebRtc_UWord32 i=0;i<10;i++)
            {
                NotifyNewArrivedPacket(rtpTs,arrivalTime);
                rtpTs+=rtpDelta;
                arrivalTime=rtpTs/90+netWorkDelay;
            }
            WebRtc_UWord32 rtt=2*netWorkDelay+k*20;
            newBitRate=RateControl(rtt);
            Sleep(10*framePeriod);
            SetLastUsedBitRate(newBitRate);
            std::cout << "RTCP Packet " << k << " RTT "<< rtt << " new bitrate " << newBitRate << std::endl;

        }

        Reset();


        //Test increasing RTT
        std::cout << "Test increasing RTT - Changed receive timing" << std::endl;
        SetLastUsedBitRate(500);

        for(WebRtc_UWord32 k=0;k<10;k++)
        {
            // Receive 10 packets
            for(WebRtc_UWord32 i=0;i<10;i++)
            {
                NotifyNewArrivedPacket(rtpTs,arrivalTime);
                rtpTs+=rtpDelta;
                arrivalTime=rtpTs/90+netWorkDelay+i+(k*20);
            }
            WebRtc_UWord32 rtt=2*netWorkDelay+k*20;
            newBitRate=RateControl(rtt);
            Sleep(10*framePeriod);
            SetLastUsedBitRate(newBitRate);
            std::cout << "RTCP Packet " << k << " RTT "<< rtt << " new bitrate " << newBitRate << std::endl;

        }



    };
};

class NULLDataZink: public RtpData
{
    virtual WebRtc_Word32 OnReceivedPayloadData(const WebRtc_UWord8* payloadData,
                                                const WebRtc_UWord16 payloadSize,
                                                const webrtc::WebRtcRTPHeader* rtpHeader)
    {
        return 0;
    };
};


int _tmain(int argc, _TCHAR* argv[])
{

    std::string str;
    std::cout << "------------------------" << std::endl;
    std::cout << "---Test RateControl ----" << std::endl;
    std::cout << "------------------------" << std::endl;
    std::cout << "  "  << std::endl;

    // --------------------
    // Test TMMBRHelp class

    // --------------------
    TestRateControl test;
    test.Start();

    printf("RateControl-class test done.\n");

    // ------------------------
    // Test RateControl single module
    // ------------------------
    RtpRtcp* rtpRtcpModuleVideo = RtpRtcp::CreateRtpRtcp(0, false);

    LoopBackTransportVideo* myLoopBackTransportVideo = new LoopBackTransportVideo(rtpRtcpModuleVideo);
    assert(0 == rtpRtcpModuleVideo->RegisterSendTransport(myLoopBackTransportVideo));
    printf("Multi module test done.\n");


    RtpRtcp::DestroyRtpRtcp(rtpRtcpModuleVideo);
    delete myLoopBackTransportVideo;

    TEST_PASSED();
    ::Sleep(5000);

    return 0;
}

