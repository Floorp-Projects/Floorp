/*
 *  Copyright (c) 2012 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "webrtc/modules/audio_coding/main/test/iSACTest.h"

#include <cctype>
#include <stdio.h>
#include <string.h>

#if _WIN32
#include <windows.h>
#elif WEBRTC_LINUX
#include <ctime>
#else
#include <sys/time.h>
#include <time.h>
#endif 

#include "webrtc/modules/audio_coding/main/source/acm_common_defs.h"
#include "webrtc/modules/audio_coding/main/test/utility.h"
#include "webrtc/system_wrappers/interface/event_wrapper.h"
#include "webrtc/system_wrappers/interface/tick_util.h"
#include "webrtc/system_wrappers/interface/trace.h"
#include "webrtc/test/testsupport/fileutils.h"

namespace webrtc {

void SetISACConfigDefault(
    ACMTestISACConfig& isacConfig)
{
    isacConfig.currentRateBitPerSec = 0;
    isacConfig.currentFrameSizeMsec = 0;
    isacConfig.maxRateBitPerSec     = 0;
    isacConfig.maxPayloadSizeByte   = 0;
    isacConfig.encodingMode         = -1;
    isacConfig.initRateBitPerSec    = 0;
    isacConfig.initFrameSizeInMsec  = 0;
    isacConfig.enforceFrameSize     = false;
    return;
}


int16_t SetISAConfig(
    ACMTestISACConfig& isacConfig,
    AudioCodingModule* acm,
    int testMode)
{

    if((isacConfig.currentRateBitPerSec != 0) ||
        (isacConfig.currentFrameSizeMsec != 0))
    {
        CodecInst sendCodec;
        acm->SendCodec(&sendCodec);
        if(isacConfig.currentRateBitPerSec < 0)
        {
            sendCodec.rate = -1;
            CHECK_ERROR(acm->RegisterSendCodec(sendCodec));
            if(testMode != 0)
            {
                printf("ISAC-%s Registered in adaptive (channel-dependent) mode.\n", 
                    (sendCodec.plfreq == 32000)? "swb":"wb");
            }
        }
        else
        {

            if(isacConfig.currentRateBitPerSec != 0)
            {
                sendCodec.rate = isacConfig.currentRateBitPerSec;
            }
            if(isacConfig.currentFrameSizeMsec != 0)
            {
                sendCodec.pacsize = isacConfig.currentFrameSizeMsec *
                    (sendCodec.plfreq / 1000);
            }
            CHECK_ERROR(acm->RegisterSendCodec(sendCodec));
            if(testMode != 0)
            {
                printf("Target rate is set to %d bit/sec with frame-size %d ms \n",
                    (int)isacConfig.currentRateBitPerSec,
                    (int)sendCodec.pacsize / (sendCodec.plfreq / 1000));
            }
        }
    }

    if(isacConfig.maxRateBitPerSec > 0)
    {
        CHECK_ERROR(acm->SetISACMaxRate(isacConfig.maxRateBitPerSec));
        if(testMode != 0)
        {
            printf("Max rate is set to %u bit/sec\n",
                isacConfig.maxRateBitPerSec);
        }
    }
    if(isacConfig.maxPayloadSizeByte > 0)
    {
        CHECK_ERROR(acm->SetISACMaxPayloadSize(isacConfig.maxPayloadSizeByte));
        if(testMode != 0)
        {
            printf("Max payload-size is set to %u bit/sec\n",
                isacConfig.maxPayloadSizeByte);
        }
    }
    if((isacConfig.initFrameSizeInMsec != 0) ||
        (isacConfig.initRateBitPerSec != 0))
    {
        CHECK_ERROR(acm->ConfigISACBandwidthEstimator(
            (uint8_t)isacConfig.initFrameSizeInMsec,
            (uint16_t)isacConfig.initRateBitPerSec, 
            isacConfig.enforceFrameSize));
        if((isacConfig.initFrameSizeInMsec != 0) && (testMode != 0))
        {
            printf("Initialize BWE to %d msec frame-size\n",
                isacConfig.initFrameSizeInMsec);
        }
        if((isacConfig.initRateBitPerSec != 0) && (testMode != 0))
        {
            printf("Initialize BWE to %u bit/sec send-bandwidth\n",
                isacConfig.initRateBitPerSec);
        }
    }

    return 0;
}


ISACTest::ISACTest(int testMode)
{
    _testMode = testMode;
}

ISACTest::~ISACTest()
{
    AudioCodingModule::Destroy(_acmA);
    AudioCodingModule::Destroy(_acmB);

    delete _channel_A2B;
    delete _channel_B2A;
}


int16_t
ISACTest::Setup()
{
    int codecCntr;
    CodecInst codecParam;

    _acmA = AudioCodingModule::Create(1);
    _acmB = AudioCodingModule::Create(2);

    for(codecCntr = 0; codecCntr < AudioCodingModule::NumberOfCodecs(); codecCntr++)
    {
        AudioCodingModule::Codec(codecCntr, &codecParam);
        if(!STR_CASE_CMP(codecParam.plname, "ISAC") && codecParam.plfreq == 16000)
        {
            memcpy(&_paramISAC16kHz, &codecParam, sizeof(CodecInst));
            _idISAC16kHz = codecCntr;
        }
        if(!STR_CASE_CMP(codecParam.plname, "ISAC") && codecParam.plfreq == 32000)
        {
            memcpy(&_paramISAC32kHz, &codecParam, sizeof(CodecInst));
            _idISAC32kHz = codecCntr;
        }        
    }

    // register both iSAC-wb & iSAC-swb in both sides as receiver codecs
    CHECK_ERROR(_acmA->RegisterReceiveCodec(_paramISAC16kHz));
    CHECK_ERROR(_acmA->RegisterReceiveCodec(_paramISAC32kHz));
    CHECK_ERROR(_acmB->RegisterReceiveCodec(_paramISAC16kHz));
    CHECK_ERROR(_acmB->RegisterReceiveCodec(_paramISAC32kHz));

    //--- Set A-to-B channel
    _channel_A2B = new Channel;
    CHECK_ERROR(_acmA->RegisterTransportCallback(_channel_A2B));
    _channel_A2B->RegisterReceiverACM(_acmB);

    //--- Set B-to-A channel
    _channel_B2A = new Channel;
    CHECK_ERROR(_acmB->RegisterTransportCallback(_channel_B2A));
    _channel_B2A->RegisterReceiverACM(_acmA);

    file_name_swb_ =
        webrtc::test::ResourcePath("audio_coding/testfile32kHz", "pcm");

    _acmB->RegisterSendCodec(_paramISAC16kHz);
    _acmA->RegisterSendCodec(_paramISAC32kHz);

    if(_testMode != 0)
    {
        printf("Side A Send Codec\n");
        printf("%s %d\n", _paramISAC32kHz.plname, _paramISAC32kHz.plfreq);

        printf("Side B Send Codec\n");
        printf("%s %d\n", _paramISAC16kHz.plname, _paramISAC16kHz.plfreq);
    }

    _inFileA.Open(file_name_swb_, 32000, "rb");
    std::string fileNameA = webrtc::test::OutputPath() + "testisac_a.pcm";
    std::string fileNameB = webrtc::test::OutputPath() + "testisac_b.pcm";
    _outFileA.Open(fileNameA, 32000, "wb");
    _outFileB.Open(fileNameB, 32000, "wb");

    while(!_inFileA.EndOfFile())
    {
        Run10ms();
    }
    CodecInst receiveCodec;
    CHECK_ERROR(_acmA->ReceiveCodec(&receiveCodec));
    if(_testMode != 0)
    {
        printf("Side A Receive Codec\n");
        printf("%s %d\n", receiveCodec.plname, receiveCodec.plfreq);
    }

    CHECK_ERROR(_acmB->ReceiveCodec(&receiveCodec));
    if(_testMode != 0)
    {
        printf("Side B Receive Codec\n");
        printf("%s %d\n", receiveCodec.plname, receiveCodec.plfreq);
    }

    _inFileA.Close();
    _outFileA.Close();
    _outFileB.Close();

    return 0;
}


void
ISACTest::Perform()
{
    if(_testMode == 0)
    {
        printf("Running iSAC Test");
        WEBRTC_TRACE(webrtc::kTraceStateInfo, webrtc::kTraceAudioCoding, -1, "---------- iSACTest ----------");
    }

    Setup();

    int16_t testNr = 0;
    ACMTestISACConfig wbISACConfig;
    ACMTestISACConfig swbISACConfig;

    SetISACConfigDefault(wbISACConfig);
    SetISACConfigDefault(swbISACConfig);

    wbISACConfig.currentRateBitPerSec = -1;
    swbISACConfig.currentRateBitPerSec = -1;
    testNr++;
    EncodeDecode(testNr, wbISACConfig, swbISACConfig);

    if (_testMode != 0)
    {
        SetISACConfigDefault(wbISACConfig);
        SetISACConfigDefault(swbISACConfig);

        wbISACConfig.currentRateBitPerSec = -1;
        swbISACConfig.currentRateBitPerSec = -1;
        wbISACConfig.initRateBitPerSec = 13000;
        wbISACConfig.initFrameSizeInMsec = 60;
        swbISACConfig.initRateBitPerSec = 20000;
        swbISACConfig.initFrameSizeInMsec = 30;
        testNr++;
        EncodeDecode(testNr, wbISACConfig, swbISACConfig);

        SetISACConfigDefault(wbISACConfig);
        SetISACConfigDefault(swbISACConfig);

        wbISACConfig.currentRateBitPerSec = 20000;
        swbISACConfig.currentRateBitPerSec = 48000;
        testNr++;
        EncodeDecode(testNr, wbISACConfig, swbISACConfig);

        wbISACConfig.currentRateBitPerSec = 16000;
        swbISACConfig.currentRateBitPerSec = 30000;
        wbISACConfig.currentFrameSizeMsec = 60;
        testNr++;
        EncodeDecode(testNr, wbISACConfig, swbISACConfig);
    }

    SetISACConfigDefault(wbISACConfig);
    SetISACConfigDefault(swbISACConfig);
    testNr++;
    EncodeDecode(testNr, wbISACConfig, swbISACConfig);

    int user_input;
    if((_testMode == 0) || (_testMode == 1))
    {
        swbISACConfig.maxPayloadSizeByte = (uint16_t)200;
        wbISACConfig.maxPayloadSizeByte = (uint16_t)200;
    }
    else
    {
        printf("Enter the max payload-size for side A: ");
        CHECK_ERROR(scanf("%d", &user_input));
        swbISACConfig.maxPayloadSizeByte = (uint16_t)user_input;
        printf("Enter the max payload-size for side B: ");
        CHECK_ERROR(scanf("%d", &user_input));
        wbISACConfig.maxPayloadSizeByte = (uint16_t)user_input;
    }
    testNr++;
    EncodeDecode(testNr, wbISACConfig, swbISACConfig);

    _acmA->ResetEncoder();
    _acmB->ResetEncoder();
    SetISACConfigDefault(wbISACConfig);
    SetISACConfigDefault(swbISACConfig);

    if((_testMode == 0) || (_testMode == 1))
    {
        swbISACConfig.maxRateBitPerSec = (uint32_t)48000;
        wbISACConfig.maxRateBitPerSec = (uint32_t)48000;
    }
    else
    {
        printf("Enter the max rate for side A: ");
        CHECK_ERROR(scanf("%d", &user_input));
        swbISACConfig.maxRateBitPerSec = (uint32_t)user_input;
        printf("Enter the max rate for side B: ");
        CHECK_ERROR(scanf("%d", &user_input));
        wbISACConfig.maxRateBitPerSec = (uint32_t)user_input;
    }
 
    testNr++;
    EncodeDecode(testNr, wbISACConfig, swbISACConfig);


    testNr++;
    if(_testMode == 0)
    {
        SwitchingSamplingRate(testNr, 4);
        printf("Done!\n");
    }
    else
    {
        SwitchingSamplingRate(testNr, 80);
    }
}


void
ISACTest::Run10ms()
{
    AudioFrame audioFrame;

    _inFileA.Read10MsData(audioFrame);
    CHECK_ERROR(_acmA->Add10MsData(audioFrame));

    CHECK_ERROR(_acmB->Add10MsData(audioFrame));

    CHECK_ERROR(_acmA->Process());
    CHECK_ERROR(_acmB->Process());

    CHECK_ERROR(_acmA->PlayoutData10Ms(32000, &audioFrame));
    _outFileA.Write10MsData(audioFrame);

    CHECK_ERROR(_acmB->PlayoutData10Ms(32000, &audioFrame));
    _outFileB.Write10MsData(audioFrame);
}

void
ISACTest::EncodeDecode(
    int                testNr,
    ACMTestISACConfig& wbISACConfig,
    ACMTestISACConfig& swbISACConfig)
{
    if(_testMode == 0)
    {
        printf(".");
    }
    else
    {
        printf("\nTest %d:\n\n", testNr);
    }

    // Files in Side A and B
    _inFileA.Open(file_name_swb_, 32000, "rb", true);
    _inFileB.Open(file_name_swb_, 32000, "rb", true);

    std::string file_name_out;
    std::stringstream file_stream_a;
    std::stringstream file_stream_b;
    file_stream_a << webrtc::test::OutputPath();
    file_stream_b << webrtc::test::OutputPath();
    if(_testMode == 0)
    {
        file_stream_a << "out_iSACTest_A_" << testNr << ".pcm";
        file_stream_b << "out_iSACTest_B_" << testNr << ".pcm";

    }
    else
    {
        file_stream_a << "outA_" << testNr << ".pcm";
        file_stream_b << "outB_" << testNr << ".pcm";
    }
    file_name_out = file_stream_a.str();
    _outFileA.Open(file_name_out, 32000, "wb");
    file_name_out = file_stream_b.str();
    _outFileB.Open(file_name_out, 32000, "wb");

    CHECK_ERROR(_acmA->RegisterSendCodec(_paramISAC16kHz));
    CHECK_ERROR(_acmA->RegisterSendCodec(_paramISAC32kHz));
    
    CHECK_ERROR(_acmB->RegisterSendCodec(_paramISAC32kHz));
    CHECK_ERROR(_acmB->RegisterSendCodec(_paramISAC16kHz));
    if(_testMode != 0)
    {
        printf("Side A Sending Super-Wideband \n");
        printf("Side B Sending Wideband\n\n");
    }

    SetISAConfig(swbISACConfig, _acmA, _testMode);
    SetISAConfig(wbISACConfig,  _acmB, _testMode);

    bool adaptiveMode = false;
    if((swbISACConfig.currentRateBitPerSec == -1) ||
        (wbISACConfig.currentRateBitPerSec == -1))
    {
        adaptiveMode = true;
    }
    _myTimer.Reset();
    _channel_A2B->ResetStats();
    _channel_B2A->ResetStats();

    char currentTime[500];
    if(_testMode == 2) printf("\n");
    CodecInst sendCodec;
    EventWrapper* myEvent = EventWrapper::Create();
    myEvent->StartTimer(true, 10);
    while(!(_inFileA.EndOfFile() || _inFileA.Rewinded()))
    {
        Run10ms();
        _myTimer.Tick10ms();
        _myTimer.CurrentTimeHMS(currentTime);
        if(_testMode == 2) printf("\r%s   ", currentTime);

        if((adaptiveMode) && (_testMode != 0))
        {
            myEvent->Wait(5000);

            _acmA->SendCodec(&sendCodec);
            if(_testMode == 2) printf("[%d]  ", sendCodec.rate);
            _acmB->SendCodec(&sendCodec);
            if(_testMode == 2) printf("[%d]  ", sendCodec.rate);
        }
    }

    if(_testMode != 0)
    {
        printf("\n\nSide A statistics\n\n");
        _channel_A2B->PrintStats(_paramISAC32kHz);

        printf("\n\nSide B statistics\n\n");
        _channel_B2A->PrintStats(_paramISAC16kHz);
    }
    
    _channel_A2B->ResetStats();
    _channel_B2A->ResetStats();

    if(_testMode != 0) printf("\n");
    _outFileA.Close();
    _outFileB.Close();
    _inFileA.Close();
    _inFileB.Close();
}

void
ISACTest::SwitchingSamplingRate(
    int testNr, 
    int maxSampRateChange)
{
    // Files in Side A 
    _inFileA.Open(file_name_swb_, 32000, "rb");
    _inFileB.Open(file_name_swb_, 32000, "rb");

    std::string file_name_out;
    std::stringstream file_stream_a;
    std::stringstream file_stream_b;
    file_stream_a << webrtc::test::OutputPath();
    file_stream_b << webrtc::test::OutputPath();
    if(_testMode == 0)
    {
        file_stream_a << "out_iSACTest_A_" << testNr << ".pcm";
        file_stream_b << "out_iSACTest_B_" << testNr << ".pcm";
    }
    else
    {
        printf("\nTest %d", testNr);
        printf("    Alternate between WB and SWB at the sender Side\n\n");
        file_stream_a << "outA_" << testNr << ".pcm";
        file_stream_b << "outB_" << testNr << ".pcm";
    }
    file_name_out = file_stream_a.str();
    _outFileA.Open(file_name_out, 32000, "wb");
    file_name_out = file_stream_b.str();
    _outFileB.Open(file_name_out, 32000, "wb");

    CHECK_ERROR(_acmA->RegisterSendCodec(_paramISAC32kHz));
    CHECK_ERROR(_acmB->RegisterSendCodec(_paramISAC16kHz));
    if(_testMode != 0)
    {
        printf("Side A Sending Super-Wideband \n");
        printf("Side B Sending Wideband\n");
    }

    int numSendCodecChanged = 0;
    _myTimer.Reset();
    char currentTime[50];
    while(numSendCodecChanged < (maxSampRateChange<<1))
    {
        Run10ms();
        _myTimer.Tick10ms();
        _myTimer.CurrentTimeHMS(currentTime);
        if(_testMode == 2) printf("\r%s", currentTime);
        if(_inFileA.EndOfFile())
        {
            if(_inFileA.SamplingFrequency() == 16000)
            {
                if(_testMode != 0) printf("\nSide A switched to Send Super-Wideband\n");
                _inFileA.Close();
                _inFileA.Open(file_name_swb_, 32000, "rb");
                CHECK_ERROR(_acmA->RegisterSendCodec(_paramISAC32kHz));
            }
            else
            {
                if(_testMode != 0) printf("\nSide A switched to Send Wideband\n");
                _inFileA.Close();
                _inFileA.Open(file_name_swb_, 32000, "rb");
                CHECK_ERROR(_acmA->RegisterSendCodec(_paramISAC16kHz));
            }
            numSendCodecChanged++;
        }

        if(_inFileB.EndOfFile())
        {
            if(_inFileB.SamplingFrequency() == 16000)
            {
                if(_testMode != 0) printf("\nSide B switched to Send Super-Wideband\n");
                _inFileB.Close();
                _inFileB.Open(file_name_swb_, 32000, "rb");
                CHECK_ERROR(_acmB->RegisterSendCodec(_paramISAC32kHz));
            }
            else
            {
                if(_testMode != 0) printf("\nSide B switched to Send Wideband\n");
                _inFileB.Close();
                _inFileB.Open(file_name_swb_, 32000, "rb");
                CHECK_ERROR(_acmB->RegisterSendCodec(_paramISAC16kHz));
            }
            numSendCodecChanged++;
        }
    }
    _outFileA.Close();
    _outFileB.Close();
    _inFileA.Close();
    _inFileB.Close();
}

} // namespace webrtc
