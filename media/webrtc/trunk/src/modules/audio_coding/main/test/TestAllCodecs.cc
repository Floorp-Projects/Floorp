/*
 *  Copyright (c) 2012 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "TestAllCodecs.h"

#include <cassert>
#include <iostream>

#include "audio_coding_module_typedefs.h"
#include "common_types.h"
#include "engine_configurations.h"
#include "testsupport/fileutils.h"
#include "trace.h"
#include "utility.h"

namespace webrtc {

// Class for simulating packet handling
TestPack::TestPack():
_receiverACM(NULL),
_seqNo(0),
_timeStampDiff(0),
_lastInTimestamp(0),
_totalBytes(0),
_payloadSize(0)
{
}
TestPack::~TestPack()
{
}

void 
TestPack::RegisterReceiverACM(AudioCodingModule* acm)
{
    _receiverACM = acm;
    return;
}
WebRtc_Word32 
TestPack::SendData(
        const FrameType       frameType,
        const WebRtc_UWord8   payloadType,
        const WebRtc_UWord32  timeStamp,
        const WebRtc_UWord8*  payloadData, 
        const WebRtc_UWord16  payloadSize,
        const RTPFragmentationHeader* fragmentation)
{
    WebRtcRTPHeader rtpInfo;
    WebRtc_Word32   status;
    WebRtc_UWord16  payloadDataSize = payloadSize;

    rtpInfo.header.markerBit = false;
    rtpInfo.header.ssrc = 0;
    rtpInfo.header.sequenceNumber = _seqNo++;
    rtpInfo.header.payloadType = payloadType;
    rtpInfo.header.timestamp = timeStamp;
    if(frameType == kAudioFrameCN)
    {
        rtpInfo.type.Audio.isCNG = true;
    }
    else
    {
        rtpInfo.type.Audio.isCNG = false;
    }
    if(frameType == kFrameEmpty)
    {
        // Skip this frame
        return 0;
    }

    rtpInfo.type.Audio.channel = 1;
    memcpy(_payloadData, payloadData, payloadDataSize);
    
    status = _receiverACM->IncomingPacket(_payloadData, payloadDataSize,
                                          rtpInfo);

    _payloadSize = payloadDataSize;
    _timeStampDiff = timeStamp - _lastInTimestamp;
    _lastInTimestamp = timeStamp;
    _totalBytes += payloadDataSize;
    return status;
}

WebRtc_UWord16
TestPack::GetPayloadSize()
{
    return _payloadSize;
}


WebRtc_UWord32
TestPack::GetTimeStampDiff()
{
    return _timeStampDiff;
}

void 
TestPack::ResetPayloadSize()
{
    _payloadSize = 0;
}

TestAllCodecs::TestAllCodecs(int testMode):
_acmA(NULL),
_acmB(NULL),
_channelA2B(NULL),
_testCntr(0),
_packSizeSamp(0),
_packSizeBytes(0),
_counter(0)
{
    // testMode = 0 for silent test (auto test)
    _testMode = testMode;
}

TestAllCodecs::~TestAllCodecs()
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

void TestAllCodecs::Perform()
{

    char file[] = "./test/data/audio_coding/testfile32kHz.pcm";
    _inFileA.Open(file, 32000, "rb");

    if(_testMode == 0)
    {
        printf("Running All Codecs Test");
        WEBRTC_TRACE(kTraceStateInfo, kTraceAudioCoding, -1,
                     "---------- TestAllCodecs ----------");
    }

    _acmA = AudioCodingModule::Create(0);
    _acmB = AudioCodingModule::Create(1);

    _acmA->InitializeReceiver();
    _acmB->InitializeReceiver();

    WebRtc_UWord8 numEncoders = _acmA->NumberOfCodecs();
    CodecInst myCodecParam;
 
    for(WebRtc_UWord8 n = 0; n < numEncoders; n++)
    {
        _acmB->Codec(n, myCodecParam);
        _acmB->RegisterReceiveCodec(myCodecParam);
    }

    // Create and connect the channel
    _channelA2B = new TestPack;    
    _acmA->RegisterTransportCallback(_channelA2B);
    _channelA2B->RegisterReceiverACM(_acmB);

    // All codecs are tested for all allowed sampling frequencies, rates and packet sizes
#ifdef WEBRTC_CODEC_GSMAMR
    if(_testMode != 0) {
        printf("=======================================================================\n");
    } else {
        printf(".");
    }
    _testCntr++;
    OpenOutFile(_testCntr);
    char codecAMR[] = "AMR";
    RegisterSendCodec('A', codecAMR, 8000, 4750, 160, 2);
    Run(_channelA2B);
    RegisterSendCodec('A', codecAMR, 8000, 4750, 320, 2);
    Run(_channelA2B);
    RegisterSendCodec('A', codecAMR, 8000, 4750, 480, 3);
    Run(_channelA2B);
    RegisterSendCodec('A', codecAMR, 8000, 5150, 160, 2);
    Run(_channelA2B);
    RegisterSendCodec('A', codecAMR, 8000, 5150, 320, 2);
    Run(_channelA2B);
    RegisterSendCodec('A', codecAMR, 8000, 5150, 480, 3);
    Run(_channelA2B);
    RegisterSendCodec('A', codecAMR, 8000, 5900, 160, 1);
    Run(_channelA2B);
    RegisterSendCodec('A', codecAMR, 8000, 5900, 320, 2);
    Run(_channelA2B);
    RegisterSendCodec('A', codecAMR, 8000, 5900, 480, 2);
    Run(_channelA2B);
    RegisterSendCodec('A', codecAMR, 8000, 6700, 160, 1);
    Run(_channelA2B);
    RegisterSendCodec('A', codecAMR, 8000, 6700, 320, 2);
    Run(_channelA2B);
    RegisterSendCodec('A', codecAMR, 8000, 6700, 480, 2);
    Run(_channelA2B);
    RegisterSendCodec('A', codecAMR, 8000, 7400, 160, 1);
    Run(_channelA2B);
    RegisterSendCodec('A', codecAMR, 8000, 7400, 320, 2);
    Run(_channelA2B);
    RegisterSendCodec('A', codecAMR, 8000, 7400, 480, 3);
    Run(_channelA2B);
    RegisterSendCodec('A', codecAMR, 8000, 7950, 160, 2);
    Run(_channelA2B);
    RegisterSendCodec('A', codecAMR, 8000, 7950, 320, 2);
    Run(_channelA2B);
    RegisterSendCodec('A', codecAMR, 8000, 7950, 480, 3);
    Run(_channelA2B);
    RegisterSendCodec('A', codecAMR, 8000, 10200, 160, 1);
    Run(_channelA2B);
    RegisterSendCodec('A', codecAMR, 8000, 10200, 320, 2);
    Run(_channelA2B);
    RegisterSendCodec('A', codecAMR, 8000, 10200, 480, 3);
    Run(_channelA2B);
    RegisterSendCodec('A', codecAMR, 8000, 12200, 160, 1);
    Run(_channelA2B);
    RegisterSendCodec('A', codecAMR, 8000, 12200, 320, 2);
    Run(_channelA2B);
    RegisterSendCodec('A', codecAMR, 8000, 12200, 480, 3);
    Run(_channelA2B);
    _outFileB.Close();
#endif
#ifdef WEBRTC_CODEC_GSMAMRWB
    if(_testMode != 0) {
        printf("=======================================================================\n");
    } else {
        printf(".");
    }
    _testCntr++;
    char codecAMRWB[] = "AMR-WB";
    OpenOutFile(_testCntr);
    RegisterSendCodec('A', codecAMRWB, 16000, 7000, 320, 0);
    Run(_channelA2B);
    RegisterSendCodec('A', codecAMRWB, 16000, 7000, 640, 0);
    Run(_channelA2B);
    RegisterSendCodec('A', codecAMRWB, 16000, 7000, 960, 0);
    Run(_channelA2B);
    RegisterSendCodec('A', codecAMRWB, 16000, 9000, 320, 1);
    Run(_channelA2B);
    RegisterSendCodec('A', codecAMRWB, 16000, 9000, 640, 2);
    Run(_channelA2B);
    RegisterSendCodec('A', codecAMRWB, 16000, 9000, 960, 2);
    Run(_channelA2B);
    RegisterSendCodec('A', codecAMRWB, 16000, 12000, 320, 3);
    Run(_channelA2B);
    RegisterSendCodec('A', codecAMRWB, 16000, 12000, 640, 6);
    Run(_channelA2B);
    RegisterSendCodec('A', codecAMRWB, 16000, 12000, 960, 8);
    Run(_channelA2B);
    RegisterSendCodec('A', codecAMRWB, 16000, 14000, 320, 2);
    Run(_channelA2B);
    RegisterSendCodec('A', codecAMRWB, 16000, 14000, 640, 4);
    Run(_channelA2B);
    RegisterSendCodec('A', codecAMRWB, 16000, 14000, 960, 5);
    Run(_channelA2B);
    RegisterSendCodec('A', codecAMRWB, 16000, 16000, 320, 1);
    Run(_channelA2B);
    RegisterSendCodec('A', codecAMRWB, 16000, 16000, 640, 2);
    Run(_channelA2B);
    RegisterSendCodec('A', codecAMRWB, 16000, 16000, 960, 2);
    Run(_channelA2B);
    RegisterSendCodec('A', codecAMRWB, 16000, 18000, 320, 2);
    Run(_channelA2B);
    RegisterSendCodec('A', codecAMRWB, 16000, 18000, 640, 4);
    Run(_channelA2B);
    RegisterSendCodec('A', codecAMRWB, 16000, 18000, 960, 5);
    Run(_channelA2B);
    RegisterSendCodec('A', codecAMRWB, 16000, 20000, 320, 1);
    Run(_channelA2B);
    RegisterSendCodec('A', codecAMRWB, 16000, 20000, 640, 2);
    Run(_channelA2B);
    RegisterSendCodec('A', codecAMRWB, 16000, 20000, 960, 2);
    Run(_channelA2B);
    RegisterSendCodec('A', codecAMRWB, 16000, 23000, 320, 1);
    Run(_channelA2B);
    RegisterSendCodec('A', codecAMRWB, 16000, 23000, 640, 3);
    Run(_channelA2B);
    RegisterSendCodec('A', codecAMRWB, 16000, 23000, 960, 3);
    Run(_channelA2B);
    RegisterSendCodec('A', codecAMRWB, 16000, 24000, 320, 1);
    Run(_channelA2B);
    RegisterSendCodec('A', codecAMRWB, 16000, 24000, 640, 2);
    Run(_channelA2B);
    RegisterSendCodec('A', codecAMRWB, 16000, 24000, 960, 2);
    Run(_channelA2B);
    _outFileB.Close();
#endif
#ifdef WEBRTC_CODEC_G722
    if(_testMode != 0) {
        printf("=======================================================================\n");
    } else {
        printf(".");
    }
    _testCntr++;
    OpenOutFile(_testCntr);
    char codecG722[] = "G722";
    RegisterSendCodec('A', codecG722, 16000, 64000, 160, 0);
    Run(_channelA2B);
    RegisterSendCodec('A', codecG722, 16000, 64000, 320, 0);
    Run(_channelA2B);
    RegisterSendCodec('A', codecG722, 16000, 64000, 480, 0);
    Run(_channelA2B);
    RegisterSendCodec('A', codecG722, 16000, 64000, 640, 0);
    Run(_channelA2B);
    RegisterSendCodec('A', codecG722, 16000, 64000, 800, 0);
    Run(_channelA2B);
    RegisterSendCodec('A', codecG722, 16000, 64000, 960, 0);
    Run(_channelA2B);
    _outFileB.Close();
#endif
#ifdef WEBRTC_CODEC_G722_1
    if(_testMode != 0) {
        printf("=======================================================================\n");
    } else {
        printf(".");
    }
    _testCntr++;
    OpenOutFile(_testCntr);
    char codecG7221_1[] = "G7221";
    RegisterSendCodec('A', codecG7221_1, 16000, 32000, 320, 0);
    Run(_channelA2B);
    RegisterSendCodec('A', codecG7221_1, 16000, 24000, 320, 0);
    Run(_channelA2B);
    RegisterSendCodec('A', codecG7221_1, 16000, 16000, 320, 0);
    Run(_channelA2B);
    _outFileB.Close();
#endif
#ifdef WEBRTC_CODEC_G722_1C
    if(_testMode != 0) {
        printf("=======================================================================\n");
    } else {
        printf(".");
    }
    _testCntr++;
    OpenOutFile(_testCntr);
    char codecG7221_2[] = "G7221";
    RegisterSendCodec('A', codecG7221_2, 32000, 48000, 640, 0);
    Run(_channelA2B);
    RegisterSendCodec('A', codecG7221_2, 32000, 32000, 640, 0);
    Run(_channelA2B);
    RegisterSendCodec('A', codecG7221_2, 32000, 24000, 640, 0);
    Run(_channelA2B);
    _outFileB.Close();
#endif
#ifdef WEBRTC_CODEC_G729
    if(_testMode != 0) {
        printf("=======================================================================\n");
    } else {
        printf(".");
    }
    _testCntr++;
    OpenOutFile(_testCntr);
    char codecG729[] = "G729";
    RegisterSendCodec('A', codecG729, 8000, 8000, 80, 0);
    Run(_channelA2B);
    RegisterSendCodec('A', codecG729, 8000, 8000, 160, 0);
    Run(_channelA2B);
    RegisterSendCodec('A', codecG729, 8000, 8000, 240, 0);
    Run(_channelA2B);
    RegisterSendCodec('A', codecG729, 8000, 8000, 320, 0);
    Run(_channelA2B);
    RegisterSendCodec('A', codecG729, 8000, 8000, 400, 0);
    Run(_channelA2B);
    RegisterSendCodec('A', codecG729, 8000, 8000, 480, 0);
    Run(_channelA2B);
    _outFileB.Close();
#endif
#ifdef WEBRTC_CODEC_G729_1
    if(_testMode != 0) {
        printf("=======================================================================\n");
    } else {
        printf(".");
    }
    _testCntr++;
    OpenOutFile(_testCntr);
    char codecG7291[] = "G7291";
    RegisterSendCodec('A', codecG7291, 16000, 8000, 320, 1);
    Run(_channelA2B);
    RegisterSendCodec('A', codecG7291, 16000, 8000, 640, 1);
    Run(_channelA2B);
    RegisterSendCodec('A', codecG7291, 16000, 8000, 960, 1);
    Run(_channelA2B);
    RegisterSendCodec('A', codecG7291, 16000, 12000, 320, 1);
    Run(_channelA2B);
    RegisterSendCodec('A', codecG7291, 16000, 12000, 640, 1);
    Run(_channelA2B);
    RegisterSendCodec('A', codecG7291, 16000, 12000, 960, 1);
    Run(_channelA2B);
    RegisterSendCodec('A', codecG7291, 16000, 14000, 320, 1);
    Run(_channelA2B);
    RegisterSendCodec('A', codecG7291, 16000, 14000, 640, 1);
    Run(_channelA2B);
    RegisterSendCodec('A', codecG7291, 16000, 14000, 960, 1);
    Run(_channelA2B);
    RegisterSendCodec('A', codecG7291, 16000, 16000, 320, 1);
    Run(_channelA2B);
    RegisterSendCodec('A', codecG7291, 16000, 16000, 640, 1);
    Run(_channelA2B);
    RegisterSendCodec('A', codecG7291, 16000, 16000, 960, 1);
    Run(_channelA2B);
    RegisterSendCodec('A', codecG7291, 16000, 18000, 320, 1);
    Run(_channelA2B);
    RegisterSendCodec('A', codecG7291, 16000, 18000, 640, 1);
    Run(_channelA2B);
    RegisterSendCodec('A', codecG7291, 16000, 18000, 960, 1);
    Run(_channelA2B);
    RegisterSendCodec('A', codecG7291, 16000, 20000, 320, 1);
    Run(_channelA2B);
    RegisterSendCodec('A', codecG7291, 16000, 20000, 640, 1);
    Run(_channelA2B);
    RegisterSendCodec('A', codecG7291, 16000, 20000, 960, 1);
    Run(_channelA2B);
    RegisterSendCodec('A', codecG7291, 16000, 22000, 320, 1);
    Run(_channelA2B);
    RegisterSendCodec('A', codecG7291, 16000, 22000, 640, 1);
    Run(_channelA2B);
    RegisterSendCodec('A', codecG7291, 16000, 22000, 960, 1);
    Run(_channelA2B);
    RegisterSendCodec('A', codecG7291, 16000, 24000, 320, 1);
    Run(_channelA2B);
    RegisterSendCodec('A', codecG7291, 16000, 24000, 640, 1);
    Run(_channelA2B);
    RegisterSendCodec('A', codecG7291, 16000, 24000, 960, 1);
    Run(_channelA2B);
    RegisterSendCodec('A', codecG7291, 16000, 26000, 320, 1);
    Run(_channelA2B);
    RegisterSendCodec('A', codecG7291, 16000, 26000, 640, 1);
    Run(_channelA2B);
    RegisterSendCodec('A', codecG7291, 16000, 26000, 960, 1);
    Run(_channelA2B);
    RegisterSendCodec('A', codecG7291, 16000, 28000, 320, 1);
    Run(_channelA2B);
    RegisterSendCodec('A', codecG7291, 16000, 28000, 640, 1);
    Run(_channelA2B);
    RegisterSendCodec('A', codecG7291, 16000, 28000, 960, 1);
    Run(_channelA2B);
    RegisterSendCodec('A', codecG7291, 16000, 30000, 320, 1);
    Run(_channelA2B);
    RegisterSendCodec('A', codecG7291, 16000, 30000, 640, 1);
    Run(_channelA2B);
    RegisterSendCodec('A', codecG7291, 16000, 30000, 960, 1);
    Run(_channelA2B);
    RegisterSendCodec('A', codecG7291, 16000, 32000, 320, 1);
    Run(_channelA2B);
    RegisterSendCodec('A', codecG7291, 16000, 32000, 640, 1);
    Run(_channelA2B);
    RegisterSendCodec('A', codecG7291, 16000, 32000, 960, 1);
    Run(_channelA2B);
    _outFileB.Close();
#endif
#ifdef WEBRTC_CODEC_GSMFR
    if(_testMode != 0) {
        printf("=======================================================================\n");
    } else {
        printf(".");
    }
    _testCntr++;
    OpenOutFile(_testCntr);
    char codecGSM[] = "GSM";
    RegisterSendCodec('A', codecGSM, 8000, 13200, 160, 0);
    Run(_channelA2B);
    RegisterSendCodec('A', codecGSM, 8000, 13200, 320, 0);
    Run(_channelA2B);
    RegisterSendCodec('A', codecGSM, 8000, 13200, 480, 0);
    Run(_channelA2B);
    _outFileB.Close();
#endif
#ifdef WEBRTC_CODEC_ILBC
    if(_testMode != 0) {
        printf("=======================================================================\n");
    } else {
        printf(".");
    }
    _testCntr++;
    OpenOutFile(_testCntr);
    char codecILBC[] = "ILBC";
    RegisterSendCodec('A', codecILBC, 8000, 13300, 240, 0);
    Run(_channelA2B);
    RegisterSendCodec('A', codecILBC, 8000, 13300, 480, 0);
    Run(_channelA2B);
    RegisterSendCodec('A', codecILBC, 8000, 15200, 160, 0);
    Run(_channelA2B);
    RegisterSendCodec('A', codecILBC, 8000, 15200, 320, 0);
    Run(_channelA2B);
    _outFileB.Close();
#endif
#if (defined(WEBRTC_CODEC_ISAC) || defined(WEBRTC_CODEC_ISACFX))
    if(_testMode != 0) {
        printf("=======================================================================\n");
    } else {
        printf(".");
    }
    _testCntr++;
    OpenOutFile(_testCntr);
    char codecISAC[] = "ISAC";
    RegisterSendCodec('A', codecISAC, 16000, -1, 480, -1);
    Run(_channelA2B);
    RegisterSendCodec('A', codecISAC, 16000, -1, 960, -1);
    Run(_channelA2B);
    RegisterSendCodec('A', codecISAC, 16000, 15000, 480, -1);
    Run(_channelA2B);
    RegisterSendCodec('A', codecISAC, 16000, 32000, 960, -1);
    Run(_channelA2B);
    _outFileB.Close();
#endif
#ifdef WEBRTC_CODEC_ISAC
    if(_testMode != 0) {
        printf("=======================================================================\n");
    } else {
        printf(".");
    }
    _testCntr++;
    OpenOutFile(_testCntr);
    RegisterSendCodec('A', codecISAC, 32000, -1, 960, -1);
    Run(_channelA2B);
    RegisterSendCodec('A', codecISAC, 32000, 56000, 960, -1);
    Run(_channelA2B);
    RegisterSendCodec('A', codecISAC, 32000, 37000, 960, -1);
    Run(_channelA2B);
    RegisterSendCodec('A', codecISAC, 32000, 32000, 960, -1);
    Run(_channelA2B);
    _outFileB.Close();
#endif
#ifdef WEBRTC_CODEC_PCM16
    if(_testMode != 0) {
        printf("=======================================================================\n");
    } else {
        printf(".");
    }
    _testCntr++; 
    OpenOutFile(_testCntr);
    char codecL16[] = "L16";
    RegisterSendCodec('A', codecL16, 8000, 128000, 80, 0);
    Run(_channelA2B);
    RegisterSendCodec('A', codecL16, 8000, 128000, 160, 0);
    Run(_channelA2B);
    RegisterSendCodec('A', codecL16, 8000, 128000, 240, 0);
    Run(_channelA2B);
    RegisterSendCodec('A', codecL16, 8000, 128000, 320, 0);
    Run(_channelA2B);
    _outFileB.Close();
    if(_testMode != 0) {
        printf("=======================================================================\n");
    } else {
        printf(".");
    }
    _testCntr++;  
    OpenOutFile(_testCntr);
    RegisterSendCodec('A', codecL16, 16000, 256000, 160, 0);
    Run(_channelA2B);
    RegisterSendCodec('A', codecL16, 16000, 256000, 320, 0);
    Run(_channelA2B);
    RegisterSendCodec('A', codecL16, 16000, 256000, 480, 0);
    Run(_channelA2B);
    RegisterSendCodec('A', codecL16, 16000, 256000, 640, 0);
    Run(_channelA2B);
    _outFileB.Close();
    if(_testMode != 0) {
        printf("=======================================================================\n");
    } else {
        printf(".");
    }
    _testCntr++; 
    OpenOutFile(_testCntr);
    RegisterSendCodec('A', codecL16, 32000, 512000, 320, 0);
    Run(_channelA2B);
    RegisterSendCodec('A', codecL16, 32000, 512000, 640, 0);
    Run(_channelA2B);
    _outFileB.Close();
#endif
    if(_testMode != 0) {
        printf("=======================================================================\n");
    } else {
        printf(".");
    }
    _testCntr++;
    OpenOutFile(_testCntr);
    char codecPCMA[] = "PCMA";
    RegisterSendCodec('A', codecPCMA, 8000, 64000, 80, 0);
    Run(_channelA2B);
    RegisterSendCodec('A', codecPCMA, 8000, 64000, 160, 0);
    Run(_channelA2B);
    RegisterSendCodec('A', codecPCMA, 8000, 64000, 240, 0);
    Run(_channelA2B);
    RegisterSendCodec('A', codecPCMA, 8000, 64000, 320, 0);
    Run(_channelA2B);
    RegisterSendCodec('A', codecPCMA, 8000, 64000, 400, 0);
    Run(_channelA2B);
    RegisterSendCodec('A', codecPCMA, 8000, 64000, 480, 0);
    Run(_channelA2B);
    if(_testMode != 0) {
        printf("=======================================================================\n");
    } else {
        printf(".");
    }
    char codecPCMU[] = "PCMU";
    RegisterSendCodec('A', codecPCMU, 8000, 64000, 80, 0);
    Run(_channelA2B);
    RegisterSendCodec('A', codecPCMU, 8000, 64000, 160, 0);
    Run(_channelA2B);
    RegisterSendCodec('A', codecPCMU, 8000, 64000, 240, 0);
    Run(_channelA2B);
    RegisterSendCodec('A', codecPCMU, 8000, 64000, 320, 0);
    Run(_channelA2B);
    RegisterSendCodec('A', codecPCMU, 8000, 64000, 400, 0);
    Run(_channelA2B);
    RegisterSendCodec('A', codecPCMU, 8000, 64000, 480, 0);
    Run(_channelA2B);
    _outFileB.Close();
#ifdef WEBRTC_CODEC_SPEEX
    if(_testMode != 0) {
        printf("=======================================================================\n");
    } else {
        printf(".");
    }
    _testCntr++;   
    OpenOutFile(_testCntr);
    char codecSPEEX[] = "SPEEX";
    RegisterSendCodec('A', codecSPEEX, 8000, 2400, 160, 0);
    Run(_channelA2B);
    RegisterSendCodec('A', codecSPEEX, 8000, 8000, 320, 0);
    Run(_channelA2B);
    RegisterSendCodec('A', codecSPEEX, 8000, 18200, 480, 0);
    Run(_channelA2B);
    _outFileB.Close();

    if(_testMode != 0) {
        printf("=======================================================================\n");
    } else {
        printf(".");
    }
    _testCntr++;  
    OpenOutFile(_testCntr);
    RegisterSendCodec('A', codecSPEEX, 16000, 4000, 320, 0);
    Run(_channelA2B);
    RegisterSendCodec('A', codecSPEEX, 16000, 12800, 640, 0);
    Run(_channelA2B);
    RegisterSendCodec('A', codecSPEEX, 16000, 34200, 960, 0);
    Run(_channelA2B);
    _outFileB.Close();
#endif
#ifdef WEBRTC_CODEC_CELT
    if(_testMode != 0) {
        printf("=======================================================================\n");
    } else {
        printf(".");
    }
    _testCntr++;
    OpenOutFile(_testCntr);
    char codecCELT_32[] = "CELT";
    RegisterSendCodec('A', codecCELT_32, 32000, 48000, 320, 0);
    Run(_channelA2B);
    RegisterSendCodec('A', codecCELT_32, 32000, 64000, 320, 0);
    Run(_channelA2B);
    RegisterSendCodec('A', codecCELT_32, 32000, 128000, 320, 0);
    Run(_channelA2B);
    _outFileB.Close();
#endif
    if(_testMode != 0) {
        printf("=======================================================================\n");
    } else {
        printf("Done!\n");
    }

    /* Print out all codecs that were not tested in the run */
    if(_testMode != 0) {
        printf("The following codecs was not included in the test:\n");
#ifndef WEBRTC_CODEC_GSMAMR
        printf("   GSMAMR\n");
#endif
#ifndef WEBRTC_CODEC_GSMAMRWB
        printf("   GSMAMR-wb\n");
#endif
#ifndef WEBRTC_CODEC_G722
        printf("   G.722\n");
#endif
#ifndef WEBRTC_CODEC_G722_1
        printf("   G.722.1\n");
#endif
#ifndef WEBRTC_CODEC_G722_1C
        printf("   G.722.1C\n");
#endif
#ifndef WEBRTC_CODEC_G729
        printf("   G.729\n");
#endif
#ifndef WEBRTC_CODEC_G729_1
        printf("   G.729.1\n");
#endif
#ifndef WEBRTC_CODEC_GSMFR
        printf("   GSMFR\n");
#endif
#ifndef WEBRTC_CODEC_ILBC
        printf("   iLBC\n");
#endif
#ifndef WEBRTC_CODEC_ISAC
        printf("   ISAC float\n");
#endif
#ifndef WEBRTC_CODEC_ISACFX
        printf("   ISAC fix\n");
#endif
#ifndef WEBRTC_CODEC_PCM16
        printf("   PCM16\n");
#endif
#ifndef WEBRTC_CODEC_SPEEX
        printf("   Speex\n");
#endif

        printf("\nTo complete the test, listen to the %d number of output files.\n", _testCntr);
    }
}

