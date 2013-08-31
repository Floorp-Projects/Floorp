/*
 *  Copyright (c) 2012 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "webrtc/modules/video_processing/main/source/frame_preprocessor.h"
#include "webrtc/system_wrappers/interface/trace.h"

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
}

int32_t
VPMFramePreprocessor::ChangeUniqueId(const int32_t id)
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

    
int32_t
VPMFramePreprocessor::SetMaxFrameRate(uint32_t maxFrameRate)
{
    if (maxFrameRate == 0)
    {
        return VPM_PARAMETER_ERROR;
    }
    //Max allowed frame rate
    _maxFrameRate = maxFrameRate;

    return _vd->SetMaxFrameRate(maxFrameRate);
}
    

int32_t
VPMFramePreprocessor::SetTargetResolution(uint32_t width, uint32_t height, uint32_t frameRate)
{
    if ( (width == 0) || (height == 0) || (frameRate == 0))
    {
        return VPM_PARAMETER_ERROR;
    }
    int32_t retVal = 0;
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

uint32_t
VPMFramePreprocessor::DecimatedFrameRate()
{
    return _vd->DecimatedFrameRate();
}


uint32_t
VPMFramePreprocessor::DecimatedWidth() const
{
    return _spatialResampler->TargetWidth();
}


uint32_t
VPMFramePreprocessor::DecimatedHeight() const
{
    return _spatialResampler->TargetHeight();
}


int32_t
VPMFramePreprocessor::PreprocessFrame(const I420VideoFrame& frame,
                                      I420VideoFrame** processedFrame)
{
    if (frame.IsZeroSize())
    {
        return VPM_PARAMETER_ERROR;
    }

    _vd->UpdateIncomingFrameRate();

    if (_vd->DropFrame())
    {
        WEBRTC_TRACE(webrtc::kTraceStream, webrtc::kTraceVideo, _id,
                     "Drop frame due to frame rate");
        return 1;  // drop 1 frame
    }

    // Resizing incoming frame if needed. Otherwise, remains NULL.
    // We are not allowed to resample the input frame (must make a copy of it).
    *processedFrame = NULL;
    if (_spatialResampler->ApplyResample(frame.width(), frame.height()))  {
      int32_t ret = _spatialResampler->ResampleFrame(frame, &_resampledFrame);
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
            _contentMetrics = _ca->ComputeContentMetrics(_resampledFrame);
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
