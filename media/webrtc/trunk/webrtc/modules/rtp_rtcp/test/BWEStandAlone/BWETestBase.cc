/*
 *  Copyright (c) 2011 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "webrtc/modules/rtp_rtcp/test/BWEStandAlone/BWETestBase.h"

#include <algorithm> // sort
#include <fstream>
#include <math.h>
#include <string>
#include <vector>

#include "webrtc/modules/rtp_rtcp/test/BWEStandAlone/TestLoadGenerator.h"
#include "webrtc/modules/rtp_rtcp/test/BWEStandAlone/TestSenderReceiver.h"
#include "webrtc/system_wrappers/interface/critical_section_wrapper.h"
#include "webrtc/system_wrappers/interface/event_wrapper.h"
#include "webrtc/system_wrappers/interface/thread_wrapper.h"
#include "webrtc/system_wrappers/interface/tick_util.h"


double StatVec::Mean()
{
    double sum = 0;

    // sanity
    if (size() <= 0) return (0);

    std::vector<double>::iterator it;
    for (it = begin(); it < end(); ++it)
    {
        sum += (*it);
    }

    return (sum / size());
}

double StatVec::Variance()
{
    double sumSqaure = 0;
    double sum = 0;

    std::vector<double>::iterator it;
    for (it = begin(); it < end(); ++it)
    {
        sum += (*it);
        sumSqaure += (*it) * (*it);
    }

    // Normalizes by N-1. This produces the best unbiased estimate of the
    // variance if X is a sample from a normal distribution.
    int M = static_cast<int> (size() - 1);

    if (M > 0)
    {
        double var = (sumSqaure / M) - (sum / (M+1)) * (sum / M);
        assert(var >= 0);
        return (var);
    }
    else
    {
        return (0);
    }
}

double StatVec::Std()
{
    return (sqrt(Variance()));
}

double StatVec::Max()
{
    // sanity
    if (size() <= 0) return (0);

    std::vector<double>::iterator it = begin();
    double maxVal = (*it);
    ++it;

    for (; it < end(); ++it)
    {
        if ((*it) > maxVal) maxVal = (*it);
    }

    return (maxVal);
}

double StatVec::Min()
{
    // sanity
    if (size() <= 0) return (0);

    std::vector<double>::iterator it = begin();
    double minVal = (*it);
    ++it;

    for (; it < end(); ++it)
    {
        if ((*it) < minVal) minVal = (*it);
    }

    return (minVal);
}

double StatVec::Median()
{
    double median;

    // sanity
    if (size() <= 0) return (0);

    // sort the vector
    sort(begin(), end());

    if ((size() % 2) == 0)
    {
        // even size; use average of two center elements
        median = (at(size()/2 - 1) + at(size()/2)) / 2.0;
    }
    else
    {
        // odd size; take center element
        median = at(size()/2);
    }

    return (median);
}

double StatVec::Percentile(double p)
{
    // sanity
    if (size() <= 0) return (0);

    // sort the vector
    sort(begin(), end());

    int rank = static_cast<int> (((size() - 1) * p) / 100 + 0.5); // between 1 and size()
    rank -= 1; // between 0 and size()-1

    assert(rank >= 0);
    assert(rank < static_cast<int>(size()));

    return (at(rank));
}

void StatVec::Export(std::fstream &file, bool colVec /*= false*/)
{
    // sanity
    if (size() <= 0) return;

    std::string separator;
    if (colVec) separator = "\n";
    else separator = ", ";

    std::vector<double>::iterator it = begin();
    file << (*it);
    ++it;

    for (; it < end(); ++it)
    {
        file << separator << (*it);
    }

    file << std::endl;
}


bool BWETestProcThreadFunction(void *obj)
{
    if (obj == NULL)
    {
        return false;
    }
    BWETest *theObj = static_cast<BWETest *>(obj);

    theObj->ProcLoop();

    theObj->Stop();

    return(true);
}


BWETest::BWETest(std::string testName, int startRateKbps):
_testName(testName),
_startRateKbps(startRateKbps),
_master(false),
_sendrec(NULL),
_gen(NULL),
_initialized(false),
_started(false),
_running(false),
_eventPtr(NULL),
_procThread(NULL),
_startTimeMs(-1),
_stopTimeMs(-1),
_statCritSect(CriticalSectionWrapper::CreateCriticalSection())
{
    _sendrec = new TestSenderReceiver();
}


BWETest::~BWETest()
{
    if (_running)
    {
        Stop();
    }

    _statCritSect->Enter();
    delete &_statCritSect;

    if (_sendrec)
    {
        delete _sendrec;
        _sendrec = NULL;
    }
}


bool BWETest::SetMaster(bool isMaster /*= true*/)
{
    if (!_initialized)
    {
        // Can only set status before initializing.
        _master = isMaster;
    }

    return (_master);
}


