/*
 *  Copyright (c) 2011 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef WEBRTC_MODULE_VIDEO_PROCESSING_IMPL_H
#define WEBRTC_MODULE_VIDEO_PROCESSING_IMPL_H

#include "video_processing.h"
#include "brighten.h"
#include "brightness_detection.h"
#include "color_enhancement.h"
#include "deflickering.h"
#include "denoising.h"
#include "frame_preprocessor.h"

namespace webrtc {
class CriticalSectionWrapper;

class VideoProcessingModuleImpl : public VideoProcessingModule
{
public:

    VideoProcessingModuleImpl(WebRtc_Word32 id);

    virtual ~VideoProcessingModuleImpl();

    WebRtc_Word32 Id() const;

    virtual WebRtc_Word32 ChangeUniqueId(const WebRtc_Word32 id);

    virtual void Reset();

    virtual WebRtc_Word32 Deflickering(I420VideoFrame* frame,
                                       FrameStats* stats);

    virtual WebRtc_Word32 Denoising(I420VideoFrame* frame);

    virtual WebRtc_Word32 BrightnessDetection(const I420VideoFrame& frame,
                                              const FrameStats& stats);

    //Frame pre-processor functions

    //Enable temporal decimation
    virtual void EnableTemporalDecimation(bool enable);

    virtual void SetInputFrameResampleMode(VideoFrameResampling resamplingMode);

    //Enable content analysis
    virtual void EnableContentAnalysis(bool enable);

    //Set max frame rate
    virtual WebRtc_Word32 SetMaxFrameRate(WebRtc_UWord32 maxFrameRate);

    // Set Target Resolution: frame rate and dimension
    virtual WebRtc_Word32 SetTargetResolution(WebRtc_UWord32 width,
                                              WebRtc_UWord32 height,
                                              WebRtc_UWord32 frameRate);


    // Get decimated values: frame rate/dimension
    virtual WebRtc_UWord32 DecimatedFrameRate();
    virtual WebRtc_UWord32 DecimatedWidth() const;
    virtual WebRtc_UWord32 DecimatedHeight() const;

    // Preprocess:
    // Pre-process incoming frame: Sample when needed and compute content
    // metrics when enabled.
    // If no resampling takes place - processedFrame is set to NULL.
    virtual WebRtc_Word32 PreprocessFrame(const I420VideoFrame& frame,
                                          I420VideoFrame** processedFrame);
    virtual VideoContentMetrics* ContentMetrics() const;

private:
    WebRtc_Word32              _id;
    CriticalSectionWrapper&    _mutex;

    VPMDeflickering            _deflickering;
    VPMDenoising               _denoising;
    VPMBrightnessDetection     _brightnessDetection;
    VPMFramePreprocessor       _framePreProcessor;
};

} // namespace

#endif
