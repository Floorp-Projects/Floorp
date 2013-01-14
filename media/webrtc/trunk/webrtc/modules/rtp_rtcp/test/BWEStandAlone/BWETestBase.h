/*
 *  Copyright (c) 2011 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef WEBRTC_MODULES_RTP_RTCP_TEST_BWESTANDALONE_BWETESTBASE_H_
#define WEBRTC_MODULES_RTP_RTCP_TEST_BWESTANDALONE_BWETESTBASE_H_

#include <string>
#include <vector>

#include "typedefs.h"

#include "TestSenderReceiver.h"


class StatVec : public std::vector<double>
{
public:
    double Mean();
    double Variance();
    double Std();
    double Max();
    double Min();
    double Median();
    double Percentile(double p); // 0 <= p <= 100%
    void Export(std::fstream &file, bool colVec = false);
};


class BWETest : public SendRecCB
{
public:
    BWETest(std::string testName, int startRateKbps);
    virtual ~BWETest();

    bool SetMaster(bool isMaster = true);
    void UseRecvTimeout() { _sendrec->SetPacketTimeout(1000); };
    virtual int Init(std::string ip, WebRtc_UWord16 port);
    virtual bool Start();
    virtual bool Stop();
    bool ProcLoop(void);
    virtual void Report(std::fstream &log);
    std::string TestName() { return (_testName); };

    // SenderReceiver callback
    virtual void OnOnNetworkChanged(const WebRtc_UWord32 bitrateTargetBps,
        const WebRtc_UWord8 fractionLost,
        const WebRtc_UWord16 roundTripTimeMs,
        const WebRtc_UWord32 jitterMS,
        const WebRtc_UWord16 bwEstimateKbitMin,
        const WebRtc_UWord16 bwEstimateKbitMax);


protected:
    virtual bool StoppingCriterionMaster() = 0;
    virtual bool StoppingCriterionSlave() { return (_sendrec->timeOutTriggered()); };

    TestSenderReceiver * _sendrec;
    TestLoadGenerator * _gen;

    std::string _testName;
    int _startRateKbps;
    bool _master;
    bool _initialized;
    bool _started;
    bool _running;
    EventWrapper *_eventPtr;
    ThreadWrapper* _procThread;
    WebRtc_Word64 _startTimeMs;
    WebRtc_Word64 _stopTimeMs;

    // Statistics, protected by separate CritSect
    CriticalSectionWrapper* _statCritSect;
    StatVec _rateVecKbps;
    StatVec _rttVecMs;
    StatVec _lossVec;
};


class BWEOneWayTest : public BWETest
{
public:
    BWEOneWayTest(std::string testName, int startRateKbps) :
      BWETest(testName, startRateKbps) {};

    virtual int Init(std::string ip, WebRtc_UWord16 port);
    virtual bool Start();

protected:
    virtual bool StoppingCriterionSlave() {return ( _sendrec->timeOutTriggered()); };

private:

};

#endif // WEBRTC_MODULES_RTP_RTCP_TEST_BWESTANDALONE_BWETESTBASE_H_