int BWETest::Init(std::string ip, uint16_t port)
{
    if (_initialized)
    {
        // cannot init twice
        return (-1);
    }

    if (!_sendrec)
    {
        throw "SenderReceiver must be created";
        exit(1);
    }

    if (_started)
    {
        // cannot init after start
        return (-1);
    }

    // initialize receiver port (for feedback)
    _sendrec->InitReceiver(port);

    // initialize sender
    _sendrec->SetLoadGenerator(_gen);
    _sendrec->InitSender(_startRateKbps, ip.c_str(), port);
    //_gen->Start();

    _sendrec->SetCallback(this);

    _initialized = true;

    return 0;
}


bool BWETest::Start()
{
    if (!_initialized)
    {
        // must init first
        return (false);
    }
    if (_started)
    {
        // already started, do nothing
        return (true);
    }

    if (_sendrec->Start() != 0)
    {
        // failed
        return (false);
    }

    if (_gen)
    {
        if (_gen->Start() != 0)
        {
            // failed
            return (false);
        }
    }

    _eventPtr = EventWrapper::Create();

    _startTimeMs = TickTime::MillisecondTimestamp();
    _started = true;
    _running = true;

    return (true);
}


bool BWETest::Stop()
{
    if (_procThread)
    {
        _stopTimeMs = TickTime::MillisecondTimestamp();
        _procThread->SetNotAlive();
        _running = false;
        _eventPtr->Set();

        while (!_procThread->Stop())
        {
            ;
        }

        delete _procThread;
        _procThread = NULL;

    }

    if (_eventPtr)
    {
        delete _eventPtr;
        _eventPtr = NULL;
    }

    _procThread = NULL;

    if(_gen)
    {
        _gen->Stop();
    }

    return(true);
}


bool BWETest::ProcLoop(void)
{
    bool receiving = false;

    // no critSect
    while (_running)
    {

        // check stopping criterions
        if (_master && StoppingCriterionMaster())
        {
            printf("StoppingCriterionMaster()\n");
            _stopTimeMs = TickTime::MillisecondTimestamp();
            _running = false;
        }
        else if (!_master && StoppingCriterionSlave())
        {
            printf("StoppingCriterionSlave()\n");
            _running = false;
        }

        // wait
        _eventPtr->Wait(1000); // 1000 ms

    }

    return true;
}


void BWETest::Report(std::fstream &log)
{
    // cannot report on a running test
    if(_running) return;

    CriticalSectionScoped cs(_statCritSect);

    log << "\n\n*** Test name = " << _testName << "\n";
    log << "Execution time = " <<  static_cast<double>(_stopTimeMs - _startTimeMs) / 1000 << " s\n";
    log << "\n";
    log << "RTT statistics\n";
    log << "\tMin     = " << _rttVecMs.Min() << " ms\n";
    log << "\tMax     = " << _rttVecMs.Max() << " ms\n";
    log << "\n";
    log << "Loss statistics\n";
    log << "\tAverage = " << _lossVec.Mean() << "%\n";
    log << "\tMax     = " << _lossVec.Max() << "%\n";

    log << "\n" << "Rates" << "\n";
    _rateVecKbps.Export(log);

    log << "\n" << "RTT" << "\n";
    _rttVecMs.Export(log);

}


// SenderReceiver callback
void BWETest::OnOnNetworkChanged(const uint32_t bitrateTargetBps,
                                 const uint8_t fractionLost,
                                 const uint16_t roundTripTimeMs,
                                 const uint32_t jitterMS,
                                 const uint16_t bwEstimateKbitMin,
                                 const uint16_t bwEstimateKbitMax)
{
    CriticalSectionScoped cs(_statCritSect);

    // bitrate statistics
    int32_t newBitrateKbps = bitrateTargetBps/1000;

    _rateVecKbps.push_back(newBitrateKbps);
    _rttVecMs.push_back(roundTripTimeMs);
    _lossVec.push_back(static_cast<double>(fractionLost) / 255.0);
}


int BWEOneWayTest::Init(std::string ip, uint16_t port)
{

    if (!_master)
    {
        // Use timeout stopping criterion by default for receiver
        UseRecvTimeout();
    }

    return (BWETest::Init(ip, port));

}


bool BWEOneWayTest::Start()
{
    bool ret = BWETest::Start();

    if (!_master)
    {
        // send one dummy RTP packet to enable RTT measurements
        const uint8_t dummy = 0;
        //_gen->sendPayload(TickTime::MillisecondTimestamp(), &dummy, 0);
        _sendrec->SendOutgoingData(
            static_cast<uint32_t>(TickTime::MillisecondTimestamp()*90),
            &dummy, 1, webrtc::kVideoFrameDelta);
    }

    return ret;
}
