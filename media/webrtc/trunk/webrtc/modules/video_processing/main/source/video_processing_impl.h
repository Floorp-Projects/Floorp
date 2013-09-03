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

#include "webrtc/modules/video_processing/main/interface/video_processing.h"
#include "webrtc/modules/video_processing/main/source/brighten.h"
#include "webrtc/modules/video_processing/main/source/brightness_detection.h"
#include "webrtc/modules/video_processing/main/source/color_enhancement.h"
#include "webrtc/modules/video_processing/main/source/deflickering.h"
#include "webrtc/modules/video_processing/main/source/denoising.h"
#include "webrtc/modules/video_processing/main/source/frame_preprocessor.h"

namespace webrtc {
class CriticalSectionWrapper;

class VideoProcessingModuleImpl : public VideoProcessingModule
{
public:

    VideoProcessingModuleImpl(int32_t id);

    virtual ~VideoProcessingModuleImpl();

    int32_t Id() const;

    virtual int32_t ChangeUniqueId(const int32_t id);

    virtual void Reset();

    virtual int32_t Deflickering(I420VideoFrame* frame, FrameStats* stats);

    virtual int32_t Denoising(I420VideoFrame* frame);

    virtual int32_t BrightnessDetection(const I420VideoFrame& frame,
                                        const FrameStats& stats);

    //Frame pre-processor functions

    //Enable temporal decimation
    virtual void EnableTemporalDecimation(bool enable);

    virtual void SetInputFrameResampleMode(VideoFrameResampling resamplingMode);

    //Enable content analysis
    virtual void EnableContentAnalysis(bool enable);

    //Set max frame rate
    virtual int32_t SetMaxFrameRate(uint32_t maxFrameRate);

    // Set Target Resolution: frame rate and dimension
    virtual int32_t SetTargetResolution(uint32_t width,
                                        uint32_t height,
                                        uint32_t frameRate);


    // Get decimated values: frame rate/dimension
    virtual uint32_t DecimatedFrameRate();
    virtual uint32_t DecimatedWidth() const;
    virtual uint32_t DecimatedHeight() const;

    // Preprocess:
    // Pre-process incoming frame: Sample when needed and compute content
    // metrics when enabled.
    // If no resampling takes place - processedFrame is set to NULL.
    virtual int32_t PreprocessFrame(const I420VideoFrame& frame,
                                    I420VideoFrame** processedFrame);
    virtual VideoContentMetrics* ContentMetrics() const;

private:
    int32_t              _id;
    CriticalSectionWrapper&    _mutex;

    VPMDeflickering            _deflickering;
    VPMDenoising               _denoising;
    VPMBrightnessDetection     _brightnessDetection;
    VPMFramePreprocessor       _framePreProcessor;
};

} // namespace

#endif
