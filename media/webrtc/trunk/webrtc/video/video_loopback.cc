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
#include "webrtc/test/run_test.h"
#include "webrtc/typedefs.h"
#include "webrtc/video/loopback.h"

namespace webrtc {

namespace flags {

DEFINE_int32(width, 640, "Video width.");
size_t Width() {
  return static_cast<size_t>(FLAGS_width);
}

DEFINE_int32(height, 480, "Video height.");
size_t Height() {
  return static_cast<size_t>(FLAGS_height);
}

DEFINE_int32(fps, 30, "Frames per second.");
int Fps() {
  return static_cast<int>(FLAGS_fps);
}

DEFINE_int32(min_bitrate, 50, "Minimum video bitrate.");
size_t MinBitrate() {
  return static_cast<size_t>(FLAGS_min_bitrate);
}

DEFINE_int32(start_bitrate, 300, "Video starting bitrate.");
size_t StartBitrate() {
  return static_cast<size_t>(FLAGS_start_bitrate);
}

DEFINE_int32(max_bitrate, 800, "Maximum video bitrate.");
size_t MaxBitrate() {
  return static_cast<size_t>(FLAGS_max_bitrate);
}

int MinTransmitBitrate() {
  return 0;
}  // No min padding for regular video.

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

void Loopback() {
  test::Loopback::Config config{flags::Width(),
                                flags::Height(),
                                flags::Fps(),
                                flags::MinBitrate(),
                                flags::StartBitrate(),
                                flags::MaxBitrate(),
                                0,  // No min transmit bitrate.
                                flags::Codec(),
                                flags::LossPercent(),
                                flags::LinkCapacity(),
                                flags::QueueSize(),
                                flags::AvgPropagationDelayMs(),
                                flags::StdPropagationDelayMs(),
                                flags::FLAGS_logs};
  test::Loopback loopback(config);
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
