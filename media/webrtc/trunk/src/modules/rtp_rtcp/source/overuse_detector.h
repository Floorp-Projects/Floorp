/*
 *  Copyright (c) 2011 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef WEBRTC_MODULES_RTP_RTCP_SOURCE_OVERUSE_DETECTOR_H_
#define WEBRTC_MODULES_RTP_RTCP_SOURCE_OVERUSE_DETECTOR_H_

#include <list>

#include "bwe_defines.h"
#include "module_common_types.h"
#include "typedefs.h"

#ifdef WEBRTC_BWE_MATLAB
#include "../test/BWEStandAlone/MatlabPlot.h"
#endif

namespace webrtc {
enum RateControlRegion;

class OverUseDetector
{
public:
    OverUseDetector();
    ~OverUseDetector();
    bool Update(const WebRtcRTPHeader& rtpHeader,
                const WebRtc_UWord16 packetSize,
                const WebRtc_Word64 nowMS);
    BandwidthUsage State() const;
    void Reset();
    double NoiseVar() const;
    void SetRateControlRegion(RateControlRegion region);

private:
    struct FrameSample
    {
        FrameSample() : _size(0), _completeTimeMs(-1), _timestamp(-1) {}

        WebRtc_UWord32 _size;
        WebRtc_Word64  _completeTimeMs;
        WebRtc_Word64  _timestamp;
    };

    void CompensatedTimeDelta(const FrameSample& currentFrame,
                              const FrameSample& prevFrame,
                              WebRtc_Word64& tDelta,
                              double& tsDelta,
                              bool wrapped);
    void UpdateKalman(WebRtc_Word64 tDelta,
                      double tsDelta,
                      WebRtc_UWord32 frameSize,
                      WebRtc_UWord32 prevFrameSize);
    double UpdateMinFramePeriod(double tsDelta);
    void UpdateNoiseEstimate(double residual, double tsDelta, bool stableState);
    BandwidthUsage Detect(double tsDelta);
    double CurrentDrift();

    bool _firstPacket;
    FrameSample _currentFrame;
    FrameSample _prevFrame;
    WebRtc_UWord16 _numOfDeltas;
    double _slope;
    double _offset;
    double _E[2][2];
    double _processNoise[2];
    double _avgNoise;
    double _varNoise;
    double _threshold;
    std::list<double> _tsDeltaHist;
    double _prevOffset;
    double _timeOverUsing;
    WebRtc_UWord16 _overUseCounter;
    BandwidthUsage _hypothesis;

#ifdef WEBRTC_BWE_MATLAB
    MatlabPlot* _plot1;
    MatlabPlot* _plot2;
    MatlabPlot* _plot3;
    MatlabPlot* _plot4;
#endif
};
} // namespace webrtc

#endif // WEBRTC_MODULES_RTP_RTCP_SOURCE_OVERUSE_DETECTOR_H_
