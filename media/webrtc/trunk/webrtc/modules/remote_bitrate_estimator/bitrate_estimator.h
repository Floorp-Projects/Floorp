/*
 *  Copyright (c) 2012 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef WEBRTC_MODULES_REMOTE_BITRATE_ESTIMATOR_BITRATE_ESTIMATOR_H_
#define WEBRTC_MODULES_REMOTE_BITRATE_ESTIMATOR_BITRATE_ESTIMATOR_H_

#include <list>

#include "typedefs.h"

namespace webrtc {

class BitRateStats
{
public:
    BitRateStats();
    ~BitRateStats();

    void Init();
    void Update(WebRtc_UWord32 packetSizeBytes, WebRtc_Word64 nowMs);
    WebRtc_UWord32 BitRate(WebRtc_Word64 nowMs);

private:
    struct DataTimeSizeTuple
    {
        DataTimeSizeTuple(uint32_t sizeBytes, int64_t timeCompleteMs)
            :
              _sizeBytes(sizeBytes),
              _timeCompleteMs(timeCompleteMs) {}

        WebRtc_UWord32    _sizeBytes;
        WebRtc_Word64     _timeCompleteMs;
    };

    void EraseOld(WebRtc_Word64 nowMs);

    std::list<DataTimeSizeTuple*> _dataSamples;
    WebRtc_UWord32 _accumulatedBytes;
};

}  // namespace webrtc

#endif  // WEBRTC_MODULES_REMOTE_BITRATE_ESTIMATOR_BITRATE_ESTIMATOR_H_
