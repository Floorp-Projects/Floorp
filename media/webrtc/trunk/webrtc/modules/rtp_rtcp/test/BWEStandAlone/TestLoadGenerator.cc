/*
 *  Copyright (c) 2011 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "webrtc/modules/rtp_rtcp/test/BWEStandAlone/TestLoadGenerator.h"

#include <stdio.h>

#include <algorithm>

#include "webrtc/modules/rtp_rtcp/test/BWEStandAlone/TestSenderReceiver.h"
#include "webrtc/system_wrappers/interface/critical_section_wrapper.h"
#include "webrtc/system_wrappers/interface/event_wrapper.h"
#include "webrtc/system_wrappers/interface/thread_wrapper.h"
#include "webrtc/system_wrappers/interface/tick_util.h"


bool SenderThreadFunction(void *obj)
{
    if (obj == NULL)
    {
        return false;
    }
    TestLoadGenerator *_genObj = static_cast<TestLoadGenerator *>(obj);

    return _genObj->GeneratorLoop();
}


TestLoadGenerator::TestLoadGenerator(TestSenderReceiver *sender, int32_t rtpSampleRate)
:
_critSect(CriticalSectionWrapper::CreateCriticalSection()),
_eventPtr(NULL),
_genThread(NULL),
_bitrateKbps(0),
_sender(sender),
_running(false),
_rtpSampleRate(rtpSampleRate)
{
}

TestLoadGenerator::~TestLoadGenerator ()
{
    if (_running)
    {
        Stop();
    }

    delete _critSect;
}

int32_t TestLoadGenerator::SetBitrate (int32_t newBitrateKbps)
{
    CriticalSectionScoped cs(_critSect);

    if (newBitrateKbps < 0)
    {
        return -1;
    }

    _bitrateKbps = newBitrateKbps;

    printf("New bitrate = %i kbps\n", _bitrateKbps);

    return _bitrateKbps;
}


int32_t TestLoadGenerator::Start (const char *threadName)
{
    CriticalSectionScoped cs(_critSect);

    _eventPtr = EventWrapper::Create();

    _genThread = ThreadWrapper::CreateThread(SenderThreadFunction, this, kRealtimePriority, threadName);
    if (_genThread == NULL)
    {
        throw "Unable to start generator thread";
        exit(1);
    }

    _running = true;

    unsigned int tid;
    _genThread->Start(tid);

    return 0;
}


int32_t TestLoadGenerator::Stop ()
{
    _critSect.Enter();

    if (_genThread)
    {
        _genThread->SetNotAlive();
        _running = false;
        _eventPtr->Set();

        while (!_genThread->Stop())
        {
            _critSect.Leave();
            _critSect.Enter();
        }

        delete _genThread;
        _genThread = NULL;

        delete _eventPtr;
        _eventPtr = NULL;
    }

    _genThread = NULL;
    _critSect.Leave();
    return (0);
}


int TestLoadGenerator::generatePayload ()
{
    return(generatePayload( static_cast<uint32_t>( TickTime::MillisecondTimestamp() * _rtpSampleRate / 1000 )));
}


int TestLoadGenerator::sendPayload (const uint32_t timeStamp,
                                    const uint8_t* payloadData,
                                    const uint32_t payloadSize,
                                    const webrtc::FrameType frameType /*= webrtc::kVideoFrameDelta*/)
{

    return (_sender->SendOutgoingData(timeStamp, payloadData, payloadSize, frameType));
}


CBRGenerator::CBRGenerator (TestSenderReceiver *sender, int32_t payloadSizeBytes, int32_t bitrateKbps, int32_t rtpSampleRate)
:
//_eventPtr(NULL),
_payloadSizeBytes(payloadSizeBytes),
_payload(new uint8_t[payloadSizeBytes]),
TestLoadGenerator(sender, rtpSampleRate)
{
    SetBitrate (bitrateKbps);
}

CBRGenerator::~CBRGenerator ()
{
    if (_running)
    {
        Stop();
    }

    if (_payload)
    {
        delete [] _payload;
    }

}

