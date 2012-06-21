/*
 *  Copyright (c) 2011 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include <math.h>
#include <string.h>

#include "common_video/libyuv/include/scaler.h"
#include "gtest/gtest.h"
#include "system_wrappers/interface/tick_util.h"
#include "testsupport/fileutils.h"

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

  Scaler test_scaler_;
  FILE* source_file_;
  uint8_t* test_buffer_;
  const int width_;
  const int height_;
  const int frame_length_;
};


// TODO (mikhal): Use scoped_ptr when handling buffers.
TestScaler::TestScaler()
    : source_file_(NULL),
      width_(352),
      height_(288),
      frame_length_(CalcBufferSize(kI420, 352, 288)) {
}

void TestScaler::SetUp() {
  const std::string input_file_name =
      webrtc::test::ResourcePath("foreman_cif", "yuv");
  source_file_  = fopen(input_file_name.c_str(), "rb");
  ASSERT_TRUE(source_file_ != NULL) << "Cannot read file: "<<
                                       input_file_name << "\n";
  test_buffer_ = new uint8_t[frame_length_];
}

void TestScaler::TearDown() {
  if (source_file_ != NULL) {
    ASSERT_EQ(0, fclose(source_file_));
  }
  source_file_ = NULL;
  delete [] test_buffer_;
}

TEST_F(TestScaler, ScaleWithoutSettingValues) {
  int size = 100;
  EXPECT_EQ(-2, test_scaler_.Scale(test_buffer_, test_buffer_, size));
}

TEST_F(TestScaler, ScaleBadInitialValues) {
  EXPECT_EQ(-1, test_scaler_.Set(0, 288, 352, 288, kI420, kI420, kScalePoint));
  EXPECT_EQ(-1, test_scaler_.Set(704, 0, 352, 288, kI420, kI420, kScaleBox));
  EXPECT_EQ(-1, test_scaler_.Set(704, 576, 352, 0, kI420, kI420,
                                 kScaleBilinear));
  EXPECT_EQ(-1, test_scaler_.Set(704, 576, 0, 288, kI420, kI420, kScalePoint));
}

TEST_F(TestScaler, ScaleSendingNullSourcePointer) {
  int size = 0;
  EXPECT_EQ(-1, test_scaler_.Scale(NULL, test_buffer_, size));
}

TEST_F(TestScaler, ScaleSendingBufferTooSmall) {
  // Sending a buffer which is too small (should reallocate and update size)
  EXPECT_EQ(0, test_scaler_.Set(352, 288, 144, 288, kI420, kI420, kScalePoint));
  uint8_t* test_buffer2 = NULL;
  int size = 0;
  EXPECT_GT(fread(test_buffer_, 1, frame_length_, source_file_), 0U);
  EXPECT_EQ(0, test_scaler_.Scale(test_buffer_, test_buffer2, size));
  EXPECT_EQ(144 * 288 * 3 / 2, size);
  delete [] test_buffer2;
}

//TODO (mikhal): Converge the test into one function that accepts the method.
TEST_F(TestScaler, PointScaleTest) {
  ScaleMethod method = kScalePoint;
  std::string out_name = webrtc::test::OutputPath() +
                         "LibYuvTest_PointScale_176_144.yuv";
  ScaleSequence(method,
                source_file_, out_name,
                width_, height_,
                width_ / 2, height_ / 2);
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
}

TEST_F(TestScaler, BiLinearScaleTest) {
  ScaleMethod method = kScaleBilinear;
  std::string out_name = webrtc::test::OutputPath() +
                         "LibYuvTest_BilinearScale_176_144.yuv";
  ScaleSequence(method,
                source_file_, out_name,
                width_, height_,
                width_ / 2, height_ / 2);
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

TEST_F(TestScaler, BoxScaleTest) {
  ScaleMethod method = kScaleBox;
  std::string out_name = webrtc::test::OutputPath() +
                         "LibYuvTest_BoxScale_176_144.yuv";
  ScaleSequence(method,
                source_file_, out_name,
                width_, height_,
                width_ / 2, height_ / 2);
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

// TODO (mikhal): Move part to a separate scale test.
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

  int out_required_size = dst_width * dst_height * 3 / 2;
  int in_required_size = src_height * src_width * 3 / 2;
  uint8_t* input_buffer = new uint8_t[in_required_size];
  uint8_t* output_buffer = new uint8_t[out_required_size];

  int64_t start_clock, total_clock;
  total_clock = 0;
  int frame_count = 0;

  // Running through entire sequence
  while (feof(source_file) == 0) {
      if ((size_t)in_required_size !=
          fread(input_buffer, 1, in_required_size, source_file))
        break;

    start_clock = TickTime::MillisecondTimestamp();
    EXPECT_EQ(0, test_scaler_.Scale(input_buffer, output_buffer,
                                   out_required_size));
    total_clock += TickTime::MillisecondTimestamp() - start_clock;
    fwrite(output_buffer, out_required_size, 1, output_file);
    frame_count++;
  }

  if (frame_count) {
    printf("Scaling[%d %d] => [%d %d]: ",
           src_width, src_height, dst_width, dst_height);
    printf("Average time per frame[ms]: %.2lf\n",
             (static_cast<double>(total_clock) / frame_count));
  }
  ASSERT_EQ(0, fclose(output_file));
  delete [] input_buffer;
  delete [] output_buffer;
}

}  // namespace webrtc
