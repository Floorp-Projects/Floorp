/*
 *  Copyright (c) 2013 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "webrtc/common_video/interface/texture_video_frame.h"

#include "testing/gtest/include/gtest/gtest.h"
#include "webrtc/common_video/interface/native_handle.h"

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

bool EqualTextureFrames(const I420VideoFrame& frame1,
                        const I420VideoFrame& frame2);

TEST(TestTextureVideoFrame, InitialValues) {
  NativeHandleImpl handle;
  TextureVideoFrame frame(&handle, 640, 480, 100, 10);
  EXPECT_EQ(640, frame.width());
  EXPECT_EQ(480, frame.height());
  EXPECT_EQ(100u, frame.timestamp());
  EXPECT_EQ(10, frame.render_time_ms());
  EXPECT_EQ(&handle, frame.native_handle());

  EXPECT_EQ(0, frame.set_width(320));
  EXPECT_EQ(320, frame.width());
  EXPECT_EQ(0, frame.set_height(240));
  EXPECT_EQ(240, frame.height());
  frame.set_timestamp(200);
  EXPECT_EQ(200u, frame.timestamp());
  frame.set_render_time_ms(20);
  EXPECT_EQ(20, frame.render_time_ms());
}

TEST(TestTextureVideoFrame, RefCount) {
  NativeHandleImpl handle;
  EXPECT_EQ(0, handle.ref_count());
  TextureVideoFrame *frame = new TextureVideoFrame(&handle, 640, 480, 100, 200);
  EXPECT_EQ(1, handle.ref_count());
  delete frame;
  EXPECT_EQ(0, handle.ref_count());
}

TEST(TestTextureVideoFrame, CloneFrame) {
  NativeHandleImpl handle;
  TextureVideoFrame frame1(&handle, 640, 480, 100, 200);
  scoped_ptr<I420VideoFrame> frame2(frame1.CloneFrame());
  EXPECT_TRUE(frame2.get() != NULL);
  EXPECT_TRUE(EqualTextureFrames(frame1, *frame2));
}

bool EqualTextureFrames(const I420VideoFrame& frame1,
                        const I420VideoFrame& frame2) {
  return ((frame1.native_handle() == frame2.native_handle()) &&
          (frame1.width() == frame2.width()) &&
          (frame1.height() == frame2.height()) &&
          (frame1.timestamp() == frame2.timestamp()) &&
          (frame1.render_time_ms() == frame2.render_time_ms()));
}

}  // namespace webrtc
