/*
 *  Copyright (c) 2012 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */


#include "webrtc/modules/video_processing/main/source/video_processing_impl.h"
#include "webrtc/system_wrappers/interface/critical_section_wrapper.h"
#include "webrtc/system_wrappers/interface/trace.h"

#include <assert.h>

namespace webrtc {

namespace
{
    void
    SetSubSampling(VideoProcessingModule::FrameStats* stats,
                   const int32_t width,
                   const int32_t height)
    {
        if (width * height >= 640 * 480)
        {
            stats->subSamplWidth = 3;
            stats->subSamplHeight = 3;
        }
        else if (width * height >= 352 * 288)
        {
            stats->subSamplWidth = 2;
            stats->subSamplHeight = 2;
        }
        else if (width * height >= 176 * 144)
        {
            stats->subSamplWidth = 1;
            stats->subSamplHeight = 1;
        }
        else
        {
            stats->subSamplWidth = 0;
            stats->subSamplHeight = 0;
        }
    }
}

VideoProcessingModule*
VideoProcessingModule::Create(const int32_t id)
{

    return new VideoProcessingModuleImpl(id);
}

void
VideoProcessingModule::Destroy(VideoProcessingModule* module)
{
    if (module)
    {
        delete static_cast<VideoProcessingModuleImpl*>(module);
    }
}

int32_t
VideoProcessingModuleImpl::ChangeUniqueId(const int32_t id)
{
    CriticalSectionScoped mutex(&_mutex);
    _id = id;
    _brightnessDetection.ChangeUniqueId(id);
    _deflickering.ChangeUniqueId(id);
    _denoising.ChangeUniqueId(id);
    _framePreProcessor.ChangeUniqueId(id);
    return VPM_OK;
}

int32_t
VideoProcessingModuleImpl::Id() const
{
    CriticalSectionScoped mutex(&_mutex);
    return _id;
}

VideoProcessingModuleImpl::VideoProcessingModuleImpl(const int32_t id) :
    _id(id),
    _mutex(*CriticalSectionWrapper::CreateCriticalSection())
{
    _brightnessDetection.ChangeUniqueId(id);
    _deflickering.ChangeUniqueId(id);
    _denoising.ChangeUniqueId(id);
    _framePreProcessor.ChangeUniqueId(id);
    WEBRTC_TRACE(webrtc::kTraceMemory, webrtc::kTraceVideoPreocessing, _id,
                 "Created");
}


VideoProcessingModuleImpl::~VideoProcessingModuleImpl()
{
    WEBRTC_TRACE(webrtc::kTraceMemory, webrtc::kTraceVideoPreocessing, _id,
                 "Destroyed");
    
    delete &_mutex;
}

void
VideoProcessingModuleImpl::Reset()
{
    CriticalSectionScoped mutex(&_mutex);
    _deflickering.Reset();
    _denoising.Reset();
    _brightnessDetection.Reset();
    _framePreProcessor.Reset();

}

int32_t
VideoProcessingModule::GetFrameStats(FrameStats* stats,
                                     const I420VideoFrame& frame)
{
    if (frame.IsZeroSize())
    {
        WEBRTC_TRACE(webrtc::kTraceError, webrtc::kTraceVideoPreocessing, -1,
                     "zero size frame");
        return VPM_PARAMETER_ERROR;
    }
    
    int width = frame.width();
    int height = frame.height();

    ClearFrameStats(stats); // The histogram needs to be zeroed out.
    SetSubSampling(stats, width, height);

    const uint8_t* buffer = frame.buffer(kYPlane);
    // Compute histogram and sum of frame
    for (int i = 0; i < height; i += (1 << stats->subSamplHeight))
    {
        int k = i * width;
        for (int j = 0; j < width; j += (1 << stats->subSamplWidth))
        { 
            stats->hist[buffer[k + j]]++;
            stats->sum += buffer[k + j];
        }
    }

    stats->numPixels = (width * height) / ((1 << stats->subSamplWidth) *
        (1 << stats->subSamplHeight));
    assert(stats->numPixels > 0);

    // Compute mean value of frame
    stats->mean = stats->sum / stats->numPixels;
    
    return VPM_OK;
}

bool
VideoProcessingModule::ValidFrameStats(const FrameStats& stats)
{
    if (stats.numPixels == 0)
    {
        return false;
    }

    return true;
}

void
VideoProcessingModule::ClearFrameStats(FrameStats* stats)
{
    stats->mean = 0;
    stats->sum = 0;
    stats->numPixels = 0;
    stats->subSamplWidth = 0;
    stats->subSamplHeight = 0;
    memset(stats->hist, 0, sizeof(stats->hist));
}

int32_t
VideoProcessingModule::ColorEnhancement(I420VideoFrame* frame)
{
    return VideoProcessing::ColorEnhancement(frame);
}

int32_t
VideoProcessingModule::Brighten(I420VideoFrame* frame, int delta)
{
    return VideoProcessing::Brighten(frame, delta);
}

int32_t
VideoProcessingModuleImpl::Deflickering(I420VideoFrame* frame,
                                        FrameStats* stats)
{
    CriticalSectionScoped mutex(&_mutex);
    return _deflickering.ProcessFrame(frame, stats);
}

int32_t
VideoProcessingModuleImpl::Denoising(I420VideoFrame* frame)
{
    CriticalSectionScoped mutex(&_mutex);
    return _denoising.ProcessFrame(frame);
}

int32_t
VideoProcessingModuleImpl::BrightnessDetection(const I420VideoFrame& frame,
                                               const FrameStats& stats)
{
    CriticalSectionScoped mutex(&_mutex);
    return _brightnessDetection.ProcessFrame(frame, stats);
}


void 
VideoProcessingModuleImpl::EnableTemporalDecimation(bool enable)
{
    CriticalSectionScoped mutex(&_mutex);
    _framePreProcessor.EnableTemporalDecimation(enable);
}


void 
VideoProcessingModuleImpl::SetInputFrameResampleMode(VideoFrameResampling
                                                     resamplingMode)
{
    CriticalSectionScoped cs(&_mutex);
    _framePreProcessor.SetInputFrameResampleMode(resamplingMode);
}

int32_t
VideoProcessingModuleImpl::SetMaxFrameRate(uint32_t maxFrameRate)
{
    CriticalSectionScoped cs(&_mutex);
    return _framePreProcessor.SetMaxFrameRate(maxFrameRate);

}

int32_t
VideoProcessingModuleImpl::SetTargetResolution(uint32_t width,
                                               uint32_t height,
                                               uint32_t frameRate)
{
    CriticalSectionScoped cs(&_mutex);
    return _framePreProcessor.SetTargetResolution(width, height, frameRate);
}


uint32_t
VideoProcessingModuleImpl::DecimatedFrameRate()
{
    CriticalSectionScoped cs(&_mutex);
    return  _framePreProcessor.DecimatedFrameRate();
}


uint32_t
VideoProcessingModuleImpl::DecimatedWidth() const
{
    CriticalSectionScoped cs(&_mutex);
    return _framePreProcessor.DecimatedWidth();
}

uint32_t
VideoProcessingModuleImpl::DecimatedHeight() const
{
    CriticalSectionScoped cs(&_mutex);
    return _framePreProcessor.DecimatedHeight();
}

int32_t
VideoProcessingModuleImpl::PreprocessFrame(const I420VideoFrame& frame,
                                           I420VideoFrame **processedFrame)
{
    CriticalSectionScoped mutex(&_mutex);
    return _framePreProcessor.PreprocessFrame(frame, processedFrame);
}

VideoContentMetrics*
VideoProcessingModuleImpl::ContentMetrics() const
{
    CriticalSectionScoped mutex(&_mutex);
    return _framePreProcessor.ContentMetrics();
}


void
VideoProcessingModuleImpl::EnableContentAnalysis(bool enable)
{
    CriticalSectionScoped mutex(&_mutex);
    _framePreProcessor.EnableContentAnalysis(enable);
}

}  // namespace
