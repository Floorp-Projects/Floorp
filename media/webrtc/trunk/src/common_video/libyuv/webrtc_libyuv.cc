/*
 *  Copyright (c) 2012 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "common_video/libyuv/include/webrtc_libyuv.h"

#include <assert.h>

#include "libyuv.h"

namespace webrtc {

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

int CalcBufferSize(VideoType type, int width, int height) {
  int buffer_size = 0;
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
      return -1;
  }
  return buffer_size;
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

libyuv::RotationMode ConvertRotationMode(VideoRotationMode rotation) {
  switch(rotation) {
    case kRotateNone:
      return libyuv::kRotate0;
    case kRotate90:
      return libyuv::kRotate90;
    case kRotate180:
      return libyuv::kRotate180;
    case kRotate270:
      return libyuv::kRotate270;
  }
  assert(false);
  return libyuv::kRotate0;
}

int ConvertVideoType(VideoType video_type) {
  switch(video_type) {
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
                  int crop_x, int crop_y,
                  int src_width, int src_height,
                  int sample_size,
                  int dst_width, int dst_height, int dst_stride,
                  VideoRotationMode rotation,
                  uint8_t* dst_frame) {
  // All sanity tests are conducted within LibYuv.
  int abs_dst_height = (dst_height < 0) ? -dst_height : dst_height;
  int half_dst_width = (dst_width + 1) >> 1;
  int half_dst_height = (abs_dst_height + 1) >> 1;
  uint8_t* dst_yplane = dst_frame;
  uint8_t* dst_uplane = dst_yplane + dst_width * abs_dst_height;
  uint8_t* dst_vplane = dst_uplane + half_dst_width * half_dst_height;
  return libyuv::ConvertToI420(src_frame, sample_size,
                               dst_yplane, dst_stride,
                               dst_uplane, (dst_stride + 1) / 2,
                               dst_vplane, (dst_stride + 1) / 2,
                               crop_x, crop_y,
                               src_width, src_height,
                               dst_width, dst_height,
                               ConvertRotationMode(rotation),
                               ConvertVideoType(src_video_type));
}

int ConvertFromI420(const uint8_t* src_frame, int src_stride,
                    VideoType dst_video_type, int dst_sample_size,
                    int width, int height,
                    uint8_t* dst_frame) {
  int abs_height = (height < 0) ? -height : height;
  int half_width = (width + 1) >> 1;
  int half_height = (abs_height + 1) >> 1;
  const uint8_t* src_yplane = src_frame;
  const uint8_t* src_uplane = src_yplane + width * abs_height;
  const uint8_t* src_vplane = src_uplane + half_width * half_height;
  return libyuv::ConvertFromI420(src_yplane, src_stride,
                                 src_uplane, (src_stride + 1) / 2,
                                 src_vplane, (src_stride + 1) / 2,
                                 dst_frame, dst_sample_size,
                                 width, height,
                                 ConvertVideoType(dst_video_type));
}

int ConvertFromYV12(const uint8_t* src_frame, int src_stride,
                    VideoType dst_video_type, int dst_sample_size,
                    int width, int height,
                    uint8_t* dst_frame) {
  int half_src_stride = (src_stride + 1) >> 1;
  int abs_height = (height < 0) ? -height : height;
  int half_height = (abs_height + 1) >> 1;
  const uint8_t* src_yplane = src_frame;
  const uint8_t* src_uplane = src_yplane + width * abs_height;
  const uint8_t* src_vplane = src_uplane + half_src_stride * half_height;
  // YV12 = Y, V, U
  return libyuv::ConvertFromI420(src_yplane, src_stride,
                                 src_vplane, half_src_stride,
                                 src_uplane, half_src_stride,
                                 dst_frame, dst_sample_size,
                                 width, height,
                                 ConvertVideoType(dst_video_type));
}

int MirrorI420LeftRight(const uint8_t* src_frame,
                        uint8_t* dst_frame,
                        int width, int height) {
  int half_width = (width + 1) >> 1;
  int half_height = (height + 1) >> 1;
  const uint8_t* src_yplane = src_frame;
  const uint8_t* src_uplane = src_yplane + width * height;
  const uint8_t* src_vplane = src_uplane + half_width * half_height;
  uint8_t* dst_yplane = dst_frame;
  uint8_t* dst_uplane = dst_yplane + width * height;
  uint8_t* dst_vplane = dst_uplane + half_width * half_height;
  return libyuv::I420Mirror(src_yplane, width,
                            src_uplane, half_width,
                            src_vplane, half_width,
                            dst_yplane, width,
                            dst_uplane, half_width,
                            dst_vplane, half_width,
                            width, height);
}

int MirrorI420UpDown(const uint8_t* src_frame, uint8_t* dst_frame,
                     int width, int height) {
  int half_width = (width + 1) >> 1;
  int half_height = (height + 1) >> 1;
  const uint8_t* src_yplane = src_frame;
  const uint8_t* src_uplane = src_frame + width * height;
  const uint8_t* src_vplane = src_uplane + half_width * half_height;
  uint8_t* dst_yplane = dst_frame;
  uint8_t* dst_uplane = dst_frame + width * height;
  uint8_t* dst_vplane = dst_uplane + half_width * half_height;

  // Inserting negative height flips the frame.
  return libyuv::I420Copy(src_yplane, width,
                          src_uplane, half_width,
                          src_vplane, half_width,
                          dst_yplane, width,
                          dst_uplane, half_width,
                          dst_vplane, half_width,
                          width, -height);
}

// Compute PSNR for an I420 frame (all planes)
double I420PSNR(const uint8_t* ref_frame,
                const uint8_t* test_frame,
                int width, int height) {
  if (!ref_frame || !test_frame)
    return -1;
  else if (height < 0 || width < 0)
    return -1;
  int half_width = (width + 1) >> 1;
  int half_height = (height + 1) >> 1;
  const uint8_t* src_y_a = ref_frame;
  const uint8_t* src_u_a = src_y_a + width * height;
  const uint8_t* src_v_a = src_u_a + half_width * half_height;
  const uint8_t* src_y_b = test_frame;
  const uint8_t* src_u_b = src_y_b + width * height;
  const uint8_t* src_v_b = src_u_b + half_width * half_height;
  // In the following: stride is determined by width.
  double psnr = libyuv::I420Psnr(src_y_a, width,
                                 src_u_a, half_width,
                                 src_v_a, half_width,
                                 src_y_b, width,
                                 src_u_b, half_width,
                                 src_v_b, half_width,
                                 width, height);
  // LibYuv sets the max psnr value to 128, we restrict it to 48.
  // In case of 0 mse in one frame, 128 can skew the results significantly.
  return (psnr > 48.0) ? 48.0 : psnr;
}
// Compute SSIM for an I420 frame (all planes)
double I420SSIM(const uint8_t* ref_frame,
                const uint8_t* test_frame,
                int width, int height) {
  if (!ref_frame || !test_frame)
     return -1;
  else if (height < 0 || width < 0)
     return -1;
  int half_width = (width + 1) >> 1;
  int half_height = (height + 1) >> 1;
  const uint8_t* src_y_a = ref_frame;
  const uint8_t* src_u_a = src_y_a + width * height;
  const uint8_t* src_v_a = src_u_a + half_width * half_height;
  const uint8_t* src_y_b = test_frame;
  const uint8_t* src_u_b = src_y_b + width * height;
  const uint8_t* src_v_b = src_u_b + half_width * half_height;
  int stride_y = width;
  int stride_uv = half_width;
  return libyuv::I420Ssim(src_y_a, stride_y,
                          src_u_a, stride_uv,
                          src_v_a, stride_uv,
                          src_y_b, stride_y,
                          src_u_b, stride_uv,
                          src_v_b, stride_uv,
                          width, height);
}

}  // namespace webrtc
