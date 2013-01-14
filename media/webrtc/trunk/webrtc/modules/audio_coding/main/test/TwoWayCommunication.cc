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

    WebRtc_UWord16 frequencyHz;
    
    //--- Input A
    std::string in_file_name =
        webrtc::test::ResourcePath("audio_coding/testfile32kHz", "pcm");
    frequencyHz = 32000;
    printf("Enter input file at side A [%s]: ", in_file_name.c_str());
    PCMFile::ChooseFile(&in_file_name, 499, &frequencyHz);
    _inFileA.Open(in_file_name, frequencyHz, "rb");

    //--- Output A
    std::string out_file_a = webrtc::test::OutputPath() + "outA.pcm";
    printf("Output file at side A: %s\n", out_file_a.c_str());
    printf("Sampling frequency (in Hz) of the above file: %u\n",
           frequencyHz);
    _outFileA.Open(out_file_a, frequencyHz, "wb");
    std::string ref_file_name = webrtc::test::OutputPath() + "ref_outA.pcm";
    _outFileRefA.Open(ref_file_name, frequencyHz, "wb");

    //--- Input B
    in_file_name =
        webrtc::test::ResourcePath("audio_coding/testfile32kHz", "pcm");
    frequencyHz = 32000;
    printf("\n\nEnter input file at side B [%s]: ", in_file_name.c_str());
    PCMFile::ChooseFile(&in_file_name, 499, &frequencyHz);
    _inFileB.Open(in_file_name, frequencyHz, "rb");

    //--- Output B
    std::string out_file_b = webrtc::test::OutputPath() + "outB.pcm";
    printf("Output file at side B: %s\n", out_file_b.c_str());
    printf("Sampling frequency (in Hz) of the above file: %u\n",
           frequencyHz);
    _outFileB.Open(out_file_b, frequencyHz, "wb");
    ref_file_name = webrtc::test::OutputPath() + "ref_outB.pcm";
    _outFileRefB.Open(ref_file_name, frequencyHz, "wb");
    
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

    _acmA->Codec("ISAC", codecInst_A, 16000, 1);
    _acmB->Codec("L16", codecInst_B, 8000, 1);
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

    WebRtc_UWord16 frequencyHz;

    //--- Input A and B
    std::string in_file_name =
        webrtc::test::ResourcePath("audio_coding/testfile32kHz", "pcm");
    frequencyHz = 16000;
    _inFileA.Open(in_file_name, frequencyHz, "rb");
    _inFileB.Open(in_file_name, frequencyHz, "rb");

    //--- Output A
    std::string output_file_a = webrtc::test::OutputPath() + "outAutotestA.pcm";
    frequencyHz = 16000;
    _outFileA.Open(output_file_a, frequencyHz, "wb");
    std::string output_ref_file_a = webrtc::test::OutputPath() +
        "ref_outAutotestA.pcm";
    _outFileRefA.Open(output_ref_file_a, frequencyHz, "wb");

    //--- Output B
    std::string output_file_b = webrtc::test::OutputPath() + "outAutotestB.pcm";
    frequencyHz = 16000;
    _outFileB.Open(output_file_b, frequencyHz, "wb");
    std::string output_ref_file_b = webrtc::test::OutputPath() +
        "ref_outAutotestB.pcm";
    _outFileRefB.Open(output_ref_file_b, frequencyHz, "wb");

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
                             "---------- Errors expected");
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
                             "----- END: Errors expected");
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
                             "---------- Errors expected");
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
                             "----- END: Errors expected");
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
