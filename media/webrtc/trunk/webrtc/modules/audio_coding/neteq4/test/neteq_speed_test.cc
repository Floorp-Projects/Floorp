/*
 *  Copyright (c) 2013 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include <stdio.h>

#include <iostream>

#include "gflags/gflags.h"
#include "webrtc/modules/audio_coding/codecs/pcm16b/include/pcm16b.h"
#include "webrtc/modules/audio_coding/neteq4/interface/neteq.h"
#include "webrtc/modules/audio_coding/neteq4/tools/audio_loop.h"
#include "webrtc/modules/audio_coding/neteq4/tools/rtp_generator.h"
#include "webrtc/test/testsupport/fileutils.h"
#include "webrtc/typedefs.h"

using webrtc::NetEq;
using webrtc::test::AudioLoop;
using webrtc::test::RtpGenerator;
using webrtc::WebRtcRTPHeader;

// Flag validators.
static bool ValidateRuntime(const char* flagname, int value) {
  if (value > 0)  // Value is ok.
    return true;
  printf("Invalid value for --%s: %d\n", flagname, static_cast<int>(value));
  return false;
}
static bool ValidateLossrate(const char* flagname, int value) {
  if (value >= 0)  // Value is ok.
    return true;
  printf("Invalid value for --%s: %d\n", flagname, static_cast<int>(value));
  return false;
}
static bool ValidateDriftfactor(const char* flagname, double value) {
  if (value >= 0.0 && value < 1.0)  // Value is ok.
    return true;
  printf("Invalid value for --%s: %f\n", flagname, value);
  return false;
}

// Define command line flags.
DEFINE_int32(runtime_ms, 10000, "Simulated runtime in ms.");
static const bool runtime_ms_dummy =
    google::RegisterFlagValidator(&FLAGS_runtime_ms, &ValidateRuntime);
DEFINE_int32(lossrate, 10,
             "Packet lossrate; drop every N packets.");
static const bool lossrate_dummy =
    google::RegisterFlagValidator(&FLAGS_lossrate, &ValidateLossrate);
DEFINE_double(drift, 0.1,
             "Clockdrift factor.");
static const bool drift_dummy =
    google::RegisterFlagValidator(&FLAGS_drift, &ValidateDriftfactor);

int main(int argc, char* argv[]) {
  static const int kMaxChannels = 1;
  static const int kMaxSamplesPerMs = 48000 / 1000;
  static const int kOutputBlockSizeMs = 10;
  const std::string kInputFileName =
        webrtc::test::ResourcePath("audio_coding/testfile32kHz", "pcm");
  const int kSampRateHz = 32000;
  const webrtc::NetEqDecoder kDecoderType = webrtc::kDecoderPCM16Bswb32kHz;
  const int kPayloadType = 95;

  std::string program_name = argv[0];
  std::string usage = "Tool for measuring the speed of NetEq.\n"
      "Usage: " + program_name + " [options]\n\n"
      "  --runtime_ms=N         runtime in ms; default is 10000 ms\n"
      "  --lossrate=N           drop every N packets; default is 10\n"
      "  --drift=F              clockdrift factor between 0.0 and 1.0; "
      "default is 0.1\n";
  google::SetUsageMessage(usage);
  google::ParseCommandLineFlags(&argc, &argv, true);

  if (argc != 1) {
    // Print usage information.
    std::cout << google::ProgramUsage();
    return 0;
  }

  // Initialize NetEq instance.
  NetEq* neteq = NetEq::Create(kSampRateHz);
  // Register decoder in |neteq|.
  int error;
  error = neteq->RegisterPayloadType(kDecoderType, kPayloadType);
  if (error) {
    std::cerr << "Cannot register decoder." << std::endl;
    exit(1);
  }

  // Set up AudioLoop object.
  AudioLoop audio_loop;
  const size_t kMaxLoopLengthSamples = kSampRateHz * 10;  // 10 second loop.
  const size_t kInputBlockSizeSamples = 60 * kSampRateHz / 1000;  // 60 ms.
  if (!audio_loop.Init(kInputFileName, kMaxLoopLengthSamples,
                       kInputBlockSizeSamples)) {
    std::cerr << "Cannot initialize AudioLoop object." << std::endl;
    exit(1);
  }

  int32_t time_now_ms = 0;

  // Get first input packet.
  WebRtcRTPHeader rtp_header;
  RtpGenerator rtp_gen(kSampRateHz / 1000);
  // Start with positive drift first half of simulation.
  double drift_factor = 0.1;
  rtp_gen.set_drift_factor(drift_factor);
  bool drift_flipped = false;
  int32_t packet_input_time_ms =
      rtp_gen.GetRtpHeader(kPayloadType, kInputBlockSizeSamples, &rtp_header);
  const int16_t* input_samples = audio_loop.GetNextBlock();
  if (!input_samples) exit(1);
  uint8_t input_payload[kInputBlockSizeSamples * sizeof(int16_t)];
  int payload_len = WebRtcPcm16b_Encode(const_cast<int16_t*>(input_samples),
                                        kInputBlockSizeSamples,
                                        input_payload);
  assert(payload_len == kInputBlockSizeSamples * sizeof(int16_t));

  // Main loop.
  while (time_now_ms < FLAGS_runtime_ms) {
    while (packet_input_time_ms <= time_now_ms) {
      // Drop every N packets, where N = FLAGS_lossrate.
      bool lost = false;
      if (FLAGS_lossrate > 0) {
        lost = ((rtp_header.header.sequenceNumber - 1) % FLAGS_lossrate) == 0;
      }
      if (!lost) {
        // Insert packet.
        int error = neteq->InsertPacket(
            rtp_header, input_payload, payload_len,
            packet_input_time_ms * kSampRateHz / 1000);
        if (error != NetEq::kOK) {
          std::cerr << "InsertPacket returned error code " <<
              neteq->LastError() << std::endl;
          exit(1);
        }
      }

      // Get next packet.
      packet_input_time_ms = rtp_gen.GetRtpHeader(kPayloadType,
                                                  kInputBlockSizeSamples,
                                                  &rtp_header);
      input_samples = audio_loop.GetNextBlock();
      if (!input_samples) exit(1);
      payload_len = WebRtcPcm16b_Encode(const_cast<int16_t*>(input_samples),
                                        kInputBlockSizeSamples,
                                        input_payload);
      assert(payload_len == kInputBlockSizeSamples * sizeof(int16_t));
    }

    // Get output audio, but don't do anything with it.
    static const int kOutDataLen = kOutputBlockSizeMs * kMaxSamplesPerMs *
        kMaxChannels;
    int16_t out_data[kOutDataLen];
    int num_channels;
    int samples_per_channel;
    int error = neteq->GetAudio(kOutDataLen, out_data, &samples_per_channel,
                                &num_channels, NULL);
    if (error != NetEq::kOK) {
      std::cerr << "GetAudio returned error code " <<
          neteq->LastError() << std::endl;
      exit(1);
    }
    assert(samples_per_channel == kSampRateHz * 10 / 1000);

    time_now_ms += kOutputBlockSizeMs;
    if (time_now_ms >= FLAGS_runtime_ms / 2 && !drift_flipped) {
      // Apply negative drift second half of simulation.
      rtp_gen.set_drift_factor(-drift_factor);
      drift_flipped = true;
    }
  }

  std::cout << "Simulation done" << std::endl;
  delete neteq;
  return 0;
}
