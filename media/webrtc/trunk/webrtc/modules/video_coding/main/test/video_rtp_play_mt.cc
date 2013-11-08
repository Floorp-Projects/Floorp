/*
 *  Copyright (c) 2012 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include <assert.h>

#include "webrtc/modules/video_coding/main/test/receiver_tests.h"
#include "webrtc/modules/video_coding/main/test/vcm_payload_sink_factory.h"
#include "webrtc/system_wrappers/interface/event_wrapper.h"
#include "webrtc/system_wrappers/interface/thread_wrapper.h"
#include "webrtc/system_wrappers/interface/trace.h"
#include "webrtc/test/testsupport/fileutils.h"

using webrtc::rtpplayer::RtpPlayerInterface;
using webrtc::rtpplayer::VcmPayloadSinkFactory;

namespace {

const bool kConfigProtectionEnabled = true;
const webrtc::VCMVideoProtection kConfigProtectionMethod =
    webrtc::kProtectionDualDecoder;
const float kConfigLossRate = 0.05f;
const uint32_t kConfigRttMs = 50;
const bool kConfigReordering = false;
const uint32_t kConfigRenderDelayMs = 0;
const uint32_t kConfigMinPlayoutDelayMs = 0;
const int64_t kConfigMaxRuntimeMs = 10000;

}  // namespace

bool PlayerThread(void* obj) {
  assert(obj);
  RtpPlayerInterface* rtp_player = static_cast<RtpPlayerInterface*>(obj);

  webrtc::scoped_ptr<webrtc::EventWrapper> wait_event(
      webrtc::EventWrapper::Create());
  if (wait_event.get() == NULL) {
    return false;
  }

  webrtc::Clock* clock = webrtc::Clock::GetRealTimeClock();
  if (rtp_player->NextPacket(clock->TimeInMilliseconds()) < 0) {
    return false;
  }
  wait_event->Wait(rtp_player->TimeUntilNextPacket());

  return true;
}

bool ProcessingThread(void* obj) {
  assert(obj);
  return static_cast<VcmPayloadSinkFactory*>(obj)->ProcessAll();
}

bool DecodeThread(void* obj) {
  assert(obj);
  return static_cast<VcmPayloadSinkFactory*>(obj)->DecodeAll();
}

int RtpPlayMT(const CmdArgs& args) {
  std::string trace_file = webrtc::test::OutputPath() + "receiverTestTrace.txt";
  webrtc::Trace::CreateTrace();
  webrtc::Trace::SetTraceFile(trace_file.c_str());
  webrtc::Trace::set_level_filter(webrtc::kTraceAll);

  webrtc::rtpplayer::PayloadTypes payload_types;
  payload_types.push_back(webrtc::rtpplayer::PayloadCodecTuple(
      VCM_VP8_PAYLOAD_TYPE, "VP8", webrtc::kVideoCodecVP8));

  std::string output_file = args.outputFile;
  if (output_file == "") {
    output_file = webrtc::test::OutputPath() + "RtpPlayMT_decoded.yuv";
  }

  webrtc::Clock* clock = webrtc::Clock::GetRealTimeClock();
  VcmPayloadSinkFactory factory(output_file, clock, kConfigProtectionEnabled,
      kConfigProtectionMethod, kConfigRttMs, kConfigRenderDelayMs,
      kConfigMinPlayoutDelayMs);
  webrtc::scoped_ptr<RtpPlayerInterface> rtp_player(webrtc::rtpplayer::Create(
      args.inputFile, &factory, clock, payload_types, kConfigLossRate,
      kConfigRttMs, kConfigReordering));
  if (rtp_player.get() == NULL) {
    return -1;
  }

  {
    webrtc::scoped_ptr<webrtc::ThreadWrapper> player_thread(
        webrtc::ThreadWrapper::CreateThread(PlayerThread, rtp_player.get(),
            webrtc::kNormalPriority, "PlayerThread"));
    if (player_thread.get() == NULL) {
      printf("Unable to start RTP reader thread\n");
      return -1;
    }

    webrtc::scoped_ptr<webrtc::ThreadWrapper> processing_thread(
        webrtc::ThreadWrapper::CreateThread(ProcessingThread, &factory,
            webrtc::kNormalPriority, "ProcessingThread"));
    if (processing_thread.get() == NULL) {
      printf("Unable to start processing thread\n");
      return -1;
    }

    webrtc::scoped_ptr<webrtc::ThreadWrapper> decode_thread(
        webrtc::ThreadWrapper::CreateThread(DecodeThread, &factory,
            webrtc::kNormalPriority, "DecodeThread"));
    if (decode_thread.get() == NULL) {
      printf("Unable to start decode thread\n");
      return -1;
    }

    webrtc::scoped_ptr<webrtc::EventWrapper> wait_event(
        webrtc::EventWrapper::Create());
    if (wait_event.get() == NULL) {
      printf("Unable to create wait event\n");
      return -1;
    }

    unsigned int dummy_thread_id;
    player_thread->Start(dummy_thread_id);
    processing_thread->Start(dummy_thread_id);
    decode_thread->Start(dummy_thread_id);

    wait_event->Wait(kConfigMaxRuntimeMs);

    while (!player_thread->Stop()) {
    }
    while (!processing_thread->Stop()) {
    }
    while (!decode_thread->Stop()) {
    }
  }

  rtp_player->Print();

  webrtc::Trace::ReturnTrace();
  return 0;
}
