/*
 *  Copyright (c) 2012 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "webrtc/modules/video_coding/main/test/receiver_tests.h"
#include "webrtc/modules/video_coding/main/test/vcm_payload_sink_factory.h"
#include "webrtc/system_wrappers/interface/trace.h"
#include "webrtc/test/testsupport/fileutils.h"

namespace {

const bool kConfigProtectionEnabled = false;
const webrtc::VCMVideoProtection kConfigProtectionMethod =
    webrtc::kProtectionNack;
const float kConfigLossRate = 0.0f;
const bool kConfigReordering = false;
const uint32_t kConfigRttMs = 100;
const uint32_t kConfigRenderDelayMs = 0;
const uint32_t kConfigMinPlayoutDelayMs = 0;
const int64_t kConfigMaxRuntimeMs = -1;

}  // namespace

int DecodeFromStorageTest(const CmdArgs& args) {
  std::string trace_file = webrtc::test::OutputPath() +
      "decodeFromStorageTestTrace.txt";
  webrtc::Trace::CreateTrace();
  webrtc::Trace::SetTraceFile(trace_file.c_str());
  webrtc::Trace::SetLevelFilter(webrtc::kTraceAll);

  webrtc::rtpplayer::PayloadTypes payload_types;
  payload_types.push_back(webrtc::rtpplayer::PayloadCodecTuple(
      VCM_VP8_PAYLOAD_TYPE, "VP8", webrtc::kVideoCodecVP8));

  std::string output_file = args.outputFile;
  if (output_file == "") {
    output_file = webrtc::test::OutputPath() + "DecodeFromStorage.yuv";
  }

  webrtc::SimulatedClock clock(0);
  webrtc::rtpplayer::VcmPayloadSinkFactory factory(output_file, &clock,
      kConfigProtectionEnabled, kConfigProtectionMethod, kConfigRttMs,
      kConfigRenderDelayMs, kConfigMinPlayoutDelayMs, true);
  webrtc::scoped_ptr<webrtc::rtpplayer::RtpPlayerInterface> rtp_player(
      webrtc::rtpplayer::Create(args.inputFile, &factory, &clock, payload_types,
          kConfigLossRate, kConfigRttMs, kConfigReordering));
  if (rtp_player.get() == NULL) {
    return -1;
  }

  int ret = 0;
  while ((ret = rtp_player->NextPacket(clock.TimeInMilliseconds())) == 0) {
    ret = factory.DecodeAndProcessAll(false);
    if (ret < 0 || (kConfigMaxRuntimeMs > -1 &&
        clock.TimeInMilliseconds() >= kConfigMaxRuntimeMs)) {
      break;
    }
    clock.AdvanceTimeMilliseconds(1);
  }

  rtp_player->Print();

  switch (ret) {
    case 1:
      printf("Success\n");
      break;
    case -1:
      printf("Failed\n");
      break;
    case 0:
      printf("Timeout\n");
      break;
  }

  webrtc::Trace::ReturnTrace();
  return 0;
}
