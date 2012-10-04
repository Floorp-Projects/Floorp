/*
 *  Copyright (c) 2012 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef WEBRTC_MODULES_VIDEO_CODING_CODECS_TEST_FRAMEWORK_NORMAL_TEST_H_
#define WEBRTC_MODULES_VIDEO_CODING_CODECS_TEST_FRAMEWORK_NORMAL_TEST_H_

#include "test.h"

class NormalTest : public CodecTest
{
public:
    NormalTest();
    NormalTest(std::string name, std::string description, unsigned int testNo);
    NormalTest(std::string name, std::string description, WebRtc_UWord32 bitRate, unsigned int testNo);
    virtual ~NormalTest() {};
    virtual void Perform();

protected:
    virtual void Setup();
    virtual void Teardown();
    virtual bool Encode();
    virtual int Decode(int lossValue = 0);
    virtual void CodecSpecific_InitBitrate()=0;
    virtual int DoPacketLoss() {return 0;};

    FILE*                   _sourceFile;
    FILE*                   _decodedFile;
    FILE*                   _encodedFile;
    double                  _totalEncodeTime;
    double                  _totalDecodeTime;
    unsigned int            _framecnt;
    bool                    _requestKeyFrame;
    unsigned int            _testNo;
    int                     _lengthEncFrame;
    bool                    _appendNext;
};

#endif // WEBRTC_MODULES_VIDEO_CODING_CODECS_TEST_FRAMEWORK_NORMAL_TEST_H_

