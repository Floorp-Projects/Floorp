/*
 *  Copyright (c) 2011 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef WEBRTC_MODULES_VIDEO_CODING_TEST_JITTER_ESTIMATE_TEST_H_
#define WEBRTC_MODULES_VIDEO_CODING_TEST_JITTER_ESTIMATE_TEST_H_

#include "typedefs.h"
#include "jitter_buffer.h"
#include "jitter_estimator.h"
#include <cstdlib>
#include <cmath>

double const pi = 4*std::atan(1.0);

class GaussDist
{
public:
    GaussDist(double m, double v): _mu(m), _sigma(sqrt(v)) {}

    double RandValue() // returns a single normally distributed number
    {
        double r1 = (std::rand() + 1.0)/(RAND_MAX + 1.0); // gives equal distribution in (0, 1]
        double r2 = (std::rand() + 1.0)/(RAND_MAX + 1.0);
        return _mu + _sigma * std::sqrt(-2*std::log(r1))*std::cos(2*pi*r2);
    }

    double GetAverage()
    {
        return _mu;
    }

    double GetVariance()
    {
        return _sigma*_sigma;
    }

    void SetParams(double m, double v)
    {
        _mu = m;
        _sigma = sqrt(v);
    }

private:
    double _mu, _sigma;
};

class JitterEstimateTestWrapper : public webrtc::VCMJitterEstimator
{
public:
    JitterEstimateTestWrapper() : VCMJitterEstimator() {}
    double GetTheta() { return _theta[0]; }
    double GetVarNoise() { return _varNoise; }
};

class FrameSample
{
public:
    FrameSample() {FrameSample(0, 0, 0, false, false);}
    FrameSample(unsigned int ts, WebRtc_Word64 wallClk, unsigned int fs, bool _keyFrame, bool _resent):
      timestamp90Khz(ts), wallClockMs(wallClk), frameSize(fs), keyFrame(_keyFrame), resent(_resent) {}

    unsigned int timestamp90Khz;
    WebRtc_Word64 wallClockMs;
    unsigned int frameSize;
    bool keyFrame;
    bool resent;
};

class JitterEstimateTest
{
public:
    JitterEstimateTest(unsigned int frameRate);
    FrameSample GenerateFrameSample();
    void SetCapacity(unsigned int c);
    void SetRate(unsigned int r);
    void SetJitter(double m, double v);
    void SetFrameSizeStats(double m, double v);
    void SetKeyFrameRate(int rate);
    void SetLossRate(double rate);

private:
    double RandUniform() { return (std::rand() + 1.0)/(RAND_MAX + 1.0); }
    unsigned int _frameRate;
    unsigned int _capacity;
    unsigned int _rate;
    GaussDist _jitter;
    //GaussDist _noResend;
    GaussDist _deltaFrameSize;
    unsigned int _prevTimestamp;
    WebRtc_Word64 _prevWallClock;
    unsigned int _nextDelay;
    double _keyFrameRate;
    unsigned int _counter;
    unsigned int _seed;
    double _lossrate;
};

#endif // WEBRTC_MODULES_VIDEO_CODING_TEST_JITTER_ESTIMATE_TEST_H_
