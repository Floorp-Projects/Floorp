/*
 *  Copyright (c) 2013 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include <cstdio>
#include <map>

#include "testing/gtest/include/gtest/gtest.h"

#include "webrtc/system_wrappers/interface/clock.h"
#include "webrtc/typedefs.h"
#include "webrtc/video_engine/new_include/video_engine.h"
#include "webrtc/video_engine/test/common/direct_transport.h"
#include "webrtc/video_engine/test/common/flags.h"
#include "webrtc/video_engine/test/common/generate_ssrcs.h"
#include "webrtc/video_engine/test/common/run_tests.h"
#include "webrtc/video_engine/test/common/video_capturer.h"
#include "webrtc/video_engine/test/common/video_renderer.h"

namespace webrtc {

class LoopbackTest : public ::testing::Test {
 protected:
  std::map<uint32_t, bool> reserved_ssrcs;
};

TEST_F(LoopbackTest, Test) {
  test::VideoRenderer* local_preview = test::VideoRenderer::Create(
      "Local Preview", test::flags::Width(), test::flags::Height());
  test::VideoRenderer* loopback_video = test::VideoRenderer::Create(
      "Loopback Video", test::flags::Width(), test::flags::Height());

  newapi::VideoEngine* video_engine =
      newapi::VideoEngine::Create(webrtc::newapi::VideoEngineConfig());

  test::DirectTransport transport(NULL);
  newapi::VideoCall* call = video_engine->CreateCall(&transport);

  // Loopback, call sends to itself.
  transport.SetReceiver(call->Receiver());

  newapi::VideoSendStream::Config send_config = call->GetDefaultSendConfig();
  test::GenerateRandomSsrcs(&send_config, &reserved_ssrcs);

  send_config.local_renderer = local_preview;

  // TODO(pbos): static_cast shouldn't be required after mflodman refactors the
  //             VideoCodec struct.
  send_config.codec.width = static_cast<uint16_t>(test::flags::Width());
  send_config.codec.height = static_cast<uint16_t>(test::flags::Height());
  send_config.codec.minBitrate =
      static_cast<unsigned int>(test::flags::MinBitrate());
  send_config.codec.startBitrate =
      static_cast<unsigned int>(test::flags::StartBitrate());
  send_config.codec.maxBitrate =
      static_cast<unsigned int>(test::flags::MaxBitrate());

  newapi::VideoSendStream* send_stream = call->CreateSendStream(send_config);

  Clock* test_clock = Clock::GetRealTimeClock();

  test::VideoCapturer* camera =
      test::VideoCapturer::Create(send_stream->Input(),
                                  test::flags::Width(),
                                  test::flags::Height(),
                                  test::flags::Fps(),
                                  test_clock);

  newapi::VideoReceiveStream::Config receive_config =
      call->GetDefaultReceiveConfig();
  receive_config.rtp.ssrc = send_config.rtp.ssrcs[0];
  receive_config.renderer = loopback_video;

  newapi::VideoReceiveStream* receive_stream =
      call->CreateReceiveStream(receive_config);

  receive_stream->StartReceive();
  send_stream->StartSend();

  camera->Start();

  // TODO(pbos): Run this time limited (optionally), so it can run automated.
  puts(">> Press ENTER to continue...");
  while (getchar() != '\n' && !feof(stdin));

  receive_stream->StopReceive();
  send_stream->StopSend();

  // Stop sending
  delete camera;

  call->DestroyReceiveStream(receive_stream);
  call->DestroySendStream(send_stream);

  delete call;
  delete video_engine;

  delete loopback_video;
  delete local_preview;
}
}  // webrtc

int main(int argc, char* argv[]) {
  ::testing::InitGoogleTest(&argc, argv);
  webrtc::test::flags::Init(&argc, &argv);

  return webrtc::test::RunAllTests();
}
