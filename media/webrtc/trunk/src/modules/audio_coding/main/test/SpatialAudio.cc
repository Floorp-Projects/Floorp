/*
 *  Copyright (c) 2012 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include <stdio.h>
#include <string.h>

#include <math.h>

#include "common_types.h"
#include "SpatialAudio.h"
#include "trace.h"
#include "testsupport/fileutils.h"
#include "utility.h"

namespace webrtc {

#define NUM_PANN_COEFFS 10

SpatialAudio::SpatialAudio(int testMode)
{
    _testMode = testMode;
}

SpatialAudio::~SpatialAudio()
{
    AudioCodingModule::Destroy(_acmLeft);
    AudioCodingModule::Destroy(_acmRight);
    AudioCodingModule::Destroy(_acmReceiver);
    delete _channel;
    _inFile.Close();
    _outFile.Close();
}

WebRtc_Word16 
SpatialAudio::Setup()
{
    // Create ACMs and the Channel;
    _acmLeft = AudioCodingModule::Create(1);
    _acmRight = AudioCodingModule::Create(2);
    _acmReceiver = AudioCodingModule::Create(3);
    _channel = new Channel;

    // Register callback for the sender side.
    CHECK_ERROR(_acmLeft->RegisterTransportCallback(_channel));
    CHECK_ERROR(_acmRight->RegisterTransportCallback(_channel));
    // Register the receiver ACM in channel
    _channel->RegisterReceiverACM(_acmReceiver);

    char audioFileName[MAX_FILE_NAME_LENGTH_BYTE];
    WebRtc_UWord16 sampFreqHz = 32000;

    strncpy(audioFileName, "./test/data/audio_coding/testfile32kHz.pcm",
            MAX_FILE_NAME_LENGTH_BYTE - 1);
    if(_testMode == 1)
    {
        printf("Enter the input file [%s]: ", audioFileName);
        PCMFile::ChooseFile(audioFileName, MAX_FILE_NAME_LENGTH_BYTE, &sampFreqHz);
    }
    _inFile.Open(audioFileName, sampFreqHz, "rb", false);

    if(_testMode == 0)
    {
        std::string outputFile = webrtc::test::OutputPath() +
            "out_spatial_autotest.pcm";
        strncpy(audioFileName, outputFile.c_str(),
                MAX_FILE_NAME_LENGTH_BYTE - 1);
    }
    else if(_testMode == 1)
    {
        printf("\n");
        std::string outputFile = webrtc::test::OutputPath() +
            "testspatial_out.pcm";
        strncpy(audioFileName, outputFile.c_str(),
                MAX_FILE_NAME_LENGTH_BYTE - 1);
        printf("Enter the output file [%s]: ", audioFileName);
        PCMFile::ChooseFile(audioFileName, MAX_FILE_NAME_LENGTH_BYTE, &sampFreqHz);
    }
    else
    {
        std::string outputFile = webrtc::test::OutputPath() +
            "testspatial_out.pcm";
        strncpy(audioFileName, outputFile.c_str(),
                MAX_FILE_NAME_LENGTH_BYTE - 1);
    }
    _outFile.Open(audioFileName, sampFreqHz, "wb", false);
    _outFile.SaveStereo(true);


    // Register couple of codecs as receive codec    
    CodecInst codecInst;

    _acmLeft->Codec((WebRtc_UWord8)0, codecInst);    
    codecInst.channels = 2;
    CHECK_ERROR(_acmReceiver->RegisterReceiveCodec(codecInst));

    _acmLeft->Codec((WebRtc_UWord8)3, codecInst);    
    codecInst.channels = 2;
    CHECK_ERROR(_acmReceiver->RegisterReceiveCodec(codecInst));
 
    _acmLeft->Codec((WebRtc_UWord8)1, codecInst);
    CHECK_ERROR(_acmReceiver->RegisterReceiveCodec(codecInst));
    
    _acmLeft->Codec((WebRtc_UWord8)4, codecInst);
    CHECK_ERROR(_acmReceiver->RegisterReceiveCodec(codecInst));

    return 0;
}

void
SpatialAudio::Perform()
{
    if(_testMode == 0)
    {
        printf("Running SpatialAudio Test");
        WEBRTC_TRACE(webrtc::kTraceStateInfo, webrtc::kTraceAudioCoding, -1, "---------- SpatialAudio ----------");
    }

    Setup();

    CodecInst codecInst;
    _acmLeft->Codec((WebRtc_UWord8)1, codecInst);
    CHECK_ERROR(_acmLeft->RegisterSendCodec(codecInst));
    EncodeDecode();

    WebRtc_Word16 pannCntr = 0;

    double leftPanning[NUM_PANN_COEFFS] =  
        {1.00, 0.95, 0.90, 0.85, 0.80, 0.75, 0.70, 0.60, 0.55, 0.50};
    double rightPanning[NUM_PANN_COEFFS] = 
        {0.50, 0.55, 0.60, 0.70, 0.75, 0.80, 0.85, 0.90, 0.95, 1.00};

    while((pannCntr + 1) < NUM_PANN_COEFFS)
    {
        _acmLeft->Codec((WebRtc_UWord8)0, codecInst);    
        codecInst.pacsize = 480;
        CHECK_ERROR(_acmLeft->RegisterSendCodec(codecInst));
        CHECK_ERROR(_acmRight->RegisterSendCodec(codecInst));

        EncodeDecode(leftPanning[pannCntr], rightPanning[pannCntr]);
        pannCntr++;

        // Change codec    
        _acmLeft->Codec((WebRtc_UWord8)3, codecInst);    
        codecInst.pacsize = 320;
        CHECK_ERROR(_acmLeft->RegisterSendCodec(codecInst));
        CHECK_ERROR(_acmRight->RegisterSendCodec(codecInst));

        EncodeDecode(leftPanning[pannCntr], rightPanning[pannCntr]);
        pannCntr++;
        if(_testMode == 0)
        {
            printf(".");
        }
    }

    _acmLeft->Codec((WebRtc_UWord8)4, codecInst);
    CHECK_ERROR(_acmLeft->RegisterSendCodec(codecInst));
    EncodeDecode();

    _acmLeft->Codec((WebRtc_UWord8)0, codecInst);    
    codecInst.pacsize = 480;
    CHECK_ERROR(_acmLeft->RegisterSendCodec(codecInst));
    CHECK_ERROR(_acmRight->RegisterSendCodec(codecInst));
    pannCntr = NUM_PANN_COEFFS -1;
    while(pannCntr >= 0)
    {
        EncodeDecode(leftPanning[pannCntr], rightPanning[pannCntr]);
        pannCntr--;
        if(_testMode == 0)
        {
            printf(".");
        }
    }
    if(_testMode == 0)
    {
        printf("Done!\n");
    }
}

void 
SpatialAudio::EncodeDecode(
    const double leftPanning, 
    const double rightPanning)
{
    AudioFrame audioFrame;
    WebRtc_Word32 outFileSampFreq = _outFile.SamplingFrequency();

    const double rightToLeftRatio = rightPanning / leftPanning;

    _channel->SetIsStereo(true);

    while(!_inFile.EndOfFile())
    {
        _inFile.Read10MsData(audioFrame);
        for(int n = 0; n < audioFrame._payloadDataLengthInSamples; n++)
        {
            audioFrame._payloadData[n] = (WebRtc_Word16)floor(
                audioFrame._payloadData[n] * leftPanning + 0.5);
        }
        CHECK_ERROR(_acmLeft->Add10MsData(audioFrame));

        for(int n = 0; n < audioFrame._payloadDataLengthInSamples; n++)
        {
            audioFrame._payloadData[n] = (WebRtc_Word16)floor(
                audioFrame._payloadData[n] * rightToLeftRatio + 0.5);
        }
        CHECK_ERROR(_acmRight->Add10MsData(audioFrame));

        CHECK_ERROR(_acmLeft->Process());
        CHECK_ERROR(_acmRight->Process());

        CHECK_ERROR(_acmReceiver->PlayoutData10Ms(outFileSampFreq, audioFrame));
        _outFile.Write10MsData(audioFrame);
    }
    _inFile.Rewind();
}

void 
SpatialAudio::EncodeDecode()
{
    AudioFrame audioFrame;
    WebRtc_Word32 outFileSampFreq = _outFile.SamplingFrequency();

    _channel->SetIsStereo(false);

    while(!_inFile.EndOfFile())
    {
        _inFile.Read10MsData(audioFrame);
        CHECK_ERROR(_acmLeft->Add10MsData(audioFrame));

        CHECK_ERROR(_acmLeft->Process());

        CHECK_ERROR(_acmReceiver->PlayoutData10Ms(outFileSampFreq, audioFrame));
        _outFile.Write10MsData(audioFrame);
    }
    _inFile.Rewind();
}

} // namespace webrtc
