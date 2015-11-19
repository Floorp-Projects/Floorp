/*
 *  Copyright (c) 2012 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

/*
 * WebRTC's wrapper to libyuv.
 */

#ifndef WEBRTC_COMMON_VIDEO_LIBYUV_INCLUDE_WEBRTC_LIBYUV_H_
#define WEBRTC_COMMON_VIDEO_LIBYUV_INCLUDE_WEBRTC_LIBYUV_H_

#include <stdio.h>

#include "webrtc/common_types.h"  // RawVideoTypes.
#include "webrtc/common_video/rotation.h"
#include "webrtc/common_video/interface/i420_video_frame.h"
#include "webrtc/typedefs.h"

namespace webrtc {

// Supported video types.
enum VideoType {
  kUnknown,
  kI420,
  kIYUV,
  kRGB24,
  kABGR,
  kARGB,
  kARGB4444,
  kRGB565,
  kARGB1555,
  kYUY2,
  kYV12,
  kUYVY,
  kMJPG,
  kNV21,
  kNV12,
  kBGRA,
};

// This is the max PSNR value our algorithms can return.
const double kPerfectPSNR = 48.0f;

// Conversion between the RawVideoType and the LibYuv videoType.
// TODO(wu): Consolidate types into one type throughout WebRtc.
VideoType RawVideoTypeToCommonVideoVideoType(RawVideoType type);

// Align integer values.
// Input:
//   - value     : Input value to be aligned.
//   - alignment : Alignment basis (power of 2).
// Return value: An aligned form of the input value.
int AlignInt(int value, int alignment);

// Align stride values for I420 Video frames.
// Input:
//   - width    : Image width.
//   - stride_y : Pointer to the stride of the y plane.
//   - stride_uv: Pointer to the stride of the u and v planes (setting identical
//                values for both).
// Setting 16 byte alignment.
void Calc16ByteAlignedStride(int width, int* stride_y, int* stride_uv);

// Calculate the required buffer size.
// Input:
//   - type         :The type of the designated video frame.
//   - width        :frame width in pixels.
//   - height       :frame height in pixels.
// Return value:    :The required size in bytes to accommodate the specified
//                   video frame.
size_t CalcBufferSize(VideoType type, int width, int height);

// TODO(mikhal): Add unit test for these two functions and determine location.
// Print I420VideoFrame to file
// Input:
//    - frame       : Reference to video frame.
//    - file        : pointer to file object. It is assumed that the file is
//                    already open for writing.
// Return value: 0 if OK, < 0 otherwise.
int PrintI420VideoFrame(const I420VideoFrame& frame, FILE* file);

// Extract buffer from I420VideoFrame (consecutive planes, no stride)
// Input:
//   - frame       : Reference to video frame.
//   - size        : pointer to the size of the allocated buffer. If size is
//                   insufficient, an error will be returned.
//   - buffer      : Pointer to buffer
// Return value: length of buffer if OK, < 0 otherwise.
int ExtractBuffer(const I420VideoFrame& input_frame,
                  size_t size, uint8_t* buffer);
// Convert To I420
// Input:
//   - src_video_type   : Type of input video.
//   - src_frame        : Pointer to a source frame.
//   - crop_x/crop_y    : Starting positions for cropping (0 for no crop).
//   - src_width        : src width in pixels.
//   - src_height       : src height in pixels.
//   - sample_size      : Required only for the parsing of MJPG (set to 0 else).
//   - rotate           : Rotation mode of output image.
// Output:
//   - dst_frame        : Reference to a destination frame.
// Return value: 0 if OK, < 0 otherwise.

int ConvertToI420(VideoType src_video_type,
                  const uint8_t* src_frame,
                  int crop_x,
                  int crop_y,
                  int src_width,
                  int src_height,
                  size_t sample_size,
                  VideoRotation rotation,
                  I420VideoFrame* dst_frame);

// Convert From I420
// Input:
//   - src_frame        : Reference to a source frame.
//   - dst_video_type   : Type of output video.
//   - dst_sample_size  : Required only for the parsing of MJPG.
//   - dst_frame        : Pointer to a destination frame.
// Return value: 0 if OK, < 0 otherwise.
// It is assumed that source and destination have equal height.
int ConvertFromI420(const I420VideoFrame& src_frame,
                    VideoType dst_video_type, int dst_sample_size,
                    uint8_t* dst_frame);
// ConvertFrom YV12.
// Interface - same as above.
int ConvertFromYV12(const I420VideoFrame& src_frame,
                    VideoType dst_video_type, int dst_sample_size,
                    uint8_t* dst_frame);

// The following list describes designated conversion functions which
// are not covered by the previous general functions.
// Input and output descriptions mostly match the above descriptions, and are
// therefore omitted.
int ConvertRGB24ToARGB(const uint8_t* src_frame,
                       uint8_t* dst_frame,
                       int width, int height,
                       int dst_stride);
int ConvertNV12ToRGB565(const uint8_t* src_frame,
                        uint8_t* dst_frame,
                        int width, int height);

// Compute PSNR for an I420 frame (all planes).
// Returns the PSNR in decibel, to a maximum of kInfinitePSNR.
double I420PSNR(const I420VideoFrame* ref_frame,
                const I420VideoFrame* test_frame);
// Compute SSIM for an I420 frame (all planes).
double I420SSIM(const I420VideoFrame* ref_frame,
                const I420VideoFrame* test_frame);
}

#endif  // WEBRTC_COMMON_VIDEO_LIBYUV_INCLUDE_WEBRTC_LIBYUV_H_
