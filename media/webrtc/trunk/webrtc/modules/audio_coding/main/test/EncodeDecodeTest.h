/*
 *  Copyright (c) 2012 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef WEBRTC_MODULES_AUDIO_CODING_MAIN_TEST_ENCODEDECODETEST_H_
#define WEBRTC_MODULES_AUDIO_CODING_MAIN_TEST_ENCODEDECODETEST_H_

#include <stdio.h>

#include "webrtc/modules/audio_coding/main/interface/audio_coding_module.h"
#include "webrtc/modules/audio_coding/main/test/ACMTest.h"
#include "webrtc/modules/audio_coding/main/test/PCMFile.h"
#include "webrtc/modules/audio_coding/main/test/RTPFile.h"
#include "webrtc/typedefs.h"

namespace webrtc {

#define MAX_INCOMING_PAYLOAD 8096

class Config;

// TestPacketization callback which writes the encoded payloads to file
class TestPacketization : public AudioPacketizationCallback {
 public:
  TestPacketization(RTPStream *rtpStream, uint16_t frequency);
  ~TestPacketization();
  virtual int32_t SendData(const FrameType frameType, const uint8_t payloadType,
                           const uint32_t timeStamp, const uint8_t* payloadData,
                           const uint16_t payloadSize,
                           const RTPFragmentationHeader* fragmentation);

 private:
  static void MakeRTPheader(uint8_t* rtpHeader, uint8_t payloadType,
                            int16_t seqNo, uint32_t timeStamp, uint32_t ssrc);
  RTPStream* _rtpStream;
  int32_t _frequency;
  int16_t _seqNo;
};

class Sender {
 public:
  Sender();
  void Setup(AudioCodingModule *acm, RTPStream *rtpStream);
  void Teardown();
  void Run();
  bool Add10MsData();

  //for auto_test and logging
  uint8_t testMode;
  uint8_t codeId;

 private:
  AudioCodingModule* _acm;
  PCMFile _pcmFile;
  AudioFrame _audioFrame;
  TestPacketization* _packetization;
};

class Receiver {
 public:
  Receiver();
  void Setup(AudioCodingModule *acm, RTPStream *rtpStream);
  void Teardown();
  void Run();
  bool IncomingPacket();
  bool PlayoutData();

  //for auto_test and logging
  uint8_t codeId;
  uint8_t testMode;

 private:
  AudioCodingModule* _acm;
  RTPStream* _rtpStream;
  PCMFile _pcmFile;
  int16_t* _playoutBuffer;
  uint16_t _playoutLengthSmpls;
  uint8_t _incomingPayload[MAX_INCOMING_PAYLOAD];
  uint16_t _payloadSizeBytes;
  uint16_t _realPayloadSizeBytes;
  int32_t _frequency;
  bool _firstTime;
  WebRtcRTPHeader _rtpInfo;
  uint32_t _nextTime;
};

class EncodeDecodeTest : public ACMTest {
 public:
  explicit EncodeDecodeTest(const Config& config);
  EncodeDecodeTest(int testMode, const Config& config);
  virtual void Perform();

  uint16_t _playoutFreq;
  uint8_t _testMode;

 private:
  void EncodeToFile(int fileType, int codeId, int* codePars, int testMode);

  const Config& config_;

 protected:
  Sender _sender;
  Receiver _receiver;
};

}  // namespace webrtc

#endif  // WEBRTC_MODULES_AUDIO_CODING_MAIN_TEST_ENCODEDECODETEST_H_
