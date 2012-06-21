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

#include "typedefs.h"
#include "module_common_types.h"

class TestSenderReceiver;
namespace webrtc {
class CriticalSectionWrapper;
class EventWrapper;
class ThreadWrapper;
}

class TestLoadGenerator
{
public:
    TestLoadGenerator (TestSenderReceiver *sender, WebRtc_Word32 rtpSampleRate = 90000);
    virtual ~TestLoadGenerator ();

    WebRtc_Word32 SetBitrate (WebRtc_Word32 newBitrateKbps);
    virtual WebRtc_Word32 Start (const char *threadName = NULL);
    virtual WebRtc_Word32 Stop ();
    virtual bool GeneratorLoop () = 0;

protected:
    virtual int generatePayload ( WebRtc_UWord32 timestamp ) = 0;
    int generatePayload ();
    int sendPayload (const WebRtc_UWord32 timeStamp,
        const WebRtc_UWord8* payloadData,
        const WebRtc_UWord32 payloadSize,
        const webrtc::FrameType frameType = webrtc::kVideoFrameDelta);

    webrtc::CriticalSectionWrapper* _critSect;
    webrtc::EventWrapper *_eventPtr;
    webrtc::ThreadWrapper* _genThread;
    WebRtc_Word32 _bitrateKbps;
    TestSenderReceiver *_sender;
    bool _running;
    WebRtc_Word32 _rtpSampleRate;
};


class CBRGenerator : public TestLoadGenerator
{
public:
    CBRGenerator (TestSenderReceiver *sender, WebRtc_Word32 payloadSizeBytes, WebRtc_Word32 bitrateKbps, WebRtc_Word32 rtpSampleRate = 90000);
    virtual ~CBRGenerator ();

    virtual WebRtc_Word32 Start () {return (TestLoadGenerator::Start("CBRGenerator"));};

    virtual bool GeneratorLoop ();

protected:
    virtual int generatePayload ( WebRtc_UWord32 timestamp );

    WebRtc_Word32 _payloadSizeBytes;
    WebRtc_UWord8 *_payload;
};


class CBRFixFRGenerator : public TestLoadGenerator // constant bitrate and fixed frame rate
{
public:
    CBRFixFRGenerator (TestSenderReceiver *sender, WebRtc_Word32 bitrateKbps, WebRtc_Word32 rtpSampleRate = 90000,
        WebRtc_Word32 frameRateFps = 30, double spread = 0.0);
    virtual ~CBRFixFRGenerator ();

    virtual WebRtc_Word32 Start () {return (TestLoadGenerator::Start("CBRFixFRGenerator"));};

    virtual bool GeneratorLoop ();

protected:
    virtual WebRtc_Word32 nextPayloadSize ();
    virtual int generatePayload ( WebRtc_UWord32 timestamp );

    WebRtc_Word32 _payloadSizeBytes;
    WebRtc_UWord8 *_payload;
    WebRtc_Word32 _payloadAllocLen;
    WebRtc_Word32 _frameRateFps;
    double      _spreadFactor;
};

class PeriodicKeyFixFRGenerator : public CBRFixFRGenerator // constant bitrate and fixed frame rate with periodically large frames
{
public:
    PeriodicKeyFixFRGenerator (TestSenderReceiver *sender, WebRtc_Word32 bitrateKbps, WebRtc_Word32 rtpSampleRate = 90000,
        WebRtc_Word32 frameRateFps = 30, double spread = 0.0, double keyFactor = 4.0, WebRtc_UWord32 keyPeriod = 300);
    virtual ~PeriodicKeyFixFRGenerator () {}

protected:
    virtual WebRtc_Word32 nextPayloadSize ();

    double          _keyFactor;
    WebRtc_UWord32    _keyPeriod;
    WebRtc_UWord32    _frameCount;
};

// Probably better to inherit CBRFixFRGenerator from CBRVarFRGenerator, but since
// the fix FR version already existed this was easier.
class CBRVarFRGenerator : public CBRFixFRGenerator // constant bitrate and variable frame rate
{
public:
    CBRVarFRGenerator(TestSenderReceiver *sender, WebRtc_Word32 bitrateKbps, const WebRtc_UWord8* frameRates,
        WebRtc_UWord16 numFrameRates, WebRtc_Word32 rtpSampleRate = 90000, double avgFrPeriodMs = 5.0,
        double frSpreadFactor = 0.05, double spreadFactor = 0.0);

    ~CBRVarFRGenerator();

protected:
    virtual void ChangeFrameRate();
    virtual WebRtc_Word32 nextPayloadSize ();

    double       _avgFrPeriodMs;
    double       _frSpreadFactor;
    WebRtc_UWord8* _frameRates;
    WebRtc_UWord16 _numFrameRates;
    WebRtc_Word64  _frChangeTimeMs;
};

class CBRFrameDropGenerator : public CBRFixFRGenerator // constant bitrate and variable frame rate
{
public:
    CBRFrameDropGenerator(TestSenderReceiver *sender, WebRtc_Word32 bitrateKbps,
                    WebRtc_Word32 rtpSampleRate = 90000, double spreadFactor = 0.0);

    ~CBRFrameDropGenerator();

protected:
    virtual WebRtc_Word32 nextPayloadSize();

    double       _accBits;
};

#endif // WEBRTC_MODULES_RTP_RTCP_TEST_BWESTANDALONE_TESTLOADGENERATOR_H_
