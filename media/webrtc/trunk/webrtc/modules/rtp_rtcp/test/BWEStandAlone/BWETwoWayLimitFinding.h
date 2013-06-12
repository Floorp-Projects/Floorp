/*
 *  Copyright (c) 2011 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef WEBRTC_MODULES_RTP_RTCP_TEST_BWESTANDALONE_BWETWOWAYLIMITFINDING_H_
#define WEBRTC_MODULES_RTP_RTCP_TEST_BWESTANDALONE_BWETWOWAYLIMITFINDING_H_

#include "BWETestBase.h"

class BWETwoWayLimitFinding : public BWETest
{
public:
    BWETwoWayLimitFinding(std::string testName,
        int masterStartRateKbps, int masterAvailBWkbps,
        int slaveStartRateKbps, int slaveAvailBWkbps,
        bool isMaster = false);

    virtual ~BWETwoWayLimitFinding();

    virtual int Init(std::string ip, uint16_t port);

protected:
    virtual bool StoppingCriterionMaster();
    //virtual bool StoppingCriterionSlave();

private:
    int _availBWkbps;
    int _incomingAvailBWkbps;
    bool _forwLimitReached;
    bool _revLimitReached;

};


#endif // WEBRTC_MODULES_RTP_RTCP_TEST_BWESTANDALONE_BWETWOWAYLIMITFINDING_H_
