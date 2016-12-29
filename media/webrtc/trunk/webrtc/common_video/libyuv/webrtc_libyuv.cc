/*
 *  Copyright (c) 2012 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "webrtc/common_video/libyuv/include/webrtc_libyuv.h"

#include <assert.h>
#include <string.h>
#include <limits>

// NOTE(ajm): Path provided by gyp.
#include "libyuv.h"  // NOLINT

namespace webrtc {

const int k16ByteAlignment = 16;

VideoType RawVideoTypeToCommonVideoVideoType(RawVideoType type) {
  switch (type) {
    case kVideoI420:
      return kI420;
    case kVideoIYUV:
      return kIYUV;
    case kVideoRGB24:
      return kRGB24;
    case kVideoARGB:
      return kARGB;
    case kVideoARGB4444:
      return kARGB4444;
    case kVideoRGB565:
      return kRGB565;
    case kVideoARGB1555:
      return kARGB1555;
    case kVideoYUY2:
      return kYUY2;
    case kVideoYV12:
      return kYV12;
    case kVideoUYVY:
      return kUYVY;
    case kVideoNV21:
      return kNV21;
    case kVideoNV12:
      return kNV12;
    case kVideoBGRA:
      return kBGRA;
    case kVideoMJPEG:
      return kMJPG;
    default:
      assert(false);
  }
  return kUnknown;
}

int AlignInt(int value, int alignment) {
  assert(!((alignment - 1) & alignment));
  return ((value + alignment - 1) & ~(alignment - 1));
}

void Calc16ByteAlignedStride(int width, int* stride_y, int* stride_uv) {
  *stride_y = AlignInt(width, k16ByteAlignment);
  *stride_uv = AlignInt((width + 1) / 2, k16ByteAlignment);
}

size_t CalcBufferSize(VideoType type, int width, int height) {
  assert(width >= 0);
  assert(height >= 0);
  // Verify we won't overflow; max is width*height*4
  // 0x7FFF * 0x7FFF * 4 = < 0x100000000
  assert(width <= 0x7FFF); // guarantees no overflow and cheaper than multiply
  assert(height <= 0x7FFF);
  assert(std::numeric_limits<std::size_t>::max() >= 0xFFFFFFFF);
  // assert() is debug only
  if (width > 0x7FFF || height > 0x7FFF) {
    return SIZE_MAX; // very likely forces OOM
  }

  size_t buffer_size = 0;
  switch (type) {
    case kI420:
    case kNV12:
    case kNV21:
    case kIYUV:
    case kYV12: {
      int half_width = (width + 1) >> 1;
      int half_height = (height + 1) >> 1;
      buffer_size = width * height + half_width * half_height * 2;
      break;
    }
    case kARGB4444:
    case kRGB565:
    case kARGB1555:
    case kYUY2:
    case kUYVY:
      buffer_size = width * height * 2;
      break;
    case kRGB24:
      buffer_size = width * height * 3;
      break;
    case kBGRA:
    case kARGB:
      buffer_size = width * height * 4;
      break;
    default:
      assert(false);
      break;
  }
  return buffer_size;
}

int PrintVideoFrame(const VideoFrame& frame, FILE* file) {
  if (file == NULL)
    return -1;
  if (frame.IsZeroSize())
    return -1;
  for (int planeNum = 0; planeNum < kNumOfPlanes; ++planeNum) {
    int width = (planeNum ? (frame.width() + 1) / 2 : frame.width());
    int height = (planeNum ? (frame.height() + 1) / 2 : frame.height());
    PlaneType plane_type = static_cast<PlaneType>(planeNum);
    const uint8_t* plane_buffer = frame.buffer(plane_type);
    for (int y = 0; y < height; y++) {
     if (fwrite(plane_buffer, 1, width, file) !=
         static_cast<unsigned int>(width)) {
       return -1;
       }
       plane_buffer += frame.stride(plane_type);
    }
  }
  return 0;
}

int ExtractBuffer(const VideoFrame& input_frame, size_t size, uint8_t* buffer) {
  assert(buffer);
  if (input_frame.IsZeroSize())
    return -1;
  size_t length =
      CalcBufferSize(kI420, input_frame.width(), input_frame.height());
  if (size < length) {
     return -1;
  }

  int pos = 0;
  uint8_t* buffer_ptr = buffer;

  for (int plane = 0; plane < kNumOfPlanes; ++plane) {
    int width = (plane ? (input_frame.width() + 1) / 2 :
      input_frame.width());
    int height = (plane ? (input_frame.height() + 1) / 2 :
      input_frame.height());
    const uint8_t* plane_ptr = input_frame.buffer(
        static_cast<PlaneType>(plane));
    for (int y = 0; y < height; y++) {
      memcpy(&buffer_ptr[pos], plane_ptr, width);
      pos += width;
      plane_ptr += input_frame.stride(static_cast<PlaneType>(plane));
    }
  }
  return static_cast<int>(length);
}


int ConvertNV12ToRGB565(const uint8_t* src_frame,
                        uint8_t* dst_frame,
                        int width, int height) {
  int abs_height = (height < 0) ? -height : height;
  const uint8_t* yplane = src_frame;
  const uint8_t* uvInterlaced = src_frame + (width * abs_height);

  return libyuv::NV12ToRGB565(yplane, width,
                              uvInterlaced, (width + 1) >> 1,
                              dst_frame, width,
                              width, height);
}

int ConvertRGB24ToARGB(const uint8_t* src_frame, uint8_t* dst_frame,
                       int width, int height, int dst_stride) {
  if (dst_stride == 0)
    dst_stride = width;
  return libyuv::RGB24ToARGB(src_frame, width,
                             dst_frame, dst_stride,
                             width, height);
}

libyuv::RotationMode ConvertRotationMode(VideoRotation rotation) {
  switch (rotation) {
    case kVideoRotation_0:
      return libyuv::kRotate0;
    case kVideoRotation_90:
      return libyuv::kRotate90;
    case kVideoRotation_180:
      return libyuv::kRotate180;
    case kVideoRotation_270:
      return libyuv::kRotate270;
  }
  assert(false);
  return libyuv::kRotate0;
}

int ConvertVideoType(VideoType video_type) {
  switch (video_type) {
    case kUnknown:
      return libyuv::FOURCC_ANY;
    case  kI420:
      return libyuv::FOURCC_I420;
    case kIYUV:  // same as KYV12
    case kYV12:
      return libyuv::FOURCC_YV12;
    case kRGB24:
      return libyuv::FOURCC_24BG;
    case kABGR:
      return libyuv::FOURCC_ABGR;
    case kRGB565:
      return libyuv::FOURCC_RGBP;
    case kYUY2:
      return libyuv::FOURCC_YUY2;
    case kUYVY:
      return libyuv::FOURCC_UYVY;
    case kMJPG:
      return libyuv::FOURCC_MJPG;
    case kNV21:
      return libyuv::FOURCC_NV21;
    case kNV12:
      return libyuv::FOURCC_NV12;
    case kARGB:
      return libyuv::FOURCC_ARGB;
    case kBGRA:
      return libyuv::FOURCC_BGRA;
    case kARGB4444:
      return libyuv::FOURCC_R444;
    case kARGB1555:
      return libyuv::FOURCC_RGBO;
  }
  assert(false);
  return libyuv::FOURCC_ANY;
}

int ConvertToI420(VideoType src_video_type,
                  const uint8_t* src_frame,
                  int crop_x,
                  int crop_y,
                  int src_width,
                  int src_height,
                  size_t sample_size,
                  VideoRotation rotation,
                  VideoFrame* dst_frame) {
  int dst_width = dst_frame->width();
  int dst_height = dst_frame->height();
  // LibYuv expects pre-rotation values for dst.
  // Stride values should correspond to the destination values.
  if (rotation == kVideoRotation_90 || rotation == kVideoRotation_270) {
    dst_width = dst_frame->height();
    dst_height = dst_frame->width();
  }
#ifdef WEBRTC_GONK
  if (src_video_type == kYV12) {
    // In gralloc buffer, yv12 color format's cb and cr's strides are aligned
    // to 16 Bytes boundary. See /system/core/include/system/graphics.h
    int stride_y = (src_width + 15) & ~0x0F;
    int stride_uv = (((stride_y + 1) / 2) + 15) & ~0x0F;
    return libyuv::I420Rotate(src_frame,
                              stride_y,
                              src_frame + (stride_y * src_height) + (stride_uv * ((src_height + 1 / 2)),
                              stride_uv,
                              src_frame + (stride_y * src_height),
                              stride_uv,
                              dst_frame->buffer(kYPlane),
                              dst_frame->stride(kYPlane),
                              dst_frame->buffer(kUPlane),
                              dst_frame->stride(kUPlane),
                              dst_frame->buffer(kVPlane),
                              dst_frame->stride(kVPlane),
                              src_width, src_height,
                              ConvertRotationMode(rotation));
  }
#endif
  return libyuv::ConvertToI420(src_frame, sample_size,
                               dst_frame->buffer(kYPlane),
                               dst_frame->stride(kYPlane),
                               dst_frame->buffer(kUPlane),
                               dst_frame->stride(kUPlane),
                               dst_frame->buffer(kVPlane),
                               dst_frame->stride(kVPlane),
                               crop_x, crop_y,
                               src_width, src_height,
                               dst_width, dst_height,
                               ConvertRotationMode(rotation),
                               ConvertVideoType(src_video_type));
}

int ConvertFromI420(const VideoFrame& src_frame,
                    VideoType dst_video_type,
                    int dst_sample_size,
                    uint8_t* dst_frame) {
  return libyuv::ConvertFromI420(src_frame.buffer(kYPlane),
                                 src_frame.stride(kYPlane),
                                 src_frame.buffer(kUPlane),
                                 src_frame.stride(kUPlane),
                                 src_frame.buffer(kVPlane),
                                 src_frame.stride(kVPlane),
                                 dst_frame, dst_sample_size,
                                 src_frame.width(), src_frame.height(),
                                 ConvertVideoType(dst_video_type));
}

// TODO(mikhal): Create a designated VideoFrame for non I420.
int ConvertFromYV12(const VideoFrame& src_frame,
                    VideoType dst_video_type,
                    int dst_sample_size,
                    uint8_t* dst_frame) {
  // YV12 = Y, V, U
  return libyuv::ConvertFromI420(src_frame.buffer(kYPlane),
                                 src_frame.stride(kYPlane),
                                 src_frame.buffer(kVPlane),
                                 src_frame.stride(kVPlane),
                                 src_frame.buffer(kUPlane),
                                 src_frame.stride(kUPlane),
                                 dst_frame, dst_sample_size,
                                 src_frame.width(), src_frame.height(),
                                 ConvertVideoType(dst_video_type));
}

// Compute PSNR for an I420 frame (all planes)
double I420PSNR(const VideoFrame* ref_frame, const VideoFrame* test_frame) {
  if (!ref_frame || !test_frame)
    return -1;
  else if ((ref_frame->width() !=  test_frame->width()) ||
          (ref_frame->height() !=  test_frame->height()))
    return -1;
  else if (ref_frame->width() < 0 || ref_frame->height() < 0)
    return -1;

  double psnr = libyuv::I420Psnr(ref_frame->buffer(kYPlane),
                                 ref_frame->stride(kYPlane),
                                 ref_frame->buffer(kUPlane),
                                 ref_frame->stride(kUPlane),
                                 ref_frame->buffer(kVPlane),
                                 ref_frame->stride(kVPlane),
                                 test_frame->buffer(kYPlane),
                                 test_frame->stride(kYPlane),
                                 test_frame->buffer(kUPlane),
                                 test_frame->stride(kUPlane),
                                 test_frame->buffer(kVPlane),
                                 test_frame->stride(kVPlane),
                                 test_frame->width(), test_frame->height());
  // LibYuv sets the max psnr value to 128, we restrict it here.
  // In case of 0 mse in one frame, 128 can skew the results significantly.
  return (psnr > kPerfectPSNR) ? kPerfectPSNR : psnr;
}

// Compute SSIM for an I420 frame (all planes)
double I420SSIM(const VideoFrame* ref_frame, const VideoFrame* test_frame) {
  if (!ref_frame || !test_frame)
    return -1;
  else if ((ref_frame->width() !=  test_frame->width()) ||
          (ref_frame->height() !=  test_frame->height()))
    return -1;
  else if (ref_frame->width() < 0 || ref_frame->height()  < 0)
    return -1;

  return libyuv::I420Ssim(ref_frame->buffer(kYPlane),
                          ref_frame->stride(kYPlane),
                          ref_frame->buffer(kUPlane),
                          ref_frame->stride(kUPlane),
                          ref_frame->buffer(kVPlane),
                          ref_frame->stride(kVPlane),
                          test_frame->buffer(kYPlane),
                          test_frame->stride(kYPlane),
                          test_frame->buffer(kUPlane),
                          test_frame->stride(kUPlane),
                          test_frame->buffer(kVPlane),
                          test_frame->stride(kVPlane),
                          test_frame->width(), test_frame->height());
}
}  // namespace webrtc
