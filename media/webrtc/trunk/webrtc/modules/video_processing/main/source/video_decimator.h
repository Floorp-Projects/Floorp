/*
 *  Copyright (c) 2011 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

/*
 * video_decimator.h
 */
#ifndef VPM_VIDEO_DECIMATOR_H
#define VPM_VIDEO_DECIMATOR_H

#include "webrtc/modules/interface/module_common_types.h"
#include "webrtc/typedefs.h"

namespace webrtc {

class VPMVideoDecimator
{
public:
    VPMVideoDecimator();
    ~VPMVideoDecimator();
    
    void Reset();
    
    void EnableTemporalDecimation(bool enable);
    
    int32_t SetMaxFrameRate(uint32_t maxFrameRate);
    int32_t SetTargetFrameRate(uint32_t frameRate);

    bool DropFrame();
    
    void UpdateIncomingFrameRate();

    // Get Decimated Frame Rate/Dimensions
    uint32_t DecimatedFrameRate();

    //Get input frame rate
    uint32_t InputFrameRate();

private:
    void ProcessIncomingFrameRate(int64_t now);

    enum { kFrameCountHistorySize = 90};
    enum { kFrameHistoryWindowMs = 2000};

    // Temporal decimation
    int32_t         _overShootModifier;
    uint32_t        _dropCount;
    uint32_t        _keepCount;
    uint32_t        _targetFrameRate;
    float               _incomingFrameRate;
    uint32_t        _maxFrameRate;
    int64_t         _incomingFrameTimes[kFrameCountHistorySize];
    bool                _enableTemporalDecimation;

};

}  // namespace

#endif