bool CBRGenerator::GeneratorLoop ()
{
    double periodMs;
    int64_t nextSendTime = TickTime::MillisecondTimestamp();


    // no critSect
    while (_running)
    {
        // send data (critSect inside)
        generatePayload( static_cast<uint32_t>(nextSendTime * _rtpSampleRate / 1000) );

        // calculate wait time
        periodMs = 8.0 * _payloadSizeBytes / ( _bitrateKbps );

        nextSendTime = static_cast<int64_t>(nextSendTime + periodMs);

        int32_t waitTime = static_cast<int32_t>(nextSendTime - TickTime::MillisecondTimestamp());
        if (waitTime < 0)
        {
            waitTime = 0;
        }
        // wait
        _eventPtr->Wait(static_cast<int32_t>(waitTime));
    }

    return true;
}

int CBRGenerator::generatePayload ( uint32_t timestamp )
{
    CriticalSectionScoped cs(_critSect);

    //uint8_t *payload = new uint8_t[_payloadSizeBytes];

    int ret = sendPayload(timestamp, _payload, _payloadSizeBytes);

    //delete [] payload;
    return ret;
}




/////////////////////

CBRFixFRGenerator::CBRFixFRGenerator (TestSenderReceiver *sender, int32_t bitrateKbps,
                                      int32_t rtpSampleRate, int32_t frameRateFps /*= 30*/,
                                      double spread /*= 0.0*/)
:
//_eventPtr(NULL),
_payloadSizeBytes(0),
_payload(NULL),
_payloadAllocLen(0),
_frameRateFps(frameRateFps),
_spreadFactor(spread),
TestLoadGenerator(sender, rtpSampleRate)
{
    SetBitrate (bitrateKbps);
}

CBRFixFRGenerator::~CBRFixFRGenerator ()
{
    if (_running)
    {
        Stop();
    }

    if (_payload)
    {
        delete [] _payload;
        _payloadAllocLen = 0;
    }

}

bool CBRFixFRGenerator::GeneratorLoop ()
{
    double periodMs;
    int64_t nextSendTime = TickTime::MillisecondTimestamp();

    _critSect.Enter();

    if (_frameRateFps <= 0)
    {
        return false;
    }

    _critSect.Leave();

    // no critSect
    while (_running)
    {
        _critSect.Enter();

        // calculate payload size
        _payloadSizeBytes = nextPayloadSize();

        if (_payloadSizeBytes > 0)
        {

            if (_payloadAllocLen < _payloadSizeBytes * (1 + _spreadFactor))
            {
                // re-allocate _payload
                if (_payload)
                {
                    delete [] _payload;
                    _payload = NULL;
                }

                _payloadAllocLen = static_cast<int32_t>((_payloadSizeBytes * (1 + _spreadFactor) * 3) / 2 + .5); // 50% extra to avoid frequent re-alloc
                _payload = new uint8_t[_payloadAllocLen];
            }


            // send data (critSect inside)
            generatePayload( static_cast<uint32_t>(nextSendTime * _rtpSampleRate / 1000) );
        }

        _critSect.Leave();

        // calculate wait time
        periodMs = 1000.0 / _frameRateFps;
        nextSendTime = static_cast<int64_t>(nextSendTime + periodMs + 0.5);

        int32_t waitTime = static_cast<int32_t>(nextSendTime - TickTime::MillisecondTimestamp());
        if (waitTime < 0)
        {
            waitTime = 0;
        }
        // wait
        _eventPtr->Wait(waitTime);
    }

    return true;
}

int32_t CBRFixFRGenerator::nextPayloadSize()
{
    const double periodMs = 1000.0 / _frameRateFps;
    return static_cast<int32_t>(_bitrateKbps * periodMs / 8 + 0.5);
}

int CBRFixFRGenerator::generatePayload ( uint32_t timestamp )
{
    CriticalSectionScoped cs(_critSect);

    double factor = ((double) rand() - RAND_MAX/2) / RAND_MAX; // [-0.5; 0.5]
    factor = 1 + 2 * _spreadFactor * factor; // [1 - _spreadFactor ; 1 + _spreadFactor]

    int32_t thisPayloadBytes = static_cast<int32_t>(_payloadSizeBytes * factor);
    // sanity
    if (thisPayloadBytes > _payloadAllocLen)
    {
        thisPayloadBytes = _payloadAllocLen;
    }

    int ret = sendPayload(timestamp, _payload, thisPayloadBytes);
    return ret;
}


/////////////////////

PeriodicKeyFixFRGenerator::PeriodicKeyFixFRGenerator (TestSenderReceiver *sender, int32_t bitrateKbps,
                                                      int32_t rtpSampleRate, int32_t frameRateFps /*= 30*/,
                                                      double spread /*= 0.0*/, double keyFactor /*= 4.0*/, uint32_t keyPeriod /*= 300*/)
