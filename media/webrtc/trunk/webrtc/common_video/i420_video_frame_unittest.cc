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
#include "webrtc/base/bind.h"
#include "webrtc/base/scoped_ptr.h"
#include "webrtc/test/fake_texture_frame.h"
#include "webrtc/video_frame.h"

namespace webrtc {

bool EqualPlane(const uint8_t* data1,
                const uint8_t* data2,
                int stride,
                int width,
                int height);
int ExpectedSize(int plane_stride, int image_height, PlaneType type);

TEST(TestVideoFrame, InitialValues) {
  VideoFrame frame;
  EXPECT_TRUE(frame.IsZeroSize());
  EXPECT_EQ(kVideoRotation_0, frame.rotation());
}

TEST(TestVideoFrame, CopiesInitialFrameWithoutCrashing) {
  VideoFrame frame;
  VideoFrame frame2;
  frame2.CopyFrame(frame);
}

TEST(TestVideoFrame, WidthHeightValues) {
  VideoFrame frame;
  const int valid_value = 10;
  EXPECT_EQ(0, frame.CreateEmptyFrame(10, 10, 10, 14, 90));
  EXPECT_EQ(valid_value, frame.width());
  EXPECT_EQ(valid_value, frame.height());
  frame.set_timestamp(123u);
  EXPECT_EQ(123u, frame.timestamp());
  frame.set_ntp_time_ms(456);
  EXPECT_EQ(456, frame.ntp_time_ms());
  frame.set_render_time_ms(789);
  EXPECT_EQ(789, frame.render_time_ms());
}

TEST(TestVideoFrame, SizeAllocation) {
  VideoFrame frame;
  EXPECT_EQ(0, frame. CreateEmptyFrame(10, 10, 12, 14, 220));
  int height = frame.height();
  int stride_y = frame.stride(kYPlane);
  int stride_u = frame.stride(kUPlane);
  int stride_v = frame.stride(kVPlane);
  // Verify that allocated size was computed correctly.
  EXPECT_EQ(ExpectedSize(stride_y, height, kYPlane),
            frame.allocated_size(kYPlane));
  EXPECT_EQ(ExpectedSize(stride_u, height, kUPlane),
            frame.allocated_size(kUPlane));
  EXPECT_EQ(ExpectedSize(stride_v, height, kVPlane),
            frame.allocated_size(kVPlane));
}

TEST(TestVideoFrame, CopyFrame) {
  uint32_t timestamp = 1;
  int64_t ntp_time_ms = 2;
  int64_t render_time_ms = 3;
  int stride_y = 15;
  int stride_u = 10;
  int stride_v = 10;
  int width = 15;
  int height = 15;
  // Copy frame.
  VideoFrame small_frame;
  EXPECT_EQ(0, small_frame.CreateEmptyFrame(width, height,
                                            stride_y, stride_u, stride_v));
  small_frame.set_timestamp(timestamp);
  small_frame.set_ntp_time_ms(ntp_time_ms);
  small_frame.set_render_time_ms(render_time_ms);
  const int kSizeY = 400;
  const int kSizeU = 100;
  const int kSizeV = 100;
  const VideoRotation kRotation = kVideoRotation_270;
  uint8_t buffer_y[kSizeY];
  uint8_t buffer_u[kSizeU];
  uint8_t buffer_v[kSizeV];
  memset(buffer_y, 16, kSizeY);
  memset(buffer_u, 8, kSizeU);
  memset(buffer_v, 4, kSizeV);
  VideoFrame big_frame;
  EXPECT_EQ(0,
            big_frame.CreateFrame(buffer_y, buffer_u, buffer_v,
                                  width + 5, height + 5, stride_y + 5,
                                  stride_u, stride_v, kRotation));
  // Frame of smaller dimensions.
  EXPECT_EQ(0, small_frame.CopyFrame(big_frame));
  EXPECT_TRUE(small_frame.EqualsFrame(big_frame));
  EXPECT_EQ(kRotation, small_frame.rotation());

  // Frame of larger dimensions.
  EXPECT_EQ(0, small_frame.CreateEmptyFrame(width, height,
                                            stride_y, stride_u, stride_v));
  memset(small_frame.buffer(kYPlane), 1, small_frame.allocated_size(kYPlane));
  memset(small_frame.buffer(kUPlane), 2, small_frame.allocated_size(kUPlane));
  memset(small_frame.buffer(kVPlane), 3, small_frame.allocated_size(kVPlane));
  EXPECT_EQ(0, big_frame.CopyFrame(small_frame));
  EXPECT_TRUE(small_frame.EqualsFrame(big_frame));
}

TEST(TestVideoFrame, ShallowCopy) {
  uint32_t timestamp = 1;
  int64_t ntp_time_ms = 2;
  int64_t render_time_ms = 3;
  int stride_y = 15;
  int stride_u = 10;
  int stride_v = 10;
  int width = 15;
  int height = 15;

  const int kSizeY = 400;
  const int kSizeU = 100;
  const int kSizeV = 100;
  const VideoRotation kRotation = kVideoRotation_270;
  uint8_t buffer_y[kSizeY];
  uint8_t buffer_u[kSizeU];
  uint8_t buffer_v[kSizeV];
  memset(buffer_y, 16, kSizeY);
  memset(buffer_u, 8, kSizeU);
  memset(buffer_v, 4, kSizeV);
  VideoFrame frame1;
  EXPECT_EQ(0, frame1.CreateFrame(buffer_y, buffer_u, buffer_v, width, height,
                                  stride_y, stride_u, stride_v, kRotation));
  frame1.set_timestamp(timestamp);
  frame1.set_ntp_time_ms(ntp_time_ms);
  frame1.set_render_time_ms(render_time_ms);
  VideoFrame frame2;
  frame2.ShallowCopy(frame1);

  // To be able to access the buffers, we need const pointers to the frames.
  const VideoFrame* const_frame1_ptr = &frame1;
  const VideoFrame* const_frame2_ptr = &frame2;

  EXPECT_TRUE(const_frame1_ptr->buffer(kYPlane) ==
              const_frame2_ptr->buffer(kYPlane));
  EXPECT_TRUE(const_frame1_ptr->buffer(kUPlane) ==
              const_frame2_ptr->buffer(kUPlane));
  EXPECT_TRUE(const_frame1_ptr->buffer(kVPlane) ==
              const_frame2_ptr->buffer(kVPlane));

  EXPECT_EQ(frame2.timestamp(), frame1.timestamp());
  EXPECT_EQ(frame2.ntp_time_ms(), frame1.ntp_time_ms());
  EXPECT_EQ(frame2.render_time_ms(), frame1.render_time_ms());
  EXPECT_EQ(frame2.rotation(), frame1.rotation());

  frame2.set_timestamp(timestamp + 1);
  frame2.set_ntp_time_ms(ntp_time_ms + 1);
  frame2.set_render_time_ms(render_time_ms + 1);
  frame2.set_rotation(kVideoRotation_90);

  EXPECT_NE(frame2.timestamp(), frame1.timestamp());
  EXPECT_NE(frame2.ntp_time_ms(), frame1.ntp_time_ms());
  EXPECT_NE(frame2.render_time_ms(), frame1.render_time_ms());
  EXPECT_NE(frame2.rotation(), frame1.rotation());
}

TEST(TestVideoFrame, Reset) {
  VideoFrame frame;
  ASSERT_EQ(frame.CreateEmptyFrame(5, 5, 5, 5, 5), 0);
  frame.set_ntp_time_ms(1);
  frame.set_timestamp(2);
  frame.set_render_time_ms(3);
  ASSERT_TRUE(frame.video_frame_buffer() != NULL);

  frame.Reset();
  EXPECT_EQ(0u, frame.ntp_time_ms());
  EXPECT_EQ(0u, frame.render_time_ms());
  EXPECT_EQ(0u, frame.timestamp());
  EXPECT_TRUE(frame.video_frame_buffer() == NULL);
}

TEST(TestVideoFrame, CopyBuffer) {
  VideoFrame frame1, frame2;
  int width = 15;
  int height = 15;
  int stride_y = 15;
  int stride_uv = 10;
  const int kSizeY = 225;
  const int kSizeUv = 80;
  EXPECT_EQ(0, frame2.CreateEmptyFrame(width, height,
                                       stride_y, stride_uv, stride_uv));
  uint8_t buffer_y[kSizeY];
  uint8_t buffer_u[kSizeUv];
  uint8_t buffer_v[kSizeUv];
  memset(buffer_y, 16, kSizeY);
  memset(buffer_u, 8, kSizeUv);
  memset(buffer_v, 4, kSizeUv);
  frame2.CreateFrame(buffer_y, buffer_u, buffer_v,
                     width, height, stride_y, stride_uv, stride_uv);
  // Expect exactly the same pixel data.
  EXPECT_TRUE(EqualPlane(buffer_y, frame2.buffer(kYPlane), stride_y, 15, 15));
  EXPECT_TRUE(EqualPlane(buffer_u, frame2.buffer(kUPlane), stride_uv, 8, 8));
  EXPECT_TRUE(EqualPlane(buffer_v, frame2.buffer(kVPlane), stride_uv, 8, 8));

  // Compare size.
  EXPECT_LE(kSizeY, frame2.allocated_size(kYPlane));
  EXPECT_LE(kSizeUv, frame2.allocated_size(kUPlane));
  EXPECT_LE(kSizeUv, frame2.allocated_size(kVPlane));
}

TEST(TestVideoFrame, ReuseAllocation) {
  VideoFrame frame;
  frame.CreateEmptyFrame(640, 320, 640, 320, 320);
  const uint8_t* y = frame.buffer(kYPlane);
  const uint8_t* u = frame.buffer(kUPlane);
  const uint8_t* v = frame.buffer(kVPlane);
  frame.CreateEmptyFrame(640, 320, 640, 320, 320);
  EXPECT_EQ(y, frame.buffer(kYPlane));
  EXPECT_EQ(u, frame.buffer(kUPlane));
  EXPECT_EQ(v, frame.buffer(kVPlane));
}

TEST(TestVideoFrame, FailToReuseAllocation) {
  VideoFrame frame1;
  frame1.CreateEmptyFrame(640, 320, 640, 320, 320);
  const uint8_t* y = frame1.buffer(kYPlane);
  const uint8_t* u = frame1.buffer(kUPlane);
  const uint8_t* v = frame1.buffer(kVPlane);
  // Make a shallow copy of |frame1|.
  VideoFrame frame2(frame1.video_frame_buffer(), 0, 0, kVideoRotation_0);
  frame1.CreateEmptyFrame(640, 320, 640, 320, 320);
  EXPECT_NE(y, frame1.buffer(kYPlane));
  EXPECT_NE(u, frame1.buffer(kUPlane));
  EXPECT_NE(v, frame1.buffer(kVPlane));
}

TEST(TestVideoFrame, TextureInitialValues) {
  test::FakeNativeHandle* handle = new test::FakeNativeHandle();
  VideoFrame frame = test::FakeNativeHandle::CreateFrame(
      handle, 640, 480, 100, 10, webrtc::kVideoRotation_0);
  EXPECT_EQ(640, frame.width());
  EXPECT_EQ(480, frame.height());
  EXPECT_EQ(100u, frame.timestamp());
  EXPECT_EQ(10, frame.render_time_ms());
  EXPECT_EQ(handle, frame.native_handle());

  frame.set_timestamp(200);
  EXPECT_EQ(200u, frame.timestamp());
  frame.set_render_time_ms(20);
  EXPECT_EQ(20, frame.render_time_ms());
}

}  // namespace webrtc
