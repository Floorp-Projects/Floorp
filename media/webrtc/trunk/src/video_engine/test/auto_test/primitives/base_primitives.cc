/*
 *  Copyright (c) 2012 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "base_primitives.h"

#include "vie_autotest.h"
#include "vie_autotest_defines.h"
#include "video_capture_factory.h"

void TestI420CallSetup(webrtc::ViECodec* codec_interface,
                       webrtc::VideoEngine* video_engine,
                       webrtc::ViEBase* base_interface,
                       webrtc::ViENetwork* network_interface,
                       int video_channel,
                       const char* device_name) {
  webrtc::VideoCodec video_codec;
  memset(&video_codec, 0, sizeof(webrtc::VideoCodec));

  // Set up the codec interface with all known receive codecs and with
  // I420 as the send codec.
  for (int i = 0; i < codec_interface->NumberOfCodecs(); i++) {
    EXPECT_EQ(0, codec_interface->GetCodec(i, video_codec));

    // Try to keep the test frame size small when I420.
    if (video_codec.codecType == webrtc::kVideoCodecI420) {
      video_codec.width = 176;
      video_codec.height = 144;
      EXPECT_EQ(0, codec_interface->SetSendCodec(video_channel, video_codec));
    }

    EXPECT_EQ(0, codec_interface->SetReceiveCodec(video_channel, video_codec));
  }

  // Verify that we really found the I420 codec.
  EXPECT_EQ(0, codec_interface->GetSendCodec(video_channel, video_codec));
  EXPECT_EQ(webrtc::kVideoCodecI420, video_codec.codecType);

  // Set up senders and receivers.
  char version[1024] = "";
  EXPECT_EQ(0, base_interface->GetVersion(version));
  ViETest::Log("\nUsing WebRTC Video Engine version: %s", version);

  const char *ipAddress = "127.0.0.1";
  WebRtc_UWord16 rtpPortListen = 6100;
  WebRtc_UWord16 rtpPortSend = 6100;
  EXPECT_EQ(0, network_interface->SetLocalReceiver(video_channel,
                                                   rtpPortListen));
  EXPECT_EQ(0, base_interface->StartReceive(video_channel));
  EXPECT_EQ(0, network_interface->SetSendDestination(video_channel, ipAddress,
                                                     rtpPortSend));
  EXPECT_EQ(0, base_interface->StartSend(video_channel));

  // Call started.
  ViETest::Log("Call started");

  AutoTestSleep(KAutoTestSleepTimeMs);

  // Done.
  EXPECT_EQ(0, base_interface->StopSend(video_channel));
}
