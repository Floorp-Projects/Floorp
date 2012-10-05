/*
 *  Copyright (c) 2012 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef WEBRTC_VOICE_ENGINE_VOE_UNIT_TEST_H
#define WEBRTC_VOICE_ENGINE_VOE_UNIT_TEST_H

#include "voice_engine/test/auto_test/voe_standard_test.h"

namespace voetest {

class VoETestManager;

class VoEUnitTest : public Encryption {
 public:
  VoEUnitTest(VoETestManager& mgr);
  ~VoEUnitTest() {}
  int DoTest();

 protected:
  // Encryption
  void encrypt(int channel_no, unsigned char * in_data,
               unsigned char * out_data, int bytes_in, int * bytes_out);
  void decrypt(int channel_no, unsigned char * in_data,
               unsigned char * out_data, int bytes_in, int * bytes_out);
  void encrypt_rtcp(int channel_no, unsigned char * in_data,
                    unsigned char * out_data, int bytes_in, int * bytes_out);
  void decrypt_rtcp(int channel_no, unsigned char * in_data,
                    unsigned char * out_data, int bytes_in, int * bytes_out);

 private:
  int MenuSelection();
  int MixerTest();
  void Sleep(unsigned int timeMillisec, bool addMarker = false);
  void Wait();
  int StartMedia(int channel,
                 int rtpPort,
                 bool listen,
                 bool playout,
                 bool send,
                 bool fileAsMic,
                 bool localFile);
  int StopMedia(int channel);
  void Test(const char* msg);
  void SetStereoExternalEncryption(int channel, bool onOff, int bitsPerSample);

 private:
  VoETestManager& _mgr;

 private:
  bool _listening[32];
  bool _playing[32];
  bool _sending[32];

 private:
  bool _extOnOff;
  int _extBitsPerSample;
  int _extChannel;
};

} //  namespace voetest
#endif // WEBRTC_VOICE_ENGINE_VOE_UNIT_TEST_H
