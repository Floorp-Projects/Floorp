/*
 *  Copyright (c) 2012 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef WEBRTC_MODULES_VIDEO_CODING_CODECS_TEST_FRAMEWORK_UNIT_TEST_H_
#define WEBRTC_MODULES_VIDEO_CODING_CODECS_TEST_FRAMEWORK_UNIT_TEST_H_

#include "webrtc/modules/video_coding/codecs/test_framework/test.h"
#include "webrtc/system_wrappers/interface/event_wrapper.h"

// Disable "conditional expression is constant" warnings on the perfectly
// acceptable
// do { ... } while (0) constructions below.
// Refer to http://stackoverflow.com/questions/1946445/
//   is-there-better-way-to-write-do-while0-construct-to-avoid-compiler-warnings
// for some discussion of the issue.
#ifdef _WIN32
#pragma warning(disable : 4127)
#endif

class VideoSource;
class UnitTestEncodeCompleteCallback;
class UnitTestDecodeCompleteCallback;

class UnitTest : public CodecTest
{
public:
    UnitTest();
    virtual ~UnitTest();
    virtual void Setup();
    virtual void Perform();
    virtual void Print();

protected:
    UnitTest(std::string name, std::string description);
    virtual uint32_t CodecSpecific_SetBitrate(
        uint32_t bitRate,
        uint32_t /* frameRate */);
    virtual void Teardown();
    virtual void RateControlTests();
    virtual int Decode();
    virtual int DecodeWithoutAssert();
    virtual int SetCodecSpecificParameters() {return 0;};

    virtual bool CheckIfBitExact(const void *ptrA, unsigned int aLengthBytes,
                                 const void *ptrB, unsigned int bLengthBytes);

    uint32_t WaitForEncodedFrame() const;
    uint32_t WaitForDecodedFrame() const;

    int _tests;
    int _errors;

    VideoSource* _source;
    unsigned char* _refFrame;
    unsigned char* _refEncFrame;
    unsigned char* _refDecFrame;
    unsigned int _refEncFrameLength;
    FILE* _sourceFile;
    bool is_key_frame_;

    UnitTestEncodeCompleteCallback* _encodeCompleteCallback;
    UnitTestDecodeCompleteCallback* _decodeCompleteCallback;
    enum { kMaxWaitEncTimeMs = 100 };
    enum { kMaxWaitDecTimeMs = 25 };
};

class UnitTestEncodeCompleteCallback : public webrtc::EncodedImageCallback
{
public:
    UnitTestEncodeCompleteCallback(webrtc::VideoFrame* buffer,
                                   uint32_t decoderSpecificSize = 0,
                                   void* decoderSpecificInfo = NULL) :
      _encodedVideoBuffer(buffer),
      _encodeComplete(false) {}
    int32_t Encoded(webrtc::EncodedImage& encodedImage,
                    const webrtc::CodecSpecificInfo* codecSpecificInfo,
                    const webrtc::RTPFragmentationHeader* fragmentation = NULL);
    bool EncodeComplete();
    // Note that this only makes sense if an encode has been completed
    webrtc::VideoFrameType EncodedFrameType() const;
private:
    webrtc::VideoFrame* _encodedVideoBuffer;
    bool _encodeComplete;
    webrtc::VideoFrameType _encodedFrameType;
};

class UnitTestDecodeCompleteCallback : public webrtc::DecodedImageCallback
{
public:
    UnitTestDecodeCompleteCallback(webrtc::I420VideoFrame* buffer) :
        _decodedVideoBuffer(buffer), _decodeComplete(false) {}
    int32_t Decoded(webrtc::I420VideoFrame& image);
    bool DecodeComplete();
private:
    webrtc::I420VideoFrame* _decodedVideoBuffer;
    bool _decodeComplete;
};

#endif // WEBRTC_MODULES_VIDEO_CODING_CODECS_TEST_FRAMEWORK_UNIT_TEST_H_
