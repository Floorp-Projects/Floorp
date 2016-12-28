/*
 *  Copyright (c) 2012 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include <math.h>
#include <string.h>

#include "testing/gtest/include/gtest/gtest.h"
#include "webrtc/common_video/libyuv/include/scaler.h"
#include "webrtc/system_wrappers/include/tick_util.h"
#include "webrtc/test/testsupport/fileutils.h"

namespace webrtc {

class TestScaler : public ::testing::Test {
 protected:
  TestScaler();
  virtual void SetUp();
  virtual void TearDown();

  void ScaleSequence(ScaleMethod method,
                     FILE* source_file, std::string out_name,
                     int src_width, int src_height,
                     int dst_width, int dst_height);
  // Computes the sequence average PSNR between an input sequence in
  // |input_file| and an output sequence with filename |out_name|. |width| and
  // |height| are the frame sizes of both sequences.
  double ComputeAvgSequencePSNR(FILE* input_file, std::string out_name,
                                int width, int height);

  Scaler test_scaler_;
  FILE* source_file_;
  VideoFrame test_frame_;
  const int width_;
  const int half_width_;
  const int height_;
  const int half_height_;
  const int size_y_;
  const int size_uv_;
  const size_t frame_length_;
};

TestScaler::TestScaler()
    : source_file_(NULL),
      width_(352),
      half_width_(width_ / 2),
      height_(288),
      half_height_(height_ / 2),
      size_y_(width_ * height_),
      size_uv_(half_width_ * half_height_),
      frame_length_(CalcBufferSize(kI420, width_, height_)) {
}

void TestScaler::SetUp() {
  const std::string input_file_name =
      webrtc::test::ResourcePath("foreman_cif", "yuv");
  source_file_  = fopen(input_file_name.c_str(), "rb");
  ASSERT_TRUE(source_file_ != NULL) << "Cannot read file: "<<
                                       input_file_name << "\n";
  test_frame_.CreateEmptyFrame(width_, height_,
                               width_, half_width_, half_width_);
}

void TestScaler::TearDown() {
  if (source_file_ != NULL) {
    ASSERT_EQ(0, fclose(source_file_));
  }
  source_file_ = NULL;
}

TEST_F(TestScaler, ScaleWithoutSettingValues) {
  EXPECT_EQ(-2, test_scaler_.Scale(test_frame_, &test_frame_));
}

TEST_F(TestScaler, ScaleBadInitialValues) {
  EXPECT_EQ(-1, test_scaler_.Set(0, 288, 352, 288, kI420, kI420, kScalePoint));
  EXPECT_EQ(-1, test_scaler_.Set(704, 0, 352, 288, kI420, kI420, kScaleBox));
  EXPECT_EQ(-1, test_scaler_.Set(704, 576, 352, 0, kI420, kI420,
                                 kScaleBilinear));
  EXPECT_EQ(-1, test_scaler_.Set(704, 576, 0, 288, kI420, kI420, kScalePoint));
}

TEST_F(TestScaler, ScaleSendingNullSourcePointer) {
  VideoFrame null_src_frame;
  EXPECT_EQ(-1, test_scaler_.Scale(null_src_frame, &test_frame_));
}

TEST_F(TestScaler, ScaleSendingBufferTooSmall) {
  // Sending a buffer which is too small (should reallocate and update size)
  EXPECT_EQ(0, test_scaler_.Set(width_, height_,
                                half_width_, half_height_,
                                kI420, kI420,
                                kScalePoint));
  VideoFrame test_frame2;
  rtc::scoped_ptr<uint8_t[]> orig_buffer(new uint8_t[frame_length_]);
  EXPECT_GT(fread(orig_buffer.get(), 1, frame_length_, source_file_), 0U);
  test_frame_.CreateFrame(orig_buffer.get(),
                          orig_buffer.get() + size_y_,
                          orig_buffer.get() + size_y_ + size_uv_,
                          width_, height_,
                          width_, half_width_, half_width_);
  EXPECT_EQ(0, test_scaler_.Scale(test_frame_, &test_frame2));
  EXPECT_GT(width_ * height_, test_frame2.allocated_size(kYPlane));
  EXPECT_GT(size_uv_, test_frame2.allocated_size(kUPlane));
  EXPECT_GT(size_uv_, test_frame2.allocated_size(kVPlane));
  EXPECT_EQ(half_width_, test_frame2.width());
  EXPECT_EQ(half_height_, test_frame2.height());
}

// TODO(mikhal): Converge the test into one function that accepts the method.
#if defined(WEBRTC_ANDROID)
#define MAYBE_PointScaleTest DISABLED_PointScaleTest
#else
#define MAYBE_PointScaleTest PointScaleTest
#endif
TEST_F(TestScaler, MAYBE_PointScaleTest) {
  double avg_psnr;
  FILE* source_file2;
  ScaleMethod method = kScalePoint;
  std::string out_name = webrtc::test::OutputPath() +
                         "LibYuvTest_PointScale_176_144.yuv";
  ScaleSequence(method,
                source_file_, out_name,
                width_, height_,
                half_width_, half_height_);
  // Upsample back up and check PSNR.
  source_file2 = fopen(out_name.c_str(), "rb");
  out_name = webrtc::test::OutputPath() + "LibYuvTest_PointScale_352_288_"
      "upfrom_176_144.yuv";
  ScaleSequence(method,
                source_file2, out_name,
                176, 144,
                352, 288);
  avg_psnr = ComputeAvgSequencePSNR(source_file_, out_name, width_, height_);
  printf("PSNR for scaling from: %d %d, down/up to: %d %d, and back to "
      "original size: %f \n", width_, height_, 176, 144, avg_psnr);
  // Average PSNR for lower bound in assert is ~0.1dB lower than the actual
  // average PSNR under same conditions.
  ASSERT_GT(avg_psnr, 27.9);
  ASSERT_EQ(0, fclose(source_file2));
  out_name = webrtc::test::OutputPath() + "LibYuvTest_PointScale_320_240.yuv";
  ScaleSequence(method,
                source_file_, out_name,
                width_, height_,
                320, 240);
  out_name = webrtc::test::OutputPath() + "LibYuvTest_PointScale_704_576.yuv";
  ScaleSequence(method,
                source_file_, out_name,
                width_, height_,
                width_ * 2, height_ * 2);
  out_name = webrtc::test::OutputPath() + "LibYuvTest_PointScale_300_200.yuv";
  ScaleSequence(method,
                source_file_, out_name,
                width_, height_,
                300, 200);
  out_name = webrtc::test::OutputPath() + "LibYuvTest_PointScale_400_300.yuv";
  ScaleSequence(method,
                source_file_, out_name,
                width_, height_,
                400, 300);
  // Down-sample to odd size frame and scale back up.
  out_name = webrtc::test::OutputPath() + "LibYuvTest_PointScale_282_231.yuv";
  ScaleSequence(method,
                source_file_, out_name,
                width_, height_,
                282, 231);
  source_file2 = fopen(out_name.c_str(), "rb");
  out_name = webrtc::test::OutputPath() + "LibYuvTest_PointScale_352_288_"
      "upfrom_282_231.yuv";
  ScaleSequence(method,
                source_file2, out_name,
                282, 231,
                352, 288);
  avg_psnr = ComputeAvgSequencePSNR(source_file_, out_name, width_, height_);
  printf("PSNR for scaling from: %d %d, down/up to: %d %d, and back to "
      "original size: %f \n", width_, height_, 282, 231, avg_psnr);
  // Average PSNR for lower bound in assert is ~0.1dB lower than the actual
  // average PSNR under same conditions.
  ASSERT_GT(avg_psnr, 25.8);
  ASSERT_EQ(0, fclose(source_file2));
}

#if defined(WEBRTC_ANDROID)
#define MAYBE_BilinearScaleTest DISABLED_BiLinearScaleTest
#else
#define MAYBE_BilinearScaleTest BiLinearScaleTest
#endif
TEST_F(TestScaler, MAYBE_BiLinearScaleTest) {
  double avg_psnr;
  FILE* source_file2;
  ScaleMethod method = kScaleBilinear;
  std::string out_name = webrtc::test::OutputPath() +
                         "LibYuvTest_BilinearScale_176_144.yuv";
  ScaleSequence(method,
                source_file_, out_name,
                width_, height_,
                width_ / 2, height_ / 2);
  // Up-sample back up and check PSNR.
  source_file2 = fopen(out_name.c_str(), "rb");
  out_name = webrtc::test::OutputPath() + "LibYuvTest_BilinearScale_352_288_"
      "upfrom_176_144.yuv";
  ScaleSequence(method,
                source_file2, out_name,
                176, 144,
                352, 288);
  avg_psnr = ComputeAvgSequencePSNR(source_file_, out_name, width_, height_);
  printf("PSNR for scaling from: %d %d, down/up to: %d %d, and back to "
      "original size: %f \n", width_, height_, 176, 144, avg_psnr);
  // Average PSNR for lower bound in assert is ~0.1dB lower than the actual
  // average PSNR under same conditions.
  ASSERT_GT(avg_psnr, 27.5);
  ComputeAvgSequencePSNR(source_file_, out_name, width_, height_);
  ASSERT_EQ(0, fclose(source_file2));
  out_name = webrtc::test::OutputPath() +
             "LibYuvTest_BilinearScale_320_240.yuv";
  ScaleSequence(method,
                source_file_, out_name,
                width_, height_,
                320, 240);
  out_name = webrtc::test::OutputPath() +
             "LibYuvTest_BilinearScale_704_576.yuv";
  ScaleSequence(method,
                source_file_, out_name,
                width_, height_,
                width_ * 2, height_ * 2);
  out_name = webrtc::test::OutputPath() +
             "LibYuvTest_BilinearScale_300_200.yuv";
  ScaleSequence(method,
                source_file_, out_name,
                width_, height_,
                300, 200);
  out_name = webrtc::test::OutputPath() +
             "LibYuvTest_BilinearScale_400_300.yuv";
  ScaleSequence(method,
                source_file_, out_name,
                width_, height_,
                400, 300);
}

#if defined(WEBRTC_ANDROID)
#define MAYBE_BoxScaleTest DISABLED_BoxScaleTest
#else
#define MAYBE_BoxScaleTest BoxScaleTest
#endif
TEST_F(TestScaler, MAYBE_BoxScaleTest) {
  double avg_psnr;
  FILE* source_file2;
  ScaleMethod method = kScaleBox;
  std::string out_name = webrtc::test::OutputPath() +
                         "LibYuvTest_BoxScale_176_144.yuv";
  ScaleSequence(method,
                source_file_, out_name,
                width_, height_,
                width_ / 2, height_ / 2);
  // Up-sample back up and check PSNR.
  source_file2 = fopen(out_name.c_str(), "rb");
  out_name = webrtc::test::OutputPath() + "LibYuvTest_BoxScale_352_288_"
      "upfrom_176_144.yuv";
  ScaleSequence(method,
                source_file2, out_name,
                176, 144,
                352, 288);
  avg_psnr = ComputeAvgSequencePSNR(source_file_, out_name, width_, height_);
  printf("PSNR for scaling from: %d %d, down/up to: %d %d, and back to "
      "original size: %f \n", width_, height_, 176, 144, avg_psnr);
  // Average PSNR for lower bound in assert is ~0.1dB lower than the actual
  // average PSNR under same conditions.
  ASSERT_GT(avg_psnr, 27.5);
  ASSERT_EQ(0, fclose(source_file2));
  out_name = webrtc::test::OutputPath() + "LibYuvTest_BoxScale_320_240.yuv";
  ScaleSequence(method,
                source_file_, out_name,
                width_, height_,
                320, 240);
  out_name = webrtc::test::OutputPath() + "LibYuvTest_BoxScale_704_576.yuv";
  ScaleSequence(method,
                source_file_, out_name,
                width_, height_,
                width_ * 2, height_ * 2);
  out_name = webrtc::test::OutputPath() + "LibYuvTest_BoxScale_300_200.yuv";
  ScaleSequence(method,
                source_file_, out_name,
                width_, height_,
                300, 200);
  out_name = webrtc::test::OutputPath() + "LibYuvTest_BoxScale_400_300.yuv";
  ScaleSequence(method,
                source_file_, out_name,
                width_, height_,
                400, 300);
}

double TestScaler::ComputeAvgSequencePSNR(FILE* input_file,
                                          std::string out_name,
                                          int width, int height) {
  FILE* output_file;
  output_file = fopen(out_name.c_str(), "rb");
  assert(output_file != NULL);
  rewind(input_file);
  rewind(output_file);

  size_t required_size = CalcBufferSize(kI420, width, height);
  uint8_t* input_buffer = new uint8_t[required_size];
  uint8_t* output_buffer = new uint8_t[required_size];

  int frame_count = 0;
  double avg_psnr = 0;
  VideoFrame in_frame, out_frame;
  const int half_width = (width + 1) / 2;
  in_frame.CreateEmptyFrame(width, height, width, half_width, half_width);
  out_frame.CreateEmptyFrame(width, height, width, half_width, half_width);
  while (feof(input_file) == 0) {
    if (fread(input_buffer, 1, required_size, input_file) != required_size) {
      break;
    }
    if (fread(output_buffer, 1, required_size, output_file) != required_size) {
      break;
    }
    frame_count++;
    EXPECT_EQ(0, ConvertToI420(kI420, input_buffer, 0, 0, width, height,
                               required_size, kVideoRotation_0, &in_frame));
    EXPECT_EQ(0, ConvertToI420(kI420, output_buffer, 0, 0, width, height,
                               required_size, kVideoRotation_0, &out_frame));
    double psnr = I420PSNR(&in_frame, &out_frame);
    avg_psnr += psnr;
  }
  avg_psnr = avg_psnr / frame_count;
  assert(0 == fclose(output_file));
  delete [] input_buffer;
  delete [] output_buffer;
  return avg_psnr;
}

// TODO(mikhal): Move part to a separate scale test.
void TestScaler::ScaleSequence(ScaleMethod method,
                   FILE* source_file, std::string out_name,
                   int src_width, int src_height,
                   int dst_width, int dst_height) {
  FILE* output_file;
  EXPECT_EQ(0, test_scaler_.Set(src_width, src_height,
                               dst_width, dst_height,
                               kI420, kI420, method));

  output_file = fopen(out_name.c_str(), "wb");
  ASSERT_TRUE(output_file != NULL);

  rewind(source_file);

  VideoFrame input_frame;
  VideoFrame output_frame;
  int64_t start_clock, total_clock;
  total_clock = 0;
  int frame_count = 0;
  size_t src_required_size = CalcBufferSize(kI420, src_width, src_height);
  rtc::scoped_ptr<uint8_t[]> frame_buffer(new uint8_t[src_required_size]);
  int size_y = src_width * src_height;
  int size_uv = ((src_width + 1) / 2) * ((src_height + 1) / 2);

  // Running through entire sequence.
  while (feof(source_file) == 0) {
    if (fread(frame_buffer.get(), 1, src_required_size, source_file) !=
        src_required_size)
      break;

    input_frame.CreateFrame(frame_buffer.get(),
                            frame_buffer.get() + size_y,
                            frame_buffer.get() + size_y + size_uv,
                            src_width, src_height,
                            src_width, (src_width + 1) / 2,
                            (src_width + 1) / 2);

    start_clock = TickTime::MillisecondTimestamp();
    EXPECT_EQ(0, test_scaler_.Scale(input_frame, &output_frame));
    total_clock += TickTime::MillisecondTimestamp() - start_clock;
    if (PrintVideoFrame(output_frame, output_file) < 0) {
        return;
    }
    frame_count++;
  }

  if (frame_count) {
    printf("Scaling[%d %d] => [%d %d]: ",
           src_width, src_height, dst_width, dst_height);
    printf("Average time per frame[ms]: %.2lf\n",
             (static_cast<double>(total_clock) / frame_count));
  }
  ASSERT_EQ(0, fclose(output_file));
}

}  // namespace webrtc
