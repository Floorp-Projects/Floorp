/*
 *  Copyright (c) 2011 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef WEBRTC_MODULES_VIDEO_CODING_TEST_CODEC_DATABASE_TEST_H_
#define WEBRTC_MODULES_VIDEO_CODING_TEST_CODEC_DATABASE_TEST_H_

#include "video_coding.h"
#include "test_util.h"

#include <string.h>

/*
Test consists of:
1. Sanity chacks: Send and Receive side (bad input, etc. )
2. Send-side control (encoder registration etc.)
3. Decoder-side control - encode with various encoders, and verify correct decoding
*/

class CodecDataBaseTest
{
public:
    CodecDataBaseTest(webrtc::VideoCodingModule* vcm);
    ~CodecDataBaseTest();
    static int RunTest(CmdArgs& args);
    int32_t Perform(CmdArgs& args);
private:
    void TearDown();
    void Setup(CmdArgs& args);
    void Print();
    webrtc::VideoCodingModule*       _vcm;
    std::string                      _inname;
    std::string                      _outname;
    std::string                      _encodedName;
    FILE*                            _sourceFile;
    FILE*                            _decodedFile;
    FILE*                            _encodedFile;
    uint16_t                   _width;
    uint16_t                   _height;
    uint32_t                   _lengthSourceFrame;
    uint32_t                   _timeStamp;
    float                            _frameRate;
}; // end of codecDBTest class definition

#endif // WEBRTC_MODULES_VIDEO_CODING_TEST_CODEC_DATABASE_TEST_H_
