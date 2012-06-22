/*
 *  Copyright (c) 2011 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#if _WIN32
#include <windows.h>
#endif

#include "trace.h"
#include "overuse_detector.h"
#include "remote_rate_control.h"
#include "rtp_utility.h"
#include <math.h>
#include <stdlib.h> //abs

#ifdef WEBRTC_BWE_MATLAB
extern MatlabEngine eng; // global variable defined elsewhere
#endif

#define INIT_CAPACITY_SLOPE 8.0/512.0
#define DETECTOR_THRESHOLD 25.0
#define OVER_USING_TIME_THRESHOLD 100
#define MIN_FRAME_PERIOD_HISTORY_LEN 60

namespace webrtc {
OverUseDetector::OverUseDetector()
:
_firstPacket(true),
_currentFrame(),
_prevFrame(),
_numOfDeltas(0),
_slope(INIT_CAPACITY_SLOPE),
_offset(0),
_E(),
_processNoise(),
_avgNoise(0.0),
_varNoise(500),
_threshold(DETECTOR_THRESHOLD),
_tsDeltaHist(),
_prevOffset(0.0),
_timeOverUsing(-1),
_overUseCounter(0),
_hypothesis(kBwNormal)
#ifdef WEBRTC_BWE_MATLAB
,_plot1(NULL),
_plot2(NULL),
_plot3(NULL),
_plot4(NULL)
#endif
{
    _E[0][0] = 100;
    _E[1][1] = 1e-1;
    _E[0][1] = _E[1][0] = 0;
    _processNoise[0] = 1e-10;
    _processNoise[1] = 1e-2;
}

OverUseDetector::~OverUseDetector()
{
#ifdef WEBRTC_BWE_MATLAB
    if (_plot1)
    {
        eng.DeletePlot(_plot1);
        _plot1 = NULL;
    }
    if (_plot2)
    {
        eng.DeletePlot(_plot2);
        _plot2 = NULL;
    }
    if (_plot3)
    {
        eng.DeletePlot(_plot3);
        _plot3 = NULL;
    }
    if (_plot4)
    {
        eng.DeletePlot(_plot4);
        _plot4 = NULL;
    }
#endif

    _tsDeltaHist.clear();
}

void OverUseDetector::Reset()
{
    _firstPacket = true;
    _currentFrame._size = 0;
    _currentFrame._completeTimeMs = -1;
    _currentFrame._timestamp = -1;
    _prevFrame._size = 0;
    _prevFrame._completeTimeMs = -1;
    _prevFrame._timestamp = -1;
    _numOfDeltas = 0;
    _slope = INIT_CAPACITY_SLOPE;
    _offset = 0;
    _E[0][0] = 100;
    _E[1][1] = 1e-1;
    _E[0][1] = _E[1][0] = 0;
    _processNoise[0] = 1e-10;
    _processNoise[1] = 1e-2;
    _avgNoise = 0.0;
    _varNoise = 500;
    _threshold = DETECTOR_THRESHOLD;
    _prevOffset = 0.0;
    _timeOverUsing = -1;
    _overUseCounter = 0;
    _hypothesis = kBwNormal;
    _tsDeltaHist.clear();
}

bool OverUseDetector::Update(const WebRtcRTPHeader& rtpHeader,
                             const WebRtc_UWord16 packetSize,
                             const WebRtc_Word64 nowMS)
{
#ifdef WEBRTC_BWE_MATLAB
    // Create plots
    const WebRtc_Word64 startTimeMs = nowMS;
    if (_plot1 == NULL)
    {
        _plot1 = eng.NewPlot(new MatlabPlot());
        _plot1->AddLine(1000, "b.", "scatter");
    }
    if (_plot2 == NULL)
    {
        _plot2 = eng.NewPlot(new MatlabPlot());
        _plot2->AddTimeLine(30, "b", "offset", startTimeMs);
        _plot2->AddTimeLine(30, "r--", "limitPos", startTimeMs);
        _plot2->AddTimeLine(30, "k.", "trigger", startTimeMs);
        _plot2->AddTimeLine(30, "ko", "detection", startTimeMs);
        //_plot2->AddTimeLine(30, "g", "slowMean", startTimeMs);
    }
    if (_plot3 == NULL)
    {
        _plot3 = eng.NewPlot(new MatlabPlot());
        _plot3->AddTimeLine(30, "b", "noiseVar", startTimeMs);
    }
    if (_plot4 == NULL)
    {
        _plot4 = eng.NewPlot(new MatlabPlot());
        //_plot4->AddTimeLine(60, "b", "p11", startTimeMs);
        //_plot4->AddTimeLine(60, "r", "p12", startTimeMs);
        _plot4->AddTimeLine(60, "g", "p22", startTimeMs);
        //_plot4->AddTimeLine(60, "g--", "p22_hat", startTimeMs);
        //_plot4->AddTimeLine(30, "b.-", "deltaFs", startTimeMs);
    }

#endif

    bool wrapped = false;
    bool completeFrame = false;
    if (_currentFrame._timestamp == -1)
    {
        _currentFrame._timestamp = rtpHeader.header.timestamp;
    }
    else if (ModuleRTPUtility::OldTimestamp(
                 rtpHeader.header.timestamp,
                 static_cast<WebRtc_UWord32>(_currentFrame._timestamp),
                 &wrapped))
    {
        // Don't update with old data
        return completeFrame;
    }
    else if (rtpHeader.header.timestamp != _currentFrame._timestamp)
    {
        // First packet of a later frame, the previous frame sample is ready
        WEBRTC_TRACE(kTraceStream, kTraceRtpRtcp, -1, "Frame complete at %I64i", _currentFrame._completeTimeMs);
        if (_prevFrame._completeTimeMs >= 0) // This is our second frame
        {
            WebRtc_Word64 tDelta = 0;
            double tsDelta = 0;
            // Check for wrap
            ModuleRTPUtility::OldTimestamp(
                static_cast<WebRtc_UWord32>(_prevFrame._timestamp),
                static_cast<WebRtc_UWord32>(_currentFrame._timestamp),
                &wrapped);
            CompensatedTimeDelta(_currentFrame, _prevFrame, tDelta, tsDelta, wrapped);
            UpdateKalman(tDelta, tsDelta, _currentFrame._size, _prevFrame._size);
        }
        // The new timestamp is now the current frame,
        // and the old timestamp becomes the previous frame.
        _prevFrame = _currentFrame;
        _currentFrame._timestamp = rtpHeader.header.timestamp;
        _currentFrame._size = 0;
        _currentFrame._completeTimeMs = -1;
        completeFrame = true;
    }
    // Accumulate the frame size
    _currentFrame._size += packetSize;
    _currentFrame._completeTimeMs = nowMS;
    return completeFrame;
}

BandwidthUsage OverUseDetector::State() const
{
    return _hypothesis;
}

double OverUseDetector::NoiseVar() const
{
    return _varNoise;
}

void OverUseDetector::SetRateControlRegion(RateControlRegion region)
{
    switch (region)
    {
    case kRcMaxUnknown:
        {
            _threshold = DETECTOR_THRESHOLD;
            break;
        }
    case kRcAboveMax:
    case kRcNearMax:
        {
            _threshold = DETECTOR_THRESHOLD / 2;
            break;
        }
    }
}

void OverUseDetector::CompensatedTimeDelta(const FrameSample& currentFrame, const FrameSample& prevFrame, WebRtc_Word64& tDelta,
                                           double& tsDelta, bool wrapped)
{
    _numOfDeltas++;
    if (_numOfDeltas > 1000)
    {
        _numOfDeltas = 1000;
    }
    // Add wrap-around compensation
    WebRtc_Word64 wrapCompensation = 0;
    if (wrapped)
    {
        wrapCompensation = static_cast<WebRtc_Word64>(1)<<32;
    }
    tsDelta = (currentFrame._timestamp + wrapCompensation - prevFrame._timestamp) / 90.0;
    tDelta = currentFrame._completeTimeMs - prevFrame._completeTimeMs;
    assert(tsDelta > 0);
}

double OverUseDetector::CurrentDrift()
{
    return 1.0;
}

void OverUseDetector::UpdateKalman(WebRtc_Word64 tDelta, double tsDelta, WebRtc_UWord32 frameSize, WebRtc_UWord32 prevFrameSize)
{
    const double minFramePeriod = UpdateMinFramePeriod(tsDelta);
    const double drift = CurrentDrift();
    // Compensate for drift
    const double tTsDelta = tDelta - tsDelta / drift;
    double fsDelta = static_cast<double>(frameSize) - prevFrameSize;

    // Update the Kalman filter
    const double scaleFactor =  minFramePeriod / (1000.0 / 30.0);
    _E[0][0] += _processNoise[0] * scaleFactor;
    _E[1][1] += _processNoise[1] * scaleFactor;

    if ((_hypothesis == kBwOverusing && _offset < _prevOffset) ||
        (_hypothesis == kBwUnderUsing && _offset > _prevOffset))
    {
        _E[1][1] += 10 * _processNoise[1] * scaleFactor;
    }

    const double h[2] = {fsDelta, 1.0};
    const double Eh[2] = {_E[0][0]*h[0] + _E[0][1]*h[1],
                         _E[1][0]*h[0] + _E[1][1]*h[1]};

    const double residual = tTsDelta - _slope*h[0] - _offset;

    const bool stableState = (BWE_MIN(_numOfDeltas, 60) * abs(_offset) < _threshold);
    // We try to filter out very late frames. For instance periodic key
    // frames doesn't fit the Gaussian model well.
    if (abs(residual) < 3 * sqrt(_varNoise))
    {
        UpdateNoiseEstimate(residual, minFramePeriod, stableState);
    }
    else
    {
        UpdateNoiseEstimate(3 * sqrt(_varNoise), minFramePeriod, stableState);
    }

    const double denom = _varNoise + h[0]*Eh[0] + h[1]*Eh[1];

    const double K[2] = {Eh[0] / denom,
                        Eh[1] / denom};

    const double IKh[2][2] = {{1.0 - K[0]*h[0], -K[0]*h[1]},
                             {-K[1]*h[0], 1.0 - K[1]*h[1]}};
    const double e00 = _E[0][0];
    const double e01 = _E[0][1];

    // Update state
    _E[0][0] = e00 * IKh[0][0] + _E[1][0] * IKh[0][1];
    _E[0][1] = e01 * IKh[0][0] + _E[1][1] * IKh[0][1];
    _E[1][0] = e00 * IKh[1][0] + _E[1][0] * IKh[1][1];
    _E[1][1] = e01 * IKh[1][0] + _E[1][1] * IKh[1][1];

    // Covariance matrix, must be positive semi-definite
    assert(_E[0][0] + _E[1][1] >= 0 &&
           _E[0][0] * _E[1][1] - _E[0][1] * _E[1][0] >= 0 &&
           _E[0][0] >= 0);

#ifdef WEBRTC_BWE_MATLAB
    //_plot4->Append("p11",_E[0][0]);
    //_plot4->Append("p12",_E[0][1]);
    _plot4->Append("p22",_E[1][1]);
    //_plot4->Append("p22_hat", 0.5*(_processNoise[1] +
    //    sqrt(_processNoise[1]*(_processNoise[1] + 4*_varNoise))));
    //_plot4->Append("deltaFs", fsDelta);
    _plot4->Plot();
#endif
    _slope = _slope + K[0] * residual;
    _prevOffset = _offset;
    _offset = _offset + K[1] * residual;

    Detect(tsDelta);

#ifdef WEBRTC_BWE_MATLAB
    _plot1->Append("scatter", static_cast<double>(_currentFrame._size) - _prevFrame._size,
        static_cast<double>(tDelta-tsDelta));
    _plot1->MakeTrend("scatter", "slope", _slope, _offset, "k-");
    _plot1->MakeTrend("scatter", "thresholdPos", _slope, _offset + 2 * sqrt(_varNoise), "r-");
    _plot1->MakeTrend("scatter", "thresholdNeg", _slope, _offset - 2 * sqrt(_varNoise), "r-");
    _plot1->Plot();

    _plot2->Append("offset", _offset);
    _plot2->Append("limitPos", _threshold/BWE_MIN(_numOfDeltas, 60));
    _plot2->Plot();

    _plot3->Append("noiseVar", _varNoise);
    _plot3->Plot();
#endif
}

double OverUseDetector::UpdateMinFramePeriod(double tsDelta) {
  double minFramePeriod = tsDelta;
  if (_tsDeltaHist.size() >= MIN_FRAME_PERIOD_HISTORY_LEN) {
    std::list<double>::iterator firstItem = _tsDeltaHist.begin();
    _tsDeltaHist.erase(firstItem);
  }
  std::list<double>::iterator it = _tsDeltaHist.begin();
  for (; it != _tsDeltaHist.end(); it++) {
    minFramePeriod = BWE_MIN(*it, minFramePeriod);
  }
  _tsDeltaHist.push_back(tsDelta);
  return minFramePeriod;
}

void OverUseDetector::UpdateNoiseEstimate(double residual, double tsDelta, bool stableState)
{
    if (!stableState)
    {
        return;
    }
    // Faster filter during startup to faster adapt to the jitter level of the network
    // alpha is tuned for 30 frames per second, but
    double alpha = 0.01;
    if (_numOfDeltas > 10*30)
    {
        alpha = 0.002;
    }
    // Only update the noise estimate if we're not over-using
    // beta is a function of alpha and the time delta since
    // the previous update.
    const double beta = pow(1 - alpha, tsDelta * 30.0 / 1000.0);
    _avgNoise = beta * _avgNoise + (1 - beta) * residual;
    _varNoise = beta * _varNoise + (1 - beta) * (_avgNoise - residual) * (_avgNoise - residual);
    if (_varNoise < 1e-7)
    {
        _varNoise = 1e-7;
    }
}

BandwidthUsage OverUseDetector::Detect(double tsDelta)
{
    if (_numOfDeltas < 2)
    {
        return kBwNormal;
    }
    const double T = BWE_MIN(_numOfDeltas, 60) * _offset;
    if (abs(T) > _threshold)
    {
        if (_offset > 0)
        {
            if (_timeOverUsing == -1)
            {
                // Initialize the timer. Assume that we've been
                // over-using half of the time since the previous
                // sample.
                _timeOverUsing = tsDelta / 2;
            }
            else
            {
                // Increment timer
                _timeOverUsing += tsDelta;
            }
            _overUseCounter++;
            if (_timeOverUsing > OVER_USING_TIME_THRESHOLD && _overUseCounter > 1)
            {
                if (_offset >= _prevOffset)
                {
#ifdef _DEBUG
                    if (_hypothesis != kBwOverusing)
                        WEBRTC_TRACE(kTraceStream, kTraceRtpRtcp, -1, "BWE: kBwOverusing");
#endif
                    _timeOverUsing = 0;
                    _overUseCounter = 0;
                    _hypothesis = kBwOverusing;
#ifdef WEBRTC_BWE_MATLAB
                    _plot2->Append("detection",_offset); // plot it later
#endif
                }
            }
#ifdef WEBRTC_BWE_MATLAB
            _plot2->Append("trigger",_offset); // plot it later
#endif
        }
        else
        {
#ifdef _DEBUG
            if (_hypothesis != kBwUnderUsing)
                    WEBRTC_TRACE(kTraceStream, kTraceRtpRtcp, -1, "BWE: kBwUnderUsing");
#endif
            _timeOverUsing = -1;
            _overUseCounter = 0;
            _hypothesis = kBwUnderUsing;
        }
    }
    else
    {
#ifdef _DEBUG
        if (_hypothesis != kBwNormal)
                WEBRTC_TRACE(kTraceStream, kTraceRtpRtcp, -1, "BWE: kBwNormal");
#endif
        _timeOverUsing = -1;
        _overUseCounter = 0;
        _hypothesis = kBwNormal;
    }
    return _hypothesis;
}

} // namespace webrtc
