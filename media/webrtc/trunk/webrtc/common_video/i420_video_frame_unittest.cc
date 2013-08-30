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
#include "webrtc/common_video/interface/i420_video_frame.h"
#include "webrtc/system_wrappers/interface/ref_count.h"
#include "webrtc/system_wrappers/interface/scoped_ptr.h"
#include "webrtc/system_wrappers/interface/scoped_refptr.h"

namespace webrtc {

bool EqualFrames(const I420VideoFrame& videoFrame1,
                 const I420VideoFrame& videoFrame2);
bool EqualFramesExceptSize(const I420VideoFrame& frame1,
                           const I420VideoFrame& frame2);
int ExpectedSize(int plane_stride, int image_height, PlaneType type);

TEST(TestI420VideoFrame, InitialValues) {
  I420VideoFrame frame;
  // Invalid arguments - one call for each variable.
  EXPECT_TRUE(frame.IsZeroSize());
  EXPECT_EQ(-1, frame.CreateEmptyFrame(0, 10, 10, 14, 14));
  EXPECT_EQ(-1, frame.CreateEmptyFrame(10, -1, 10, 90, 14));
  EXPECT_EQ(-1, frame.CreateEmptyFrame(10, 10, 0, 14, 18));
  EXPECT_EQ(-1, frame.CreateEmptyFrame(10, 10, 10, -2, 13));
  EXPECT_EQ(-1, frame.CreateEmptyFrame(10, 10, 10, 14, 0));
  EXPECT_EQ(0, frame.CreateEmptyFrame(10, 10, 10, 14, 90));
  EXPECT_FALSE(frame.IsZeroSize());
}

TEST(TestI420VideoFrame, WidthHeightValues) {
  I420VideoFrame frame;
  const int valid_value = 10;
  const int invalid_value = -1;
  EXPECT_EQ(0, frame.CreateEmptyFrame(10, 10, 10, 14, 90));
  EXPECT_EQ(valid_value, frame.width());
  EXPECT_EQ(invalid_value, frame.set_width(invalid_value));
  EXPECT_EQ(valid_value, frame.height());
  EXPECT_EQ(valid_value, frame.height());
  EXPECT_EQ(invalid_value, frame.set_height(0));
  EXPECT_EQ(valid_value, frame.height());
  frame.set_timestamp(100u);
  EXPECT_EQ(100u, frame.timestamp());
  frame.set_render_time_ms(100);
  EXPECT_EQ(100, frame.render_time_ms());
}

TEST(TestI420VideoFrame, SizeAllocation) {
  I420VideoFrame frame;
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

TEST(TestI420VideoFrame, ResetSize) {
  I420VideoFrame frame;
  EXPECT_EQ(0, frame. CreateEmptyFrame(10, 10, 12, 14, 220));
  EXPECT_FALSE(frame.IsZeroSize());
  frame.ResetSize();
  EXPECT_TRUE(frame.IsZeroSize());
}

TEST(TestI420VideoFrame, CopyFrame) {
  I420VideoFrame frame1, frame2;
  uint32_t timestamp = 1;
  int64_t render_time_ms = 1;
  int stride_y = 15;
  int stride_u = 10;
  int stride_v = 10;
  int width = 15;
  int height = 15;
  // Copy frame.
  EXPECT_EQ(0, frame1.CreateEmptyFrame(width, height,
                                       stride_y, stride_u, stride_v));
  frame1.set_timestamp(timestamp);
  frame1.set_render_time_ms(render_time_ms);
  const int kSizeY = 225;
  const int kSizeU = 80;
  const int kSizeV = 80;
  uint8_t buffer_y[kSizeY];
  uint8_t buffer_u[kSizeU];
  uint8_t buffer_v[kSizeV];
  memset(buffer_y, 16, kSizeY);
  memset(buffer_u, 8, kSizeU);
  memset(buffer_v, 4, kSizeV);
  frame2.CreateFrame(kSizeY, buffer_y,
                     kSizeU, buffer_u,
                     kSizeV, buffer_v,
                     width + 5, height + 5, stride_y + 5, stride_u, stride_v);
  // Frame of smaller dimensions - allocated sizes should not vary.
  EXPECT_EQ(0, frame1.CopyFrame(frame2));
  EXPECT_TRUE(EqualFramesExceptSize(frame1, frame2));
  EXPECT_EQ(kSizeY, frame1.allocated_size(kYPlane));
  EXPECT_EQ(kSizeU, frame1.allocated_size(kUPlane));
  EXPECT_EQ(kSizeV, frame1.allocated_size(kVPlane));
  // Verify copy of all parameters.
  // Frame of larger dimensions - update allocated sizes.
  EXPECT_EQ(0, frame2.CopyFrame(frame1));
  EXPECT_TRUE(EqualFrames(frame1, frame2));
}

TEST(TestI420VideoFrame, CopyBuffer) {
  I420VideoFrame frame1, frame2;
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
  frame2.CreateFrame(kSizeY, buffer_y,
                     kSizeUv, buffer_u,
                     kSizeUv, buffer_v,
                     width, height, stride_y, stride_uv, stride_uv);
  // Copy memory (at least allocated size).
  EXPECT_EQ(memcmp(buffer_y, frame2.buffer(kYPlane), kSizeY), 0);
  EXPECT_EQ(memcmp(buffer_u, frame2.buffer(kUPlane), kSizeUv), 0);
  EXPECT_EQ(memcmp(buffer_v, frame2.buffer(kVPlane), kSizeUv), 0);
  // Comapre size.
  EXPECT_LE(kSizeY, frame2.allocated_size(kYPlane));
  EXPECT_LE(kSizeUv, frame2.allocated_size(kUPlane));
  EXPECT_LE(kSizeUv, frame2.allocated_size(kVPlane));
}

TEST(TestI420VideoFrame, FrameSwap) {
  I420VideoFrame frame1, frame2;
  uint32_t timestamp1 = 1;
  int64_t render_time_ms1 = 1;
  int stride_y1 = 15;
  int stride_u1 = 10;
  int stride_v1 = 10;
  int width1 = 15;
  int height1 = 15;
  const int kSizeY1 = 225;
  const int kSizeU1 = 80;
  const int kSizeV1 = 80;
  uint32_t timestamp2 = 2;
  int64_t render_time_ms2 = 4;
  int stride_y2 = 30;
  int stride_u2 = 20;
  int stride_v2 = 20;
  int width2 = 30;
  int height2 = 30;
  const int kSizeY2 = 900;
  const int kSizeU2 = 300;
  const int kSizeV2 = 300;
  // Initialize frame1 values.
  EXPECT_EQ(0, frame1.CreateEmptyFrame(width1, height1,
                                       stride_y1, stride_u1, stride_v1));
  frame1.set_timestamp(timestamp1);
  frame1.set_render_time_ms(render_time_ms1);
  // Set memory for frame1.
  uint8_t buffer_y1[kSizeY1];
  uint8_t buffer_u1[kSizeU1];
  uint8_t buffer_v1[kSizeV1];
  memset(buffer_y1, 2, kSizeY1);
  memset(buffer_u1, 4, kSizeU1);
  memset(buffer_v1, 8, kSizeV1);
  frame1.CreateFrame(kSizeY1, buffer_y1,
                     kSizeU1, buffer_u1,
                     kSizeV1, buffer_v1,
                     width1, height1, stride_y1, stride_u1, stride_v1);
  // Initialize frame2 values.
  EXPECT_EQ(0, frame2.CreateEmptyFrame(width2, height2,
                                       stride_y2, stride_u2, stride_v2));
  frame2.set_timestamp(timestamp2);
  frame2.set_render_time_ms(render_time_ms2);
  // Set memory for frame2.
  uint8_t buffer_y2[kSizeY2];
  uint8_t buffer_u2[kSizeU2];
  uint8_t buffer_v2[kSizeV2];
  memset(buffer_y2, 0, kSizeY2);
  memset(buffer_u2, 1, kSizeU2);
  memset(buffer_v2, 2, kSizeV2);
  frame2.CreateFrame(kSizeY2, buffer_y2,
                     kSizeU2, buffer_u2,
                     kSizeV2, buffer_v2,
                     width2, height2, stride_y2, stride_u2, stride_v2);
  // Copy frames for subsequent comparison.
  I420VideoFrame frame1_copy, frame2_copy;
  frame1_copy.CopyFrame(frame1);
  frame2_copy.CopyFrame(frame2);
  // Swap frames.
  frame1.SwapFrame(&frame2);
  // Verify swap.
  EXPECT_TRUE(EqualFrames(frame1_copy, frame2));
  EXPECT_TRUE(EqualFrames(frame2_copy, frame1));
}

TEST(TestI420VideoFrame, RefCountedInstantiation) {
  // Refcounted instantiation - ref_count should correspond to the number of
  // instances.
  scoped_refptr<I420VideoFrame> ref_count_frame(
      new RefCountImpl<I420VideoFrame>());
  EXPECT_EQ(2, ref_count_frame->AddRef());
  EXPECT_EQ(3, ref_count_frame->AddRef());
  EXPECT_EQ(2, ref_count_frame->Release());
  EXPECT_EQ(1, ref_count_frame->Release());
}

bool EqualFrames(const I420VideoFrame& frame1,
                 const I420VideoFrame& frame2) {
  if (!EqualFramesExceptSize(frame1, frame2))
    return false;
  // Compare allocated memory size.
  bool ret = true;
  ret |= (frame1.allocated_size(kYPlane) == frame2.allocated_size(kYPlane));
  ret |= (frame1.allocated_size(kUPlane) == frame2.allocated_size(kUPlane));
  ret |= (frame1.allocated_size(kVPlane) == frame2.allocated_size(kVPlane));
  return ret;
}

bool EqualFramesExceptSize(const I420VideoFrame& frame1,
                           const I420VideoFrame& frame2) {
  bool ret = true;
  ret |= (frame1.width() == frame2.width());
  ret |= (frame1.height() == frame2.height());
  ret |= (frame1.stride(kYPlane) == frame2.stride(kYPlane));
  ret |= (frame1.stride(kUPlane) == frame2.stride(kUPlane));
  ret |= (frame1.stride(kVPlane) == frame2.stride(kVPlane));
  ret |= (frame1.timestamp() == frame2.timestamp());
  ret |= (frame1.render_time_ms() == frame2.render_time_ms());
  if (!ret)
    return false;
  // Memory should be the equal for the minimum of the two sizes.
  int size_y = std::min(frame1.allocated_size(kYPlane),
                        frame2.allocated_size(kYPlane));
  int size_u = std::min(frame1.allocated_size(kUPlane),
                        frame2.allocated_size(kUPlane));
  int size_v = std::min(frame1.allocated_size(kVPlane),
                        frame2.allocated_size(kVPlane));
  int ret_val = 0;
  ret_val += memcmp(frame1.buffer(kYPlane), frame2.buffer(kYPlane), size_y);
  ret_val += memcmp(frame1.buffer(kUPlane), frame2.buffer(kUPlane), size_u);
  ret_val += memcmp(frame1.buffer(kVPlane), frame2.buffer(kVPlane), size_v);
  if (ret_val == 0)
    return true;
  return false;
}

int ExpectedSize(int plane_stride, int image_height, PlaneType type) {
  if (type == kYPlane) {
    return (plane_stride * image_height);
  } else {
    int half_height = (image_height + 1) / 2;
    return (plane_stride * half_height);
  }
}

}  // namespace webrtc
