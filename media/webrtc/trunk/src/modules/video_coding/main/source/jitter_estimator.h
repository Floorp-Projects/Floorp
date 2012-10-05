/*
 *  Copyright (c) 2011 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef WEBRTC_MODULES_VIDEO_CODING_JITTER_ESTIMATOR_H_
#define WEBRTC_MODULES_VIDEO_CODING_JITTER_ESTIMATOR_H_

#include "typedefs.h"
#include "rtt_filter.h"

namespace webrtc
{

class VCMJitterEstimator
{
public:
    VCMJitterEstimator(WebRtc_Word32 vcmId = 0, WebRtc_Word32 receiverId = 0);

    VCMJitterEstimator& operator=(const VCMJitterEstimator& rhs);

    // Resets the estimate to the initial state
    void Reset();
    void ResetNackCount();

    // Updates the jitter estimate with the new data.
    //
    // Input:
    //          - frameDelay      : Delay-delta calculated by UTILDelayEstimate in milliseconds
    //          - frameSize       : Frame size of the current frame.
    //          - incompleteFrame : Flags if the frame is used to update the estimate before it
    //                              was complete. Default is false.
    void UpdateEstimate(WebRtc_Word64 frameDelayMS,
                        WebRtc_UWord32 frameSizeBytes,
                        bool incompleteFrame = false);

    // Returns the current jitter estimate in milliseconds and adds
    // also adds an RTT dependent term in cases of retransmission.
    //  Input:
    //          - rttMultiplier  : RTT param multiplier (when applicable).
    //
    // Return value                   : Jitter estimate in milliseconds
    double GetJitterEstimate(double rttMultiplier);

    // Updates the nack counter.
    void FrameNacked();

    // Updates the RTT filter.
    //
    // Input:
    //          - rttMs               : RTT in ms
    void UpdateRtt(WebRtc_UWord32 rttMs);

    void UpdateMaxFrameSize(WebRtc_UWord32 frameSizeBytes);

    // A constant describing the delay from the jitter buffer
    // to the delay on the receiving side which is not accounted
    // for by the jitter buffer nor the decoding delay estimate.
    static const WebRtc_UWord32 OPERATING_SYSTEM_JITTER = 10;

protected:
    // These are protected for better testing possibilities
    double              _theta[2]; // Estimated line parameters (slope, offset)
    double              _varNoise; // Variance of the time-deviation from the line

private:
    // Updates the Kalman filter for the line describing
    // the frame size dependent jitter.
    //
    // Input:
    //          - frameDelayMS    : Delay-delta calculated by UTILDelayEstimate in milliseconds
    //          - deltaFSBytes    : Frame size delta, i.e.
    //                            : frame size at time T minus frame size at time T-1
    void KalmanEstimateChannel(WebRtc_Word64 frameDelayMS, WebRtc_Word32 deltaFSBytes);

    // Updates the random jitter estimate, i.e. the variance
    // of the time deviations from the line given by the Kalman filter.
    //
    // Input:
    //          - d_dT              : The deviation from the kalman estimate
    //          - incompleteFrame   : True if the frame used to update the estimate
    //                                with was incomplete
    void EstimateRandomJitter(double d_dT, bool incompleteFrame);

    double NoiseThreshold() const;

    // Calculates the current jitter estimate.
    //
    // Return value                 : The current jitter estimate in milliseconds
    double CalculateEstimate();

    // Post process the calculated estimate
    void PostProcessEstimate();

    // Calculates the difference in delay between a sample and the
    // expected delay estimated by the Kalman filter.
    //
    // Input:
    //          - frameDelayMS    : Delay-delta calculated by UTILDelayEstimate in milliseconds
    //          - deltaFS         : Frame size delta, i.e. frame size at time
    //                              T minus frame size at time T-1
    //
    // Return value                 : The difference in milliseconds
    double DeviationFromExpectedDelay(WebRtc_Word64 frameDelayMS,
                                      WebRtc_Word32 deltaFSBytes) const;

    // Constants, filter parameters
    WebRtc_Word32         _vcmId;
    WebRtc_Word32         _receiverId;
    const double          _phi;
    const double          _psi;
    const WebRtc_UWord32  _alphaCountMax;
    const double          _thetaLow;
    const WebRtc_UWord32  _nackLimit;
    const WebRtc_Word32   _numStdDevDelayOutlier;
    const WebRtc_Word32   _numStdDevFrameSizeOutlier;
    const double          _noiseStdDevs;
    const double          _noiseStdDevOffset;

    double                _thetaCov[2][2]; // Estimate covariance
    double                _Qcov[2][2];     // Process noise covariance
    double                _avgFrameSize;   // Average frame size
    double                _varFrameSize;   // Frame size variance
    double                _maxFrameSize;   // Largest frame size received (descending
                                           // with a factor _psi)
    WebRtc_UWord32        _fsSum;
    WebRtc_UWord32        _fsCount;

    WebRtc_Word64         _lastUpdateT;
    double                _prevEstimate;         // The previously returned jitter estimate
    WebRtc_UWord32        _prevFrameSize;        // Frame size of the previous frame
    double                _avgNoise;             // Average of the random jitter
    WebRtc_UWord32        _alphaCount;
    double                _filterJitterEstimate; // The filtered sum of jitter estimates

    WebRtc_UWord32        _startupCount;

    WebRtc_Word64         _latestNackTimestamp;  // Timestamp in ms when the latest nack was seen
    WebRtc_UWord32        _nackCount;            // Keeps track of the number of nacks received,
                                                 // but never goes above _nackLimit
    VCMRttFilter          _rttFilter;

    enum { kStartupDelaySamples = 30 };
    enum { kFsAccuStartupSamples = 5 };
};

} // namespace webrtc

#endif // WEBRTC_MODULES_VIDEO_CODING_JITTER_ESTIMATOR_H_
