/*
 *  Copyright (c) 2012 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef VPM_CONTENT_ANALYSIS_H
#define VPM_CONTENT_ANALYSIS_H

#include "common_video/interface/i420_video_frame.h"
#include "typedefs.h"
#include "module_common_types.h"
#include "video_processing_defines.h"

namespace webrtc {

class VPMContentAnalysis
{
public:
    // When |runtime_cpu_detection| is true, runtime selection of an optimized
    // code path is allowed.
    VPMContentAnalysis(bool runtime_cpu_detection);
    ~VPMContentAnalysis();

    // Initialize ContentAnalysis - should be called prior to
    //  extractContentFeature
    // Inputs:         width, height
    // Return value:   0 if OK, negative value upon error
    WebRtc_Word32 Initialize(int width, int height);

    // Extract content Feature - main function of ContentAnalysis
    // Input:           new frame
    // Return value:    pointer to structure containing content Analysis
    //                  metrics or NULL value upon error
    VideoContentMetrics* ComputeContentMetrics(const I420VideoFrame&
                                               inputFrame);

    // Release all allocated memory
    // Output: 0 if OK, negative value upon error
    WebRtc_Word32 Release();

private:

    // return motion metrics
    VideoContentMetrics* ContentMetrics();

    // Normalized temporal difference metric: for motion magnitude
    typedef WebRtc_Word32 (VPMContentAnalysis::*TemporalDiffMetricFunc)();
    TemporalDiffMetricFunc TemporalDiffMetric;
    WebRtc_Word32 TemporalDiffMetric_C();

    // Motion metric method: call 2 metrics (magnitude and size)
    WebRtc_Word32 ComputeMotionMetrics();

    // Spatial metric method: computes the 3 frame-average spatial
    //  prediction errors (1x2,2x1,2x2)
    typedef WebRtc_Word32 (VPMContentAnalysis::*ComputeSpatialMetricsFunc)();
    ComputeSpatialMetricsFunc ComputeSpatialMetrics;
    WebRtc_Word32 ComputeSpatialMetrics_C();

#if defined(WEBRTC_ARCH_X86_FAMILY)
    WebRtc_Word32 ComputeSpatialMetrics_SSE2();
    WebRtc_Word32 TemporalDiffMetric_SSE2();
#endif

    const WebRtc_UWord8*       _origFrame;
    WebRtc_UWord8*             _prevFrame;
    int                        _width;
    int                        _height;
    int                        _skipNum;
    int                        _border;

    // Content Metrics:
    // stores the local average of the metrics
    float                  _motionMagnitude;    // motion class
    float                  _spatialPredErr;     // spatial class
    float                  _spatialPredErrH;    // spatial class
    float                  _spatialPredErrV;    // spatial class
    bool                   _firstFrame;
    bool                   _CAInit;

    VideoContentMetrics*   _cMetrics;

}; // end of VPMContentAnalysis class definition

} // namespace

#endif