:
_keyFactor(keyFactor),
_keyPeriod(keyPeriod),
_frameCount(0),
CBRFixFRGenerator(sender, bitrateKbps, rtpSampleRate, frameRateFps, spread)
{
}

int32_t PeriodicKeyFixFRGenerator::nextPayloadSize()
{
    // calculate payload size for a delta frame
    int32_t payloadSizeBytes = static_cast<int32_t>(1000 * _bitrateKbps / (8.0 * _frameRateFps * (1.0 + (_keyFactor - 1.0) / _keyPeriod)) + 0.5);

    if (_frameCount % _keyPeriod == 0)
    {
        // this is a key frame, scale the payload size
        payloadSizeBytes = static_cast<int32_t>(_keyFactor * _payloadSizeBytes + 0.5);
    }
    _frameCount++;

    return payloadSizeBytes;
}

////////////////////

CBRVarFRGenerator::CBRVarFRGenerator(TestSenderReceiver *sender, int32_t bitrateKbps, const uint8_t* frameRates,
                                     uint16_t numFrameRates, int32_t rtpSampleRate, double avgFrPeriodMs,
                                     double frSpreadFactor, double spreadFactor)
:
_avgFrPeriodMs(avgFrPeriodMs),
_frSpreadFactor(frSpreadFactor),
_frameRates(NULL),
_numFrameRates(numFrameRates),
_frChangeTimeMs(TickTime::MillisecondTimestamp() + _avgFrPeriodMs),
CBRFixFRGenerator(sender, bitrateKbps, rtpSampleRate, frameRates[0], spreadFactor)
{
    _frameRates = new uint8_t[_numFrameRates];
    memcpy(_frameRates, frameRates, _numFrameRates);
}

CBRVarFRGenerator::~CBRVarFRGenerator()
{
    delete [] _frameRates;
}

void CBRVarFRGenerator::ChangeFrameRate()
{
    const int64_t nowMs = TickTime::MillisecondTimestamp();
    if (nowMs < _frChangeTimeMs)
    {
        return;
    }
    // Time to change frame rate
    uint16_t frIndex = static_cast<uint16_t>(static_cast<double>(rand()) / RAND_MAX
                                            * (_numFrameRates - 1) + 0.5) ;
    assert(frIndex < _numFrameRates);
    _frameRateFps = _frameRates[frIndex];
    // Update the next frame rate change time
    double factor = ((double) rand() - RAND_MAX/2) / RAND_MAX; // [-0.5; 0.5]
    factor = 1 + 2 * _frSpreadFactor * factor; // [1 - _frSpreadFactor ; 1 + _frSpreadFactor]
    _frChangeTimeMs = nowMs + static_cast<int64_t>(1000.0 * factor *
                                                   _avgFrPeriodMs + 0.5);

    printf("New frame rate: %d\n", _frameRateFps);
}

int32_t CBRVarFRGenerator::nextPayloadSize()
{
    ChangeFrameRate();
    return CBRFixFRGenerator::nextPayloadSize();
}

////////////////////

CBRFrameDropGenerator::CBRFrameDropGenerator(TestSenderReceiver *sender, int32_t bitrateKbps,
                                         int32_t rtpSampleRate, double spreadFactor)
:
_accBits(0),
CBRFixFRGenerator(sender, bitrateKbps, rtpSampleRate, 30, spreadFactor)
{
}

CBRFrameDropGenerator::~CBRFrameDropGenerator()
{
}

int32_t CBRFrameDropGenerator::nextPayloadSize()
{
    _accBits -= 1000 * _bitrateKbps / _frameRateFps;
    if (_accBits < 0)
    {
        _accBits = 0;
    }
    if (_accBits > 0.3 * _bitrateKbps * 1000)
    {
        //printf("drop\n");
        return 0;
    }
    else
    {
        //printf("keep\n");
        const double periodMs = 1000.0 / _frameRateFps;
        int32_t frameSize = static_cast<int32_t>(_bitrateKbps * periodMs / 8 + 0.5);
        frameSize = std::max(frameSize, static_cast<int32_t>(300 * periodMs / 8 + 0.5));
        _accBits += frameSize * 8;
        return frameSize;
    }
}
