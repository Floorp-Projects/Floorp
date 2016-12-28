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

#include "testing/gtest/include/gtest/gtest.h"
#include "webrtc/video/video_quality_test.h"

namespace webrtc {

static const int kFullStackTestDurationSecs = 60;

class FullStackTest : public VideoQualityTest {
 public:
  void RunTest(const VideoQualityTest::Params &params) {
    RunWithAnalyzer(params);
  }
};

// VideoQualityTest::Params params = {
//   { ... },      // Common.
//   { ... },      // Video-specific settings.
//   { ... },      // Screenshare-specific settings.
//   { ... },      // Analyzer settings.
//   pipe,         // FakeNetworkPipe::Config
//   { ... },      // Spatial scalability.
//   logs          // bool
// };

TEST_F(FullStackTest, ParisQcifWithoutPacketLoss) {
  VideoQualityTest::Params paris_qcif = {
      {176, 144, 30, 300000, 300000, 300000, "VP8", 1},
      {"paris_qcif"},
      {},
      {"net_delay_0_0_plr_0", 36.0, 0.96, kFullStackTestDurationSecs}};
  RunTest(paris_qcif);
}

TEST_F(FullStackTest, ForemanCifWithoutPacketLoss) {
  // TODO(pbos): Decide on psnr/ssim thresholds for foreman_cif.
  VideoQualityTest::Params foreman_cif = {
      {352, 288, 30, 700000, 700000, 700000, "VP8", 1},
      {"foreman_cif"},
      {},
      {"foreman_cif_net_delay_0_0_plr_0", 0.0, 0.0, kFullStackTestDurationSecs}
  };
  RunTest(foreman_cif);
}

TEST_F(FullStackTest, ForemanCifPlr5) {
  VideoQualityTest::Params foreman_cif = {
      {352, 288, 30, 30000, 500000, 2000000, "VP8", 1},
      {"foreman_cif"},
      {},
      {"foreman_cif_delay_50_0_plr_5", 0.0, 0.0, kFullStackTestDurationSecs}};
  foreman_cif.pipe.loss_percent = 5;
  foreman_cif.pipe.queue_delay_ms = 50;
  RunTest(foreman_cif);
}

TEST_F(FullStackTest, ForemanCif500kbps) {
  VideoQualityTest::Params foreman_cif = {
      {352, 288, 30, 30000, 500000, 2000000, "VP8", 1},
      {"foreman_cif"},
      {},
      {"foreman_cif_500kbps", 0.0, 0.0, kFullStackTestDurationSecs}};
  foreman_cif.pipe.queue_length_packets = 0;
  foreman_cif.pipe.queue_delay_ms = 0;
  foreman_cif.pipe.link_capacity_kbps = 500;
  RunTest(foreman_cif);
}

TEST_F(FullStackTest, ForemanCif500kbpsLimitedQueue) {
  VideoQualityTest::Params foreman_cif = {
      {352, 288, 30, 30000, 500000, 2000000, "VP8", 1},
      {"foreman_cif"},
      {},
      {"foreman_cif_500kbps_32pkts_queue", 0.0, 0.0, kFullStackTestDurationSecs}
  };
  foreman_cif.pipe.queue_length_packets = 32;
  foreman_cif.pipe.queue_delay_ms = 0;
  foreman_cif.pipe.link_capacity_kbps = 500;
  RunTest(foreman_cif);
}

TEST_F(FullStackTest, ForemanCif500kbps100ms) {
  VideoQualityTest::Params foreman_cif = {
      {352, 288, 30, 30000, 500000, 2000000, "VP8", 1},
      {"foreman_cif"},
      {},
      {"foreman_cif_500kbps_100ms", 0.0, 0.0, kFullStackTestDurationSecs}};
  foreman_cif.pipe.queue_length_packets = 0;
  foreman_cif.pipe.queue_delay_ms = 100;
  foreman_cif.pipe.link_capacity_kbps = 500;
  RunTest(foreman_cif);
}

TEST_F(FullStackTest, ForemanCif500kbps100msLimitedQueue) {
  VideoQualityTest::Params foreman_cif = {
      {352, 288, 30, 30000, 500000, 2000000, "VP8", 1},
      {"foreman_cif"},
      {},
      {"foreman_cif_500kbps_100ms_32pkts_queue", 0.0, 0.0,
        kFullStackTestDurationSecs}};
  foreman_cif.pipe.queue_length_packets = 32;
  foreman_cif.pipe.queue_delay_ms = 100;
  foreman_cif.pipe.link_capacity_kbps = 500;
  RunTest(foreman_cif);
}

TEST_F(FullStackTest, ForemanCif1000kbps100msLimitedQueue) {
  VideoQualityTest::Params foreman_cif = {
      {352, 288, 30, 30000, 2000000, 2000000, "VP8", 1},
      {"foreman_cif"},
      {},
      {"foreman_cif_1000kbps_100ms_32pkts_queue", 0.0, 0.0,
        kFullStackTestDurationSecs}};
  foreman_cif.pipe.queue_length_packets = 32;
  foreman_cif.pipe.queue_delay_ms = 100;
  foreman_cif.pipe.link_capacity_kbps = 1000;
  RunTest(foreman_cif);
}

TEST_F(FullStackTest, ScreenshareSlidesVP8_2TL) {
  VideoQualityTest::Params screenshare = {
      {1850, 1110, 5, 50000, 200000, 2000000, "VP8", 2, 1, 400000},
      {},
      {true, 10},
      {"screenshare_slides", 0.0, 0.0, kFullStackTestDurationSecs}};
  RunTest(screenshare);
}

TEST_F(FullStackTest, ScreenshareSlidesVP8_2TL_Scroll) {
  VideoQualityTest::Params config = {
      {1850, 1110 / 2, 5, 50000, 200000, 2000000, "VP8", 2, 1, 400000},
      {},
      {true, 10, 2},
      {"screenshare_slides_scrolling", 0.0, 0.0, kFullStackTestDurationSecs}};
  RunTest(config);
}

TEST_F(FullStackTest, ScreenshareSlidesVP8_2TL_LossyNet) {
  VideoQualityTest::Params screenshare = {
      {1850, 1110, 5, 50000, 200000, 2000000, "VP8", 2, 1, 400000},
      {},          // Video-specific.
      {true, 10},  // Screenshare-specific.
      {"screenshare_slides_lossy_net", 0.0, 0.0, kFullStackTestDurationSecs}};
  screenshare.pipe.loss_percent = 5;
  screenshare.pipe.queue_delay_ms = 200;
  screenshare.pipe.link_capacity_kbps = 500;
  RunTest(screenshare);
}

TEST_F(FullStackTest, ScreenshareSlidesVP8_2TL_VeryLossyNet) {
  VideoQualityTest::Params screenshare = {
      {1850, 1110, 5, 50000, 200000, 2000000, "VP8", 2, 1, 400000},
      {},          // Video-specific.
      {true, 10},  // Screenshare-specific.
      {"screenshare_slides_very_lossy", 0.0, 0.0, kFullStackTestDurationSecs}};
  screenshare.pipe.loss_percent = 10;
  screenshare.pipe.queue_delay_ms = 200;
  screenshare.pipe.link_capacity_kbps = 500;
  RunTest(screenshare);
}

TEST_F(FullStackTest, ScreenshareSlidesVP9_2SL) {
  VideoQualityTest::Params screenshare = {
      {1850, 1110, 5, 50000, 200000, 2000000, "VP9", 1, 0, 400000},
      {},
      {true, 10},
      {"screenshare_slides_vp9_2sl", 0.0, 0.0, kFullStackTestDurationSecs},
      {},
      false,
      {std::vector<VideoStream>(), 0, 2, 1}};
  RunTest(screenshare);
}
}  // namespace webrtc
