/*
 *  Copyright (c) 2011 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "webrtc/modules/rtp_rtcp/test/BWEStandAlone/BWEStabilityTest.h"

#include <math.h>

#include <fstream>

#include "webrtc/modules/rtp_rtcp/test/BWEStandAlone/TestLoadGenerator.h"
#include "webrtc/system_wrappers/interface/critical_section_wrapper.h"
#include "webrtc/system_wrappers/interface/tick_util.h"


BWEStabilityTest::BWEStabilityTest(std::string testName, int rateKbps, int testDurationSeconds)
:
_testDurationSeconds(testDurationSeconds),
BWEOneWayTest(testName, rateKbps)
{
}


BWEStabilityTest::~BWEStabilityTest()
{
    if (_gen)
    {
        delete _gen;
        _gen = NULL;
    }
}


int BWEStabilityTest::Init(std::string ip, uint16_t port)
{
    // create the load generator object
    const int rtpSampleRate = 90000;
    const int frameRate = 30;
    const double spreadFactor = 0.2;
    const double keyToDeltaRatio = 7;
    const int keyFramePeriod = 300;

    if (_master)
    {
        _gen = new CBRFixFRGenerator(_sendrec, _startRateKbps, rtpSampleRate, frameRate, spreadFactor);
        //_gen = new PeriodicKeyFixFRGenerator(_sendrec, _startRateKbps, rtpSampleRate, frameRate,
        //                                     spreadFactor, keyToDeltaRatio, keyFramePeriod);
        if (!_gen)
        {
            return (-1);
        }

    }

    return BWEOneWayTest::Init(ip, port);
}


void BWEStabilityTest::Report(std::fstream &log)
{
    // cannot report on a running test
    if(_running) return;

    BWETest::Report(log);

    CriticalSectionScoped cs(_statCritSect);

    log << "Bitrate statistics\n";
    log << "\tAverage = " <<  _rateVecKbps.Mean() << " kbps\n";
    log << "\tMin     = " <<  _rateVecKbps.Min() << " kbps\n";
    log << "\tMax     = " <<  _rateVecKbps.Max() << " kbps\n";
    log << "\tStd     = " <<  _rateVecKbps.Std() << " kbps\n";

}


bool BWEStabilityTest::StoppingCriterionMaster()
{
    return (TickTime::MillisecondTimestamp() - _startTimeMs >= _testDurationSeconds * 1000);
}
