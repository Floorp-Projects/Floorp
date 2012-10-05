/*
 *  Copyright (c) 2012 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef WEBRTC_MODULES_VIDEO_CODING_CODECS_VP8_VP8_UNITTEST_H_
#define WEBRTC_MODULES_VIDEO_CODING_CODECS_VP8_VP8_UNITTEST_H_

#include "modules/video_coding/codecs/test_framework/unit_test.h"

class VP8UnitTest : public UnitTest
{
public:
    VP8UnitTest();
    VP8UnitTest(std::string name, std::string description);
    virtual void Perform();

protected:
    virtual WebRtc_UWord32 CodecSpecific_SetBitrate(
        WebRtc_UWord32 bitRate,
        WebRtc_UWord32 /*frameRate*/);
};

#endif // WEBRTC_MODULES_VIDEO_CODING_CODECS_VP8_VP8_UNITTEST_H_
