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
 * Interface to the LibYuv scaling functionality
 */

#ifndef WEBRTC_COMMON_VIDEO_LIBYUV_INCLUDE_SCALER_H_
#define WEBRTC_COMMON_VIDEO_LIBYUV_INCLUDE_SCALER_H_

#include "webrtc/common_video/interface/i420_video_frame.h"
#include "webrtc/common_video/libyuv/include/webrtc_libyuv.h"
#include "webrtc/typedefs.h"

namespace webrtc {

// Supported scaling types
enum ScaleMethod {
  kScalePoint,  // no interpolation
  kScaleBilinear,
  kScaleBox
};

class Scaler {
 public:
  Scaler();
  ~Scaler();

  // Set interpolation properties:
  //
  // Return value: 0 - OK
  //              -1 - parameter error
  int Set(int src_width, int src_height,
          int dst_width, int dst_height,
          VideoType src_video_type, VideoType dst_video_type,
          ScaleMethod method);

  // Scale frame
  // Memory is allocated by user. If dst_frame is not of sufficient size,
  // the frame will be reallocated to the appropriate size.
  // Return value: 0 - OK,
  //               -1 - parameter error
  //               -2 - scaler not set
  int Scale(const I420VideoFrame& src_frame,
            I420VideoFrame* dst_frame);

 private:
  // Determine if the VideoTypes are currently supported.
  bool SupportedVideoType(VideoType src_video_type,
                          VideoType dst_video_type);

  ScaleMethod   method_;
  int           src_width_;
  int           src_height_;
  int           dst_width_;
  int           dst_height_;
  bool          set_;
};

}  // namespace webrtc

#endif  // WEBRTC_COMMON_VIDEO_LIBYUV_INCLUDE_SCALER_H_
