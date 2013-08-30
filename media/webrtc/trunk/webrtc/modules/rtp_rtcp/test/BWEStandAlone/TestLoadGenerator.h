/*
 *  Copyright (c) 2011 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef WEBRTC_MODULES_RTP_RTCP_TEST_BWESTANDALONE_TESTLOADGENERATOR_H_
#define WEBRTC_MODULES_RTP_RTCP_TEST_BWESTANDALONE_TESTLOADGENERATOR_H_

#include <stdlib.h>

#include "webrtc/modules/interface/module_common_types.h"
#include "webrtc/typedefs.h"

class TestSenderReceiver;
namespace webrtc {
class CriticalSectionWrapper;
class EventWrapper;
class ThreadWrapper;
}

class TestLoadGenerator
{
public:
    TestLoadGenerator (TestSenderReceiver *sender, int32_t rtpSampleRate = 90000);
    virtual ~TestLoadGenerator ();

    int32_t SetBitrate (int32_t newBitrateKbps);
    virtual int32_t Start (const char *threadName = NULL);
    virtual int32_t Stop ();
    virtual bool GeneratorLoop () = 0;

protected:
    virtual int generatePayload ( uint32_t timestamp ) = 0;
    int generatePayload ();
    int sendPayload (const uint32_t timeStamp,
        const uint8_t* payloadData,
        const uint32_t payloadSize,
        const webrtc::FrameType frameType = webrtc::kVideoFrameDelta);

    webrtc::CriticalSectionWrapper* _critSect;
    webrtc::EventWrapper *_eventPtr;
    webrtc::ThreadWrapper* _genThread;
    int32_t _bitrateKbps;
    TestSenderReceiver *_sender;
    bool _running;
    int32_t _rtpSampleRate;
};


class CBRGenerator : public TestLoadGenerator
{
public:
    CBRGenerator (TestSenderReceiver *sender, int32_t payloadSizeBytes, int32_t bitrateKbps, int32_t rtpSampleRate = 90000);
    virtual ~CBRGenerator ();

    virtual int32_t Start () {return (TestLoadGenerator::Start("CBRGenerator"));};

    virtual bool GeneratorLoop ();

protected:
    virtual int generatePayload ( uint32_t timestamp );

    int32_t _payloadSizeBytes;
    uint8_t *_payload;
};


class CBRFixFRGenerator : public TestLoadGenerator // constant bitrate and fixed frame rate
{
public:
    CBRFixFRGenerator (TestSenderReceiver *sender, int32_t bitrateKbps, int32_t rtpSampleRate = 90000,
        int32_t frameRateFps = 30, double spread = 0.0);
    virtual ~CBRFixFRGenerator ();

    virtual int32_t Start () {return (TestLoadGenerator::Start("CBRFixFRGenerator"));};

    virtual bool GeneratorLoop ();

protected:
    virtual int32_t nextPayloadSize ();
    virtual int generatePayload ( uint32_t timestamp );

    int32_t _payloadSizeBytes;
    uint8_t *_payload;
    int32_t _payloadAllocLen;
    int32_t _frameRateFps;
    double      _spreadFactor;
};

class PeriodicKeyFixFRGenerator : public CBRFixFRGenerator // constant bitrate and fixed frame rate with periodically large frames
{
public:
    PeriodicKeyFixFRGenerator (TestSenderReceiver *sender, int32_t bitrateKbps, int32_t rtpSampleRate = 90000,
        int32_t frameRateFps = 30, double spread = 0.0, double keyFactor = 4.0, uint32_t keyPeriod = 300);
    virtual ~PeriodicKeyFixFRGenerator () {}

protected:
    virtual int32_t nextPayloadSize ();

    double          _keyFactor;
    uint32_t    _keyPeriod;
    uint32_t    _frameCount;
};

// Probably better to inherit CBRFixFRGenerator from CBRVarFRGenerator, but since
// the fix FR version already existed this was easier.
class CBRVarFRGenerator : public CBRFixFRGenerator // constant bitrate and variable frame rate
{
public:
    CBRVarFRGenerator(TestSenderReceiver *sender, int32_t bitrateKbps, const uint8_t* frameRates,
        uint16_t numFrameRates, int32_t rtpSampleRate = 90000, double avgFrPeriodMs = 5.0,
        double frSpreadFactor = 0.05, double spreadFactor = 0.0);

    ~CBRVarFRGenerator();

protected:
    virtual void ChangeFrameRate();
    virtual int32_t nextPayloadSize ();

    double       _avgFrPeriodMs;
    double       _frSpreadFactor;
    uint8_t* _frameRates;
    uint16_t _numFrameRates;
    int64_t  _frChangeTimeMs;
};

class CBRFrameDropGenerator : public CBRFixFRGenerator // constant bitrate and variable frame rate
{
public:
    CBRFrameDropGenerator(TestSenderReceiver *sender, int32_t bitrateKbps,
                    int32_t rtpSampleRate = 90000, double spreadFactor = 0.0);

    ~CBRFrameDropGenerator();

protected:
    virtual int32_t nextPayloadSize();

    double       _accBits;
};

#endif // WEBRTC_MODULES_RTP_RTCP_TEST_BWESTANDALONE_TESTLOADGENERATOR_H_
