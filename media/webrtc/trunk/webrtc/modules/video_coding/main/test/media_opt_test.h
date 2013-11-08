/*
 *  Copyright (c) 2012 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

// VCM Media Optimization Test
#ifndef WEBRTC_MODULES_VIDEO_CODING_TEST_MEDIA_OPT_TEST_H_
#define WEBRTC_MODULES_VIDEO_CODING_TEST_MEDIA_OPT_TEST_H_


#include <string>

#include "webrtc/modules/rtp_rtcp/interface/rtp_rtcp.h"
#include "webrtc/modules/video_coding/main/interface/video_coding.h"
#include "webrtc/modules/video_coding/main/test/receiver_tests.h"  // receive side callbacks
#include "webrtc/modules/video_coding/main/test/test_callbacks.h"
#include "webrtc/modules/video_coding/main/test/test_util.h"
#include "webrtc/modules/video_coding/main/test/video_source.h"

// media optimization test
// This test simulates a complete encode-decode cycle via the RTP module.
// allows error resilience tests, packet loss tests, etc.
// Does not test the media optimization deirectly, but via the VCM API only.
// The test allows two modes:
// 1 - Standard, basic settings, one run
// 2 - Release test - iterates over a number of video sequences, bit rates, packet loss values ,etc.

class MediaOptTest
{
public:
    MediaOptTest(webrtc::VideoCodingModule* vcm,
                 webrtc::Clock* clock);
    ~MediaOptTest();

    static int RunTest(int testNum, CmdArgs& args);
    // perform encode-decode of an entire sequence
    int32_t Perform();
    // Set up for a single mode test
    void Setup(int testType, CmdArgs& args);
    // General set up - applicable for both modes
    void GeneralSetup();
    // Run release testing
    void RTTest();
    void TearDown();
    // mode = 1; will print to screen, otherwise only to log file
    void Print(int mode);

private:

    webrtc::VideoCodingModule*       _vcm;
    webrtc::RtpReceiver*             rtp_receiver_;
    webrtc::RtpRtcp*                 _rtp;
    webrtc::RTPSendCompleteCallback* _outgoingTransport;
    RtpDataCallback*                 _dataCallback;

    webrtc::Clock*                   _clock;
    std::string                      _inname;
    std::string                      _outname;
    std::string                      _actualSourcename;
    std::fstream                     _log;
    FILE*                            _sourceFile;
    FILE*                            _decodedFile;
    FILE*                            _actualSourceFile;
    FILE*                            _outputRes;
    uint16_t                   _width;
    uint16_t                   _height;
    uint32_t                   _lengthSourceFrame;
    uint32_t                   _timeStamp;
    float                            _frameRate;
    bool                             _nackEnabled;
    bool                             _fecEnabled;
    bool                             _nackFecEnabled;
    uint8_t                    _rttMS;
    float                            _bitRate;
    double                           _lossRate;
    uint32_t                   _renderDelayMs;
    int32_t                    _frameCnt;
    float                            _sumEncBytes;
    int32_t                    _numFramesDropped;
    std::string                      _codecName;
    webrtc::VideoCodecType           _sendCodecType;
    int32_t                    _numberOfCores;

    //for release test#2
    FILE*                            _fpinp;
    FILE*                            _fpout;
    FILE*                            _fpout2;
    int                              _testType;
    int                              _testNum;
    int                              _numParRuns;

}; // end of MediaOptTest class definition

#endif // WEBRTC_MODULES_VIDEO_CODING_TEST_MEDIA_OPT_TEST_H_
