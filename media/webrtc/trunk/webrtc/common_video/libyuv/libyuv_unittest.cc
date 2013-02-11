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

#include "common_video/interface/i420_video_frame.h"
#include "common_video/libyuv/include/webrtc_libyuv.h"
#include "gtest/gtest.h"
#include "system_wrappers/interface/tick_util.h"
#include "system_wrappers/interface/scoped_ptr.h"
#include "testsupport/fileutils.h"

namespace webrtc {

int PrintBuffer(const uint8_t* buffer, int width, int height, int stride) {
  if (buffer == NULL)
    return -1;
  int k;
  const uint8_t* tmp_buffer = buffer;
  for (int i = 0; i < height; i++) {
    k = 0;
    for (int j = 0; j < width; j++) {
      printf("%d ", tmp_buffer[k++]);
    }
    tmp_buffer += stride;
    printf(" \n");
  }
  printf(" \n");
  return 0;
}


int PrintFrame(const I420VideoFrame* frame, const char* str) {
  if (frame == NULL)
     return -1;
  printf("%s %dx%d \n", str, frame->width(), frame->height());

  int ret = 0;
  for (int plane_num = 0; plane_num < kNumOfPlanes; ++plane_num) {
    PlaneType plane_type = static_cast<PlaneType>(plane_num);
    int width = (plane_num ? (frame->width() + 1) / 2 : frame->width());
    int height = (plane_num ? (frame->height() + 1) / 2 : frame->height());
    ret += PrintBuffer(frame->buffer(plane_type), width, height,
                       frame->stride(plane_type));
  }
  return ret;
}


// Create an image from on a YUV frame. Every plane value starts with a start
// value, and will be set to increasing values.
void CreateImage(I420VideoFrame* frame, int plane_offset[kNumOfPlanes]) {
  if (frame == NULL)
    return;
  for (int plane_num = 0; plane_num < kNumOfPlanes; ++plane_num) {
    int width = (plane_num != kYPlane ? (frame->width() + 1) / 2 :
      frame->width());
    int height = (plane_num != kYPlane ? (frame->height() + 1) / 2 :
      frame->height());
    PlaneType plane_type = static_cast<PlaneType>(plane_num);
    uint8_t *data = frame->buffer(plane_type);
    for (int i = 0; i < height; i++) {
      for (int j = 0; j < width; j++) {
        data[j] = static_cast<uint8_t>(i + plane_offset[plane_num] + j);
      }
      data += frame->stride(plane_type);
    }
  }
}

class TestLibYuv : public ::testing::Test {
 protected:
  TestLibYuv();
  virtual void SetUp();
  virtual void TearDown();

