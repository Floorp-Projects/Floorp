/*
 *  Copyright (c) 2012 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef WEBRTC_MODULES_RTP_RTCP_SOURCE_REMOTE_RATE_CONTROL_H_
#define WEBRTC_MODULES_RTP_RTCP_SOURCE_REMOTE_RATE_CONTROL_H_

#include "bwe_defines.h"
#include "typedefs.h"

#ifdef MATLAB
#include "../test/BWEStandAlone/MatlabPlot.h"
#endif

namespace webrtc {
class RemoteRateControl
{
public:
    RemoteRateControl();
    ~RemoteRateControl();
    WebRtc_Word32 SetConfiguredBitRates(WebRtc_UWord32 minBitRate,
                                        WebRtc_UWord32 maxBitRate);
    WebRtc_UWord32 LatestEstimate() const;
    WebRtc_UWord32 UpdateBandwidthEstimate(WebRtc_UWord32 RTT,
                                           WebRtc_Word64 nowMS);
    RateControlRegion Update(const RateControlInput& input, bool& firstOverUse,
                             WebRtc_Word64 nowMS);
    void Reset();

    // Returns true if there is a valid estimate of the incoming bitrate, false
    // otherwise.
    bool ValidEstimate() const;

private:
    WebRtc_UWord32 ChangeBitRate(WebRtc_UWord32 currentBitRate,
                                 WebRtc_UWord32 incomingBitRate,
                                 double delayFactor, WebRtc_UWord32 RTT,
                                 WebRtc_Word64 nowMS);
    double RateIncreaseFactor(WebRtc_Word64 nowMs,
                              WebRtc_Word64 lastMs,
                              WebRtc_UWord32 reactionTimeMs,
                              double noiseVar) const;
    void UpdateChangePeriod(WebRtc_Word64 nowMs);
    void UpdateMaxBitRateEstimate(float incomingBitRateKbps);
    void ChangeState(const RateControlInput& input, WebRtc_Word64 nowMs);
    void ChangeState(RateControlState newState);
    void ChangeRegion(RateControlRegion region);
    static void StateStr(RateControlState state, char* str);
    static void StateStr(BandwidthUsage state, char* str);

    WebRtc_UWord32        _minConfiguredBitRate;
    WebRtc_UWord32        _maxConfiguredBitRate;
    WebRtc_UWord32        _currentBitRate;
    WebRtc_UWord32        _maxHoldRate;
    float               _avgMaxBitRate;
    float               _varMaxBitRate;
    RateControlState    _rcState;
    RateControlState    _cameFromState;
    RateControlRegion   _rcRegion;
    WebRtc_Word64         _lastBitRateChange;
    RateControlInput    _currentInput;
    bool                _updated;
    WebRtc_Word64         _timeFirstIncomingEstimate;
    bool                _initializedBitRate;

    float               _avgChangePeriod;
    WebRtc_Word64         _lastChangeMs;
    float               _beta;
#ifdef MATLAB
    MatlabPlot          *_plot1;
    MatlabPlot          *_plot2;
#endif
};
} // namespace webrtc

#endif // WEBRTC_MODULES_RTP_RTCP_SOURCE_REMOTE_RATE_CONTROL_H_
