/*
 *  Copyright (c) 2011 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef WEBRTC_MODULES_VIDEO_CODING_RTT_FILTER_H_
#define WEBRTC_MODULES_VIDEO_CODING_RTT_FILTER_H_

#include "typedefs.h"

namespace webrtc
{

class VCMRttFilter
{
public:
    VCMRttFilter(WebRtc_Word32 vcmId = 0, WebRtc_Word32 receiverId = 0);

    VCMRttFilter& operator=(const VCMRttFilter& rhs);

    // Resets the filter.
    void Reset();
    // Updates the filter with a new sample.
    void Update(WebRtc_UWord32 rttMs);
    // A getter function for the current RTT level in ms.
    WebRtc_UWord32 RttMs() const;

private:
    // The size of the drift and jump memory buffers
    // and thus also the detection threshold for these
    // detectors in number of samples.
    enum { kMaxDriftJumpCount = 5 };
    // Detects RTT jumps by comparing the difference between
    // samples and average to the standard deviation.
    // Returns true if the long time statistics should be updated
    // and false otherwise
    bool JumpDetection(WebRtc_UWord32 rttMs);
    // Detects RTT drifts by comparing the difference between
    // max and average to the standard deviation.
    // Returns true if the long time statistics should be updated
    // and false otherwise
    bool DriftDetection(WebRtc_UWord32 rttMs);
    // Computes the short time average and maximum of the vector buf.
    void ShortRttFilter(WebRtc_UWord32* buf, WebRtc_UWord32 length);

    WebRtc_Word32         _vcmId;
    WebRtc_Word32         _receiverId;
    bool                  _gotNonZeroUpdate;
    double                _avgRtt;
    double                _varRtt;
    WebRtc_UWord32        _maxRtt;
    WebRtc_UWord32        _filtFactCount;
    const WebRtc_UWord32  _filtFactMax;
    const double          _jumpStdDevs;
    const double          _driftStdDevs;
    WebRtc_Word32         _jumpCount;
    WebRtc_Word32         _driftCount;
    const WebRtc_Word32   _detectThreshold;
    WebRtc_UWord32        _jumpBuf[kMaxDriftJumpCount];
    WebRtc_UWord32        _driftBuf[kMaxDriftJumpCount];
};

} // namespace webrtc

#endif // WEBRTC_MODULES_VIDEO_CODING_RTT_FILTER_H_
