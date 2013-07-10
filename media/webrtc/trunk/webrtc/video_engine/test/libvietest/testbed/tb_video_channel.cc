/*
 *  Copyright (c) 2012 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "video_engine/test/libvietest/include/tb_video_channel.h"

#include "gtest/gtest.h"
#include "webrtc/test/channel_transport/include/channel_transport.h"

TbVideoChannel::TbVideoChannel(TbInterfaces& Engine,
                               webrtc::VideoCodecType sendCodec, int width,
                               int height, int frameRate, int startBitrate)
    : videoChannel(-1),
      ViE(Engine) {
  EXPECT_EQ(0, ViE.base->CreateChannel(videoChannel));
  channel_transport_.reset(new webrtc::test::VideoChannelTransport(
      ViE.network, videoChannel));

  webrtc::VideoCodec videoCodec;
  memset(&videoCodec, 0, sizeof(webrtc::VideoCodec));
  bool sendCodecSet = false;
  for (int idx = 0; idx < ViE.codec->NumberOfCodecs(); idx++) {
    EXPECT_EQ(0, ViE.codec->GetCodec(idx, videoCodec));
    videoCodec.width = width;
    videoCodec.height = height;
    videoCodec.maxFramerate = frameRate;

    if (videoCodec.codecType == sendCodec && sendCodecSet == false) {
      if (videoCodec.codecType != webrtc::kVideoCodecI420) {
        videoCodec.startBitrate = startBitrate;
        videoCodec.maxBitrate = startBitrate * 3;
      }
      EXPECT_EQ(0, ViE.codec->SetSendCodec(videoChannel, videoCodec));
      sendCodecSet = true;
    }
    if (videoCodec.codecType == webrtc::kVideoCodecVP8) {
      videoCodec.width = 352;
      videoCodec.height = 288;
    }
    EXPECT_EQ(0, ViE.codec->SetReceiveCodec(videoChannel, videoCodec));
  }
  EXPECT_TRUE(sendCodecSet);
}

TbVideoChannel::~TbVideoChannel() {
  EXPECT_EQ(0, ViE.base->DeleteChannel(videoChannel));
}

void TbVideoChannel::StartSend(const unsigned short rtp_port,
                               const char* ip_address) {
  EXPECT_EQ(0, channel_transport_->SetSendDestination(ip_address, rtp_port));
  EXPECT_EQ(0, ViE.base->StartSend(videoChannel));
}

void TbVideoChannel::SetFrameSettings(int width, int height, int frameRate) {
  webrtc::VideoCodec videoCodec;
  EXPECT_EQ(0, ViE.codec->GetSendCodec(videoChannel, videoCodec));
  videoCodec.width = width;
  videoCodec.height = height;
  videoCodec.maxFramerate = frameRate;

  EXPECT_EQ(0, ViE.codec->SetSendCodec(videoChannel, videoCodec));
  EXPECT_EQ(0, ViE.codec->SetReceiveCodec(videoChannel, videoCodec));
}

void TbVideoChannel::StopSend() {
  EXPECT_EQ(0, ViE.base->StopSend(videoChannel));
}

void TbVideoChannel::StartReceive(unsigned short rtp_port) {
  EXPECT_EQ(0, channel_transport_->SetLocalReceiver(rtp_port));
  EXPECT_EQ(0, ViE.base->StartReceive(videoChannel));
}

void TbVideoChannel::StopReceive() {
  EXPECT_EQ(0, ViE.base->StopReceive(videoChannel));
}
