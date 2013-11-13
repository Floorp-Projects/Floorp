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
 * frame_preprocessor.h
 */
#ifndef VPM_FRAME_PREPROCESSOR_H
#define VPM_FRAME_PREPROCESSOR_H

#include "webrtc/modules/video_processing/main/interface/video_processing.h"
#include "webrtc/modules/video_processing/main/source/content_analysis.h"
#include "webrtc/modules/video_processing/main/source/spatial_resampler.h"
#include "webrtc/modules/video_processing/main/source/video_decimator.h"
#include "webrtc/typedefs.h"

namespace webrtc {


class VPMFramePreprocessor
{
public:

    VPMFramePreprocessor();
    ~VPMFramePreprocessor();

    int32_t ChangeUniqueId(const int32_t id);

    void Reset();

    // Enable temporal decimation
    void EnableTemporalDecimation(bool enable);

    void SetInputFrameResampleMode(VideoFrameResampling resamplingMode);

    //Enable content analysis
    void EnableContentAnalysis(bool enable);

    //Set max frame rate
    int32_t SetMaxFrameRate(uint32_t maxFrameRate);

    //Set target resolution: frame rate and dimension
    int32_t SetTargetResolution(uint32_t width, uint32_t height,
                                uint32_t frameRate);

    //Update incoming frame rate/dimension
    void UpdateIncomingFrameRate();

    int32_t updateIncomingFrameSize(uint32_t width, uint32_t height);

    //Set decimated values: frame rate/dimension
    uint32_t DecimatedFrameRate();
    uint32_t DecimatedWidth() const;
    uint32_t DecimatedHeight() const;

    //Preprocess output:
    int32_t PreprocessFrame(const I420VideoFrame& frame,
                            I420VideoFrame** processedFrame);
    VideoContentMetrics* ContentMetrics() const;

private:
    // The content does not change so much every frame, so to reduce complexity
    // we can compute new content metrics every |kSkipFrameCA| frames.
    enum { kSkipFrameCA = 2 };

    int32_t              _id;
    VideoContentMetrics*      _contentMetrics;
    uint32_t             _maxFrameRate;
    I420VideoFrame           _resampledFrame;
    VPMSpatialResampler*     _spatialResampler;
    VPMContentAnalysis*      _ca;
    VPMVideoDecimator*       _vd;
    bool                     _enableCA;
    int                      _frameCnt;
    
}; // end of VPMFramePreprocessor class definition

}  // namespace

#endif // VPM_FRAME_PREPROCESS_H
