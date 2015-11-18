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
#include "webrtc/base/scoped_ptr.h"
#include "webrtc/common_video/interface/i420_video_frame.h"

namespace webrtc {

class NativeHandleImpl : public NativeHandle {
 public:
  NativeHandleImpl() : ref_count_(0) {}
  virtual ~NativeHandleImpl() {}
  virtual int32_t AddRef() { return ++ref_count_; }
  virtual int32_t Release() { return --ref_count_; }
  virtual void* GetHandle() { return NULL; }

  int32_t ref_count() { return ref_count_; }
 private:
  int32_t ref_count_;
};

bool EqualPlane(const uint8_t* data1,
                const uint8_t* data2,
                int stride,
                int width,
                int height);
bool EqualFrames(const I420VideoFrame& frame1, const I420VideoFrame& frame2);
bool EqualTextureFrames(const I420VideoFrame& frame1,
                        const I420VideoFrame& frame2);
int ExpectedSize(int plane_stride, int image_height, PlaneType type);

TEST(TestI420VideoFrame, InitialValues) {
  I420VideoFrame frame;
  EXPECT_TRUE(frame.IsZeroSize());
  EXPECT_EQ(kVideoRotation_0, frame.rotation());
}

TEST(TestI420VideoFrame, CopiesInitialFrameWithoutCrashing) {
  I420VideoFrame frame;
  I420VideoFrame frame2;
  frame2.CopyFrame(frame);
}

TEST(TestI420VideoFrame, WidthHeightValues) {
  I420VideoFrame frame;
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

TEST(TestI420VideoFrame, CopyFrame) {
  uint32_t timestamp = 1;
  int64_t ntp_time_ms = 2;
  int64_t render_time_ms = 3;
  int stride_y = 15;
  int stride_u = 10;
  int stride_v = 10;
  int width = 15;
  int height = 15;
  // Copy frame.
  I420VideoFrame small_frame;
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
  I420VideoFrame big_frame;
  EXPECT_EQ(0,
            big_frame.CreateFrame(buffer_y, buffer_u, buffer_v,
                                  width + 5, height + 5, stride_y + 5,
                                  stride_u, stride_v, kRotation));
  // Frame of smaller dimensions.
  EXPECT_EQ(0, small_frame.CopyFrame(big_frame));
  EXPECT_TRUE(EqualFrames(small_frame, big_frame));
  EXPECT_EQ(kRotation, small_frame.rotation());

  // Frame of larger dimensions.
  EXPECT_EQ(0, small_frame.CreateEmptyFrame(width, height,
                                            stride_y, stride_u, stride_v));
  memset(small_frame.buffer(kYPlane), 1, small_frame.allocated_size(kYPlane));
  memset(small_frame.buffer(kUPlane), 2, small_frame.allocated_size(kUPlane));
  memset(small_frame.buffer(kVPlane), 3, small_frame.allocated_size(kVPlane));
  EXPECT_EQ(0, big_frame.CopyFrame(small_frame));
  EXPECT_TRUE(EqualFrames(small_frame, big_frame));
}

TEST(TestI420VideoFrame, ShallowCopy) {
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
  I420VideoFrame frame1;
  EXPECT_EQ(0, frame1.CreateFrame(buffer_y, buffer_u, buffer_v, width, height,
                                  stride_y, stride_u, stride_v, kRotation));
  frame1.set_timestamp(timestamp);
  frame1.set_ntp_time_ms(ntp_time_ms);
  frame1.set_render_time_ms(render_time_ms);
  I420VideoFrame frame2;
  frame2.ShallowCopy(frame1);

  // To be able to access the buffers, we need const pointers to the frames.
  const I420VideoFrame* const_frame1_ptr = &frame1;
  const I420VideoFrame* const_frame2_ptr = &frame2;

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

TEST(TestI420VideoFrame, Reset) {
  I420VideoFrame frame;
  ASSERT_TRUE(frame.CreateEmptyFrame(5, 5, 5, 5, 5) == 0);
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

TEST(TestI420VideoFrame, ReuseAllocation) {
  I420VideoFrame frame;
  frame.CreateEmptyFrame(640, 320, 640, 320, 320);
  const uint8_t* y = frame.buffer(kYPlane);
  const uint8_t* u = frame.buffer(kUPlane);
  const uint8_t* v = frame.buffer(kVPlane);
  frame.CreateEmptyFrame(640, 320, 640, 320, 320);
  EXPECT_EQ(y, frame.buffer(kYPlane));
  EXPECT_EQ(u, frame.buffer(kUPlane));
  EXPECT_EQ(v, frame.buffer(kVPlane));
}

TEST(TestI420VideoFrame, FailToReuseAllocation) {
  I420VideoFrame frame1;
  frame1.CreateEmptyFrame(640, 320, 640, 320, 320);
  const uint8_t* y = frame1.buffer(kYPlane);
  const uint8_t* u = frame1.buffer(kUPlane);
  const uint8_t* v = frame1.buffer(kVPlane);
  // Make a shallow copy of |frame1|.
  I420VideoFrame frame2(frame1.video_frame_buffer(), 0, 0, kVideoRotation_0);
  frame1.CreateEmptyFrame(640, 320, 640, 320, 320);
  EXPECT_NE(y, frame1.buffer(kYPlane));
  EXPECT_NE(u, frame1.buffer(kUPlane));
  EXPECT_NE(v, frame1.buffer(kVPlane));
}

TEST(TestI420VideoFrame, TextureInitialValues) {
  NativeHandleImpl handle;
  I420VideoFrame frame(&handle, 640, 480, 100, 10);
  EXPECT_EQ(640, frame.width());
  EXPECT_EQ(480, frame.height());
  EXPECT_EQ(100u, frame.timestamp());
  EXPECT_EQ(10, frame.render_time_ms());
  EXPECT_EQ(&handle, frame.native_handle());

  frame.set_timestamp(200);
  EXPECT_EQ(200u, frame.timestamp());
  frame.set_render_time_ms(20);
  EXPECT_EQ(20, frame.render_time_ms());
}

TEST(TestI420VideoFrame, RefCount) {
  NativeHandleImpl handle;
  EXPECT_EQ(0, handle.ref_count());
  I420VideoFrame *frame = new I420VideoFrame(&handle, 640, 480, 100, 200);
  EXPECT_EQ(1, handle.ref_count());
  delete frame;
  EXPECT_EQ(0, handle.ref_count());
}

bool EqualPlane(const uint8_t* data1,
                const uint8_t* data2,
                int stride,
                int width,
                int height) {
  for (int y = 0; y < height; ++y) {
    if (memcmp(data1, data2, width) != 0)
      return false;
    data1 += stride;
    data2 += stride;
  }
  return true;
}

bool EqualFrames(const I420VideoFrame& frame1, const I420VideoFrame& frame2) {
  if ((frame1.width() != frame2.width()) ||
      (frame1.height() != frame2.height()) ||
      (frame1.stride(kYPlane) != frame2.stride(kYPlane)) ||
      (frame1.stride(kUPlane) != frame2.stride(kUPlane)) ||
      (frame1.stride(kVPlane) != frame2.stride(kVPlane)) ||
      (frame1.timestamp() != frame2.timestamp()) ||
      (frame1.ntp_time_ms() != frame2.ntp_time_ms()) ||
      (frame1.render_time_ms() != frame2.render_time_ms())) {
    return false;
  }
  const int half_width = (frame1.width() + 1) / 2;
  const int half_height = (frame1.height() + 1) / 2;
  return EqualPlane(frame1.buffer(kYPlane), frame2.buffer(kYPlane),
                    frame1.stride(kYPlane), frame1.width(), frame1.height()) &&
         EqualPlane(frame1.buffer(kUPlane), frame2.buffer(kUPlane),
                    frame1.stride(kUPlane), half_width, half_height) &&
         EqualPlane(frame1.buffer(kVPlane), frame2.buffer(kVPlane),
                    frame1.stride(kVPlane), half_width, half_height);
}

bool EqualTextureFrames(const I420VideoFrame& frame1,
                        const I420VideoFrame& frame2) {
  return ((frame1.native_handle() == frame2.native_handle()) &&
          (frame1.width() == frame2.width()) &&
          (frame1.height() == frame2.height()) &&
          (frame1.timestamp() == frame2.timestamp()) &&
          (frame1.render_time_ms() == frame2.render_time_ms()));
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
