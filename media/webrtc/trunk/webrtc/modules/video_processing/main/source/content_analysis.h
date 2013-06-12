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
    int32_t Initialize(int width, int height);

    // Extract content Feature - main function of ContentAnalysis
    // Input:           new frame
    // Return value:    pointer to structure containing content Analysis
    //                  metrics or NULL value upon error
    VideoContentMetrics* ComputeContentMetrics(const I420VideoFrame&
                                               inputFrame);

    // Release all allocated memory
    // Output: 0 if OK, negative value upon error
    int32_t Release();

private:

    // return motion metrics
    VideoContentMetrics* ContentMetrics();

    // Normalized temporal difference metric: for motion magnitude
    typedef int32_t (VPMContentAnalysis::*TemporalDiffMetricFunc)();
    TemporalDiffMetricFunc TemporalDiffMetric;
    int32_t TemporalDiffMetric_C();

    // Motion metric method: call 2 metrics (magnitude and size)
    int32_t ComputeMotionMetrics();

    // Spatial metric method: computes the 3 frame-average spatial
    //  prediction errors (1x2,2x1,2x2)
    typedef int32_t (VPMContentAnalysis::*ComputeSpatialMetricsFunc)();
    ComputeSpatialMetricsFunc ComputeSpatialMetrics;
    int32_t ComputeSpatialMetrics_C();

#if defined(WEBRTC_ARCH_X86_FAMILY)
    int32_t ComputeSpatialMetrics_SSE2();
    int32_t TemporalDiffMetric_SSE2();
#endif

    const uint8_t*       _origFrame;
    uint8_t*             _prevFrame;
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
