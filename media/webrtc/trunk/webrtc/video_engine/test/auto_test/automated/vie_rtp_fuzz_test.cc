/*
 *  Copyright (c) 2012 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include <ctime>

#include "gflags/gflags.h"
#include "gtest/gtest.h"
#include "webrtc/test/libtest/include/bit_flip_encryption.h"
#include "webrtc/test/libtest/include/random_encryption.h"
#include "webrtc/video_engine/test/auto_test/automated/two_windows_fixture.h"
#include "webrtc/video_engine/test/auto_test/interface/vie_autotest_window_manager_interface.h"
#include "webrtc/video_engine/test/auto_test/interface/vie_window_creator.h"
#include "webrtc/video_engine/test/auto_test/primitives/general_primitives.h"
#include "webrtc/video_engine/test/libvietest/include/tb_capture_device.h"
#include "webrtc/video_engine/test/libvietest/include/tb_interfaces.h"
#include "webrtc/video_engine/test/libvietest/include/tb_video_channel.h"

namespace {

DEFINE_int32(rtp_fuzz_test_rand_seed, 0, "The rand seed to use for "
    "the RTP fuzz test. Defaults to time(). 0 cannot be specified.");

class ViERtpFuzzTest : public TwoWindowsFixture {
protected:
  TbVideoChannel* video_channel_;
  TbInterfaces* video_engine_;
  TbCaptureDevice* capture_device_;

  void SetUp() {
    video_engine_ = new TbInterfaces(
        "ViERtpTryInjectingRandomPacketsIntoRtpStream");
    video_channel_ = new TbVideoChannel(
        *video_engine_, webrtc::kVideoCodecVP8);
    capture_device_ = new TbCaptureDevice(*video_engine_);

    capture_device_->ConnectTo(video_channel_->videoChannel);

    // Enable PLI RTCP, which will allow the video engine to recover better.
    video_engine_->rtp_rtcp->SetKeyFrameRequestMethod(
        video_channel_->videoChannel, webrtc::kViEKeyFrameRequestPliRtcp);

    video_channel_->StartReceive();
    video_channel_->StartSend();

    RenderInWindow(
        video_engine_->render, capture_device_->captureId, window_1_, 0);
    RenderInWindow(
        video_engine_->render, video_channel_->videoChannel, window_2_, 1);
  }

  void TearDown() {
    delete capture_device_;
    delete video_channel_;
    delete video_engine_;
  }

  unsigned int FetchRandSeed() {
    if (FLAGS_rtp_fuzz_test_rand_seed != 0) {
      return FLAGS_rtp_fuzz_test_rand_seed;
    }
    return std::time(NULL);
  }

  // Pass in a number [0, 1] which will be the bit flip probability per byte.
  void BitFlipFuzzTest(float flip_probability) {
    unsigned int rand_seed = FetchRandSeed();
    ViETest::Log("Running test with rand seed %d.", rand_seed);

    ViETest::Log("Running as usual. You should see video output.");
    AutoTestSleep(2000);

    ViETest::Log("Starting to flip bits in packets (%f%% chance per byte).",
                 flip_probability * 100);
    BitFlipEncryption bit_flip_encryption(rand_seed, flip_probability);
    video_engine_->encryption->RegisterExternalEncryption(
        video_channel_->videoChannel, bit_flip_encryption);

    AutoTestSleep(5000);

    ViETest::Log("Back to normal. Flipped %d bits.",
                 bit_flip_encryption.flip_count());
    video_engine_->encryption->DeregisterExternalEncryption(
        video_channel_->videoChannel);

    AutoTestSleep(5000);
  }
};

TEST_F(ViERtpFuzzTest, VideoEngineDealsWithASmallNumberOfTamperedPackets) {
  // Try 0.005% bit flip chance per byte.
  BitFlipFuzzTest(0.00005f);
}

TEST_F(ViERtpFuzzTest, VideoEngineDealsWithAMediumNumberOfTamperedPackets) {
  // Try 0.05% bit flip chance per byte.
  BitFlipFuzzTest(0.0005f);
}

TEST_F(ViERtpFuzzTest, VideoEngineDealsWithALargeNumberOfTamperedPackets) {
  // Try 0.5% bit flip chance per byte.
  BitFlipFuzzTest(0.005f);
}

TEST_F(ViERtpFuzzTest, VideoEngineDealsWithAVeryLargeNumberOfTamperedPackets) {
  // Try 5% bit flip chance per byte.
  BitFlipFuzzTest(0.05f);
}

TEST_F(ViERtpFuzzTest,
       VideoEngineDealsWithAExtremelyLargeNumberOfTamperedPackets) {
  // Try 25% bit flip chance per byte (madness!)
  BitFlipFuzzTest(0.25f);
}

TEST_F(ViERtpFuzzTest, VideoEngineDealsWithSeveralPeriodsOfTamperedPackets) {
  // Try 0.05% bit flip chance per byte.
  BitFlipFuzzTest(0.0005f);
  BitFlipFuzzTest(0.0005f);
  BitFlipFuzzTest(0.0005f);
}

TEST_F(ViERtpFuzzTest, VideoEngineRecoversAfterSomeCompletelyRandomPackets) {
  unsigned int rand_seed = FetchRandSeed();
  ViETest::Log("Running test with rand seed %d.", rand_seed);

  ViETest::Log("Running as usual. You should see video output.");
  AutoTestSleep(2000);

  ViETest::Log("Injecting completely random packets...");
  RandomEncryption random_encryption(rand_seed);
  video_engine_->encryption->RegisterExternalEncryption(
      video_channel_->videoChannel, random_encryption);

  AutoTestSleep(5000);

  ViETest::Log("Back to normal.");
  video_engine_->encryption->DeregisterExternalEncryption(
      video_channel_->videoChannel);

  AutoTestSleep(5000);
}

}
