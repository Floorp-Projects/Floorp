/*
 *  Copyright (c) 2012 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef WEBRTC_VIDEO_ENGINE_MAIN_TEST_AUTOTEST_INTERFACE_TB_VIDEO_CHANNEL_H_
#define WEBRTC_VIDEO_ENGINE_MAIN_TEST_AUTOTEST_INTERFACE_TB_VIDEO_CHANNEL_H_

#include "webrtc/base/scoped_ptr.h"
#include "webrtc/video_engine/test/libvietest/include/tb_interfaces.h"

namespace webrtc {
namespace test {
class VideoChannelTransport;
}  // namespace test
}  // namespace webrtc

class TbVideoChannel {
 public:
  TbVideoChannel(TbInterfaces& Engine,
                 webrtc::VideoCodecType sendCodec = webrtc::kVideoCodecVP8,
                 int width = 352, int height = 288, int frameRate = 30,
                 int startBitrate = 300);

  ~TbVideoChannel(void);

  void SetFrameSettings(int width, int height, int frameRate);

  void StartSend(const unsigned short rtpPort = 11000,
                 const char* ipAddress = "127.0.0.1");

  void StopSend();

  void StartReceive(const unsigned short rtpPort = 11000);

  void StopReceive();

  int videoChannel;

 private:
  TbInterfaces& ViE;
  rtc::scoped_ptr<webrtc::test::VideoChannelTransport> channel_transport_;
};


#endif  // WEBRTC_VIDEO_ENGINE_MAIN_TEST_AUTOTEST_INTERFACE_TB_VIDEO_CHANNEL_H_
