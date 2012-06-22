/*
 *  Copyright (c) 2012 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "EncodeDecodeTest.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "audio_coding_module.h"
#include "common_types.h"
#include "gtest/gtest.h"
#include "trace.h"
#include "testsupport/fileutils.h"
#include "utility.h"

namespace webrtc {

TestPacketization::TestPacketization(RTPStream *rtpStream,
                                     WebRtc_UWord16 frequency)
    : _rtpStream(rtpStream),
      _frequency(frequency),
      _seqNo(0) {
}

TestPacketization::~TestPacketization() { }

WebRtc_Word32 TestPacketization::SendData(
    const FrameType /* frameType */,
    const WebRtc_UWord8 payloadType,
    const WebRtc_UWord32 timeStamp,
    const WebRtc_UWord8* payloadData,
    const WebRtc_UWord16 payloadSize,
    const RTPFragmentationHeader* /* fragmentation */) {
  _rtpStream->Write(payloadType, timeStamp, _seqNo++, payloadData, payloadSize,
                    _frequency);
  return 1;
}

Sender::Sender()
    : _acm(NULL),
      _pcmFile(),
      _audioFrame(),
      _payloadSize(0),
      _timeStamp(0),
      _packetization(NULL) {
}

void Sender::Setup(AudioCodingModule *acm, RTPStream *rtpStream) {
  acm->InitializeSender();
  struct CodecInst sendCodec;
  int noOfCodecs = acm->NumberOfCodecs();
  int codecNo;

  if (testMode == 1) {
    // Set the codec, input file, and parameters for the current test.
    codecNo = codeId;
    // Use same input file for now.
    char fileName[] = "./test/data/audio_coding/testfile32kHz.pcm";
    _pcmFile.Open(fileName, 32000, "rb");
  } else if (testMode == 0) {
    // Set the codec, input file, and parameters for the current test.
    codecNo = codeId;
    acm->Codec(codecNo, sendCodec);
    // Use same input file for now.
    char fileName[] = "./test/data/audio_coding/testfile32kHz.pcm";
    _pcmFile.Open(fileName, 32000, "rb");
  } else {
    printf("List of supported codec.\n");
    for (int n = 0; n < noOfCodecs; n++) {
      acm->Codec(n, sendCodec);
      printf("%d %s\n", n, sendCodec.plname);
    }
    printf("Choose your codec:");
    ASSERT_GT(scanf("%d", &codecNo), 0);
    char fileName[] = "./test/data/audio_coding/testfile32kHz.pcm";
    _pcmFile.Open(fileName, 32000, "rb");
  }

  acm->Codec(codecNo, sendCodec);
  if (!strcmp(sendCodec.plname, "CELT")) {
    sendCodec.channels = 1;
  }
  acm->RegisterSendCodec(sendCodec);
  _packetization = new TestPacketization(rtpStream, sendCodec.plfreq);
  if (acm->RegisterTransportCallback(_packetization) < 0) {
    printf("Registering Transport Callback failed, for run: codecId: %d: --\n",
           codeId);
  }

    _acm = acm;
  }

void Sender::Teardown() {
  _pcmFile.Close();
  delete _packetization;
}

bool Sender::Add10MsData() {
  if (!_pcmFile.EndOfFile()) {
    _pcmFile.Read10MsData(_audioFrame);
    WebRtc_Word32 ok = _acm->Add10MsData(_audioFrame);
    if (ok != 0) {
      printf("Error calling Add10MsData: for run: codecId: %d\n", codeId);
      exit(1);
    }
    return true;
  }
  return false;
}

bool Sender::Process() {
  WebRtc_Word32 ok = _acm->Process();
  if (ok < 0) {
    printf("Error calling Add10MsData: for run: codecId: %d\n", codeId);
    exit(1);
  }
  return true;
}

void Sender::Run() {
  while (true) {
    if (!Add10MsData()) {
      break;
    }
    if (!Process()) { // This could be done in a processing thread
      break;
    }
  }
}

Receiver::Receiver()
    : _playoutLengthSmpls(WEBRTC_10MS_PCM_AUDIO),
      _payloadSizeBytes(MAX_INCOMING_PAYLOAD) {
}