// Register Codec to use in the test
//
// Input:   side            - which ACM to use, 'A' or 'B'
//          codecName       - name to use when register the codec
//          samplingFreqHz  - sampling frequency in Herz
//          rate            - bitrate in bytes
//          packSize        - packet size in samples
//          extraByte       - if extra bytes needed compared to the bitrate 
//                            used when registering, can be an internal header
//                            set to -1 if the codec is a variable rate codec
WebRtc_Word16 TestAllCodecs::RegisterSendCodec(char side, 
                                             char* codecName, 
                                             WebRtc_Word32 samplingFreqHz,
                                             int rate,
                                             int packSize,
                                             int extraByte)
{
    if(_testMode != 0) {
        // Print out codec and settings
        printf("codec: %s Freq: %d Rate: %d PackSize: %d", codecName, samplingFreqHz, rate, packSize);
    }

    // Store packetsize in samples, used to validate the recieved packet
    _packSizeSamp = packSize;

    // Store the expected packet size in bytes, used to validate the recieved packet
    // If variable rate codec (extraByte == -1), set to -1 (65535)
    if (extraByte != -1) 
    {
        // Add 0.875 to always round up to a whole byte
        _packSizeBytes = (WebRtc_UWord16)((float)(packSize*rate)/(float)(samplingFreqHz*8)+0.875)+extraByte;
    } 
    else 
    {
        // Packets will have a variable size
        _packSizeBytes = -1;
    }

    // Set pointer to the ACM where to register the codec
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

    // Get all codec paramters before registering
    CHECK_ERROR(AudioCodingModule::Codec(codecName, myCodecParam, samplingFreqHz));
    myCodecParam.rate = rate;
    myCodecParam.pacsize = packSize;
    CHECK_ERROR(myACM->RegisterSendCodec(myCodecParam));

    // initialization was succesful
    return 0;
}

