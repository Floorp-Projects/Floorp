/*
 *  Copyright 2017 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "sdk/objc/native/src/objc_frame_buffer.h"

#include "api/make_ref_counted.h"
#import "base/RTCVideoFrameBuffer.h"
#import "sdk/objc/api/video_frame_buffer/RTCNativeI420Buffer+Private.h"

namespace webrtc {

namespace {

/** ObjCFrameBuffer that conforms to I420BufferInterface by wrapping RTC_OBJC_TYPE(RTCI420Buffer) */
class ObjCI420FrameBuffer : public I420BufferInterface {
 public:
  explicit ObjCI420FrameBuffer(id<RTC_OBJC_TYPE(RTCI420Buffer)> frame_buffer)
      : frame_buffer_(frame_buffer), width_(frame_buffer.width), height_(frame_buffer.height) {}
  ~ObjCI420FrameBuffer() override {}

  int width() const override { return width_; }

  int height() const override { return height_; }

  const uint8_t* DataY() const override { return frame_buffer_.dataY; }

  const uint8_t* DataU() const override { return frame_buffer_.dataU; }

  const uint8_t* DataV() const override { return frame_buffer_.dataV; }

  int StrideY() const override { return frame_buffer_.strideY; }

  int StrideU() const override { return frame_buffer_.strideU; }

  int StrideV() const override { return frame_buffer_.strideV; }

 private:
  id<RTC_OBJC_TYPE(RTCI420Buffer)> frame_buffer_;
  int width_;
  int height_;
};

}  // namespace

ObjCFrameBuffer::ObjCFrameBuffer(id<RTC_OBJC_TYPE(RTCVideoFrameBuffer)> frame_buffer)
    : frame_buffer_(frame_buffer), width_(frame_buffer.width), height_(frame_buffer.height) {}

ObjCFrameBuffer::~ObjCFrameBuffer() {}

VideoFrameBuffer::Type ObjCFrameBuffer::type() const {
  return Type::kNative;
}

int ObjCFrameBuffer::width() const {
  return width_;
}

int ObjCFrameBuffer::height() const {
  return height_;
}

rtc::scoped_refptr<I420BufferInterface> ObjCFrameBuffer::ToI420() {
  return rtc::make_ref_counted<ObjCI420FrameBuffer>([frame_buffer_ toI420]);
}

rtc::scoped_refptr<VideoFrameBuffer> ObjCFrameBuffer::CropAndScale(int offset_x,
                                                                   int offset_y,
                                                                   int crop_width,
                                                                   int crop_height,
                                                                   int scaled_width,
                                                                   int scaled_height) {
  if ([frame_buffer_ respondsToSelector:@selector
                     (cropAndScaleWith:offsetY:cropWidth:cropHeight:scaleWidth:scaleHeight:)]) {
    return rtc::make_ref_counted<ObjCFrameBuffer>([frame_buffer_ cropAndScaleWith:offset_x
                                                                          offsetY:offset_y
                                                                        cropWidth:crop_width
                                                                       cropHeight:crop_height
                                                                       scaleWidth:scaled_width
                                                                      scaleHeight:scaled_height]);
  }

  // Use the default implementation.
  return VideoFrameBuffer::CropAndScale(
      offset_x, offset_y, crop_width, crop_height, scaled_width, scaled_height);
}

id<RTC_OBJC_TYPE(RTCVideoFrameBuffer)> ObjCFrameBuffer::wrapped_frame_buffer() const {
  return frame_buffer_;
}

id<RTC_OBJC_TYPE(RTCVideoFrameBuffer)> ToObjCVideoFrameBuffer(
    const rtc::scoped_refptr<VideoFrameBuffer>& buffer) {
  if (buffer->type() == VideoFrameBuffer::Type::kNative) {
    return static_cast<ObjCFrameBuffer*>(buffer.get())->wrapped_frame_buffer();
  } else {
    return [[RTC_OBJC_TYPE(RTCI420Buffer) alloc] initWithFrameBuffer:buffer->ToI420()];
  }
}

}  // namespace webrtc
