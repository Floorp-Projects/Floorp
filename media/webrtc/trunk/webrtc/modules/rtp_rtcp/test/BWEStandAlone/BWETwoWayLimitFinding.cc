/*
 *  Copyright (c) 2011 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "BWETwoWayLimitFinding.h"
#include "TestLoadGenerator.h"


BWETwoWayLimitFinding::BWETwoWayLimitFinding(
    std::string testName,
    int masterStartRateKbps, int masterAvailBWkbps,
    int slaveStartRateKbps, int slaveAvailBWkbps,
    bool isMaster /*= false*/)
    :
BWETest(testName, (isMaster ? masterStartRateKbps : slaveStartRateKbps)),
_availBWkbps(isMaster ? masterAvailBWkbps : slaveAvailBWkbps),
_incomingAvailBWkbps(isMaster ? slaveAvailBWkbps : masterAvailBWkbps),
_forwLimitReached(false),
_revLimitReached(false)
{
    _master = isMaster;
}


BWETwoWayLimitFinding::~BWETwoWayLimitFinding()
{
    if (_gen)
    {
        delete _gen;
        _gen = NULL;
    }
}


int BWETwoWayLimitFinding::Init(std::string ip, uint16_t port)
{
    // create the load generator object
    const int rtpSampleRate = 90000;
    const int frameRate = 30;
    const double spreadFactor = 0.2;

    _gen = new CBRFixFRGenerator(_sendrec, _startRateKbps, rtpSampleRate, frameRate, spreadFactor);
    if (!_gen)
    {
        return (-1);
    }

    if (!_master) UseRecvTimeout(); // slave shuts down when incoming stream dies

    return BWETest::Init(ip, port);
}


bool BWETwoWayLimitFinding::StoppingCriterionMaster()
{
    if ((_sendrec->BitrateSent() / 1000.0) > (0.95 * _availBWkbps))
    {
        _forwLimitReached = true;
    }

    int32_t revRateKbps = _sendrec->ReceiveBitrateKbps();
    if (revRateKbps > (0.95 * _incomingAvailBWkbps))
    {
        _revLimitReached = true;
    }

    return (_forwLimitReached && _revLimitReached);
}