void TestAllCodecs::Run(TestPack* channel)
{
    AudioFrame audioFrame;

    WebRtc_Word32 outFreqHzB = _outFileB.SamplingFrequency();
    WebRtc_UWord16 recSize;
    WebRtc_UWord32 timeStampDiff;
    channel->ResetPayloadSize();
    int errorCount = 0;

    // Only run 1 second for each test case
    while((_counter<1000)&& (!_inFileA.EndOfFile()))
    {
        // Add 10 msec to ACM
         _inFileA.Read10MsData(audioFrame);
        CHECK_ERROR(_acmA->Add10MsData(audioFrame));

        // Run sender side of ACM
        CHECK_ERROR(_acmA->Process());

        // Verify that the received packet size matches the settings
        recSize = channel->GetPayloadSize();
        if (recSize) {
            if ((recSize != _packSizeBytes) && (_packSizeBytes < 65535)) {
                errorCount++;
            }

        // Verify that the timestamp is updated with expected length
        timeStampDiff = channel->GetTimeStampDiff();
        if ((_counter > 10) && (timeStampDiff != _packSizeSamp))
            errorCount++;
        }


        // Run received side of ACM
        CHECK_ERROR(_acmB->PlayoutData10Ms(outFreqHzB, audioFrame));

        // Write output speech to file
        _outFileB.Write10MsData(audioFrame._payloadData, audioFrame._payloadDataLengthInSamples);
    }

    if (errorCount) 
    {
        printf(" - test FAILED\n");
    }
    else if(_testMode != 0)
    {
        printf(" - test PASSED\n");
    }

    // Reset _counter
    if (_counter == 1000) {
        _counter = 0;
    }
    if (_inFileA.EndOfFile()) {
        _inFileA.Rewind();
    }
}

void TestAllCodecs::OpenOutFile(WebRtc_Word16 testNumber)
{
    char fileName[500];
    sprintf(fileName, "%s/testallcodecs_out_%02d.pcm",
            webrtc::test::OutputPath().c_str(), testNumber);
    _outFileB.Open(fileName, 32000, "wb");
}

void TestAllCodecs::DisplaySendReceiveCodec()
{
    CodecInst myCodecParam;
    _acmA->SendCodec(myCodecParam);
    printf("%s -> ", myCodecParam.plname);
    _acmB->ReceiveCodec(myCodecParam);
    printf("%s\n", myCodecParam.plname);
}

} // namespace webrtc

