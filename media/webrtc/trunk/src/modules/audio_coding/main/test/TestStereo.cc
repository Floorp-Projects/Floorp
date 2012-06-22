/*
 *  Copyright (c) 2012 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "TestStereo.h"

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
TestPackStereo::TestPackStereo():
_receiverACM(NULL),
_seqNo(0),
_timeStampDiff(0),
_lastInTimestamp(0),
_totalBytes(0),
_payloadSize(0),
_codec_mode(kNotSet),
_lost_packet(false)
{
}
TestPackStereo::~TestPackStereo()
{
}

void 
TestPackStereo::RegisterReceiverACM(AudioCodingModule* acm)
{
    _receiverACM = acm;
    return;
}


WebRtc_Word32 TestPackStereo::SendData(
    const FrameType frameType,
    const WebRtc_UWord8 payloadType,
    const WebRtc_UWord32 timeStamp,
    const WebRtc_UWord8* payloadData,
    const WebRtc_UWord16 payloadSize,
    const RTPFragmentationHeader* fragmentation) {
  WebRtcRTPHeader rtpInfo;
  WebRtc_Word32 status = 0;

  rtpInfo.header.markerBit = false;
  rtpInfo.header.ssrc = 0;
  rtpInfo.header.sequenceNumber = _seqNo++;
  rtpInfo.header.payloadType = payloadType;
  rtpInfo.header.timestamp = timeStamp;
  if (frameType == kFrameEmpty) {
    // Skip this frame
    return 0;
  }

  if (_lost_packet == false) {
    if (frameType != kAudioFrameCN) {
      rtpInfo.type.Audio.isCNG = false;
      rtpInfo.type.Audio.channel = (int) _codec_mode;
    } else {
      rtpInfo.type.Audio.isCNG = true;
      rtpInfo.type.Audio.channel = (int) kMono;
    }
    status = _receiverACM->IncomingPacket(payloadData, payloadSize,
                                          rtpInfo);

    if (frameType != kAudioFrameCN) {
      _payloadSize = payloadSize;
    } else {
      _payloadSize = -1;
    }

    _timeStampDiff = timeStamp - _lastInTimestamp;
    _lastInTimestamp = timeStamp;
    _totalBytes += payloadSize;
  }
  return status;
}

WebRtc_UWord16
TestPackStereo::GetPayloadSize()
{
    return _payloadSize;
}


WebRtc_UWord32
TestPackStereo::GetTimeStampDiff()
{
    return _timeStampDiff;
}

void 
TestPackStereo::ResetPayloadSize()
{
    _payloadSize = 0;
}

void TestPackStereo::set_codec_mode(enum StereoMonoMode mode) {
  _codec_mode = mode;
}

void TestPackStereo::set_lost_packet(bool lost) {
  _lost_packet = lost;
}

TestStereo::TestStereo(int testMode):
_acmA(NULL),
_acmB(NULL),
_channelA2B(NULL),
_testCntr(0),
_packSizeSamp(0),
_packSizeBytes(0),
_counter(0),
g722_pltype_(0),
l16_8khz_pltype_(-1),
l16_16khz_pltype_(-1),
l16_32khz_pltype_(-1),
pcma_pltype_(-1),
pcmu_pltype_(-1),
celt_pltype_(-1),
cn_8khz_pltype_(-1),
cn_16khz_pltype_(-1),
cn_32khz_pltype_(-1)
{
    // testMode = 0 for silent test (auto test)
    _testMode = testMode;
}

TestStereo::~TestStereo()
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

void TestStereo::Perform()
{
     char file_name_stereo[500];
     char file_name_mono[500];
     WebRtc_UWord16 frequencyHz;
     int audio_channels;
     int codec_channels;
     int status;

     if(_testMode == 0)
      {
          printf("Running Stereo Test");
          WEBRTC_TRACE(kTraceStateInfo, kTraceAudioCoding, -1,
                       "---------- TestStereo ----------");
      }

     strcpy(file_name_stereo, "./test/data/audio_coding/teststereo32kHz.pcm");
     strcpy(file_name_mono, "./test/data/audio_coding/testfile32kHz.pcm");
     frequencyHz = 32000;

    _in_file_stereo.Open(file_name_stereo, frequencyHz, "rb");
    _in_file_stereo.ReadStereo(true);
    _in_file_mono.Open(file_name_mono, frequencyHz, "rb");
    _in_file_mono.ReadStereo(false);

    _acmA = AudioCodingModule::Create(0);
    _acmB = AudioCodingModule::Create(1);

    _acmA->InitializeReceiver();
    _acmB->InitializeReceiver();

    WebRtc_UWord8 numEncoders = _acmA->NumberOfCodecs();
    CodecInst myCodecParam;
 
    // Register receiving codecs, some of them as stereo.
    for(WebRtc_UWord8 n = 0; n < numEncoders; n++) {
        _acmB->Codec(n, myCodecParam);
        if (!strcmp(myCodecParam.plname, "L16")) {
          if (myCodecParam.plfreq == 8000) {
            l16_8khz_pltype_ = myCodecParam.pltype;
          } else if (myCodecParam.plfreq == 16000) {
            l16_16khz_pltype_ = myCodecParam.pltype;
          } else if (myCodecParam.plfreq == 32000) {
            l16_32khz_pltype_ = myCodecParam.pltype;
          }
          myCodecParam.channels=2;
        } else if (!strcmp(myCodecParam.plname, "PCMA")) {
          pcma_pltype_ = myCodecParam.pltype;
          myCodecParam.channels=2;
        } else if (!strcmp(myCodecParam.plname, "PCMU")) {
          pcmu_pltype_ = myCodecParam.pltype;
          myCodecParam.channels=2;
        } else if (!strcmp(myCodecParam.plname, "G722")) {
          g722_pltype_ = myCodecParam.pltype;
          myCodecParam.channels=2;
        } else if (!strcmp(myCodecParam.plname, "CELT")) {
          celt_pltype_ = myCodecParam.pltype;
          myCodecParam.channels=2;
        }

        _acmB->RegisterReceiveCodec(myCodecParam);
    }

    // Test that unregister all receive codecs works for stereo.
    for(WebRtc_UWord8 n = 0; n < numEncoders; n++)
    {
        status = _acmB->Codec(n, myCodecParam);
        if (status < 0) {
            printf("Error in Codec(), no matching codec found");
        }
        status = _acmB->UnregisterReceiveCodec(myCodecParam.pltype);
        if (status < 0) {
            printf("Error in UnregisterReceiveCodec() for payload type %d",
                   myCodecParam.pltype);
        }
    }

    // Register receiving mono codecs, except comfort noise.
    for(WebRtc_UWord8 n = 0; n < numEncoders; n++)
    {
        status = _acmB->Codec(n, myCodecParam);
        if (status < 0) {
            printf("Error in Codec(), no matching codec found");
        }
        if(!strcmp(myCodecParam.plname, "L16") ||
            !strcmp(myCodecParam.plname, "PCMA")||
            !strcmp(myCodecParam.plname, "PCMU")||
            !strcmp(myCodecParam.plname, "G722")||
            !strcmp(myCodecParam.plname, "CELT")||
            !strcmp(myCodecParam.plname, "CN")){
        } else {
            status = _acmB->RegisterReceiveCodec(myCodecParam);
            if (status < 0) {
                printf("Error in UnregisterReceiveCodec() for codec number %d",
                       n);
            }
        }
    }

    // TODO(tlegrand): Take care of return values of all function calls.
    // Re-register all stereo codecs needed in the test, with new payload
    // numbers.
    g722_pltype_ = 117;
    l16_8khz_pltype_ = 120;
    l16_16khz_pltype_ = 121;
    l16_32khz_pltype_ = 122;
    pcma_pltype_ = 110;
    pcmu_pltype_ = 118;
    celt_pltype_ = 119;
    cn_8khz_pltype_ = 123;
    cn_16khz_pltype_ = 124;
    cn_32khz_pltype_ = 125;

    // Register all stereo codecs with new payload types.
#ifdef WEBRTC_CODEC_G722
   // G722
    _acmB->Codec("G722", myCodecParam, 16000);
    myCodecParam.pltype = g722_pltype_;
    myCodecParam.channels = 2;
    _acmB->RegisterReceiveCodec(myCodecParam);
#endif
#ifdef WEBRTC_CODEC_PCM16
    // L16
    _acmB->Codec("L16", myCodecParam, 8000);
    myCodecParam.pltype = l16_8khz_pltype_;
    myCodecParam.channels = 2;
    _acmB->RegisterReceiveCodec(myCodecParam);
    _acmB->Codec("L16", myCodecParam, 16000);
    myCodecParam.pltype = l16_16khz_pltype_;
    myCodecParam.channels = 2;
    _acmB->RegisterReceiveCodec(myCodecParam);
    _acmB->Codec("L16", myCodecParam, 32000);
    myCodecParam.pltype = l16_32khz_pltype_;
    myCodecParam.channels = 2;
    _acmB->RegisterReceiveCodec(myCodecParam);
#endif
    // PCM Alaw and u-law
    _acmB->Codec("PCMA", myCodecParam ,8000);
    myCodecParam.pltype = pcma_pltype_;
    myCodecParam.channels = 2;
    _acmB->RegisterReceiveCodec(myCodecParam);
    _acmB->Codec("PCMU", myCodecParam, 8000);
    myCodecParam.pltype = pcmu_pltype_;
    myCodecParam.channels = 2;
    _acmB->RegisterReceiveCodec(myCodecParam);
#ifdef WEBRTC_CODEC_CELT
    // Celt
    _acmB->Codec("CELT", myCodecParam, 32000);
    myCodecParam.pltype = celt_pltype_;
    myCodecParam.channels = 2;
    _acmB->RegisterReceiveCodec(myCodecParam);
#endif

    // Register CNG with new payload type on both send and receive side.
    _acmB->Codec("CN", myCodecParam, 8000);
    myCodecParam.pltype = cn_8khz_pltype_;
    _acmA->RegisterSendCodec(myCodecParam);
    _acmB->RegisterReceiveCodec(myCodecParam);
    _acmB->Codec("CN", myCodecParam, 16000);
    myCodecParam.pltype = cn_16khz_pltype_;
    _acmA->RegisterSendCodec(myCodecParam);
    _acmB->RegisterReceiveCodec(myCodecParam);
    _acmB->Codec("CN", myCodecParam, 32000);
    myCodecParam.pltype = cn_32khz_pltype_;
    _acmA->RegisterSendCodec(myCodecParam);
    _acmB->RegisterReceiveCodec(myCodecParam);

    // Create and connect the channel.
    _channelA2B = new TestPackStereo;
    _acmA->RegisterTransportCallback(_channelA2B);
    _channelA2B->RegisterReceiverACM(_acmB);

    //
    // Test Stereo-To-Stereo for all codecs.
    //
    audio_channels = 2;
    codec_channels = 2;

    // All codecs are tested for all allowed sampling frequencies, rates and
    // packet sizes.
#ifdef WEBRTC_CODEC_G722
    if(_testMode != 0) {
        printf("===========================================================\n");
        printf("Test number: %d\n",_testCntr + 1);
        printf("Test type: Stereo-to-stereo\n");
    } else {
        printf(".");
    }
    _channelA2B->set_codec_mode(kStereo);
    _testCntr++;
    OpenOutFile(_testCntr);
    char codecG722[] = "G722";
    RegisterSendCodec('A', codecG722, 16000, 64000, 160, codec_channels,
                      g722_pltype_);
    Run(_channelA2B, audio_channels, codec_channels);
    RegisterSendCodec('A', codecG722, 16000, 64000, 320, codec_channels,
                      g722_pltype_);
    Run(_channelA2B, audio_channels, codec_channels);
    RegisterSendCodec('A', codecG722, 16000, 64000, 480, codec_channels,
                      g722_pltype_);
    Run(_channelA2B, audio_channels, codec_channels);
    RegisterSendCodec('A', codecG722, 16000, 64000, 640, codec_channels,
                      g722_pltype_);
    Run(_channelA2B, audio_channels, codec_channels);
    RegisterSendCodec('A', codecG722, 16000, 64000, 800, codec_channels,
                      g722_pltype_);
    Run(_channelA2B, audio_channels, codec_channels);
    RegisterSendCodec('A', codecG722, 16000, 64000, 960, codec_channels,
                      g722_pltype_);
    Run(_channelA2B, audio_channels, codec_channels);
    _acmA->SetVAD(true, true, VADNormal);
    RegisterSendCodec('A', codecG722, 16000, 64000, 320, codec_channels,
                      g722_pltype_);
    Run(_channelA2B, audio_channels, codec_channels);
    _acmA->SetVAD(false, false, VADNormal);
    _outFileB.Close();
#endif
#ifdef WEBRTC_CODEC_PCM16
    if(_testMode != 0) {
        printf("===========================================================\n");
        printf("Test number: %d\n",_testCntr + 1);
        printf("Test type: Stereo-to-stereo\n");
    } else {
        printf(".");
    }
    _channelA2B->set_codec_mode(kStereo);
    _testCntr++; 
    OpenOutFile(_testCntr);
    char codecL16[] = "L16";
    RegisterSendCodec('A', codecL16, 8000, 128000, 80, codec_channels,
                      l16_8khz_pltype_);
    Run(_channelA2B, audio_channels, codec_channels);
    RegisterSendCodec('A', codecL16, 8000, 128000, 160, codec_channels,
                      l16_8khz_pltype_);
    Run(_channelA2B, audio_channels, codec_channels);
    RegisterSendCodec('A', codecL16, 8000, 128000, 240, codec_channels,
                      l16_8khz_pltype_);
    Run(_channelA2B, audio_channels, codec_channels);
    RegisterSendCodec('A', codecL16, 8000, 128000, 320, codec_channels,
                      l16_8khz_pltype_);
    Run(_channelA2B, audio_channels, codec_channels);
    _acmA->SetVAD(true, true, VADNormal);
    RegisterSendCodec('A', codecL16, 8000, 128000, 80, codec_channels,
                      l16_8khz_pltype_);
    Run(_channelA2B, audio_channels, codec_channels);
    _acmA->SetVAD(false, false, VADNormal);
    _outFileB.Close();

    if(_testMode != 0) {
        printf("===========================================================\n");
        printf("Test number: %d\n",_testCntr + 1);
        printf("Test type: Stereo-to-stereo\n");
    } else {
        printf(".");
    }
    _testCntr++;  
    OpenOutFile(_testCntr);
    RegisterSendCodec('A', codecL16, 16000, 256000, 160, codec_channels,
                      l16_16khz_pltype_);
    Run(_channelA2B, audio_channels, codec_channels);
    RegisterSendCodec('A', codecL16, 16000, 256000, 320, codec_channels,
                      l16_16khz_pltype_);
    Run(_channelA2B, audio_channels, codec_channels);
    RegisterSendCodec('A', codecL16, 16000, 256000, 480, codec_channels,
                      l16_16khz_pltype_);
    Run(_channelA2B, audio_channels, codec_channels);
    RegisterSendCodec('A', codecL16, 16000, 256000, 640, codec_channels,
                      l16_16khz_pltype_);
    Run(_channelA2B, audio_channels, codec_channels);
    _acmA->SetVAD(true, true, VADNormal);
    RegisterSendCodec('A', codecL16, 16000, 256000, 160, codec_channels,
                      l16_16khz_pltype_);
    Run(_channelA2B, audio_channels, codec_channels);
    _acmA->SetVAD(false, false, VADNormal);
    _outFileB.Close();

    if(_testMode != 0) {
        printf("===========================================================\n");
        printf("Test number: %d\n",_testCntr + 1);
        printf("Test type: Stereo-to-stereo\n");
    } else {
        printf(".");
    }
    _testCntr++; 
    OpenOutFile(_testCntr);
    RegisterSendCodec('A', codecL16, 32000, 512000, 320, codec_channels,
                      l16_32khz_pltype_);
    Run(_channelA2B, audio_channels, codec_channels);
    RegisterSendCodec('A', codecL16, 32000, 512000, 640, codec_channels,
                      l16_32khz_pltype_);
    Run(_channelA2B, audio_channels, codec_channels);
    _acmA->SetVAD(true, true, VADNormal);
    RegisterSendCodec('A', codecL16, 32000, 512000, 320, codec_channels,
                      l16_32khz_pltype_);
    Run(_channelA2B, audio_channels, codec_channels);
    _acmA->SetVAD(false, false, VADNormal);
    _outFileB.Close();
#endif
#define PCMA_AND_PCMU
#ifdef PCMA_AND_PCMU
    if(_testMode != 0) {
        printf("===========================================================\n");
        printf("Test number: %d\n",_testCntr + 1);
        printf("Test type: Stereo-to-stereo\n");
    } else {
        printf(".");
    }
    _channelA2B->set_codec_mode(kStereo);
    audio_channels = 2;
    codec_channels = 2;
    _testCntr++; 
    OpenOutFile(_testCntr);
    char codecPCMA[] = "PCMA";
    RegisterSendCodec('A', codecPCMA, 8000, 64000, 80, codec_channels,
                      pcma_pltype_);
    Run(_channelA2B, audio_channels, codec_channels);
    RegisterSendCodec('A', codecPCMA, 8000, 64000, 160, codec_channels,
                      pcma_pltype_);
    Run(_channelA2B, audio_channels, codec_channels);
    RegisterSendCodec('A', codecPCMA, 8000, 64000, 240, codec_channels,
                      pcma_pltype_);
    Run(_channelA2B, audio_channels, codec_channels);
    RegisterSendCodec('A', codecPCMA, 8000, 64000, 320, codec_channels,
                      pcma_pltype_);
    Run(_channelA2B, audio_channels, codec_channels);
    RegisterSendCodec('A', codecPCMA, 8000, 64000, 400, codec_channels,
                      pcma_pltype_);
    Run(_channelA2B, audio_channels, codec_channels);
    RegisterSendCodec('A', codecPCMA, 8000, 64000, 480, codec_channels,
                      pcma_pltype_);
    Run(_channelA2B, audio_channels, codec_channels);
    _acmA->SetVAD(true, true, VADNormal);
    RegisterSendCodec('A', codecPCMA, 8000, 64000, 80, codec_channels,
                      pcma_pltype_);
    Run(_channelA2B, audio_channels, codec_channels);
    _acmA->SetVAD(false, false, VADNormal);
    _outFileB.Close();
    if(_testMode != 0) {
        printf("===========================================================\n");
        printf("Test number: %d\n",_testCntr + 1);
        printf("Test type: Stereo-to-stereo\n");
    } else {
        printf(".");
    }
    _testCntr++;
    OpenOutFile(_testCntr);
    char codecPCMU[] = "PCMU";
    RegisterSendCodec('A', codecPCMU, 8000, 64000, 80, codec_channels,
                      pcmu_pltype_);
    Run(_channelA2B, audio_channels, codec_channels);
    RegisterSendCodec('A', codecPCMU, 8000, 64000, 160, codec_channels,
                      pcmu_pltype_);
    Run(_channelA2B, audio_channels, codec_channels);
    RegisterSendCodec('A', codecPCMU, 8000, 64000, 240, codec_channels,
                      pcmu_pltype_);
    Run(_channelA2B, audio_channels, codec_channels);
    RegisterSendCodec('A', codecPCMU, 8000, 64000, 320, codec_channels,
                      pcmu_pltype_);
    Run(_channelA2B, audio_channels, codec_channels);
    RegisterSendCodec('A', codecPCMU, 8000, 64000, 400, codec_channels,
                      pcmu_pltype_);
    Run(_channelA2B, audio_channels, codec_channels);
    RegisterSendCodec('A', codecPCMU, 8000, 64000, 480, codec_channels,
                      pcmu_pltype_);
    Run(_channelA2B, audio_channels, codec_channels);
    _acmA->SetVAD(true, true, VADNormal);
    RegisterSendCodec('A', codecPCMU, 8000, 64000, 80, codec_channels,
                      pcmu_pltype_);
    Run(_channelA2B, audio_channels, codec_channels);
    _acmA->SetVAD(false, false, VADNormal);
    _outFileB.Close();
#endif
#ifdef WEBRTC_CODEC_CELT
    if(_testMode != 0) {
        printf("===========================================================\n");
        printf("Test number: %d\n",_testCntr + 1);
        printf("Test type: Stereo-to-stereo\n");
    } else {
        printf(".");
    }
    _channelA2B->set_codec_mode(kStereo);
    audio_channels = 2;
    codec_channels = 2;
    _testCntr++;
    OpenOutFile(_testCntr);
    char codecCELT[] = "CELT";
    RegisterSendCodec('A', codecCELT, 32000, 48000, 320, codec_channels,
                      celt_pltype_);
    Run(_channelA2B, audio_channels, codec_channels);
    RegisterSendCodec('A', codecCELT, 32000, 64000, 320, codec_channels,
                      celt_pltype_);
    Run(_channelA2B, audio_channels, codec_channels);
    RegisterSendCodec('A', codecCELT, 32000, 128000, 320, codec_channels,
                      celt_pltype_);
    Run(_channelA2B, audio_channels, codec_channels);
    _acmA->SetVAD(true, true, VADNormal);
    RegisterSendCodec('A', codecCELT, 32000, 48000, 320, codec_channels,
                      celt_pltype_);
    Run(_channelA2B, audio_channels, codec_channels);
    _acmA->SetVAD(false, false, VADNormal);
    _outFileB.Close();
#endif
  //
  // Test Mono-To-Stereo for all codecs.
  //
  audio_channels = 1;
  codec_channels = 2;

#ifdef WEBRTC_CODEC_G722
  if(_testMode != 0) {
    printf("===============================================================\n");
    printf("Test number: %d\n",_testCntr + 1);
    printf("Test type: Mono-to-stereo\n");
  }
  _testCntr++;
  _channelA2B->set_codec_mode(kStereo);
  OpenOutFile(_testCntr);
  RegisterSendCodec('A', codecG722, 16000, 64000, 160, codec_channels,
                    g722_pltype_);
  Run(_channelA2B, audio_channels, codec_channels);
  _outFileB.Close();
#endif
#ifdef WEBRTC_CODEC_PCM16
  if(_testMode != 0) {
    printf("===============================================================\n");
    printf("Test number: %d\n",_testCntr + 1);
    printf("Test type: Mono-to-stereo\n");
  }
  _testCntr++;
  _channelA2B->set_codec_mode(kStereo);
  OpenOutFile(_testCntr);
  RegisterSendCodec('A', codecL16, 8000, 128000, 80, codec_channels,
                    l16_8khz_pltype_);
  Run(_channelA2B, audio_channels, codec_channels);
  _outFileB.Close();
  if(_testMode != 0) {
    printf("===============================================================\n");
    printf("Test number: %d\n",_testCntr + 1);
    printf("Test type: Mono-to-stereo\n");
  }
  _testCntr++;
  OpenOutFile(_testCntr);
  RegisterSendCodec('A', codecL16, 16000, 256000, 160, codec_channels,
                    l16_16khz_pltype_);
  Run(_channelA2B, audio_channels, codec_channels);
  _outFileB.Close();
  if(_testMode != 0) {
    printf("===============================================================\n");
    printf("Test number: %d\n",_testCntr + 1);
    printf("Test type: Mono-to-stereo\n");
  }
  _testCntr++;
  OpenOutFile(_testCntr);
  RegisterSendCodec('A', codecL16, 32000, 512000, 320, codec_channels,
                    l16_32khz_pltype_);
  Run(_channelA2B, audio_channels, codec_channels);
  _outFileB.Close();
#endif
#ifdef PCMA_AND_PCMU
  if(_testMode != 0) {
    printf("===============================================================\n");
    printf("Test number: %d\n",_testCntr + 1);
    printf("Test type: Mono-to-stereo\n");
  }
  _testCntr++;
  _channelA2B->set_codec_mode(kStereo);
  OpenOutFile(_testCntr);
  RegisterSendCodec('A', codecPCMU, 8000, 64000, 80, codec_channels,
                    pcmu_pltype_);
  Run(_channelA2B, audio_channels, codec_channels);
  RegisterSendCodec('A', codecPCMA, 8000, 64000, 80, codec_channels,
                    pcma_pltype_);
  Run(_channelA2B, audio_channels, codec_channels);
  _outFileB.Close();
#endif
#ifdef WEBRTC_CODEC_CELT
  if(_testMode != 0) {
    printf("===============================================================\n");
    printf("Test number: %d\n",_testCntr + 1);
    printf("Test type: Mono-to-stereo\n");
  }
  _testCntr++;
  _channelA2B->set_codec_mode(kStereo);
  OpenOutFile(_testCntr);
  RegisterSendCodec('A', codecCELT, 32000, 64000, 320, codec_channels,
                    celt_pltype_);
  Run(_channelA2B, audio_channels, codec_channels);
  _outFileB.Close();
#endif

  //
  // Test Stereo-To-Mono for all codecs.
  //
  audio_channels = 2;
  codec_channels = 1;
  _channelA2B->set_codec_mode(kMono);

  // Register receivers as mono.
  for(WebRtc_UWord8 n = 0; n < numEncoders; n++) {
      _acmB->Codec(n, myCodecParam);
      if (!strcmp(myCodecParam.plname, "L16")) {
        if (myCodecParam.plfreq == 8000) {
          myCodecParam.pltype = l16_8khz_pltype_;
        } else if (myCodecParam.plfreq == 16000) {
          myCodecParam.pltype = l16_16khz_pltype_ ;
        } else if (myCodecParam.plfreq == 32000) {
          myCodecParam.pltype = l16_32khz_pltype_;
        }
      } else if (!strcmp(myCodecParam.plname, "PCMA")) {
        myCodecParam.pltype = pcma_pltype_;
      } else if (!strcmp(myCodecParam.plname, "PCMU")) {
        myCodecParam.pltype = pcmu_pltype_;
      } else if (!strcmp(myCodecParam.plname, "G722")) {
        myCodecParam.pltype = g722_pltype_;
      } else if (!strcmp(myCodecParam.plname, "CELT")) {
        myCodecParam.pltype = celt_pltype_;
        myCodecParam.channels = 1;
      }
      _acmB->RegisterReceiveCodec(myCodecParam);
  }
#ifdef WEBRTC_CODEC_G722
  // Run stereo audio and mono codec.
  if(_testMode != 0) {
    printf("===============================================================\n");
    printf("Test number: %d\n",_testCntr + 1);
    printf("Test type: Stereo-to-mono\n");
  }
  _testCntr++;
  OpenOutFile(_testCntr);
  RegisterSendCodec('A', codecG722, 16000, 64000, 160, codec_channels,
                    g722_pltype_);
  Run(_channelA2B, audio_channels, codec_channels);
  _outFileB.Close();
#endif
#ifdef WEBRTC_CODEC_PCM16
  if(_testMode != 0) {
    printf("===============================================================\n");
    printf("Test number: %d\n",_testCntr + 1);
    printf("Test type: Stereo-to-mono\n");
  }
  _testCntr++;
  OpenOutFile(_testCntr);
  RegisterSendCodec('A', codecL16, 8000, 128000, 80, codec_channels,
                    l16_8khz_pltype_);
  Run(_channelA2B, audio_channels, codec_channels);
  _outFileB.Close();
  if(_testMode != 0) {
    printf("===============================================================\n");
    printf("Test number: %d\n",_testCntr + 1);
    printf("Test type: Stereo-to-mono\n");
   }
  _testCntr++;
  OpenOutFile(_testCntr);
  RegisterSendCodec('A', codecL16, 16000, 256000, 160, codec_channels,
                    l16_16khz_pltype_);
  Run(_channelA2B, audio_channels, codec_channels);
  _outFileB.Close();
  if(_testMode != 0) {
     printf("==============================================================\n");
     printf("Test number: %d\n",_testCntr + 1);
     printf("Test type: Stereo-to-mono\n");
   }
   _testCntr++;
   OpenOutFile(_testCntr);
   RegisterSendCodec('A', codecL16, 32000, 512000, 320, codec_channels,
                     l16_32khz_pltype_);
   Run(_channelA2B, audio_channels, codec_channels);
   _outFileB.Close();
#endif
#ifdef PCMA_AND_PCMU
  if(_testMode != 0) {
    printf("===============================================================\n");
    printf("Test number: %d\n",_testCntr + 1);
    printf("Test type: Stereo-to-mono\n");
  }
  _testCntr++;
  OpenOutFile(_testCntr);
  RegisterSendCodec('A', codecPCMU, 8000, 64000, 80, codec_channels,
                    pcmu_pltype_);
  Run(_channelA2B, audio_channels, codec_channels);
  RegisterSendCodec('A', codecPCMA, 8000, 64000, 80, codec_channels,
                    pcma_pltype_);
  Run(_channelA2B, audio_channels, codec_channels);
  _outFileB.Close();
#endif
#ifdef WEBRTC_CODEC_CELT
  if(_testMode != 0) {
    printf("===============================================================\n");
    printf("Test number: %d\n",_testCntr + 1);
    printf("Test type: Stereo-to-mono\n");
  }
  _testCntr++;
  OpenOutFile(_testCntr);
  RegisterSendCodec('A', codecCELT, 32000, 64000, 320, codec_channels,
                    celt_pltype_);
  Run(_channelA2B, audio_channels, codec_channels);
  _outFileB.Close();
#endif

    // Print out which codecs were tested, and which were not, in the run.
    if(_testMode != 0) {
        printf("\nThe following codecs was INCLUDED in the test:\n");
#ifdef WEBRTC_CODEC_G722
        printf("   G.722\n");
#endif
#ifdef WEBRTC_CODEC_PCM16
        printf("   PCM16\n");
#endif
        printf("   G.711\n");

        printf("\nTo complete the test, listen to the %d number of output "
               "files.\n", _testCntr);
    } else {
        printf("Done!\n");
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
WebRtc_Word16 TestStereo::RegisterSendCodec(char side, 
                                          char* codecName, 
                                          WebRtc_Word32 samplingFreqHz,
                                          int rate,
                                          int packSize,
                                          int channels,
                                          int payload_type)
{
    if(_testMode != 0) {
        // Print out codec and settings
        printf("Codec: %s Freq: %d Rate: %d PackSize: %d", codecName,
               samplingFreqHz, rate, packSize);
    }

    // Store packetsize in samples, used to validate the received packet
    _packSizeSamp = packSize;

    // Store the expected packet size in bytes, used to validate the received
    // packet. Add 0.875 to always round up to a whole byte.
    // For Celt the packet size in bytes is already counting the stereo part.
    if (!strcmp(codecName, "CELT")) {
      _packSizeBytes = (WebRtc_UWord16)((float)(packSize*rate)/
          (float)(samplingFreqHz*8)+0.875) / channels;
    } else {
      _packSizeBytes = (WebRtc_UWord16)((float)(packSize*rate)/
          (float)(samplingFreqHz*8)+0.875);
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

    // Get all codec parameters before registering
    CHECK_ERROR(AudioCodingModule::Codec(codecName, myCodecParam,
                                         samplingFreqHz));
    myCodecParam.rate = rate;
    myCodecParam.pacsize = packSize;
    myCodecParam.pltype = payload_type;
    myCodecParam.channels = channels;
    CHECK_ERROR(myACM->RegisterSendCodec(myCodecParam));

    // Initialization was successful.
    return 0;
}

void TestStereo::Run(TestPackStereo* channel, int in_channels, int out_channels,
                     int percent_loss) {
    AudioFrame audioFrame;

    WebRtc_Word32 outFreqHzB = _outFileB.SamplingFrequency();
    WebRtc_UWord16 recSize;
    WebRtc_UWord32 timeStampDiff;
    channel->ResetPayloadSize();
    int errorCount = 0;

    // Only run 1 second for each test case
    // TODO(tlegrand): either remove |_counter| or start using it as the comment
    // above says. Now |_counter| is always 0.
    while(1)
    {
        // Simulate packet loss by setting |packet_loss_| to "true" in
        // |percent_loss| percent of the loops.
        if (percent_loss > 0) {
           if (_counter == floor((100 / percent_loss) + 0.5)) {
             _counter = 0;
             channel->set_lost_packet(true);
           } else {
             channel->set_lost_packet(false);
           }
           _counter++;
        }

        // Add 10 msec to ACM
        if (in_channels == 1) {
          if (_in_file_mono.EndOfFile()) {
            break;
          }
          _in_file_mono.Read10MsData(audioFrame);
        } else {
          if (_in_file_stereo.EndOfFile()) {
            break;
          }
          _in_file_stereo.Read10MsData(audioFrame);
        }
        CHECK_ERROR(_acmA->Add10MsData(audioFrame));

        // Run sender side of ACM
        CHECK_ERROR(_acmA->Process());

        // Verify that the received packet size matches the settings
        recSize = channel->GetPayloadSize();
        if ((0 < recSize) & (recSize < 65535)) {
            if ((recSize != _packSizeBytes * out_channels) &&
                (_packSizeBytes < 65535)) {
                errorCount++;
            }

            // Verify that the timestamp is updated with expected length
            timeStampDiff = channel->GetTimeStampDiff();
            if ((_counter > 10) && (timeStampDiff != _packSizeSamp)) {
                errorCount++;
            }
        }

        // Run received side of ACM
        CHECK_ERROR(_acmB->PlayoutData10Ms(outFreqHzB, audioFrame));

        // Write output speech to file
        _outFileB.Write10MsData(
            audioFrame._payloadData,
            audioFrame._payloadDataLengthInSamples * audioFrame._audioChannel);
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
    if (_in_file_mono.EndOfFile()) {
        _in_file_mono.Rewind();
    }
    if (_in_file_stereo.EndOfFile()) {
        _in_file_stereo.Rewind();
    }
    // Reset in case we ended with a lost packet
    channel->set_lost_packet(false);
}

void TestStereo::OpenOutFile(WebRtc_Word16 testNumber)
{
    char fileName[500];
    sprintf(fileName, "%s/teststereo_out_%02d.pcm",
            webrtc::test::OutputPath().c_str(), testNumber);
    _outFileB.Open(fileName, 32000, "wb");
}

void TestStereo::DisplaySendReceiveCodec()
{
    CodecInst myCodecParam;
    _acmA->SendCodec(myCodecParam);
    if(_testMode != 0) {
        printf("%s -> ", myCodecParam.plname);
    }
    _acmB->ReceiveCodec(myCodecParam);
    if(_testMode != 0) {
        printf("%s\n", myCodecParam.plname);
    }
}

} // namespace webrtc
