/*
 *  Copyright (c) 2011 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef WEBRTC_MODULES_VIDEO_CODING_TIMESTAMP_EXTRAPOLATOR_H_
#define WEBRTC_MODULES_VIDEO_CODING_TIMESTAMP_EXTRAPOLATOR_H_

#include "typedefs.h"
#include "rw_lock_wrapper.h"

namespace webrtc
{

class Clock;

class VCMTimestampExtrapolator
{
public:
    VCMTimestampExtrapolator(Clock* clock,
                             int32_t vcmId = 0,
                             int32_t receiverId = 0);
    ~VCMTimestampExtrapolator();
    void Update(int64_t tMs, uint32_t ts90khz, bool trace = true);
    int64_t ExtrapolateLocalTime(uint32_t timestamp90khz);
    void Reset();

private:
    void CheckForWrapArounds(uint32_t ts90khz);
    bool DelayChangeDetection(double error, bool trace = true);
    RWLockWrapper*        _rwLock;
    int32_t         _vcmId;
    int32_t         _id;
    Clock*                _clock;
    double                _w[2];
    double                _P[2][2];
    int64_t         _startMs;
    int64_t         _prevMs;
    uint32_t        _firstTimestamp;
    int32_t         _wrapArounds;
    int64_t         _prevUnwrappedTimestamp;
    int64_t         _prevWrapTimestamp;
    const double          _lambda;
    bool                  _firstAfterReset;
    uint32_t        _packetCount;
    const uint32_t  _startUpFilterDelayInPackets;

    double              _detectorAccumulatorPos;
    double              _detectorAccumulatorNeg;
    const double        _alarmThreshold;
    const double        _accDrift;
    const double        _accMaxError;
    const double        _P11;
};

} // namespace webrtc

#endif // WEBRTC_MODULES_VIDEO_CODING_TIMESTAMP_EXTRAPOLATOR_H_
