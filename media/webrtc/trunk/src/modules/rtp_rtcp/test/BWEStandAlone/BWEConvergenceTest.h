/*
 *  Copyright (c) 2011 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef WEBRTC_MODULES_RTP_RTCP_TEST_BWESTANDALONE_BWECONVERGENCETEST_H_
#define WEBRTC_MODULES_RTP_RTCP_TEST_BWESTANDALONE_BWECONVERGENCETEST_H_

#include <string>

#include "BWETestBase.h"

#include "typedefs.h"

#include "TestSenderReceiver.h"

class BWEConvergenceTestUp : public BWEOneWayTest
{
public:
    BWEConvergenceTestUp(std::string testName, int startRateKbps, int availBWkbps);
    virtual ~BWEConvergenceTestUp();

    virtual int Init(std::string ip, WebRtc_UWord16 port);

protected:
    virtual bool StoppingCriterionMaster();

private:
    int _availBWkbps;
};


#endif // WEBRTC_MODULES_RTP_RTCP_TEST_BWESTANDALONE_BWECONVERGENCETEST_H_
