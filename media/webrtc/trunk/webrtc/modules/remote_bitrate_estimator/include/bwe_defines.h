/*
 *  Copyright (c) 2012 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef WEBRTC_MODULES_RTP_RTCP_SOURCE_BWE_DEFINES_H_
#define WEBRTC_MODULES_RTP_RTCP_SOURCE_BWE_DEFINES_H_

#include "typedefs.h"

#define BWE_MAX(a,b) ((a)>(b)?(a):(b))
#define BWE_MIN(a,b) ((a)<(b)?(a):(b))

namespace webrtc {
enum BandwidthUsage
{
    kBwNormal = 0,
    kBwUnderusing = 1,
    kBwOverusing = 2,
};

enum RateControlState
{
    kRcHold,
    kRcIncrease,
    kRcDecrease
};

enum RateControlRegion
{
    kRcNearMax,
    kRcAboveMax,
    kRcMaxUnknown
};

class RateControlInput
{
public:
    RateControlInput(BandwidthUsage bwState,
                     uint32_t incomingBitRate,
                     double noiseVar)
        : _bwState(bwState),
          _incomingBitRate(incomingBitRate),
          _noiseVar(noiseVar) {}

    BandwidthUsage  _bwState;
    uint32_t      _incomingBitRate;
    double              _noiseVar;
};
} //namespace webrtc

#endif // WEBRTC_MODULES_RTP_RTCP_SOURCE_BWE_DEFINES_H_
