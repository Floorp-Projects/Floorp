/*
 *  Copyright (c) 2015 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include <string>

#include "webrtc/config.h"

namespace webrtc {

class VideoSendStream;
class Clock;

namespace test {

class VideoCapturer;

class Loopback {
 public:
  struct Config {
    size_t width;
    size_t height;
    int32_t fps;
    size_t min_bitrate_kbps;
    size_t start_bitrate_kbps;
    size_t max_bitrate_kbps;
    int32_t min_transmit_bitrate_kbps;
    std::string codec;
    int32_t loss_percent;
    int32_t link_capacity_kbps;
    int32_t queue_size;
    int32_t avg_propagation_delay_ms;
    int32_t std_propagation_delay_ms;
    bool logs;
  };

  explicit Loopback(const Config& config);
  virtual ~Loopback();

  void Run();

 protected:
  virtual VideoEncoderConfig CreateEncoderConfig();
  virtual VideoCapturer* CreateCapturer(VideoSendStream* send_stream);

  const Config config_;
  Clock* const clock_;
};

}  // namespace test
}  // namespace webrtc
