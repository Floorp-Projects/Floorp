/*
 *  Copyright (c) 2011 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

/*
 *  Interface for starting test
 */

#ifndef WEBRTC_VOICE_ENGINE_VOE_TEST_INTERFACE_H
#define WEBRTC_VOICE_ENGINE_VOE_TEST_INTERFACE_H

#include "common_types.h"

namespace webrtc {
class CriticalSectionWrapper;
class EventWrapper;
class ThreadWrapper;
class VoENetwork;
}

namespace voetest {
// TODO(andrew): using directives are not permitted.
using namespace webrtc;

// TestType enumerator
enum TestType {
  Invalid = -1, Standard = 0, Extended = 1, Stress = 2, Unit = 3, CPU = 4
};

// ExtendedSelection enumerator
enum ExtendedSelection {
  XSEL_Invalid = -1,
  XSEL_None = 0,
  XSEL_All,
  XSEL_Base,
  XSEL_CallReport,
  XSEL_Codec,
  XSEL_DTMF,
  XSEL_Encryption,
  XSEL_ExternalMedia,
  XSEL_File,
  XSEL_Mixing,
  XSEL_Hardware,
  XSEL_NetEqStats,
  XSEL_Network,
  XSEL_RTP_RTCP,
  XSEL_VideoSync,
  XSEL_VolumeControl,
  XSEL_AudioProcessing,
};

// ----------------------------------------------------------------------------
//  External transport (Transport)
// ----------------------------------------------------------------------------

class FakeExternalTransport : public Transport {
 public:
  FakeExternalTransport(VoENetwork* ptr);
  virtual ~FakeExternalTransport();
  VoENetwork* my_network_;
  int SendPacket(int channel, const void *data, int len);
  int SendRTCPPacket(int channel, const void *data, int len);
  void SetDelayStatus(bool enabled, unsigned int delayInMs = 100);
 private:
  static bool Run(void* ptr);
  bool Process();
 private:
  ThreadWrapper*          thread_;
  CriticalSectionWrapper* lock_;
  EventWrapper*           event_;
 private:
  unsigned char           packet_buffer_[1612];
  int                     length_;
  int                     channel_;
  bool                    delay_is_enabled_;
  int                     delay_time_in_ms_;
};

// Main test function
int runAutoTest(TestType testType, ExtendedSelection extendedSel);

} //  namespace voetest
#endif // WEBRTC_VOICE_ENGINE_VOE_TEST_INTERFACE_H
