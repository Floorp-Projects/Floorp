/*
 *  Copyright (c) 2011 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef WEBRTC_MODULES_RTP_RTCP_TEST_BWESTANDALONE_BWESTABILITYTEST_H_
#define WEBRTC_MODULES_RTP_RTCP_TEST_BWESTANDALONE_BWESTABILITYTEST_H_

#include <string>

#include "webrtc/modules/rtp_rtcp/test/BWEStandAlone/BWETestBase.h"
#include "webrtc/modules/rtp_rtcp/test/BWEStandAlone/TestSenderReceiver.h"
#include "webrtc/typedefs.h"

class BWEStabilityTest : public BWEOneWayTest
{
public:
    BWEStabilityTest(std::string testName, int rateKbps, int testDurationSeconds);
    virtual ~BWEStabilityTest();

    virtual int Init(std::string ip, uint16_t port);
    virtual void Report(std::fstream &log);

protected:
    virtual bool StoppingCriterionMaster();

private:
    int _testDurationSeconds;
};


#endif // WEBRTC_MODULES_RTP_RTCP_TEST_BWESTANDALONE_BWESTABILITYTEST_H_
