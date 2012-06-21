/*
 *  Copyright (c) 2011 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "TestFEC.h"

#include <cassert>
#include <iostream>

#include "audio_coding_module_typedefs.h"
#include "common_types.h"
#include "engine_configurations.h"
#include "trace.h"
#include "testsupport/fileutils.h"
#include "utility.h"

namespace webrtc {

TestFEC::TestFEC(int testMode):
_acmA(NULL),
_acmB(NULL),
_channelA2B(NULL),
_testCntr(0)
{
    _testMode = testMode;
}

TestFEC::~TestFEC()
{
    if(_acmA != NULL)
    {
        AudioCodingModule::Destroy(_acmA);
        _acmA = NULL;
    }
    if(_acmB != NULL)
    {
        AudioCodingModule::Destroy(_acmB);
        _acmB = NULL;
    }
    if(_channelA2B != NULL)
    {
        delete _channelA2B;
        _channelA2B = NULL;
    }
}

void TestFEC::Perform()
{

    if(_testMode == 0)
    {
        printf("Running FEC Test");
        WEBRTC_TRACE(kTraceStateInfo, kTraceAudioCoding, -1,
                     "---------- TestFEC ----------");
    }
    char fileName[] = "./test/data/audio_coding/testfile32kHz.pcm";
    _inFileA.Open(fileName, 32000, "rb");


    bool fecEnabled;

    _acmA = AudioCodingModule::Create(0);
    _acmB = AudioCodingModule::Create(1);

    _acmA->InitializeReceiver();
    _acmB->InitializeReceiver();

    WebRtc_UWord8 numEncoders = _acmA->NumberOfCodecs();
    CodecInst myCodecParam;
    if(_testMode != 0)
    {
        printf("Registering codecs at receiver... \n");
    }
    for(WebRtc_UWord8 n = 0; n < numEncoders; n++)
    {
        _acmB->Codec(n, myCodecParam);
        if(_testMode != 0)
        {
            printf("%s\n", myCodecParam.plname);
        }
        _acmB->RegisterReceiveCodec(myCodecParam);
    }

    // Create and connect the channel
    _channelA2B = new Channel;    
    _acmA->RegisterTransportCallback(_channelA2B);
    _channelA2B->RegisterReceiverACM(_acmB);

    if(_testMode != 0)
    {
        printf("=======================================================================\n");
        printf("%d ",_testCntr++);
    }
    else
    {
        printf(".");
    }
#ifndef WEBRTC_CODEC_G722
    printf("G722 needs to be activated to run this test\n");
    exit(-1);
#endif
    char nameG722[] = "G722";
    RegisterSendCodec('A', nameG722, 16000);
    char nameCN[] = "CN";
    RegisterSendCodec('A', nameCN, 16000);
    char nameRED[] = "RED";
    RegisterSendCodec('A', nameRED);
    OpenOutFile(_testCntr);
    SetVAD(true, true, VADAggr);
    _acmA->SetFECStatus(false);
    fecEnabled = _acmA->FECStatus();
    if(_testMode != 0)
    {
        printf("FEC currently %s\n",(fecEnabled?"ON":"OFF"));
        DisplaySendReceiveCodec();
    }
    Run();
    _outFileB.Close();

    if(_testMode != 0)
    {
        printf("=======================================================================\n");
        printf("%d ",_testCntr++);
    }
    else
    {
        printf(".");
    }
    _acmA->SetFECStatus(true);
    fecEnabled = _acmA->FECStatus();
    if(_testMode != 0)
    {
        printf("FEC currently %s\n",(fecEnabled?"ON":"OFF"));
        DisplaySendReceiveCodec();
    }
    OpenOutFile(_testCntr);
    Run();
    _outFileB.Close();



    if(_testMode != 0)
    {
        printf("=======================================================================\n");
        printf("%d ",_testCntr++);
    }
    else
    {
        printf(".");
    }
    char nameISAC[] = "iSAC";
    RegisterSendCodec('A',nameISAC, 16000);
    OpenOutFile(_testCntr);
    SetVAD(true, true, VADVeryAggr);
    _acmA->SetFECStatus(false);
    fecEnabled = _acmA->FECStatus();
    if(_testMode != 0)
    {
        printf("FEC currently %s\n",(fecEnabled?"ON":"OFF"));
        DisplaySendReceiveCodec();
    }
    Run();
    _outFileB.Close();



    if(_testMode != 0)
    {
        printf("=======================================================================\n");
        printf("%d ",_testCntr++);
    }
    else
    {
        printf(".");
    }
    _acmA->SetFECStatus(true);
    fecEnabled = _acmA->FECStatus();
    if(_testMode != 0)
    {
        printf("FEC currently %s\n",(fecEnabled?"ON":"OFF"));
        DisplaySendReceiveCodec();
    }
    OpenOutFile(_testCntr);
    Run();
    _outFileB.Close();



    if(_testMode != 0)
    {
        printf("=======================================================================\n");
        printf("%d ",_testCntr++);
    }
    else
    {
        printf(".");
    }

    RegisterSendCodec('A', nameISAC, 32000);
    OpenOutFile(_testCntr);
    SetVAD(true, true, VADVeryAggr);
    _acmA->SetFECStatus(false);
    fecEnabled = _acmA->FECStatus();
    if(_testMode != 0)
    {
        printf("FEC currently %s\n",(fecEnabled?"ON":"OFF"));
        DisplaySendReceiveCodec();
    }
    Run();
    _outFileB.Close();



    if(_testMode != 0)
    {
        printf("=======================================================================\n");
        printf("%d ",_testCntr++);
    }
    else
    {
        printf(".");
    }
    _acmA->SetFECStatus(true);
    fecEnabled = _acmA->FECStatus();
    if(_testMode != 0)
    {
        printf("FEC currently %s\n",(fecEnabled?"ON":"OFF"));
        DisplaySendReceiveCodec();
    }
    OpenOutFile(_testCntr);
    Run();
    _outFileB.Close();




    if(_testMode != 0)
    {
        printf("=======================================================================\n");
        printf("%d ",_testCntr++);
    }
    else
    {
        printf(".");
    }

    RegisterSendCodec('A', nameISAC, 32000);
    OpenOutFile(_testCntr);
    SetVAD(false, false, VADNormal);
    _acmA->SetFECStatus(true);
    fecEnabled = _acmA->FECStatus();
    if(_testMode != 0)
    {
        printf("FEC currently %s\n",(fecEnabled?"ON":"OFF"));
        DisplaySendReceiveCodec();
    }
    Run();

    
    RegisterSendCodec('A', nameISAC, 16000);
    fecEnabled = _acmA->FECStatus();
    if(_testMode != 0)
    {
        printf("FEC currently %s\n",(fecEnabled?"ON":"OFF"));
        DisplaySendReceiveCodec();
    }
    Run();

    RegisterSendCodec('A', nameISAC, 32000);
    fecEnabled = _acmA->FECStatus();
    if(_testMode != 0)
    {
        printf("FEC currently %s\n",(fecEnabled?"ON":"OFF"));
        DisplaySendReceiveCodec();
    }
    Run();

    RegisterSendCodec('A', nameISAC, 16000);
    fecEnabled = _acmA->FECStatus();
    if(_testMode != 0)
    {
        printf("FEC currently %s\n",(fecEnabled?"ON":"OFF"));
        DisplaySendReceiveCodec();
    }
    Run();
    _outFileB.Close();







    _channelA2B->SetFECTestWithPacketLoss(true);



    if(_testMode != 0)
    {
        printf("=======================================================================\n");
        printf("%d ",_testCntr++);
    }
    else
    {
        printf(".");
    }

    RegisterSendCodec('A',nameG722);
    RegisterSendCodec('A', nameCN, 16000);
    OpenOutFile(_testCntr);
    SetVAD(true, true, VADAggr);
    _acmA->SetFECStatus(false);
    fecEnabled = _acmA->FECStatus();
    if(_testMode != 0)
    {
        printf("FEC currently %s\n",(fecEnabled?"ON":"OFF"));
        DisplaySendReceiveCodec();
    }
    Run();
    _outFileB.Close();



    if(_testMode != 0)
    {
        printf("=======================================================================\n");
        printf("%d ",_testCntr++);
    }
    else
    {
        printf(".");
    }
    _acmA->SetFECStatus(true);
    fecEnabled = _acmA->FECStatus();
    if(_testMode != 0)
    {
        printf("FEC currently %s\n",(fecEnabled?"ON":"OFF"));
        DisplaySendReceiveCodec();
    }
    OpenOutFile(_testCntr);
    Run();
    _outFileB.Close();



    if(_testMode != 0)
    {
        printf("=======================================================================\n");
        printf("%d ",_testCntr++);
    }
    else
    {
        printf(".");
    }
    RegisterSendCodec('A', nameISAC, 16000);
    OpenOutFile(_testCntr);
    SetVAD(true, true, VADVeryAggr);
    _acmA->SetFECStatus(false);
    fecEnabled = _acmA->FECStatus();
    if(_testMode != 0)
    {
        printf("FEC currently %s\n",(fecEnabled?"ON":"OFF"));
        DisplaySendReceiveCodec();
    }
    Run();
    _outFileB.Close();



    if(_testMode != 0)
    {
        printf("=======================================================================\n");
        printf("%d ",_testCntr++);
    }
    else
    {
        printf(".");
    }
    _acmA->SetFECStatus(true);
    fecEnabled = _acmA->FECStatus();
    if(_testMode != 0)
    {
        printf("FEC currently %s\n",(fecEnabled?"ON":"OFF"));
        DisplaySendReceiveCodec();
    }
    OpenOutFile(_testCntr);
    Run();
    _outFileB.Close();




    if(_testMode != 0)
    {
        printf("=======================================================================\n");
        printf("%d ",_testCntr++);
    }
    else
    {
        printf(".");
    }
    RegisterSendCodec('A', nameISAC, 32000);
    OpenOutFile(_testCntr);
    SetVAD(true, true, VADVeryAggr);
    _acmA->SetFECStatus(false);
    fecEnabled = _acmA->FECStatus();
    if(_testMode != 0)
    {
        printf("FEC currently %s\n",(fecEnabled?"ON":"OFF"));
        DisplaySendReceiveCodec();
    }
    Run();
    _outFileB.Close();



    if(_testMode != 0)
    {
        printf("=======================================================================\n");
        printf("%d ",_testCntr++);
    }
    else
    {
        printf(".");
    }
    _acmA->SetFECStatus(true);
    fecEnabled = _acmA->FECStatus();
    if(_testMode != 0)
    {
        printf("FEC currently %s\n",(fecEnabled?"ON":"OFF"));
        DisplaySendReceiveCodec();
    }
    OpenOutFile(_testCntr);
    Run();
    _outFileB.Close();






    if(_testMode != 0)
    {
        printf("=======================================================================\n");
        printf("%d ",_testCntr++);
    }
    else
    {
        printf(".");
    }
    RegisterSendCodec('A', nameISAC, 32000);
    OpenOutFile(_testCntr);
    SetVAD(false, false, VADNormal);
    _acmA->SetFECStatus(true);
    fecEnabled = _acmA->FECStatus();
    if(_testMode != 0)
    {
        printf("FEC currently %s\n",(fecEnabled?"ON":"OFF"));
        DisplaySendReceiveCodec();
    }
    Run();

    
    RegisterSendCodec('A', nameISAC, 16000);
    fecEnabled = _acmA->FECStatus();
    if(_testMode != 0)
    {
        printf("FEC currently %s\n",(fecEnabled?"ON":"OFF"));
        DisplaySendReceiveCodec();
    }
    Run();

    RegisterSendCodec('A', nameISAC, 32000);
    fecEnabled = _acmA->FECStatus();
    if(_testMode != 0)
    {
        printf("FEC currently %s\n",(fecEnabled?"ON":"OFF"));
        DisplaySendReceiveCodec();
    }
    Run();

    RegisterSendCodec('A', nameISAC, 16000);
    fecEnabled = _acmA->FECStatus();
    if(_testMode != 0)
    {
        printf("FEC currently %s\n",(fecEnabled?"ON":"OFF"));
        DisplaySendReceiveCodec();
    }
    Run();
    _outFileB.Close();



    if(_testMode == 0)
    {
        printf("Done!\n");
    }
}

WebRtc_Word32 TestFEC::SetVAD(bool enableDTX, bool enableVAD, ACMVADMode vadMode)
{
    if(_testMode != 0)
    {
        printf("DTX %s; VAD %s; VAD-Mode %d\n", 
            enableDTX? "ON":"OFF", 
            enableVAD? "ON":"OFF", 
            (WebRtc_Word16)vadMode);
    }
    return _acmA->SetVAD(enableDTX, enableVAD, vadMode);
}

WebRtc_Word16 TestFEC::RegisterSendCodec(char side, char* codecName, WebRtc_Word32 samplingFreqHz)
{
    if(_testMode != 0)
    {
        if(samplingFreqHz > 0)
        {
            printf("Registering %s-%d for side %c\n", codecName, samplingFreqHz, side);
        }
        else
        {
            printf("Registering %s for side %c\n", codecName, side);
        }
    }
    std::cout << std::flush;
    AudioCodingModule* myACM;
    switch(side)
    {
    case 'A':
        {
            myACM = _acmA;
            break;
        }
    case 'B':
        {
            myACM = _acmB;
            break;
        }
    default:
        return -1;
    }

    if(myACM == NULL)
    {
        assert(false);
        return -1;
    }
    CodecInst myCodecParam;

    CHECK_ERROR(AudioCodingModule::Codec(codecName, myCodecParam, samplingFreqHz));

    CHECK_ERROR(myACM->RegisterSendCodec(myCodecParam));

    // initialization was succesful
    return 0;
}

void TestFEC::Run()
{
    AudioFrame audioFrame;

    WebRtc_UWord16 msecPassed = 0;
    WebRtc_UWord32 secPassed  = 0;
    WebRtc_Word32 outFreqHzB = _outFileB.SamplingFrequency();

    while(!_inFileA.EndOfFile())
    {
        _inFileA.Read10MsData(audioFrame);
        CHECK_ERROR(_acmA->Add10MsData(audioFrame));
        CHECK_ERROR(_acmA->Process());
        CHECK_ERROR(_acmB->PlayoutData10Ms(outFreqHzB, audioFrame));
        _outFileB.Write10MsData(audioFrame._payloadData, audioFrame._payloadDataLengthInSamples);
        msecPassed += 10;
        if(msecPassed >= 1000)
        {
            msecPassed = 0;
            secPassed++;
        }
        if(((secPassed%5) == 4) && (msecPassed == 0) && (_testCntr > 14))
        {
            printf("%3u:%3u  ", secPassed, msecPassed);
            _acmA->SetFECStatus(false);
            printf("FEC currently %s\n",(_acmA->FECStatus()?"ON":"OFF"));
        }
        if(((secPassed%5) == 4) && (msecPassed >= 990) && (_testCntr > 14))
        {
            printf("%3u:%3u  ", secPassed, msecPassed);
            _acmA->SetFECStatus(true);
            printf("FEC currently %s\n",(_acmA->FECStatus()?"ON":"OFF"));
        }
    }
    _inFileA.Rewind();
}

void TestFEC::OpenOutFile(WebRtc_Word16 testNumber)
{
    char fileName[500];

    if(_testMode == 0)
    {
        sprintf(fileName, "%s/TestFEC_autoFile_%02d.pcm",
                webrtc::test::OutputPath().c_str(), testNumber);
    }
    else
    {
        sprintf(fileName, "%s/TestFEC_outFile_%02d.pcm",
                webrtc::test::OutputPath().c_str(), testNumber);
    }
    _outFileB.Open(fileName, 32000, "wb");
}

void TestFEC::DisplaySendReceiveCodec()
{
    CodecInst myCodecParam;
    _acmA->SendCodec(myCodecParam);
    printf("%s -> ", myCodecParam.plname);
    _acmB->ReceiveCodec(myCodecParam);
    printf("%s\n", myCodecParam.plname);
}

} // namespace webrtc
