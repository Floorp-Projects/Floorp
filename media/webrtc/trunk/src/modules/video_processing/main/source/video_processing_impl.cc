/*
 *  Copyright (c) 2012 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "video_processing_impl.h"
#include "critical_section_wrapper.h"
#include "trace.h"

#include <cassert>

namespace webrtc {

namespace
{
    void
    SetSubSampling(VideoProcessingModule::FrameStats& stats,
                   const WebRtc_Word32 width,
                   const WebRtc_Word32 height)
    {
        if (width * height >= 640 * 480)
        {
            stats.subSamplWidth = 3; 
            stats.subSamplHeight = 3;
        }
        else if (width * height >= 352 * 288)
        {
            stats.subSamplWidth = 2; 
            stats.subSamplHeight = 2;
        }
        else if (width * height >= 176 * 144)
        {
            stats.subSamplWidth = 1; 
            stats.subSamplHeight = 1;
        }
        else
        {
            stats.subSamplWidth = 0; 
            stats.subSamplHeight = 0;
        }
    }
}

VideoProcessingModule*
VideoProcessingModule::Create(const WebRtc_Word32 id)
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

WebRtc_Word32
VideoProcessingModuleImpl::ChangeUniqueId(const WebRtc_Word32 id)
{
    CriticalSectionScoped mutex(_mutex);
    _id = id;
    _brightnessDetection.ChangeUniqueId(id);
    _deflickering.ChangeUniqueId(id);
    _denoising.ChangeUniqueId(id);
    _framePreProcessor.ChangeUniqueId(id);
    return VPM_OK;
}

WebRtc_Word32
VideoProcessingModuleImpl::Id() const
{
    CriticalSectionScoped mutex(_mutex);
    return _id;
}

VideoProcessingModuleImpl::VideoProcessingModuleImpl(const WebRtc_Word32 id) :
    _id(id),
    _mutex(*CriticalSectionWrapper::CreateCriticalSection())
{
    _brightnessDetection.ChangeUniqueId(id);
    _deflickering.ChangeUniqueId(id);
    _denoising.ChangeUniqueId(id);
    _framePreProcessor.ChangeUniqueId(id);
    WEBRTC_TRACE(webrtc::kTraceMemory, webrtc::kTraceVideoPreocessing, _id, "Created");
}


VideoProcessingModuleImpl::~VideoProcessingModuleImpl()
{
    WEBRTC_TRACE(webrtc::kTraceMemory, webrtc::kTraceVideoPreocessing, _id, "Destroyed");
    
    delete &_mutex;
}

void
VideoProcessingModuleImpl::Reset()
{
    CriticalSectionScoped mutex(_mutex);
    _deflickering.Reset();
    _denoising.Reset();
    _brightnessDetection.Reset();
    _framePreProcessor.Reset();

}

WebRtc_Word32
VideoProcessingModule::GetFrameStats(FrameStats& stats,
                                         const VideoFrame& frame)
{
    return GetFrameStats(stats, frame.Buffer(), frame.Width(), frame.Height());
}

WebRtc_Word32
VideoProcessingModule::GetFrameStats(FrameStats& stats,
                                         const WebRtc_UWord8* frame,
                                         const WebRtc_UWord32 width,
                                         const WebRtc_UWord32 height)
{
    if (frame == NULL)
    {
        WEBRTC_TRACE(webrtc::kTraceError, webrtc::kTraceVideoPreocessing, -1, "Null frame pointer");
        return VPM_PARAMETER_ERROR;
    }
    
    if (width == 0 || height == 0)
    {
        WEBRTC_TRACE(webrtc::kTraceError, webrtc::kTraceVideoPreocessing, -1, "Invalid frame size");
        return VPM_PARAMETER_ERROR;
    }

    ClearFrameStats(stats); // The histogram needs to be zeroed out.
    SetSubSampling(stats, width, height);

    // Compute histogram and sum of frame
    for (WebRtc_UWord32 i = 0; i < height; i += (1 << stats.subSamplHeight))
    {
        WebRtc_Word32 k = i * width;
        for (WebRtc_UWord32 j = 0; j < width; j += (1 << stats.subSamplWidth))
        { 
            stats.hist[frame[k + j]]++;
            stats.sum += frame[k + j];
        }
    }

    stats.numPixels = (width * height) / ((1 << stats.subSamplWidth) * (1 << stats.subSamplHeight));
    assert(stats.numPixels > 0);

    // Compute mean value of frame
    stats.mean = stats.sum / stats.numPixels;
    
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
VideoProcessingModule::ClearFrameStats(FrameStats& stats)
{
    stats.mean = 0;
    stats.sum = 0;
    stats.numPixels = 0;
    stats.subSamplWidth = 0;
    stats.subSamplHeight = 0;
    memset(stats.hist, 0, sizeof(stats.hist));
}

WebRtc_Word32
VideoProcessingModule::ColorEnhancement(VideoFrame& frame)
{
    return ColorEnhancement(frame.Buffer(), frame.Width(), frame.Height());
}

WebRtc_Word32
VideoProcessingModule::ColorEnhancement(WebRtc_UWord8* frame,
                                            const WebRtc_UWord32 width,
                                            const WebRtc_UWord32 height)
{
    return VideoProcessing::ColorEnhancement(frame, width, height);
}

WebRtc_Word32
VideoProcessingModule::Brighten(VideoFrame& frame, int delta)
{
    return Brighten(frame.Buffer(), frame.Width(), frame.Height(), delta);
}

WebRtc_Word32
VideoProcessingModule::Brighten(WebRtc_UWord8* frame,
                                    int width,
                                    int height,
                                    int delta)
{
    return VideoProcessing::Brighten(frame, width, height, delta);
}

WebRtc_Word32
VideoProcessingModuleImpl::Deflickering(VideoFrame& frame,
                                            FrameStats& stats)
{
    return Deflickering(frame.Buffer(), frame.Width(), frame.Height(), 
        frame.TimeStamp(), stats);
}

WebRtc_Word32
VideoProcessingModuleImpl::Deflickering(WebRtc_UWord8* frame,
                                            const WebRtc_UWord32 width,
                                            const WebRtc_UWord32 height,
                                            const WebRtc_UWord32 timestamp,
                                            FrameStats& stats)
{
    CriticalSectionScoped mutex(_mutex);
    return _deflickering.ProcessFrame(frame, width, height, timestamp, stats);
}

WebRtc_Word32
VideoProcessingModuleImpl::Denoising(VideoFrame& frame)
{
    return Denoising(frame.Buffer(), frame.Width(), frame.Height());
}

WebRtc_Word32
VideoProcessingModuleImpl::Denoising(WebRtc_UWord8* frame,
                                         const WebRtc_UWord32 width,
                                         const WebRtc_UWord32 height)
{
    CriticalSectionScoped mutex(_mutex);
    return _denoising.ProcessFrame(frame, width, height);
}

WebRtc_Word32
VideoProcessingModuleImpl::BrightnessDetection(const VideoFrame& frame,
                                                   const FrameStats& stats)
{
    return BrightnessDetection(frame.Buffer(), frame.Width(), frame.Height(), stats);
}

WebRtc_Word32
VideoProcessingModuleImpl::BrightnessDetection(const WebRtc_UWord8* frame,
                                                   const WebRtc_UWord32 width,
                                                   const WebRtc_UWord32 height,
                                                   const FrameStats& stats)
{
    CriticalSectionScoped mutex(_mutex);
    return _brightnessDetection.ProcessFrame(frame, width, height, stats);
}


void 
VideoProcessingModuleImpl::EnableTemporalDecimation(bool enable)
{
    CriticalSectionScoped mutex(_mutex);
    _framePreProcessor.EnableTemporalDecimation(enable);
}


void 
VideoProcessingModuleImpl::SetInputFrameResampleMode(VideoFrameResampling resamplingMode)
{
    CriticalSectionScoped cs(_mutex);
    _framePreProcessor.SetInputFrameResampleMode(resamplingMode);
}

WebRtc_Word32
VideoProcessingModuleImpl::SetMaxFrameRate(WebRtc_UWord32 maxFrameRate)
{
    CriticalSectionScoped cs(_mutex);
    return _framePreProcessor.SetMaxFrameRate(maxFrameRate);

}

WebRtc_Word32
VideoProcessingModuleImpl::SetTargetResolution(WebRtc_UWord32 width, WebRtc_UWord32 height, WebRtc_UWord32 frameRate)
{
    CriticalSectionScoped cs(_mutex);
    return _framePreProcessor.SetTargetResolution(width, height, frameRate);
}


WebRtc_UWord32
VideoProcessingModuleImpl::DecimatedFrameRate()
{
    CriticalSectionScoped cs(_mutex);
    return  _framePreProcessor.DecimatedFrameRate();
}


WebRtc_UWord32
VideoProcessingModuleImpl::DecimatedWidth() const
{
    CriticalSectionScoped cs(_mutex);
    return _framePreProcessor.DecimatedWidth();
}

WebRtc_UWord32
VideoProcessingModuleImpl::DecimatedHeight() const
{
    CriticalSectionScoped cs(_mutex);
    return _framePreProcessor.DecimatedHeight();
}

WebRtc_Word32
VideoProcessingModuleImpl::PreprocessFrame(const VideoFrame *frame, VideoFrame **processedFrame)
{
    CriticalSectionScoped mutex(_mutex);
    return _framePreProcessor.PreprocessFrame(frame, processedFrame);
}

VideoContentMetrics*
VideoProcessingModuleImpl::ContentMetrics() const
{
    CriticalSectionScoped mutex(_mutex);
    return _framePreProcessor.ContentMetrics();
}


void
VideoProcessingModuleImpl::EnableContentAnalysis(bool enable)
{
    CriticalSectionScoped mutex(_mutex);
    _framePreProcessor.EnableContentAnalysis(enable);
}

} //namespace
