/*
 *  Copyright (c) 2012 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "TwoWayCommunication.h"

#include <cctype>
#include <stdio.h>
#include <string.h>

#ifdef WIN32
#include <Windows.h>
#endif

#include "common_types.h"
#include "engine_configurations.h"
#include "gtest/gtest.h"
#include "PCMFile.h"
#include "trace.h"
#include "testsupport/fileutils.h"
#include "utility.h"

namespace webrtc {

#define MAX_FILE_NAME_LENGTH_BYTE 500

TwoWayCommunication::TwoWayCommunication(int testMode)
{
    _testMode = testMode;
}

TwoWayCommunication::~TwoWayCommunication()
{
    AudioCodingModule::Destroy(_acmA);
    AudioCodingModule::Destroy(_acmB);

    AudioCodingModule::Destroy(_acmRefA);
    AudioCodingModule::Destroy(_acmRefB);

    delete _channel_A2B;
    delete _channel_B2A;

    delete _channelRef_A2B;
    delete _channelRef_B2A;
#ifdef WEBRTC_DTMF_DETECTION
    if(_dtmfDetectorA != NULL)
    {
        delete _dtmfDetectorA;
    }
    if(_dtmfDetectorB != NULL)
    {
        delete _dtmfDetectorB;
    }
#endif
    _inFileA.Close();
    _inFileB.Close();
    _outFileA.Close();
    _outFileB.Close();
    _outFileRefA.Close();
    _outFileRefB.Close();
}


WebRtc_UWord8
TwoWayCommunication::ChooseCodec(WebRtc_UWord8* codecID_A,
                                 WebRtc_UWord8* codecID_B)
{
    AudioCodingModule* tmpACM = AudioCodingModule::Create(0);
    WebRtc_UWord8 noCodec = tmpACM->NumberOfCodecs();
    CodecInst codecInst;
    printf("List of Supported Codecs\n");
    printf("========================\n");
    for(WebRtc_UWord8 codecCntr = 0; codecCntr < noCodec; codecCntr++)
    {
        tmpACM->Codec(codecCntr, codecInst);
        printf("%d- %s\n", codecCntr, codecInst.plname);
    }
    printf("\nChoose a send codec for side A [0]: ");
    char myStr[15] = "";
    EXPECT_TRUE(fgets(myStr, 10, stdin) != NULL);
    *codecID_A = (WebRtc_UWord8)atoi(myStr);

    printf("\nChoose a send codec for side B [0]: ");
    EXPECT_TRUE(fgets(myStr, 10, stdin) != NULL);
    *codecID_B = (WebRtc_UWord8)atoi(myStr);

    AudioCodingModule::Destroy(tmpACM);
    printf("\n");
    return 0;
}

WebRtc_Word16
TwoWayCommunication::ChooseFile(char* fileName, WebRtc_Word16 maxLen,
                                WebRtc_UWord16* frequencyHz)
{
    char tmpName[MAX_FILE_NAME_LENGTH_BYTE];
    //strcpy(_fileName, "in.pcm");
    //printf("\n\nPlease enter the input file: ");
    EXPECT_TRUE(fgets(tmpName, MAX_FILE_NAME_LENGTH_BYTE, stdin) != NULL);
    tmpName[MAX_FILE_NAME_LENGTH_BYTE-1] = '\0';
    WebRtc_Word16 n = 0;

    // removing leading spaces
    while((isspace(tmpName[n]) || iscntrl(tmpName[n])) && 
        (tmpName[n] != 0) && 
        (n < MAX_FILE_NAME_LENGTH_BYTE))
    {
        n++;
    }
    if(n > 0)
    {
        memmove(tmpName, &tmpName[n], MAX_FILE_NAME_LENGTH_BYTE - n);
    }

    //removing trailing spaces
    n = (WebRtc_Word16)(strlen(tmpName) - 1);
    if(n >= 0)
    {
        while((isspace(tmpName[n]) || iscntrl(tmpName[n])) && 
            (n >= 0))
        {
            n--;
        }
    }
    if(n >= 0)
    {
        tmpName[n + 1] = '\0';
    }

    WebRtc_Word16 len = (WebRtc_Word16)strlen(tmpName);
    if(len > maxLen)
    {
        return -1;
    }    
    if(len > 0)
    {
        strncpy(fileName, tmpName, len+1);
    }
    printf("Enter the sampling frequency (in Hz) of the above file [%u]: ",
           *frequencyHz);
    EXPECT_TRUE(fgets(tmpName, 6, stdin) != NULL);
    WebRtc_UWord16 tmpFreq = (WebRtc_UWord16)atoi(tmpName);
    if(tmpFreq > 0)
    {
        *frequencyHz = tmpFreq;
    }
    return 0;
}

WebRtc_Word16 TwoWayCommunication::SetUp()
{
    _acmA = AudioCodingModule::Create(1);
    _acmB = AudioCodingModule::Create(2);

    _acmRefA = AudioCodingModule::Create(3);
    _acmRefB = AudioCodingModule::Create(4);

    WebRtc_UWord8 codecID_A;
    WebRtc_UWord8 codecID_B;

    ChooseCodec(&codecID_A, &codecID_B);
    CodecInst codecInst_A;
    CodecInst codecInst_B;
    CodecInst dummyCodec;
    _acmA->Codec(codecID_A, codecInst_A);
    _acmB->Codec(codecID_B, codecInst_B);

    _acmA->Codec(6, dummyCodec);

    //--- Set A codecs
    CHECK_ERROR(_acmA->RegisterSendCodec(codecInst_A));
    CHECK_ERROR(_acmA->RegisterReceiveCodec(codecInst_B));
#ifdef WEBRTC_DTMF_DETECTION
    _dtmfDetectorA = new(DTMFDetector);
    CHECK_ERROR(_acmA->RegisterIncomingMessagesCallback(_dtmfDetectorA,
                                                        ACMUSA));
#endif
    //--- Set ref-A codecs
    CHECK_ERROR(_acmRefA->RegisterSendCodec(codecInst_A));
    CHECK_ERROR(_acmRefA->RegisterReceiveCodec(codecInst_B));

    //--- Set B codecs
    CHECK_ERROR(_acmB->RegisterSendCodec(codecInst_B));
    CHECK_ERROR(_acmB->RegisterReceiveCodec(codecInst_A));
#ifdef WEBRTC_DTMF_DETECTION
    _dtmfDetectorB = new(DTMFDetector);
    CHECK_ERROR(_acmB->RegisterIncomingMessagesCallback(_dtmfDetectorB,
                                                        ACMUSA));
#endif

    //--- Set ref-B codecs
    CHECK_ERROR(_acmRefB->RegisterSendCodec(codecInst_B));
    CHECK_ERROR(_acmRefB->RegisterReceiveCodec(codecInst_A));

    char fileName[500];
    char refFileName[500];
    WebRtc_UWord16 frequencyHz;
    
    //--- Input A
    strcpy(fileName, "./test/data/audio_coding/testfile32kHz.pcm");
    frequencyHz = 32000;
    printf("Enter input file at side A [%s]: ", fileName);
    ChooseFile(fileName, 499, &frequencyHz);


    _inFileA.Open(fileName, frequencyHz, "rb");

    //--- Output A
    std::string outputFileA = webrtc::test::OutputPath() + "outA.pcm";
    strcpy(fileName, outputFileA.c_str());
    frequencyHz = 16000;
    printf("Enter output file at side A [%s]: ", fileName);
    ChooseFile(fileName, 499, &frequencyHz);
    _outFileA.Open(fileName, frequencyHz, "wb");
    strcpy(refFileName, "ref_");
    strcat(refFileName, fileName);
    _outFileRefA.Open(refFileName, frequencyHz, "wb");

    //--- Input B
    strcpy(fileName, "./test/data/audio_coding/testfile32kHz.pcm");
    frequencyHz = 32000;
    printf("\n\nEnter input file at side B [%s]: ", fileName);
    ChooseFile(fileName, 499, &frequencyHz);
    _inFileB.Open(fileName, frequencyHz, "rb");

    //--- Output B
    std::string outputFileB = webrtc::test::OutputPath() + "outB.pcm";
    strcpy(fileName, outputFileB.c_str());
    frequencyHz = 16000;
    printf("Enter output file at side B [%s]: ", fileName);
    ChooseFile(fileName, 499, &frequencyHz);
    _outFileB.Open(fileName, frequencyHz, "wb");
    strcpy(refFileName, "ref_");
    strcat(refFileName, fileName);
    _outFileRefB.Open(refFileName, frequencyHz, "wb");
    
    //--- Set A-to-B channel
    _channel_A2B = new Channel;
    _acmA->RegisterTransportCallback(_channel_A2B);
    _channel_A2B->RegisterReceiverACM(_acmB);
    //--- Do the same for the reference
    _channelRef_A2B = new Channel;
    _acmRefA->RegisterTransportCallback(_channelRef_A2B);
    _channelRef_A2B->RegisterReceiverACM(_acmRefB);

    //--- Set B-to-A channel
    _channel_B2A = new Channel;
    _acmB->RegisterTransportCallback(_channel_B2A);
    _channel_B2A->RegisterReceiverACM(_acmA);
    //--- Do the same for reference
    _channelRef_B2A = new Channel;
    _acmRefB->RegisterTransportCallback(_channelRef_B2A);
    _channelRef_B2A->RegisterReceiverACM(_acmRefA);

    // The clicks will be more obvious when we 
    // are in FAX mode.
    _acmB->SetPlayoutMode(fax);
    _acmRefB->SetPlayoutMode(fax);

    return 0;
}

WebRtc_Word16 TwoWayCommunication::SetUpAutotest()
{
    _acmA = AudioCodingModule::Create(1);
    _acmB = AudioCodingModule::Create(2);

    _acmRefA = AudioCodingModule::Create(3);
    _acmRefB = AudioCodingModule::Create(4);

    CodecInst codecInst_A;
    CodecInst codecInst_B;
    CodecInst dummyCodec;

    _acmA->Codec("ISAC", codecInst_A, 16000);
    _acmB->Codec("L16", codecInst_B, 8000);
    _acmA->Codec(6, dummyCodec);

    //--- Set A codecs
    CHECK_ERROR(_acmA->RegisterSendCodec(codecInst_A));
    CHECK_ERROR(_acmA->RegisterReceiveCodec(codecInst_B));
#ifdef WEBRTC_DTMF_DETECTION
    _dtmfDetectorA = new(DTMFDetector);
    CHECK_ERROR(_acmA->RegisterIncomingMessagesCallback(_dtmfDetectorA,
                                                        ACMUSA));
#endif

    //--- Set ref-A codecs
    CHECK_ERROR(_acmRefA->RegisterSendCodec(codecInst_A));
    CHECK_ERROR(_acmRefA->RegisterReceiveCodec(codecInst_B));

    //--- Set B codecs
    CHECK_ERROR(_acmB->RegisterSendCodec(codecInst_B));
    CHECK_ERROR(_acmB->RegisterReceiveCodec(codecInst_A));
#ifdef WEBRTC_DTMF_DETECTION
    _dtmfDetectorB = new(DTMFDetector);
    CHECK_ERROR(_acmB->RegisterIncomingMessagesCallback(_dtmfDetectorB,
                                                        ACMUSA));
#endif

    //--- Set ref-B codecs
    CHECK_ERROR(_acmRefB->RegisterSendCodec(codecInst_B));
    CHECK_ERROR(_acmRefB->RegisterReceiveCodec(codecInst_A));

    char fileName[500];
    char refFileName[500];
    WebRtc_UWord16 frequencyHz;


    //--- Input A
    strcpy(fileName, "./test/data/audio_coding/testfile32kHz.pcm");
    frequencyHz = 16000;
    _inFileA.Open(fileName, frequencyHz, "rb");

    //--- Output A
    std::string outputFileA = webrtc::test::OutputPath() + "outAutotestA.pcm";
    strcpy(fileName, outputFileA.c_str());
    frequencyHz = 16000;
    _outFileA.Open(fileName, frequencyHz, "wb");
    std::string outputRefFileA = webrtc::test::OutputPath() + "ref_outAutotestA.pcm";
    strcpy(refFileName, outputRefFileA.c_str());
    _outFileRefA.Open(refFileName, frequencyHz, "wb");

    //--- Input B
    strcpy(fileName, "./test/data/audio_coding/testfile32kHz.pcm");
    frequencyHz = 16000;
    _inFileB.Open(fileName, frequencyHz, "rb");

    //--- Output B
    std::string outputFileB = webrtc::test::OutputPath() + "outAutotestB.pcm";
    strcpy(fileName, outputFileB.c_str());
    frequencyHz = 16000;
    _outFileB.Open(fileName, frequencyHz, "wb");
    std::string outputRefFileB = webrtc::test::OutputPath() + "ref_outAutotestB.pcm";
    strcpy(refFileName, outputRefFileB.c_str());
    _outFileRefB.Open(refFileName, frequencyHz, "wb");

    //--- Set A-to-B channel
    _channel_A2B = new Channel;
    _acmA->RegisterTransportCallback(_channel_A2B);
    _channel_A2B->RegisterReceiverACM(_acmB);
    //--- Do the same for the reference
    _channelRef_A2B = new Channel;
    _acmRefA->RegisterTransportCallback(_channelRef_A2B);
    _channelRef_A2B->RegisterReceiverACM(_acmRefB);

    //--- Set B-to-A channel
    _channel_B2A = new Channel;
    _acmB->RegisterTransportCallback(_channel_B2A);
    _channel_B2A->RegisterReceiverACM(_acmA);
    //--- Do the same for reference
    _channelRef_B2A = new Channel;
    _acmRefB->RegisterTransportCallback(_channelRef_B2A);
    _channelRef_B2A->RegisterReceiverACM(_acmRefA);

    // The clicks will be more obvious when we 
    // are in FAX mode.
    _acmB->SetPlayoutMode(fax);
    _acmRefB->SetPlayoutMode(fax);

    return 0;
}

void
TwoWayCommunication::Perform()
{
    if(_testMode == 0)
    {
        printf("Running TwoWayCommunication Test");
        WEBRTC_TRACE(kTraceStateInfo, kTraceAudioCoding, -1,
                     "---------- TwoWayCommunication ----------");
        SetUpAutotest();
    }
    else
    {
        SetUp();
    }
    unsigned int msecPassed = 0;
    unsigned int secPassed  = 0;

    WebRtc_Word32 outFreqHzA = _outFileA.SamplingFrequency();
    WebRtc_Word32 outFreqHzB = _outFileB.SamplingFrequency();

    AudioFrame audioFrame;

    CodecInst codecInst_B;
    CodecInst dummy;

    _acmB->SendCodec(codecInst_B);

    if(_testMode != 0)
    {
        printf("\n");
        printf("sec:msec                   A                              B\n");
        printf("--------                 -----                        -----\n");
    }

    while(!_inFileA.EndOfFile() && !_inFileB.EndOfFile())
    {
        _inFileA.Read10MsData(audioFrame);
        _acmA->Add10MsData(audioFrame);
        _acmRefA->Add10MsData(audioFrame);

        _inFileB.Read10MsData(audioFrame);
        _acmB->Add10MsData(audioFrame);
        _acmRefB->Add10MsData(audioFrame);


        _acmA->Process();
        _acmB->Process();
        _acmRefA->Process();
        _acmRefB->Process();

        _acmA->PlayoutData10Ms(outFreqHzA, audioFrame);
        _outFileA.Write10MsData(audioFrame);

        _acmRefA->PlayoutData10Ms(outFreqHzA, audioFrame);
        _outFileRefA.Write10MsData(audioFrame);

        _acmB->PlayoutData10Ms(outFreqHzB, audioFrame);
        _outFileB.Write10MsData(audioFrame);

        _acmRefB->PlayoutData10Ms(outFreqHzB, audioFrame);
        _outFileRefB.Write10MsData(audioFrame);

        msecPassed += 10;
        if(msecPassed >= 1000)
        {
            msecPassed = 0;
            secPassed++;
        }
        if(((secPassed%5) == 4) && (msecPassed == 0))
        {
            if(_testMode != 0)
            {
                printf("%3u:%3u  ", secPassed, msecPassed);
            }
            _acmA->ResetEncoder();
            if(_testMode == 0)
            {
                WEBRTC_TRACE(kTraceStateInfo, kTraceAudioCoding, -1,
                             "---------- Errors epected");
                printf(".");
            }
            else
            {
                printf("Reset Encoder (click in side B)               ");
                printf("Initialize Sender (no audio in side A)\n");
            }
            CHECK_ERROR(_acmB->InitializeSender());
        }
        if(((secPassed%5) == 4) && (msecPassed >= 990))
        {
            if(_testMode == 0)
            {
                WEBRTC_TRACE(kTraceStateInfo, kTraceAudioCoding, -1,
                             "----- END: Errors epected");
                printf(".");
            }
            else
            {
                printf("%3u:%3u  ", secPassed, msecPassed);
                printf("                                              ");
                printf("Register Send Codec (audio back in side A)\n");
            }
            CHECK_ERROR(_acmB->RegisterSendCodec(codecInst_B));
            CHECK_ERROR(_acmB->SendCodec(dummy));
        }
        if(((secPassed%7) == 6) && (msecPassed == 0))
        {
            CHECK_ERROR(_acmB->ResetDecoder());
            if(_testMode == 0)
            {
                WEBRTC_TRACE(kTraceStateInfo, kTraceAudioCoding, -1,
                             "---------- Errors epected");
                printf(".");
            }
            else
            {
                printf("%3u:%3u  ", secPassed, msecPassed);
                printf("Initialize Receiver (no audio in side A)      ");
                printf("Reset Decoder\n");
            }
            CHECK_ERROR(_acmA->InitializeReceiver());
        }
        if(((secPassed%7) == 6) && (msecPassed >= 990))
        {
            if(_testMode == 0)
            {
                WEBRTC_TRACE(kTraceStateInfo, kTraceAudioCoding, -1,
                             "----- END: Errors epected");
                printf(".");
            }
            else
            {
                printf("%3u:%3u  ", secPassed, msecPassed);
                printf("Register Receive Coded (audio back in side A)\n");
            }
            CHECK_ERROR(_acmA->RegisterReceiveCodec(codecInst_B));
        }
        //Sleep(9);
    }
    if(_testMode == 0)
    {
        printf("Done!\n");
    }

#ifdef WEBRTC_DTMF_DETECTION
    printf("\nDTMF at Side A\n");
    _dtmfDetectorA->PrintDetectedDigits();

    printf("\nDTMF at Side B\n");
    _dtmfDetectorB->PrintDetectedDigits();
#endif

}

} // namespace webrtc