void Receiver::Setup(AudioCodingModule *acm, RTPStream *rtpStream) {
  struct CodecInst recvCodec;
  int noOfCodecs;
  acm->InitializeReceiver();

  noOfCodecs = acm->NumberOfCodecs();
  for (int i = 0; i < noOfCodecs; i++) {
    acm->Codec((WebRtc_UWord8) i, recvCodec);
    if (!strcmp(recvCodec.plname, "CELT")) {
      recvCodec.channels = 1;
    }
    if (acm->RegisterReceiveCodec(recvCodec) != 0) {
      printf("Unable to register codec: for run: codecId: %d\n", codeId);
      exit(1);
    }
  }

  char filename[128];
  _rtpStream = rtpStream;
  int playSampFreq;

  if (testMode == 1) {
    playSampFreq=recvCodec.plfreq;
    //output file for current run
    sprintf(filename,"%s/out%dFile.pcm", webrtc::test::OutputPath().c_str(),
            codeId);
    _pcmFile.Open(filename, recvCodec.plfreq, "wb+");
  } else if (testMode == 0) {
    playSampFreq=32000;
    //output file for current run
    sprintf(filename,  "%s/encodeDecode_out%d.pcm",
            webrtc::test::OutputPath().c_str(), codeId);
    _pcmFile.Open(filename, 32000/*recvCodec.plfreq*/, "wb+");
  } else {
    printf("\nValid output frequencies:\n");
    printf("8000\n16000\n32000\n-1,");
    printf("which means output freq equal to received signal freq");
    printf("\n\nChoose output sampling frequency: ");
    ASSERT_GT(scanf("%d", &playSampFreq), 0);
    sprintf(filename,  "%s/outFile.pcm", webrtc::test::OutputPath().c_str());
    _pcmFile.Open(filename, 32000, "wb+");
  }

  _realPayloadSizeBytes = 0;
  _playoutBuffer = new WebRtc_Word16[WEBRTC_10MS_PCM_AUDIO];
  _frequency = playSampFreq;
  _acm = acm;
  _firstTime = true;
}

void Receiver::Teardown() {
  delete [] _playoutBuffer;
  _pcmFile.Close();
  if (testMode > 1)
    Trace::ReturnTrace();
}

bool Receiver::IncomingPacket() {
  if (!_rtpStream->EndOfFile()) {
    if (_firstTime) {
      _firstTime = false;
      _realPayloadSizeBytes = _rtpStream->Read(&_rtpInfo, _incomingPayload,
                                               _payloadSizeBytes, &_nextTime);
      if (_realPayloadSizeBytes == 0) {
        if (_rtpStream->EndOfFile()) {
          _firstTime = true;
          return true;
        } else {
          printf("Error in reading incoming payload.\n");
          return false;
        }
      }
   }

   WebRtc_Word32 ok = _acm->IncomingPacket(_incomingPayload,
                                           _realPayloadSizeBytes, _rtpInfo);
   if (ok != 0) {
     printf("Error when inserting packet to ACM, for run: codecId: %d\n",
            codeId);
     exit(1);
   }
   _realPayloadSizeBytes = _rtpStream->Read(&_rtpInfo, _incomingPayload,
                                            _payloadSizeBytes, &_nextTime);
    if (_realPayloadSizeBytes == 0 && _rtpStream->EndOfFile()) {
      _firstTime = true;
    }
  }
  return true;
}

bool Receiver::PlayoutData() {
  AudioFrame audioFrame;

  if (_acm->PlayoutData10Ms(_frequency, audioFrame) != 0) {
    printf("Error when calling PlayoutData10Ms, for run: codecId: %d\n",
           codeId);
    exit(1);
  }
  if (_playoutLengthSmpls == 0) {
    return false;
  }
  _pcmFile.Write10MsData(audioFrame._payloadData,
                         audioFrame._payloadDataLengthInSamples);
  return true;
}

void Receiver::Run() {
  WebRtc_UWord8 counter500Ms = 50;
  WebRtc_UWord32 clock = 0;

  while (counter500Ms > 0) {
    if (clock == 0 || clock >= _nextTime) {
      IncomingPacket();
      if (clock == 0) {
        clock = _nextTime;
      }
    }
    if ((clock % 10) == 0) {
      if (!PlayoutData()) {
        clock++;
        continue;
      }
    }
    if (_rtpStream->EndOfFile()) {
      counter500Ms--;
    }
    clock++;
  }
}

EncodeDecodeTest::EncodeDecodeTest() {
  _testMode = 2;
  Trace::CreateTrace();
  Trace::SetTraceFile("acm_encdec_test.txt");
}

EncodeDecodeTest::EncodeDecodeTest(int testMode) {
  //testMode == 0 for autotest
  //testMode == 1 for testing all codecs/parameters
  //testMode > 1 for specific user-input test (as it was used before)
 _testMode = testMode;
 if(_testMode != 0) {
   Trace::CreateTrace();
   Trace::SetTraceFile("acm_encdec_test.txt");
 }
}

