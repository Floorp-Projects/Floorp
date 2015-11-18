/*
 *  Copyright (c) 2015 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include <stdio.h>

#include <map>

#include "gflags/gflags.h"
#include "testing/gtest/include/gtest/gtest.h"

#include "webrtc/test/field_trial.h"
#include "webrtc/test/frame_generator.h"
#include "webrtc/test/frame_generator_capturer.h"
#include "webrtc/test/run_test.h"
#include "webrtc/test/testsupport/fileutils.h"
#include "webrtc/typedefs.h"
#include "webrtc/video/loopback.h"
#include "webrtc/video/video_send_stream.h"

namespace webrtc {
namespace flags {

// Fixed for prerecorded screenshare content.
size_t Width() {
  return 1850;
}
size_t Height() {
  return 1110;
}

DEFINE_int32(fps, 5, "Frames per second.");
int Fps() {
  return static_cast<int>(FLAGS_fps);
}

DEFINE_int32(min_bitrate, 50, "Minimum video bitrate.");
size_t MinBitrate() {
  return static_cast<size_t>(FLAGS_min_bitrate);
}

DEFINE_int32(tl0_bitrate, 100, "Temporal layer 0 target bitrate.");
size_t StartBitrate() {
  return static_cast<size_t>(FLAGS_tl0_bitrate);
}

DEFINE_int32(tl1_bitrate, 1000, "Temporal layer 1 target bitrate.");
size_t MaxBitrate() {
  return static_cast<size_t>(FLAGS_tl1_bitrate);
}

DEFINE_int32(min_transmit_bitrate, 400, "Min transmit bitrate incl. padding.");
int MinTransmitBitrate() {
  return FLAGS_min_transmit_bitrate;
}

DEFINE_string(codec, "VP8", "Video codec to use.");
std::string Codec() {
  return static_cast<std::string>(FLAGS_codec);
}

DEFINE_int32(loss_percent, 0, "Percentage of packets randomly lost.");
int LossPercent() {
  return static_cast<int>(FLAGS_loss_percent);
}

DEFINE_int32(link_capacity,
             0,
             "Capacity (kbps) of the fake link. 0 means infinite.");
int LinkCapacity() {
  return static_cast<int>(FLAGS_link_capacity);
}

DEFINE_int32(queue_size, 0, "Size of the bottleneck link queue in packets.");
int QueueSize() {
  return static_cast<int>(FLAGS_queue_size);
}

DEFINE_int32(avg_propagation_delay_ms,
             0,
             "Average link propagation delay in ms.");
int AvgPropagationDelayMs() {
  return static_cast<int>(FLAGS_avg_propagation_delay_ms);
}

DEFINE_int32(std_propagation_delay_ms,
             0,
             "Link propagation delay standard deviation in ms.");
int StdPropagationDelayMs() {
  return static_cast<int>(FLAGS_std_propagation_delay_ms);
}

DEFINE_bool(logs, false, "print logs to stderr");

DEFINE_string(
    force_fieldtrials,
    "",
    "Field trials control experimental feature code which can be forced. "
    "E.g. running with --force_fieldtrials=WebRTC-FooFeature/Enable/"
    " will assign the group Enable to field trial WebRTC-FooFeature. Multiple "
    "trials are separated by \"/\"");
}  // namespace flags

class ScreenshareLoopback : public test::Loopback {
 public:
  explicit ScreenshareLoopback(const Config& config) : Loopback(config) {}
  virtual ~ScreenshareLoopback() {}

 protected:
  VideoEncoderConfig CreateEncoderConfig() override {
    VideoEncoderConfig encoder_config(test::Loopback::CreateEncoderConfig());
    VideoStream* stream = &encoder_config.streams[0];
    encoder_config.content_type = VideoEncoderConfig::kScreenshare;
    encoder_config.min_transmit_bitrate_bps = flags::MinTransmitBitrate();
    VideoCodecVP8 vp8_settings = VideoEncoder::GetDefaultVp8Settings();
    vp8_settings.denoisingOn = false;
    vp8_settings.frameDroppingOn = false;
    vp8_settings.numberOfTemporalLayers = 2;
    encoder_config.encoder_specific_settings = &vp8_settings;
    stream->temporal_layer_thresholds_bps.clear();
    stream->target_bitrate_bps =
        static_cast<int>(config_.start_bitrate_kbps) * 1000;
    stream->temporal_layer_thresholds_bps.push_back(stream->target_bitrate_bps);
    return encoder_config;
  }

  test::VideoCapturer* CreateCapturer(VideoSendStream* send_stream) override {
    std::vector<std::string> slides;
    slides.push_back(test::ResourcePath("web_screenshot_1850_1110", "yuv"));
    slides.push_back(test::ResourcePath("presentation_1850_1110", "yuv"));
    slides.push_back(test::ResourcePath("photo_1850_1110", "yuv"));
    slides.push_back(test::ResourcePath("difficult_photo_1850_1110", "yuv"));

    test::FrameGenerator* frame_generator =
        test::FrameGenerator::CreateFromYuvFile(
            slides, flags::Width(), flags::Height(), 10 * flags::Fps());
    test::FrameGeneratorCapturer* capturer(new test::FrameGeneratorCapturer(
        clock_, send_stream->Input(), frame_generator, flags::Fps()));
    EXPECT_TRUE(capturer->Init());
    return capturer;
  }
};

void Loopback() {
  test::Loopback::Config config{flags::Width(),
                                flags::Height(),
                                flags::Fps(),
                                flags::MinBitrate(),
                                flags::StartBitrate(),
                                flags::MaxBitrate(),
                                flags::MinTransmitBitrate(),
                                flags::Codec(),
                                flags::LossPercent(),
                                flags::LinkCapacity(),
                                flags::QueueSize(),
                                flags::AvgPropagationDelayMs(),
                                flags::StdPropagationDelayMs(),
                                flags::FLAGS_logs};
  ScreenshareLoopback loopback(config);
  loopback.Run();
}
}  // namespace webrtc

int main(int argc, char* argv[]) {
  ::testing::InitGoogleTest(&argc, argv);
  google::ParseCommandLineFlags(&argc, &argv, true);
  webrtc::test::InitFieldTrialsFromString(
      webrtc::flags::FLAGS_force_fieldtrials);
  webrtc::test::RunTest(webrtc::Loopback);
  return 0;
}