  FILE* source_file_;
  I420VideoFrame orig_frame_;
  scoped_array<uint8_t> orig_buffer_;
  const int width_;
  const int height_;
  const int size_y_;
  const int size_uv_;
  const int frame_length_;
};

TestLibYuv::TestLibYuv()
    : source_file_(NULL),
      orig_frame_(),
      width_(352),
      height_(288),
      size_y_(width_ * height_),
      size_uv_(((width_ + 1 ) / 2) * ((height_ + 1) / 2)),
      frame_length_(CalcBufferSize(kI420, 352, 288)) {
  orig_buffer_.reset(new uint8_t[frame_length_]);
}

void TestLibYuv::SetUp() {
  const std::string input_file_name = webrtc::test::ProjectRootPath() +
                                      "resources/foreman_cif.yuv";
  source_file_  = fopen(input_file_name.c_str(), "rb");
  ASSERT_TRUE(source_file_ != NULL) << "Cannot read file: "<<
                                       input_file_name << "\n";

  EXPECT_EQ(fread(orig_buffer_.get(), 1, frame_length_, source_file_),
            static_cast<unsigned int>(frame_length_));
  EXPECT_EQ(0, orig_frame_.CreateFrame(size_y_, orig_buffer_.get(),
                                       size_uv_, orig_buffer_.get() + size_y_,
                                       size_uv_, orig_buffer_.get() +
                                       size_y_ + size_uv_,
                                       width_, height_,
                                       width_, (width_ + 1) / 2,
                                       (width_ + 1) / 2));
}

void TestLibYuv::TearDown() {
  if (source_file_ != NULL) {
    ASSERT_EQ(0, fclose(source_file_));
  }
  source_file_ = NULL;
}

TEST_F(TestLibYuv, ConvertSanityTest) {
  // TODO(mikhal)
}

TEST_F(TestLibYuv, ConvertTest) {
  // Reading YUV frame - testing on the first frame of the foreman sequence
  int j = 0;
  std::string output_file_name = webrtc::test::OutputPath() +
                                 "LibYuvTest_conversion.yuv";
  FILE*  output_file = fopen(output_file_name.c_str(), "wb");
  ASSERT_TRUE(output_file != NULL);

  double psnr = 0.0;

  I420VideoFrame res_i420_frame;
  EXPECT_EQ(0,res_i420_frame.CreateEmptyFrame(width_, height_, width_,
                                              (width_ + 1) / 2,
                                              (width_ + 1) / 2));
  printf("\nConvert #%d I420 <-> I420 \n", j);
  scoped_array<uint8_t> out_i420_buffer(new uint8_t[frame_length_]);
  EXPECT_EQ(0, ConvertFromI420(orig_frame_, kI420, 0,
                               out_i420_buffer.get()));
  EXPECT_EQ(0, ConvertToI420(kI420, out_i420_buffer.get(), 0, 0,
                             width_, height_,
                             0, kRotateNone, &res_i420_frame));

  if (PrintI420VideoFrame(res_i420_frame, output_file) < 0) {
    return;
  }
  psnr = I420PSNR(&orig_frame_, &res_i420_frame);
  EXPECT_EQ(48.0, psnr);
  j++;

  printf("\nConvert #%d I420 <-> RGB24\n", j);
  scoped_array<uint8_t> res_rgb_buffer2(new uint8_t[width_ * height_ * 3]);
  // Align the stride values for the output frame.
  int stride_y = 0;
  int stride_uv = 0;
  Calc16ByteAlignedStride(width_, &stride_y, &stride_uv);
  res_i420_frame.CreateEmptyFrame(width_, height_, stride_y,
                                  stride_uv, stride_uv);
  EXPECT_EQ(0, ConvertFromI420(orig_frame_, kRGB24, 0, res_rgb_buffer2.get()));

  EXPECT_EQ(0, ConvertToI420(kRGB24, res_rgb_buffer2.get(), 0, 0, width_,
                             height_, 0, kRotateNone, &res_i420_frame));

  if (PrintI420VideoFrame(res_i420_frame, output_file) < 0) {
    return;
  }
  psnr = I420PSNR(&orig_frame_, &res_i420_frame);

  // Optimization Speed- quality trade-off => 45 dB only (platform dependant).
  EXPECT_GT(ceil(psnr), 44);
  j++;

  printf("\nConvert #%d I420 <-> UYVY\n", j);
  scoped_array<uint8_t> out_uyvy_buffer(new uint8_t[width_ * height_ * 2]);
  EXPECT_EQ(0, ConvertFromI420(orig_frame_,  kUYVY, 0, out_uyvy_buffer.get()));
  EXPECT_EQ(0, ConvertToI420(kUYVY, out_uyvy_buffer.get(), 0, 0, width_,
                             height_, 0, kRotateNone, &res_i420_frame));
  psnr = I420PSNR(&orig_frame_, &res_i420_frame);
  EXPECT_EQ(48.0, psnr);
  if (PrintI420VideoFrame(res_i420_frame, output_file) < 0) {
    return;
  }
  j++;

  printf("\nConvert #%d I420 <-> YV12\n", j);
  scoped_array<uint8_t> outYV120Buffer(new uint8_t[frame_length_]);
  scoped_array<uint8_t> res_i420_buffer(new uint8_t[frame_length_]);
  I420VideoFrame yv12_frame;
  EXPECT_EQ(0, ConvertFromI420(orig_frame_, kYV12, 0, outYV120Buffer.get()));
  yv12_frame.CreateFrame(size_y_, outYV120Buffer.get(),
                         size_uv_, outYV120Buffer.get() + size_y_,
                         size_uv_, outYV120Buffer.get() + size_y_ + size_uv_,
                         width_, height_,
                         width_, (width_ + 1) / 2, (width_ + 1) / 2);
  EXPECT_EQ(0, ConvertFromYV12(yv12_frame, kI420, 0, res_i420_buffer.get()));
  if (fwrite(res_i420_buffer.get(), 1, frame_length_,
             output_file) != static_cast<unsigned int>(frame_length_)) {
    return;
  }

  psnr = I420PSNR(orig_buffer_.get(), res_i420_buffer.get(), width_, height_);
  EXPECT_EQ(48.0, psnr);
  j++;

  printf("\nConvert #%d I420 <-> YUY2\n", j);
  scoped_array<uint8_t> out_yuy2_buffer(new uint8_t[width_ * height_ * 2]);
  EXPECT_EQ(0, ConvertFromI420(orig_frame_,  kYUY2, 0, out_yuy2_buffer.get()));

  EXPECT_EQ(0, ConvertToI420(kYUY2, out_yuy2_buffer.get(), 0, 0, width_,
                             height_, 0, kRotateNone, &res_i420_frame));

  if (PrintI420VideoFrame(res_i420_frame, output_file) < 0) {
    return;
  }

  psnr = I420PSNR(&orig_frame_, &res_i420_frame);
  EXPECT_EQ(48.0, psnr);
  printf("\nConvert #%d I420 <-> RGB565\n", j);
  scoped_array<uint8_t> out_rgb565_buffer(new uint8_t[width_ * height_ * 2]);
  EXPECT_EQ(0, ConvertFromI420(orig_frame_, kRGB565, 0,
                               out_rgb565_buffer.get()));

  EXPECT_EQ(0, ConvertToI420(kRGB565, out_rgb565_buffer.get(), 0, 0, width_,
                             height_, 0, kRotateNone, &res_i420_frame));

  if (PrintI420VideoFrame(res_i420_frame, output_file) < 0) {
    return;
  }
  j++;

  psnr = I420PSNR(&orig_frame_, &res_i420_frame);
  // TODO(leozwang) Investigate the right psnr should be set for I420ToRGB565,
  // Another example is I420ToRGB24, the psnr is 44
  // TODO(mikhal): Add psnr for RGB565, 1555, 4444, convert to ARGB.
  EXPECT_GT(ceil(psnr), 40);

  printf("\nConvert #%d I420 <-> ARGB8888\n", j);
  scoped_array<uint8_t> out_argb8888_buffer(new uint8_t[width_ * height_ * 4]);
  EXPECT_EQ(0, ConvertFromI420(orig_frame_, kARGB, 0,
                               out_argb8888_buffer.get()));

  EXPECT_EQ(0, ConvertToI420(kARGB, out_argb8888_buffer.get(), 0, 0, width_,
                             height_, 0, kRotateNone, &res_i420_frame));

  if (PrintI420VideoFrame(res_i420_frame, output_file) < 0) {
    return;
  }

  psnr = I420PSNR(&orig_frame_, &res_i420_frame);
  // TODO(leozwang) Investigate the right psnr should be set for I420ToARGB8888,
  EXPECT_GT(ceil(psnr), 42);

  ASSERT_EQ(0, fclose(output_file));
}

TEST_F(TestLibYuv, ConvertAlignedFrame) {
  // Reading YUV frame - testing on the first frame of the foreman sequence
  std::string output_file_name = webrtc::test::OutputPath() +
                                 "LibYuvTest_conversion.yuv";
  FILE*  output_file = fopen(output_file_name.c_str(), "wb");
  ASSERT_TRUE(output_file != NULL);

  double psnr = 0.0;

  I420VideoFrame res_i420_frame;
  int stride_y = 0;
  int stride_uv = 0;
  Calc16ByteAlignedStride(width_, &stride_y, &stride_uv);
  EXPECT_EQ(0,res_i420_frame.CreateEmptyFrame(width_, height_,
                                              stride_y, stride_uv, stride_uv));
  scoped_array<uint8_t> out_i420_buffer(new uint8_t[frame_length_]);
  EXPECT_EQ(0, ConvertFromI420(orig_frame_, kI420, 0,
                               out_i420_buffer.get()));
  EXPECT_EQ(0, ConvertToI420(kI420, out_i420_buffer.get(), 0, 0,
                             width_, height_,
                             0, kRotateNone, &res_i420_frame));

  if (PrintI420VideoFrame(res_i420_frame, output_file) < 0) {
    return;
  }
  psnr = I420PSNR(&orig_frame_, &res_i420_frame);
  EXPECT_EQ(48.0, psnr);
}


TEST_F(TestLibYuv, RotateTest) {
  // Use ConvertToI420 for multiple roatations - see that nothing breaks, all
  // memory is properly allocated and end result is equal to the starting point.
  I420VideoFrame rotated_res_i420_frame;
  int rotated_width = height_;
  int rotated_height = width_;
  int stride_y ;
  int stride_uv;
  Calc16ByteAlignedStride(rotated_width, &stride_y, &stride_uv);
  EXPECT_EQ(0,rotated_res_i420_frame.CreateEmptyFrame(rotated_width,
                                                      rotated_height,
                                                      stride_y,
                                                      stride_uv,
                                                      stride_uv));
  EXPECT_EQ(0, ConvertToI420(kI420, orig_buffer_.get(), 0, 0,
                             width_, height_,
                             0, kRotate90, &rotated_res_i420_frame));
  EXPECT_EQ(0, ConvertToI420(kI420, orig_buffer_.get(), 0, 0,
                             width_, height_,
                             0, kRotate270, &rotated_res_i420_frame));
  EXPECT_EQ(0,rotated_res_i420_frame.CreateEmptyFrame(width_, height_,
                                                      width_, (width_ + 1) / 2,
                                                      (width_ + 1) / 2));
  EXPECT_EQ(0, ConvertToI420(kI420, orig_buffer_.get(), 0, 0,
                             width_, height_,
                             0, kRotate180, &rotated_res_i420_frame));
}

TEST_F(TestLibYuv, MirrorTest) {
  // TODO(mikhal): Add an automated test to confirm output.
  std::string str;
  int width = 16;
  int half_width = (width + 1) / 2;
  int height = 8;
  int half_height = (height + 1) / 2;

  I420VideoFrame test_frame;
  test_frame.CreateEmptyFrame(width, height, width,
                              half_width, half_width);
  memset(test_frame.buffer(kYPlane), 255, width * height);
  memset(test_frame.buffer(kUPlane), 255, half_width * half_height);
  memset(test_frame.buffer(kVPlane), 255, half_width * half_height);

  // Create input frame.
  I420VideoFrame in_frame, test_in_frame;
  in_frame.CreateEmptyFrame(width, height, width,
                            half_width ,half_width);
  int plane_offset[kNumOfPlanes];
  plane_offset[kYPlane] = 10;
  plane_offset[kUPlane] = 100;
  plane_offset[kVPlane] = 200;
  CreateImage(&in_frame, plane_offset);
  EXPECT_EQ(0, PrintFrame(&in_frame, "InputFrame"));
  test_in_frame.CopyFrame(in_frame);

  I420VideoFrame out_frame, test_out_frame;
  out_frame.CreateEmptyFrame(width, height, width,
                             half_width ,half_width);
  CreateImage(&out_frame, plane_offset);
  test_out_frame.CopyFrame(out_frame);

  // Left-Right.
  std::cout << "Test Mirror function: LeftRight" << std::endl;
  EXPECT_EQ(0, MirrorI420LeftRight(&in_frame, &out_frame));
  EXPECT_EQ(0, PrintFrame(&out_frame, "OutputFrame"));
  EXPECT_EQ(0, MirrorI420LeftRight(&out_frame, &in_frame));

  EXPECT_EQ(0, memcmp(in_frame.buffer(kYPlane),
    test_in_frame.buffer(kYPlane), width * height));
  EXPECT_EQ(0, memcmp(in_frame.buffer(kUPlane),
    test_in_frame.buffer(kUPlane), half_width * half_height));
  EXPECT_EQ(0, memcmp(in_frame.buffer(kVPlane),
    test_in_frame.buffer(kVPlane), half_width * half_height));

  // UpDown
  std::cout << "Test Mirror function: UpDown" << std::endl;
  EXPECT_EQ(0, MirrorI420UpDown(&in_frame, &out_frame));
  EXPECT_EQ(0, PrintFrame(&out_frame, "OutputFrame"));
  EXPECT_EQ(0, MirrorI420UpDown(&out_frame, &test_frame));
  EXPECT_EQ(0, memcmp(in_frame.buffer(kYPlane),
    test_in_frame.buffer(kYPlane), width * height));
  EXPECT_EQ(0, memcmp(in_frame.buffer(kUPlane),
    test_in_frame.buffer(kUPlane), half_width * half_height));
  EXPECT_EQ(0, memcmp(in_frame.buffer(kVPlane),
    test_in_frame.buffer(kVPlane), half_width * half_height));

  // TODO(mikhal): Write to a file, and ask to look at the file.

  std::cout << "Do the mirrored frames look correct?" << std::endl;
}

TEST_F(TestLibYuv, alignment) {
  int value = 0x3FF; // 1023
  EXPECT_EQ(0x400, AlignInt(value, 128));  // Low 7 bits are zero.
  EXPECT_EQ(0x400, AlignInt(value, 64));  // Low 6 bits are zero.
  EXPECT_EQ(0x400, AlignInt(value, 32));  // Low 5 bits are zero.
}

TEST_F(TestLibYuv, StrideAlignment) {
  int stride_y = 0;
  int stride_uv = 0;
  int width = 52;
  Calc16ByteAlignedStride(width, &stride_y, &stride_uv);
  EXPECT_EQ(64, stride_y);
  EXPECT_EQ(32, stride_uv);
  width = 128;
  Calc16ByteAlignedStride(width, &stride_y, &stride_uv);
  EXPECT_EQ(128, stride_y);
  EXPECT_EQ(64, stride_uv);
  width = 127;
  Calc16ByteAlignedStride(width, &stride_y, &stride_uv);
  EXPECT_EQ(128, stride_y);
  EXPECT_EQ(64, stride_uv);
}

}  // namespace
