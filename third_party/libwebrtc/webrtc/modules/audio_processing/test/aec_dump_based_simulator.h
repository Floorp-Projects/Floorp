/*
 *  Copyright (c) 2016 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef MODULES_AUDIO_PROCESSING_TEST_AEC_DUMP_BASED_SIMULATOR_H_
#define MODULES_AUDIO_PROCESSING_TEST_AEC_DUMP_BASED_SIMULATOR_H_

#include "modules/audio_processing/test/audio_processing_simulator.h"

#include "rtc_base/constructormagic.h"
#include "rtc_base/ignore_wundef.h"

RTC_PUSH_IGNORING_WUNDEF()
#ifdef WEBRTC_ANDROID_PLATFORM_BUILD
#include "external/webrtc/webrtc/modules/audio_processing/debug.pb.h"
#else
#include "modules/audio_processing/debug.pb.h"
#endif
RTC_POP_IGNORING_WUNDEF()

namespace webrtc {
namespace test {

// Used to perform an audio processing simulation from an aec dump.
class AecDumpBasedSimulator final : public AudioProcessingSimulator {
 public:
  explicit AecDumpBasedSimulator(const SimulationSettings& settings);
  ~AecDumpBasedSimulator() override;

  // Processes the messages in the aecdump file.
  void Process() override;

 private:
  void HandleMessage(const webrtc::audioproc::Init& msg);
  void HandleMessage(const webrtc::audioproc::Stream& msg);
  void HandleMessage(const webrtc::audioproc::ReverseStream& msg);
  void HandleMessage(const webrtc::audioproc::Config& msg);
  void PrepareProcessStreamCall(const webrtc::audioproc::Stream& msg);
  void PrepareReverseProcessStreamCall(
      const webrtc::audioproc::ReverseStream& msg);
  void VerifyProcessStreamBitExactness(const webrtc::audioproc::Stream& msg);

  enum InterfaceType {
    kFixedInterface,
    kFloatInterface,
    kNotSpecified,
  };

  FILE* dump_input_file_;
  std::unique_ptr<ChannelBuffer<float>> artificial_nearend_buf_;
  std::unique_ptr<ChannelBufferWavReader> artificial_nearend_buffer_reader_;
  bool artificial_nearend_eof_reported_ = false;
  InterfaceType interface_used_ = InterfaceType::kNotSpecified;

  RTC_DISALLOW_IMPLICIT_CONSTRUCTORS(AecDumpBasedSimulator);
};

}  // namespace test
}  // namespace webrtc

#endif  // MODULES_AUDIO_PROCESSING_TEST_AEC_DUMP_BASED_SIMULATOR_H_
