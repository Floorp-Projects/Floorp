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
 * spatial_resampler.h
 */

#ifndef VPM_SPATIAL_RESAMPLER_H
#define VPM_SPATIAL_RESAMPLER_H

#include "webrtc/typedefs.h"

#include "webrtc/modules/interface/module_common_types.h"
#include "webrtc/modules/video_processing/main/interface/video_processing_defines.h"

#include "webrtc/common_video/libyuv/include/scaler.h"
#include "webrtc/common_video/libyuv/include/webrtc_libyuv.h"

namespace webrtc {

class VPMSpatialResampler
{
public:
  virtual ~VPMSpatialResampler() {};
  virtual int32_t SetTargetFrameSize(int32_t width, int32_t height) = 0;
  virtual void SetInputFrameResampleMode(VideoFrameResampling
                                         resamplingMode) = 0;
  virtual void Reset() = 0;
  virtual int32_t ResampleFrame(const I420VideoFrame& inFrame,
                                I420VideoFrame* outFrame) = 0;
  virtual int32_t TargetWidth() = 0;
  virtual int32_t TargetHeight() = 0;
  virtual bool ApplyResample(int32_t width, int32_t height) = 0;
};

class VPMSimpleSpatialResampler : public VPMSpatialResampler
{
public:
  VPMSimpleSpatialResampler();
  ~VPMSimpleSpatialResampler();
  virtual int32_t SetTargetFrameSize(int32_t width, int32_t height);
  virtual void SetInputFrameResampleMode(VideoFrameResampling resamplingMode);
  virtual void Reset();
  virtual int32_t ResampleFrame(const I420VideoFrame& inFrame,
                                I420VideoFrame* outFrame);
  virtual int32_t TargetWidth();
  virtual int32_t TargetHeight();
  virtual bool ApplyResample(int32_t width, int32_t height);

private:

  VideoFrameResampling        _resamplingMode;
  int32_t                     _targetWidth;
  int32_t                     _targetHeight;
  Scaler                      _scaler;
};

}  // namespace

#endif