void EncodeDecodeTest::Perform() {
  if (_testMode == 0) {
    printf("Running Encode/Decode Test");
    WEBRTC_TRACE(webrtc::kTraceStateInfo, webrtc::kTraceAudioCoding, -1,
                 "---------- EncodeDecodeTest ----------");
  }

  int numCodecs = 1;
  int codePars[3]; //freq, pacsize, rate
  int numPars[52]; //number of codec parameters sets (rate,freq,pacsize)to test,
                   //for a given codec

  codePars[0] = 0;
  codePars[1] = 0;
  codePars[2] = 0;

  if (_testMode == 1) {
    AudioCodingModule *acmTmp = AudioCodingModule::Create(0);
    struct CodecInst sendCodecTmp;
    numCodecs = acmTmp->NumberOfCodecs();
    printf("List of supported codec.\n");
    for(int n = 0; n < numCodecs; n++) {
      acmTmp->Codec(n, sendCodecTmp);
      if (STR_CASE_CMP(sendCodecTmp.plname, "telephone-event") == 0) {
        numPars[n] = 0;
      } else if (STR_CASE_CMP(sendCodecTmp.plname, "cn") == 0) {
        numPars[n] = 0;
      } else if (STR_CASE_CMP(sendCodecTmp.plname, "red") == 0) {
        numPars[n] = 0;
      } else {
        numPars[n] = 1;
        printf("%d %s\n", n, sendCodecTmp.plname);
      }
    }
    AudioCodingModule::Destroy(acmTmp);
  } else if (_testMode == 0) {
    AudioCodingModule *acmTmp = AudioCodingModule::Create(0);
    numCodecs = acmTmp->NumberOfCodecs();
    AudioCodingModule::Destroy(acmTmp);
    struct CodecInst dummyCodec;

    //chose range of testing for codecs/parameters
    for(int i = 0 ; i < numCodecs ; i++) {
      numPars[i] = 1;
      acmTmp->Codec(i, dummyCodec);
      if (STR_CASE_CMP(dummyCodec.plname, "telephone-event") == 0) {
        numPars[i] = 0;
      } else if (STR_CASE_CMP(dummyCodec.plname, "cn") == 0) {
        numPars[i] = 0;
      } else if (STR_CASE_CMP(dummyCodec.plname, "red") == 0) {
        numPars[i] = 0;
      }
    }
  } else {
    numCodecs = 1;
    numPars[0] = 1;
  }

  _receiver.testMode = _testMode;

  //loop over all codecs:
  for (int codeId = 0; codeId < numCodecs; codeId++) {
    //only encode using real encoders, not telephone-event anc cn
    for (int loopPars = 1; loopPars <= numPars[codeId]; loopPars++) {
      if (_testMode == 1) {
        printf("\n");
        printf("***FOR RUN: codeId: %d\n", codeId);
        printf("\n");
      } else if (_testMode == 0) {
        printf(".");
      }

      EncodeToFile(1, codeId, codePars, _testMode);

      AudioCodingModule *acm = AudioCodingModule::Create(10);
      RTPFile rtpFile;
      std::string fileName = webrtc::test::OutputPath() + "outFile.rtp";
      rtpFile.Open(fileName.c_str(), "rb");

      _receiver.codeId = codeId;

      rtpFile.ReadHeader();
      _receiver.Setup(acm, &rtpFile);
      _receiver.Run();
      _receiver.Teardown();
      rtpFile.Close();
      AudioCodingModule::Destroy(acm);

      if (_testMode == 1) {
        printf("***COMPLETED RUN FOR: codecID: %d ***\n", codeId);
      }
    }
  }
  if (_testMode == 0) {
    printf("Done!\n");
  }
  if (_testMode == 1)
    Trace::ReturnTrace();
}

void EncodeDecodeTest::EncodeToFile(int fileType, int codeId, int* codePars,
                                    int testMode) {
  AudioCodingModule *acm = AudioCodingModule::Create(0);
  RTPFile rtpFile;
  std::string fileName = webrtc::test::OutputPath() + "outFile.rtp";
  rtpFile.Open(fileName.c_str(), "wb+");
  rtpFile.WriteHeader();

  //for auto_test and logging
  _sender.testMode = testMode;
  _sender.codeId = codeId;

  _sender.Setup(acm, &rtpFile);
  struct CodecInst sendCodecInst;
  if (acm->SendCodec(sendCodecInst) >= 0) {
    _sender.Run();
  }
  _sender.Teardown();
  rtpFile.Close();
  AudioCodingModule::Destroy(acm);
}

} // namespace webrtc
