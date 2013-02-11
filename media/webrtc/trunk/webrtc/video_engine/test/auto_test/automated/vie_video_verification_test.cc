/*
 *  Copyright (c) 2012 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include <vector>

#include "gtest/gtest.h"
#include "testsupport/fileutils.h"
#include "testsupport/metrics/video_metrics.h"
#include "webrtc/video_engine/test/auto_test/interface/vie_autotest.h"
#include "webrtc/video_engine/test/auto_test/interface/vie_file_based_comparison_tests.h"
#include "webrtc/video_engine/test/auto_test/primitives/framedrop_primitives.h"
#include "webrtc/video_engine/test/libvietest/include/tb_external_transport.h"
#include "webrtc/video_engine/test/libvietest/include/vie_to_file_renderer.h"

namespace {

// The input file must be QCIF since I420 gets scaled to that in the tests
// (it is so bandwidth-heavy we have no choice). Our comparison algorithms
// wouldn't like scaling, so this will work when we compare with the original.
const int kInputWidth = 176;
const int kInputHeight = 144;

class ViEVideoVerificationTest : public testing::Test {
 protected:
  void SetUp() {
    input_file_ = webrtc::test::ResourcePath("paris_qcif", "yuv");
    local_file_renderer_ = NULL;
    remote_file_renderer_ = NULL;
  }

  void InitializeFileRenderers() {
    local_file_renderer_ = new ViEToFileRenderer();
    remote_file_renderer_ = new ViEToFileRenderer();
    SetUpLocalFileRenderer(local_file_renderer_);
    SetUpRemoteFileRenderer(remote_file_renderer_);
  }

  void SetUpLocalFileRenderer(ViEToFileRenderer* file_renderer) {
    SetUpFileRenderer(file_renderer, "-local-preview.yuv");
  }

  void SetUpRemoteFileRenderer(ViEToFileRenderer* file_renderer) {
    SetUpFileRenderer(file_renderer, "-remote.yuv");
  }

  // Must be called manually inside the tests.
  void StopRenderers() {
    local_file_renderer_->StopRendering();
    remote_file_renderer_->StopRendering();
  }

  void CompareFiles(const std::string& reference_file,
                    const std::string& test_file,
                    double* psnr_result, double *ssim_result) {
    webrtc::test::QualityMetricsResult psnr;
    int error = I420PSNRFromFiles(reference_file.c_str(), test_file.c_str(),
                                  kInputWidth, kInputHeight, &psnr);

    EXPECT_EQ(0, error) << "PSNR routine failed - output files missing?";
    *psnr_result = psnr.average;

    webrtc::test::QualityMetricsResult ssim;
    error = I420SSIMFromFiles(reference_file.c_str(), test_file.c_str(),
                              kInputWidth, kInputHeight, &ssim);
    EXPECT_EQ(0, error) << "SSIM routine failed - output files missing?";
    *ssim_result = ssim.average;

    ViETest::Log("Results: PSNR is %f (dB; 48 is max), "
                 "SSIM is %f (1 is perfect)",
                 psnr.average, ssim.average);
  }

  // Note: must call AFTER CompareFiles.
  void TearDownFileRenderers() {
    TearDownFileRenderer(local_file_renderer_);
    TearDownFileRenderer(remote_file_renderer_);
  }

  std::string input_file_;
  ViEToFileRenderer* local_file_renderer_;
  ViEToFileRenderer* remote_file_renderer_;
  ViEFileBasedComparisonTests tests_;

 private:
  void SetUpFileRenderer(ViEToFileRenderer* file_renderer,
                         const std::string& suffix) {
    std::string output_path = ViETest::GetResultOutputPath();
    std::string filename = "render_output" + suffix;

    if (!file_renderer->PrepareForRendering(output_path, filename)) {
      FAIL() << "Could not open output file " << filename <<
          " for writing.";
    }
  }

  void TearDownFileRenderer(ViEToFileRenderer* file_renderer) {
      assert(file_renderer);
      bool test_failed = ::testing::UnitTest::GetInstance()->
          current_test_info()->result()->Failed();
      if (test_failed) {
        // Leave the files for analysis if the test failed.
        file_renderer->SaveOutputFile("failed-");
      }
      delete file_renderer;
    }
};

class ParameterizedFullStackTest : public ViEVideoVerificationTest,
                                   public ::testing::WithParamInterface<int> {
 public:
  static const int kNumFullStackInstances = 4;

 protected:
  struct TestParameters {
    NetworkParameters network;
    std::string file_name;
    int width;
    int height;
    int bitrate;
    double avg_psnr_threshold;
    double avg_ssim_threshold;
    ProtectionMethod protection_method;
    std::string test_label;
  };

  void SetUp() {
    for (int i = 0; i < kNumFullStackInstances; ++i) {
      parameter_table_[i].file_name = webrtc::test::ResourcePath("foreman_cif",
                                                                 "yuv");
      parameter_table_[i].width = 352;
      parameter_table_[i].height = 288;
    }
    int i = 0;
    parameter_table_[i].protection_method = kNack;
    // Uniform loss => Setting burst length to -1.
    parameter_table_[i].network.loss_model = kUniformLoss;
    parameter_table_[i].network.packet_loss_rate = 0;
    parameter_table_[i].network.burst_length = -1;
    parameter_table_[i].network.mean_one_way_delay = 0;
    parameter_table_[i].network.std_dev_one_way_delay = 0;
    parameter_table_[i].bitrate = 300;
    parameter_table_[i].avg_psnr_threshold = 35;
    parameter_table_[i].avg_ssim_threshold = 0.96;
    parameter_table_[i].test_label = "net_delay_0_0_plr_0";
    ++i;
    parameter_table_[i].protection_method = kNack;
    parameter_table_[i].network.loss_model = kUniformLoss;
    parameter_table_[i].network.packet_loss_rate = 5;
    parameter_table_[i].network.burst_length = -1;
    parameter_table_[i].network.mean_one_way_delay = 50;
    parameter_table_[i].network.std_dev_one_way_delay = 5;
    parameter_table_[i].bitrate = 300;
    parameter_table_[i].avg_psnr_threshold = 35;
    parameter_table_[i].avg_ssim_threshold = 0.96;
    parameter_table_[i].test_label = "net_delay_50_5_plr_5";
    ++i;
    parameter_table_[i].protection_method = kNack;
    parameter_table_[i].network.loss_model = kUniformLoss;
    parameter_table_[i].network.packet_loss_rate = 0;
    parameter_table_[i].network.burst_length = -1;
    parameter_table_[i].network.mean_one_way_delay = 100;
    parameter_table_[i].network.std_dev_one_way_delay = 10;
    parameter_table_[i].bitrate = 300;
    parameter_table_[i].avg_psnr_threshold = 35;
    parameter_table_[i].avg_ssim_threshold = 0.96;
    parameter_table_[i].test_label = "net_delay_100_10_plr_0";
    ++i;
    parameter_table_[i].protection_method = kNack;
    parameter_table_[i].network.loss_model = kGilbertElliotLoss;
    parameter_table_[i].network.packet_loss_rate = 5;
    parameter_table_[i].network.burst_length = 3;
    parameter_table_[i].network.mean_one_way_delay = 100;
    parameter_table_[i].network.std_dev_one_way_delay = 10;
    parameter_table_[i].bitrate = 300;
    parameter_table_[i].avg_psnr_threshold = 35;
    parameter_table_[i].avg_ssim_threshold = 0.96;
    parameter_table_[i].test_label = "net_delay_100_10_plr_5_gilbert_elliot";

    ASSERT_EQ(kNumFullStackInstances - 1, i);
  }

  TestParameters parameter_table_[kNumFullStackInstances];
};

TEST_F(ViEVideoVerificationTest, RunsBaseStandardTestWithoutErrors) {
  // I420 is lossless, so the I420 test should obviously get perfect results -
  // the local preview and remote output files should be bit-exact. This test
  // runs on external transport to ensure we do not drop packets.
  // However, it's hard to make 100% stringent requirements on the video engine
  // since for instance the jitter buffer has non-deterministic elements. If it
  // breaks five times in a row though, you probably introduced a bug.
  const double kReasonablePsnr = webrtc::test::kMetricsPerfectPSNR - 2.0f;
  const double kReasonableSsim = 0.99f;
  const int kNumAttempts = 5;
  for (int attempt = 0; attempt < kNumAttempts; ++attempt) {
    InitializeFileRenderers();
    ASSERT_TRUE(tests_.TestCallSetup(input_file_, kInputWidth, kInputHeight,
                                     local_file_renderer_,
                                     remote_file_renderer_));
    std::string remote_file = remote_file_renderer_->GetFullOutputPath();
    std::string local_preview = local_file_renderer_->GetFullOutputPath();
    StopRenderers();

    double actual_psnr = 0;
    double actual_ssim = 0;
    CompareFiles(local_preview, remote_file, &actual_psnr, &actual_ssim);

    TearDownFileRenderers();

    if (actual_psnr > kReasonablePsnr && actual_ssim > kReasonableSsim) {
      // Test successful.
      return;
    } else {
      ViETest::Log("Retrying; attempt %d of %d.", attempt + 1, kNumAttempts);
    }
  }

  FAIL() << "Failed to achieve near-perfect PSNR and SSIM results after " <<
      kNumAttempts << " attempts.";
}

// Runs a whole stack processing with tracking of which frames are dropped
// in the encoder. Tests show that they start at the same frame, which is
// the important thing when doing frame-to-frame comparison with PSNR/SSIM.
TEST_P(ParameterizedFullStackTest, RunsFullStackWithoutErrors)  {
  // Using CIF here since it's a more common resolution than QCIF, and higher
  // resolutions shouldn't be a problem for a test using VP8.
  input_file_ = parameter_table_[GetParam()].file_name;
  FrameDropDetector detector;
  local_file_renderer_ = new ViEToFileRenderer();
  remote_file_renderer_ = new FrameDropMonitoringRemoteFileRenderer(&detector);
  SetUpLocalFileRenderer(local_file_renderer_);
  SetUpRemoteFileRenderer(remote_file_renderer_);

  // Set a low bit rate so the encoder budget will be tight, causing it to drop
  // frames every now and then.
  const int kBitRateKbps = parameter_table_[GetParam()].bitrate;
  const NetworkParameters network = parameter_table_[GetParam()].network;
  int width = parameter_table_[GetParam()].width;
  int height = parameter_table_[GetParam()].height;
  ProtectionMethod protection_method =
      parameter_table_[GetParam()].protection_method;
  ViETest::Log("Bit rate     : %5d kbps", kBitRateKbps);
  ViETest::Log("Packet loss  : %5d %%", network.packet_loss_rate);
  ViETest::Log("Network delay: mean=%dms std dev=%d ms",
               network.mean_one_way_delay, network.std_dev_one_way_delay);
  tests_.TestFullStack(input_file_, width, height, kBitRateKbps,
                       protection_method, network, local_file_renderer_,
                       remote_file_renderer_, &detector);
  const std::string reference_file = local_file_renderer_->GetFullOutputPath();
  const std::string output_file = remote_file_renderer_->GetFullOutputPath();
  StopRenderers();

  detector.CalculateResults();
  detector.PrintReport(parameter_table_[GetParam()].test_label);

  if (detector.GetNumberOfFramesDroppedAt(FrameDropDetector::kRendered) >
      detector.GetNumberOfFramesDroppedAt(FrameDropDetector::kDecoded)) {
    detector.PrintDebugDump();
  }

  ASSERT_GE(detector.GetNumberOfFramesDroppedAt(FrameDropDetector::kRendered),
      detector.GetNumberOfFramesDroppedAt(FrameDropDetector::kDecoded))
      << "The number of dropped frames on the decode and render steps are not "
      "equal. This may be because we have a major problem in the buffers of "
      "the ViEToFileRenderer?";

  // We may have dropped frames during the processing, which means the output
  // file does not contain all the frames that are present in the input file.
  // To make the quality measurement correct, we must adjust the output file to
  // that by copying the last successful frame into the place where the dropped
  // frame would be, for all dropped frames.
  const int frame_length_in_bytes = 3 * width * height / 2;
  ViETest::Log("Frame length: %d bytes", frame_length_in_bytes);
  std::vector<Frame*> all_frames = detector.GetAllFrames();
  FixOutputFileForComparison(output_file, frame_length_in_bytes, all_frames);

  // Verify all sent frames are present in the output file.
  size_t output_file_size = webrtc::test::GetFileSize(output_file);
  EXPECT_EQ(all_frames.size(), output_file_size / frame_length_in_bytes)
      << "The output file size is incorrect. It should be equal to the number "
      "of frames multiplied by the frame size. This will likely affect "
      "PSNR/SSIM calculations in a bad way.";

  TearDownFileRenderers();

  // We are running on a lower bitrate here so we need to settle for somewhat
  // lower PSNR and SSIM values.
  double actual_psnr = 0;
  double actual_ssim = 0;
  CompareFiles(reference_file, output_file, &actual_psnr, &actual_ssim);

  const double kExpectedMinimumPSNR =
      parameter_table_[GetParam()].avg_psnr_threshold;
  const double kExpectedMinimumSSIM =
      parameter_table_[GetParam()].avg_ssim_threshold;

  EXPECT_GE(actual_psnr, kExpectedMinimumPSNR);
  EXPECT_GE(actual_ssim, kExpectedMinimumSSIM);
}

INSTANTIATE_TEST_CASE_P(FullStackTests, ParameterizedFullStackTest,
    ::testing::Range(0, ParameterizedFullStackTest::kNumFullStackInstances));

}  // namespace
