/*
 *  Copyright (c) 2011 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "normal_async_test.h"

using namespace webrtc;

VP8NormalAsyncTest::VP8NormalAsyncTest(WebRtc_UWord32 bitRate) :
    NormalAsyncTest("VP8 Normal Test 1", "Tests VP8 normal execution", bitRate, 1),
    _hasReceivedRPSI(false)
{
}

VP8NormalAsyncTest::VP8NormalAsyncTest(WebRtc_UWord32 bitRate, unsigned int testNo):
    NormalAsyncTest("VP8 Normal Test 1", "Tests VP8 normal execution", bitRate, testNo),
    _hasReceivedRPSI(false)
{
}

void
VP8NormalAsyncTest::CodecSettings(int width, int height, WebRtc_UWord32 frameRate /*=30*/, WebRtc_UWord32 bitRate /*=0*/)
{
    if (bitRate > 0)
    {
        _bitRate = bitRate;

    }else if (_bitRate == 0)
    {
        _bitRate = 600;
    }
    _inst.codecType = kVideoCodecVP8;
    _inst.codecSpecific.VP8.feedbackModeOn = true;
    _inst.codecSpecific.VP8.pictureLossIndicationOn = true;
    _inst.codecSpecific.VP8.complexity = kComplexityNormal;
    _inst.maxFramerate = (unsigned char)frameRate;
    _inst.startBitrate = _bitRate;
    _inst.maxBitrate = 8000;
    _inst.width = width;
    _inst.height = height;
}

void
VP8NormalAsyncTest::CodecSpecific_InitBitrate()
{
    if (_bitRate == 0)
    {
        _encoder->SetRates(600, _inst.maxFramerate);
    }else
    {
         _encoder->SetRates(_bitRate, _inst.maxFramerate);
    }
}

WebRtc_Word32
VP8NormalAsyncTest::ReceivedDecodedReferenceFrame(const WebRtc_UWord64 pictureId)
{
    _pictureIdRPSI = pictureId;
    _hasReceivedRPSI = true;
    return 0;
}

CodecSpecificInfo*
VP8NormalAsyncTest::CreateEncoderSpecificInfo() const
{
    CodecSpecificInfo* vp8CodecSpecificInfo = new CodecSpecificInfo();
    vp8CodecSpecificInfo->codecType = kVideoCodecVP8;
    vp8CodecSpecificInfo->codecSpecific.VP8.hasReceivedRPSI = _hasReceivedRPSI;
    vp8CodecSpecificInfo->codecSpecific.VP8.pictureIdRPSI = _pictureIdRPSI;
    vp8CodecSpecificInfo->codecSpecific.VP8.hasReceivedSLI = _hasReceivedSLI;
    vp8CodecSpecificInfo->codecSpecific.VP8.pictureIdSLI = _pictureIdSLI;

    _hasReceivedSLI = false;
    _hasReceivedRPSI = false;

    return vp8CodecSpecificInfo;
}
