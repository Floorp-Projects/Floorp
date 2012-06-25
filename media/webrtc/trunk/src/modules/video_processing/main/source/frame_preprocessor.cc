/*
 *  Copyright (c) 2012 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "frame_preprocessor.h"
#include "trace.h"

namespace webrtc {

VPMFramePreprocessor::VPMFramePreprocessor():
_id(0),
_contentMetrics(NULL),
_maxFrameRate(0),
_resampledFrame(),
_enableCA(false),
_frameCnt(0)
{
    _spatialResampler = new VPMSimpleSpatialResampler();
    _ca = new VPMContentAnalysis(true);
    _vd = new VPMVideoDecimator();
}

VPMFramePreprocessor::~VPMFramePreprocessor()
{
    Reset();
    delete _spatialResampler;
    delete _ca;
    delete _vd;
    _resampledFrame.Free(); // is this needed?
}

WebRtc_Word32
VPMFramePreprocessor::ChangeUniqueId(const WebRtc_Word32 id)
{
    _id = id;
    return VPM_OK;
}

void 
VPMFramePreprocessor::Reset()
{
    _ca->Release();
    _vd->Reset();
    _contentMetrics = NULL;
    _spatialResampler->Reset();
    _enableCA = false;
    _frameCnt = 0;
}
	
    
void 
VPMFramePreprocessor::EnableTemporalDecimation(bool enable)
{
    _vd->EnableTemporalDecimation(enable);
}
void
VPMFramePreprocessor::EnableContentAnalysis(bool enable)
{
    _enableCA = enable;
}

void 
VPMFramePreprocessor::SetInputFrameResampleMode(VideoFrameResampling resamplingMode)
{
    _spatialResampler->SetInputFrameResampleMode(resamplingMode);
}

    
WebRtc_Word32
VPMFramePreprocessor::SetMaxFrameRate(WebRtc_UWord32 maxFrameRate)
{
    if (maxFrameRate == 0)
    {
        return VPM_PARAMETER_ERROR;
    }
    //Max allowed frame rate
    _maxFrameRate = maxFrameRate;

    return _vd->SetMaxFrameRate(maxFrameRate);
}
    

WebRtc_Word32
VPMFramePreprocessor::SetTargetResolution(WebRtc_UWord32 width, WebRtc_UWord32 height, WebRtc_UWord32 frameRate)
{
    if ( (width == 0) || (height == 0) || (frameRate == 0))
    {
        return VPM_PARAMETER_ERROR;
    }
    WebRtc_Word32 retVal = 0;
    retVal = _spatialResampler->SetTargetFrameSize(width, height);
    if (retVal < 0)
    {
        return retVal;
    }
    retVal = _vd->SetTargetFrameRate(frameRate);
    if (retVal < 0)
    {
        return retVal;
    }

	  return VPM_OK;
}

void 
VPMFramePreprocessor::UpdateIncomingFrameRate()
{
    _vd->UpdateIncomingFrameRate();
}

WebRtc_UWord32
VPMFramePreprocessor::DecimatedFrameRate()
{
    return _vd->DecimatedFrameRate();
}


WebRtc_UWord32
VPMFramePreprocessor::DecimatedWidth() const
{
    return _spatialResampler->TargetWidth();
}


WebRtc_UWord32
VPMFramePreprocessor::DecimatedHeight() const
{
    return _spatialResampler->TargetHeight();
}


WebRtc_Word32
VPMFramePreprocessor::PreprocessFrame(const VideoFrame* frame, VideoFrame** processedFrame)
{
    if (frame == NULL || frame->Height() == 0 || frame->Width() == 0)
    {
        return VPM_PARAMETER_ERROR;
    }

    _vd->UpdateIncomingFrameRate();

    if (_vd->DropFrame())
    {
        WEBRTC_TRACE(webrtc::kTraceStream, webrtc::kTraceVideo, _id, "Drop frame due to frame rate");
        return 1;  // drop 1 frame
    }

    // Resizing incoming frame if needed.
    // Note that we must make a copy of it.
    // We are not allowed to resample the input frame.
    *processedFrame = NULL;
    if (_spatialResampler->ApplyResample(frame->Width(), frame->Height()))  {
      WebRtc_Word32 ret = _spatialResampler->ResampleFrame(*frame, _resampledFrame);
      if (ret != VPM_OK)
        return ret;
      *processedFrame = &_resampledFrame;
    }

    // Perform content analysis on the frame to be encoded.
    if (_enableCA)
    {
        // Compute new metrics every |kSkipFramesCA| frames, starting with
        // the first frame.
        if (_frameCnt % kSkipFrameCA == 0) {
          if (*processedFrame == NULL)  {
            _contentMetrics = _ca->ComputeContentMetrics(frame);
          } else {
            _contentMetrics = _ca->ComputeContentMetrics(&_resampledFrame);
          }
        }
        ++_frameCnt;
    }
    return VPM_OK;
}


VideoContentMetrics*
VPMFramePreprocessor::ContentMetrics() const
{
    return _contentMetrics;
}

} //namespace
