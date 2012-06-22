/*
 *  Copyright (c) 2012 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "common_video/libyuv/include/libyuv.h"

#include <assert.h>

// LibYuv includes
#include "third_party/libyuv/include/libyuv.h"

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
  int bits_per_pixel = 32;
  switch (type) {
    case kI420:
    case kNV12:
    case kNV21:
    case kIYUV:
    case kYV12:
      bits_per_pixel = 12;
      break;
    case kARGB4444:
    case kRGB565:
    case kARGB1555:
    case kYUY2:
    case kUYVY:
      bits_per_pixel = 16;
      break;
    case kRGB24:
      bits_per_pixel = 24;
      break;
    case kBGRA:
    case kARGB:
      bits_per_pixel = 32;
      break;
    default:
      assert(false);
      return -1;
  }
  return (width * height * bits_per_pixel) / 8;  // bytes
}

int CalcBufferSize(VideoType src_video_type,
                   VideoType dst_video_type,
                   int length) {
  int src_bits_per_pixel = 32;
  switch (src_video_type) {
    case kI420:
    case kNV12:
    case kNV21:
    case kIYUV:
    case kYV12:
      src_bits_per_pixel = 12;
      break;
    case kARGB4444:
    case kRGB565:
    case kARGB1555:
    case kYUY2:
    case kUYVY:
      src_bits_per_pixel = 16;
      break;
    case kRGB24:
      src_bits_per_pixel = 24;
      break;
    case kBGRA:
    case kARGB:
      src_bits_per_pixel = 32;
      break;
    default:
      assert(false);
      return -1;
  }

  int dst_bits_per_pixel = 32;
  switch (dst_video_type) {
    case kI420:
    case kIYUV:
    case kYV12:
      dst_bits_per_pixel = 12;
      break;
    case kARGB4444:
    case kRGB565:
    case kARGB1555:
    case kYUY2:
    case kUYVY:
      dst_bits_per_pixel = 16;
      break;
    case kRGB24:
      dst_bits_per_pixel = 24;
      break;
    case kBGRA:
    case kARGB:
      dst_bits_per_pixel = 32;
      break;
    default:
      assert(false);
      return -1;
  }
  return (length * dst_bits_per_pixel) / src_bits_per_pixel;
}

int ConvertI420ToARGB4444(const uint8_t* src_frame,
                          uint8_t* dst_frame,
                          int width, int height,
                          int dst_stride) {
  if (dst_stride == 0 || dst_stride == width)
    dst_stride = 2 * width;
  const uint8_t* yplane = src_frame;
  const uint8_t* uplane = src_frame + width * height;
  const uint8_t* vplane = uplane + (width * height / 4);

  return libyuv::I420ToARGB4444(yplane, width,
                                uplane, width / 2,
                                vplane, width / 2,
                                dst_frame, dst_stride,
                                width, height);
}

int ConvertI420ToRGB565(const uint8_t* src_frame,
                        uint8_t* dst_frame,
                        int width, int height) {
  int abs_height = (height < 0) ? -height : height;
  const uint8_t* yplane = src_frame;
  const uint8_t* uplane = src_frame + width * abs_height;
  const uint8_t* vplane = uplane + (width * abs_height / 4);

  return libyuv::I420ToRGB565(yplane, width,
                              uplane, width / 2,
                              vplane, width / 2,
                              dst_frame, width,
                              width, height);
}

int ConvertI420ToARGB1555(const uint8_t* src_frame,
                          uint8_t* dst_frame,
                          int width, int height,
                          int dst_stride) {
  if (dst_stride == 0 || dst_stride == width)
    dst_stride = 2 * width;
  else if (dst_stride < 2 * width)
    return -1;

  const uint8_t* yplane = src_frame;
  const uint8_t* uplane = src_frame + width * height;
  const uint8_t* vplane = uplane + (width * height / 4);

  return libyuv::I420ToARGB1555(yplane, width,
                                uplane, width / 2,
                                vplane, width / 2,
                                dst_frame, dst_stride,
                                width, height);
}

int ConvertNV12ToRGB565(const uint8_t* src_frame,
                        uint8_t* dst_frame,
                        int width, int height) {
  const uint8_t* yplane = src_frame;
  const uint8_t* uvInterlaced = src_frame + (width * height);

  return libyuv::NV12ToRGB565(yplane, width,
                              uvInterlaced, width / 2,
                              dst_frame, width,
                              width, height);
}

int ConvertRGB24ToARGB(const uint8_t* src_frame, uint8_t* dst_frame,
                       int width, int height, int dst_stride) {
  if (dst_stride == 0 || dst_stride == width)
    dst_stride = width;
  // Stride - currently webrtc style
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
    case kARGB4444:
    case kRGB565:
    case kARGB1555:
      // TODO(mikhal): Not supported;
      assert(false);
      return libyuv::FOURCC_ANY;
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
  uint8_t* dst_yplane = dst_frame;
  uint8_t* dst_uplane = dst_yplane + dst_width * dst_height;
  uint8_t* dst_vplane = dst_uplane + (dst_width * dst_height / 4);
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
  const uint8_t* src_yplane = src_frame;
  const uint8_t* src_uplane = src_yplane + width * height;
  const uint8_t* src_vplane = src_uplane + (width * height / 4);
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
  const uint8_t* src_yplane = src_frame;
  const uint8_t* src_uplane = src_yplane + width * height;
  const uint8_t* src_vplane = src_uplane + (width * height / 4);
  // YV12 = Y, V, U
  return libyuv::ConvertFromI420(src_yplane, src_stride,
                                 src_vplane, (src_stride + 1) / 2,
                                 src_uplane, (src_stride + 1) / 2,
                                 dst_frame, dst_sample_size,
                                 width, height,
                                 ConvertVideoType(dst_video_type));
}

int MirrorI420LeftRight(const uint8_t* src_frame,
                        uint8_t* dst_frame,
                        int width, int height) {
  const uint8_t* src_yplane = src_frame;
  const uint8_t* src_uplane = src_yplane + width * height;
  const uint8_t* src_vplane = src_uplane + (width * height / 4);
  uint8_t* dst_yplane = dst_frame;
  uint8_t* dst_uplane = dst_yplane + width * height;
  uint8_t* dst_vplane = dst_uplane + (width * height / 4);
  return libyuv::I420Mirror(src_yplane, width,
                            src_uplane, width / 2,
                            src_vplane, width / 2,
                            dst_yplane, width,
                            dst_uplane, width / 2,
                            dst_vplane, width / 2,
                            width, height);
}

int MirrorI420UpDown(const uint8_t* src_frame, uint8_t* dst_frame,
                     int width, int height) {
  const uint8_t* src_yplane = src_frame;
  const uint8_t* src_uplane = src_frame + width * height;
  const uint8_t* src_vplane = src_uplane + (width * height / 4);
  uint8_t* dst_yplane = dst_frame;
  uint8_t* dst_uplane = dst_frame + width * height;
  uint8_t* dst_vplane = dst_uplane + (width * height / 4);

  // Inserting negative height flips the frame.
  return libyuv::I420Copy(src_yplane, width,
                          src_uplane, width / 2,
                          src_vplane, width / 2,
                          dst_yplane, width,
                          dst_uplane, width / 2,
                          dst_vplane, width / 2,
                          width, -height);
}

int ConvertToI420AndMirrorUpDown(const uint8_t* src_frame,
                                 uint8_t* dst_frame,
                                 int src_width, int src_height,
                                 VideoType src_video_type) {
  if (src_video_type != kI420 && src_video_type != kYV12)
    return -1;
  // TODO(mikhal): Use a more general convert function - with negative height
  // Inserting negative height flips the frame.
  // Using I420Copy with a negative height.
  const uint8_t* src_yplane = src_frame;
  const uint8_t* src_uplane = src_frame + src_width * src_height;
  const uint8_t* src_vplane = src_uplane + (src_width * src_height / 4);
  uint8_t* dst_yplane = dst_frame;
  uint8_t* dst_uplane = dst_frame + src_width * src_height;
  uint8_t* dst_vplane = dst_uplane + (src_width * src_height / 4);
  if (src_video_type == kYV12) {
    // Switch U and V
    dst_vplane = dst_frame + src_width * src_height;
    dst_uplane = dst_vplane + (src_width * src_height / 4);
  }
  // Inserting negative height flips the frame.
  return libyuv::I420Copy(src_yplane, src_width,
                          src_uplane, src_width / 2,
                          src_vplane, src_width / 2,
                          dst_yplane, src_width,
                          dst_uplane, src_width / 2,
                          dst_vplane, src_width / 2,
                          src_width, -src_height);
}

// Compute PSNR for an I420 frame (all planes)
double I420PSNR(const uint8_t* ref_frame,
                const uint8_t* test_frame,
                int width, int height) {
  if (!ref_frame || !test_frame)
    return -1;
  else if (height < 0 || width < 0)
    return -1;
  const uint8_t* src_y_a = ref_frame;
  const uint8_t* src_u_a = src_y_a + width * height;
  const uint8_t* src_v_a = src_u_a + (width * height / 4);
  const uint8_t* src_y_b = test_frame;
  const uint8_t* src_u_b = src_y_b + width * height;
  const uint8_t* src_v_b = src_u_b + (width * height / 4);
  int stride_y = width;
  int stride_uv = (width + 1) / 2;
  double psnr = libyuv::I420Psnr(src_y_a, stride_y,
                                 src_u_a, stride_uv,
                                 src_v_a, stride_uv,
                                 src_y_b, stride_y,
                                 src_u_b, stride_uv,
                                 src_v_b, stride_uv,
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
  const uint8_t* src_y_a = ref_frame;
  const uint8_t* src_u_a = src_y_a + width * height;
  const uint8_t* src_v_a = src_u_a + (width * height / 4);
  const uint8_t* src_y_b = test_frame;
  const uint8_t* src_u_b = src_y_b + width * height;
  const uint8_t* src_v_b = src_u_b + (width * height / 4);
  int stride_y = width;
  int stride_uv = (width + 1) / 2;
  return libyuv::I420Ssim(src_y_a, stride_y,
                          src_u_a, stride_uv,
                          src_v_a, stride_uv,
                          src_y_b, stride_y,
                          src_u_b, stride_uv,
                          src_v_b, stride_uv,
                          width, height);
}

}  // namespace webrtc
