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

#include "typedefs.h"
#include "video_processing.h"
#include "content_analysis.h"
#include "spatial_resampler.h"
#include "video_decimator.h"

namespace webrtc {


class VPMFramePreprocessor
{
public:

    VPMFramePreprocessor();
    ~VPMFramePreprocessor();

    WebRtc_Word32 ChangeUniqueId(const WebRtc_Word32 id);

    void Reset();

    // Enable temporal decimation
    void EnableTemporalDecimation(bool enable);

    void SetInputFrameResampleMode(VideoFrameResampling resamplingMode);

    //Enable content analysis
    void EnableContentAnalysis(bool enable);

    //Set max frame rate
    WebRtc_Word32 SetMaxFrameRate(WebRtc_UWord32 maxFrameRate);

    //Set target resolution: frame rate and dimension
    WebRtc_Word32 SetTargetResolution(WebRtc_UWord32 width, WebRtc_UWord32 height, WebRtc_UWord32 frameRate);

    //Update incoming frame rate/dimension
    void UpdateIncomingFrameRate();

    WebRtc_Word32 updateIncomingFrameSize(WebRtc_UWord32 width, WebRtc_UWord32 height);

    //Set decimated values: frame rate/dimension
    WebRtc_UWord32 DecimatedFrameRate();
    WebRtc_UWord32 DecimatedWidth() const;
    WebRtc_UWord32 DecimatedHeight() const;

    //Preprocess output:
    WebRtc_Word32 PreprocessFrame(const VideoFrame* frame, VideoFrame** processedFrame);
    VideoContentMetrics* ContentMetrics() const;

private:
    // The content does not change so much every frame, so to reduce complexity
    // we can compute new content metrics every |kSkipFrameCA| frames.
    enum { kSkipFrameCA = 2 };

    WebRtc_Word32              _id;
    VideoContentMetrics*      _contentMetrics;
    WebRtc_UWord32             _maxFrameRate;
    VideoFrame                _resampledFrame;
    VPMSpatialResampler*     _spatialResampler;
    VPMContentAnalysis*      _ca;
    VPMVideoDecimator*       _vd;
    bool                     _enableCA;
    int                      _frameCnt;
    
}; // end of VPMFramePreprocessor class definition

} //namespace

#endif // VPM_FRAME_PREPROCESS_H
