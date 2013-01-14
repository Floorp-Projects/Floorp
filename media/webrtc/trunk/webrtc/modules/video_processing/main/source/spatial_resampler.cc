/*
 *  Copyright (c) 2012 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "spatial_resampler.h"


namespace webrtc {

VPMSimpleSpatialResampler::VPMSimpleSpatialResampler()
:
_resamplingMode(kFastRescaling),
_targetWidth(0),
_targetHeight(0),
_scaler()
{
}

VPMSimpleSpatialResampler::~VPMSimpleSpatialResampler()
{
  //
}


WebRtc_Word32
VPMSimpleSpatialResampler::SetTargetFrameSize(WebRtc_Word32 width,
                                              WebRtc_Word32 height)
{
  if (_resamplingMode == kNoRescaling)  {
    return VPM_OK;
  }

  if (width < 1 || height < 1)  {
    return VPM_PARAMETER_ERROR;
  }

  _targetWidth = width;
  _targetHeight = height;

  return VPM_OK;
}

void
VPMSimpleSpatialResampler::SetInputFrameResampleMode(VideoFrameResampling
                                                     resamplingMode)
{
  _resamplingMode = resamplingMode;
}

void
VPMSimpleSpatialResampler::Reset()
{
  _resamplingMode = kFastRescaling;
  _targetWidth = 0;
  _targetHeight = 0;
}

WebRtc_Word32
VPMSimpleSpatialResampler::ResampleFrame(const I420VideoFrame& inFrame,
                                         I420VideoFrame* outFrame)
{
  // Don't copy if frame remains as is.
  if (_resamplingMode == kNoRescaling)
     return VPM_OK;
  // Check if re-sampling is needed
  else if ((inFrame.width() == _targetWidth) &&
    (inFrame.height() == _targetHeight))  {
    return VPM_OK;
  }

  // Setting scaler
  // TODO(mikhal/marpan): Should we allow for setting the filter mode in
  // _scale.Set() with |_resamplingMode|?
  int retVal = 0;
  retVal = _scaler.Set(inFrame.width(), inFrame.height(),
                       _targetWidth, _targetHeight, kI420, kI420, kScaleBox);
  if (retVal < 0)
    return retVal;

  retVal = _scaler.Scale(inFrame, outFrame);

  // Setting time parameters to the output frame.
  // Timestamp will be reset in Scale call above, so we should set it after.
  outFrame->set_timestamp(inFrame.timestamp());
  outFrame->set_render_time_ms(inFrame.render_time_ms());

  if (retVal == 0)
    return VPM_OK;
  else
    return VPM_SCALE_ERROR;
}

WebRtc_Word32
VPMSimpleSpatialResampler::TargetHeight()
{
  return _targetHeight;
}

WebRtc_Word32
VPMSimpleSpatialResampler::TargetWidth()
{
  return _targetWidth;
}

bool
VPMSimpleSpatialResampler::ApplyResample(WebRtc_Word32 width,
                                         WebRtc_Word32 height)
{
  if ((width == _targetWidth && height == _targetHeight) ||
       _resamplingMode == kNoRescaling)
    return false;
  else
    return true;
}

} //namespace
