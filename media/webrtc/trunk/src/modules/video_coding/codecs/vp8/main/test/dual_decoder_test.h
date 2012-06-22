/*
 *  Copyright (c) 2011 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef WEBRTC_MODULES_VIDEO_CODING_CODECS_VP8_DUAL_DECODER_TEST_H_
#define WEBRTC_MODULES_VIDEO_CODING_CODECS_VP8_DUAL_DECODER_TEST_H_

#include "vp8.h"
#include "normal_async_test.h"

class DualDecoderCompleteCallback;

class VP8DualDecoderTest : public VP8NormalAsyncTest
{
public:
    VP8DualDecoderTest(float bitRate);
    VP8DualDecoderTest();
    virtual ~VP8DualDecoderTest();
    virtual void Perform();
protected:
    VP8DualDecoderTest(std::string name, std::string description,
                       unsigned int testNo)
    : VP8NormalAsyncTest(name, description, testNo) {}
    virtual int Decode(int lossValue = 0);

    webrtc::VP8Decoder*     _decoder2;
    TestVideoBuffer         _decodedVideoBuffer2;
    static bool CheckIfBitExact(const void *ptrA, unsigned int aLengthBytes, 
        const void *ptrB, unsigned int bLengthBytes);
private:
};

class DualDecoderCompleteCallback : public webrtc::DecodedImageCallback
{
public:
    DualDecoderCompleteCallback(TestVideoBuffer* buffer)
    : _decodedVideoBuffer(buffer), _decodeComplete(false) {}
    WebRtc_Word32 Decoded(webrtc::RawImage& decodedImage);
    bool DecodeComplete();
private:
    TestVideoBuffer* _decodedVideoBuffer;
    bool _decodeComplete;
};


#endif
