/*
 *  Copyright (c) 2011 The WebRTC project authors. All Rights Reserved.
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
VPMSimpleSpatialResampler::ResampleFrame(const VideoFrame& inFrame,
                                         VideoFrame& outFrame)
{
  if (_resamplingMode == kNoRescaling)
     return outFrame.CopyFrame(inFrame);
  // Check if re-sampling is needed
  if ((inFrame.Width() == (WebRtc_UWord32)_targetWidth) &&
    (inFrame.Height() == (WebRtc_UWord32)_targetHeight))  {
    return outFrame.CopyFrame(inFrame);
  }

  // Setting scaler
  //TODO: Modify scaler types
  int retVal = 0;
  retVal = _scaler.Set(inFrame.Width(), inFrame.Height(),
                       _targetWidth, _targetHeight, kI420, kI420, kScaleBox);
  if (retVal < 0)
    return retVal;


  // Disabling cut/pad for now - only scaling.
  int requiredSize = (WebRtc_UWord32)(_targetWidth * _targetHeight * 3 >> 1);
  outFrame.VerifyAndAllocate(requiredSize);
  outFrame.SetTimeStamp(inFrame.TimeStamp());
  outFrame.SetWidth(_targetWidth);
  outFrame.SetHeight(_targetHeight);

  retVal = _scaler.Scale(inFrame.Buffer(), outFrame.Buffer(), requiredSize);
  outFrame.SetLength(requiredSize);
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
