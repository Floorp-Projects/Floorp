/*
 *  Copyright (c) 2012 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "TestFEC.h"

#include <assert.h>

#include <iostream>

#include "audio_coding_module_typedefs.h"
#include "common_types.h"
#include "engine_configurations.h"
#include "trace.h"
#include "utility.h"
#include "webrtc/test/testsupport/fileutils.h"

namespace webrtc {

TestFEC::TestFEC()
    : _acmA(AudioCodingModule::Create(0)),
      _acmB(AudioCodingModule::Create(1)),
      _channelA2B(NULL),
      _testCntr(0) {
}

TestFEC::~TestFEC() {
  if (_channelA2B != NULL) {
    delete _channelA2B;
    _channelA2B = NULL;
  }
}

void TestFEC::Perform() {
  const std::string file_name = webrtc::test::ResourcePath(
      "audio_coding/testfile32kHz", "pcm");
  _inFileA.Open(file_name, 32000, "rb");

  ASSERT_EQ(0, _acmA->InitializeReceiver());
  ASSERT_EQ(0, _acmB->InitializeReceiver());

  uint8_t numEncoders = _acmA->NumberOfCodecs();
  CodecInst myCodecParam;
  for (uint8_t n = 0; n < numEncoders; n++) {
    EXPECT_EQ(0, _acmB->Codec(n, &myCodecParam));
    EXPECT_EQ(0, _acmB->RegisterReceiveCodec(myCodecParam));
  }

  // Create and connect the channel
  _channelA2B = new Channel;
  _acmA->RegisterTransportCallback(_channelA2B);
  _channelA2B->RegisterReceiverACM(_acmB.get());

#ifndef WEBRTC_CODEC_G722
  EXPECT_TRUE(false);
  printf("G722 needs to be activated to run this test\n");
  return;
#endif
  char nameG722[] = "G722";
  EXPECT_EQ(0, RegisterSendCodec('A', nameG722, 16000));
  char nameCN[] = "CN";
  EXPECT_EQ(0, RegisterSendCodec('A', nameCN, 16000));
  char nameRED[] = "RED";
  EXPECT_EQ(0, RegisterSendCodec('A', nameRED));
  OpenOutFile(_testCntr);
  EXPECT_EQ(0, SetVAD(true, true, VADAggr));
  EXPECT_EQ(0, _acmA->SetFECStatus(false));
  EXPECT_FALSE(_acmA->FECStatus());
  Run();
  _outFileB.Close();

  EXPECT_EQ(0, _acmA->SetFECStatus(true));
  EXPECT_TRUE(_acmA->FECStatus());
  OpenOutFile(_testCntr);
  Run();
  _outFileB.Close();

  char nameISAC[] = "iSAC";
  RegisterSendCodec('A', nameISAC, 16000);
  OpenOutFile(_testCntr);
  EXPECT_EQ(0, SetVAD(true, true, VADVeryAggr));
  EXPECT_EQ(0, _acmA->SetFECStatus(false));
  EXPECT_FALSE(_acmA->FECStatus());
  Run();
  _outFileB.Close();

  EXPECT_EQ(0, _acmA->SetFECStatus(true));
  EXPECT_TRUE(_acmA->FECStatus());
  OpenOutFile(_testCntr);
  Run();
  _outFileB.Close();

  RegisterSendCodec('A', nameISAC, 32000);
  OpenOutFile(_testCntr);
  EXPECT_EQ(0, SetVAD(true, true, VADVeryAggr));
  EXPECT_EQ(0, _acmA->SetFECStatus(false));
  EXPECT_FALSE(_acmA->FECStatus());
  Run();
  _outFileB.Close();

  EXPECT_EQ(0, _acmA->SetFECStatus(true));
  EXPECT_TRUE(_acmA->FECStatus());
  OpenOutFile(_testCntr);
  Run();
  _outFileB.Close();

  RegisterSendCodec('A', nameISAC, 32000);
  OpenOutFile(_testCntr);
  EXPECT_EQ(0, SetVAD(false, false, VADNormal));
  EXPECT_EQ(0, _acmA->SetFECStatus(true));
  EXPECT_TRUE(_acmA->FECStatus());
  Run();

  RegisterSendCodec('A', nameISAC, 16000);
  EXPECT_TRUE(_acmA->FECStatus());
  Run();

  RegisterSendCodec('A', nameISAC, 32000);
  EXPECT_TRUE(_acmA->FECStatus());
  Run();

  RegisterSendCodec('A', nameISAC, 16000);
  EXPECT_TRUE(_acmA->FECStatus());
  Run();
  _outFileB.Close();

  _channelA2B->SetFECTestWithPacketLoss(true);

  EXPECT_EQ(0, RegisterSendCodec('A', nameG722));
  EXPECT_EQ(0, RegisterSendCodec('A', nameCN, 16000));
  OpenOutFile(_testCntr);
  EXPECT_EQ(0, SetVAD(true, true, VADAggr));
  EXPECT_EQ(0, _acmA->SetFECStatus(false));
  EXPECT_FALSE(_acmA->FECStatus());
  Run();
  _outFileB.Close();

  EXPECT_EQ(0, _acmA->SetFECStatus(true));
  EXPECT_TRUE(_acmA->FECStatus());
  OpenOutFile(_testCntr);
  Run();
  _outFileB.Close();

  RegisterSendCodec('A', nameISAC, 16000);
  OpenOutFile(_testCntr);
  EXPECT_EQ(0, SetVAD(true, true, VADVeryAggr));
  EXPECT_EQ(0, _acmA->SetFECStatus(false));
  EXPECT_FALSE(_acmA->FECStatus());
  Run();
  _outFileB.Close();

  EXPECT_EQ(0, _acmA->SetFECStatus(true));
  EXPECT_TRUE(_acmA->FECStatus());
  OpenOutFile(_testCntr);
  Run();
  _outFileB.Close();

  RegisterSendCodec('A', nameISAC, 32000);
  OpenOutFile(_testCntr);
  EXPECT_EQ(0, SetVAD(true, true, VADVeryAggr));
  EXPECT_EQ(0, _acmA->SetFECStatus(false));
  EXPECT_FALSE(_acmA->FECStatus());
  Run();
  _outFileB.Close();

  EXPECT_EQ(0, _acmA->SetFECStatus(true));
  EXPECT_TRUE(_acmA->FECStatus());
  OpenOutFile(_testCntr);
  Run();
  _outFileB.Close();

  RegisterSendCodec('A', nameISAC, 32000);
  OpenOutFile(_testCntr);
  EXPECT_EQ(0, SetVAD(false, false, VADNormal));
  EXPECT_EQ(0, _acmA->SetFECStatus(true));
  EXPECT_TRUE(_acmA->FECStatus());
  Run();

  RegisterSendCodec('A', nameISAC, 16000);
  EXPECT_TRUE(_acmA->FECStatus());
  Run();

  RegisterSendCodec('A', nameISAC, 32000);
  EXPECT_TRUE(_acmA->FECStatus());
  Run();

  RegisterSendCodec('A', nameISAC, 16000);
  EXPECT_TRUE(_acmA->FECStatus());
  Run();
  _outFileB.Close();
}

int32_t TestFEC::SetVAD(bool enableDTX, bool enableVAD, ACMVADMode vadMode) {
  return _acmA->SetVAD(enableDTX, enableVAD, vadMode);
}

int16_t TestFEC::RegisterSendCodec(char side, char* codecName,
                                   int32_t samplingFreqHz) {
  std::cout << std::flush;
  AudioCodingModule* myACM;
  switch (side) {
    case 'A': {
      myACM = _acmA.get();
      break;
    }
    case 'B': {
      myACM = _acmB.get();
      break;
    }
    default:
      return -1;
  }

  if (myACM == NULL) {
    assert(false);
    return -1;
  }
  CodecInst myCodecParam;
  EXPECT_GT(AudioCodingModule::Codec(codecName, &myCodecParam,
                                     samplingFreqHz, 1), -1);
  EXPECT_GT(myACM->RegisterSendCodec(myCodecParam), -1);

  // Initialization was successful.
  return 0;
}

void TestFEC::Run() {
  AudioFrame audioFrame;

  uint16_t msecPassed = 0;
  uint32_t secPassed = 0;
  int32_t outFreqHzB = _outFileB.SamplingFrequency();

  while (!_inFileA.EndOfFile()) {
    EXPECT_GT(_inFileA.Read10MsData(audioFrame), 0);
    EXPECT_EQ(0, _acmA->Add10MsData(audioFrame));
    EXPECT_GT(_acmA->Process(), -1);
    EXPECT_EQ(0, _acmB->PlayoutData10Ms(outFreqHzB, &audioFrame));
    _outFileB.Write10MsData(audioFrame.data_, audioFrame.samples_per_channel_);
    msecPassed += 10;
    if (msecPassed >= 1000) {
      msecPassed = 0;
      secPassed++;
    }
    // Test that toggling FEC on and off works.
    if (((secPassed % 5) == 4) && (msecPassed == 0) && (_testCntr > 14)) {
      EXPECT_EQ(0, _acmA->SetFECStatus(false));
    }
    if (((secPassed % 5) == 4) && (msecPassed >= 990) && (_testCntr > 14)) {
      EXPECT_EQ(0, _acmA->SetFECStatus(true));
    }
  }
  _inFileA.Rewind();
}

void TestFEC::OpenOutFile(int16_t test_number) {
  std::string file_name;
  std::stringstream file_stream;
  file_stream << webrtc::test::OutputPath();
  file_stream << "TestFEC_outFile_";
  file_stream << test_number << ".pcm";
  file_name = file_stream.str();
  _outFileB.Open(file_name, 16000, "wb");
}

}  // namespace webrtc
