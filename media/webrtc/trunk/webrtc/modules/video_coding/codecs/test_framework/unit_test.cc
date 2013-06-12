/*
 *  Copyright (c) 2012 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include <math.h>
#include <stdlib.h>
#include <string.h>

#include <cassert>

#include "gtest/gtest.h"
#include "testsupport/fileutils.h"
#include "tick_util.h"
#include "unit_test.h"
#include "video_source.h"

using namespace webrtc;

UnitTest::UnitTest()
:
CodecTest("UnitTest", "Unit test"),
_tests(0),
_errors(0),
_source(NULL),
_refFrame(NULL),
_refEncFrame(NULL),
_refDecFrame(NULL),
_refEncFrameLength(0),
_sourceFile(NULL),
_encodeCompleteCallback(NULL),
_decodeCompleteCallback(NULL)
{
}

UnitTest::UnitTest(std::string name, std::string description)
:
CodecTest(name, description),
_tests(0),
_errors(0),
_source(NULL),
_refFrame(NULL),
_refEncFrame(NULL),
_refDecFrame(NULL),
_refEncFrameLength(0),
_sourceFile(NULL),
_encodeCompleteCallback(NULL),
_decodeCompleteCallback(NULL)
{
}

UnitTest::~UnitTest()
{
    if (_encodeCompleteCallback) {
        delete _encodeCompleteCallback;
    }

    if (_decodeCompleteCallback) {
        delete _decodeCompleteCallback;
    }

    if (_source) {
        delete _source;
    }

    if (_refFrame) {
        delete [] _refFrame;
    }

    if (_refDecFrame) {
        delete [] _refDecFrame;
    }

    if (_sourceBuffer) {
        delete [] _sourceBuffer;
    }

    if (_sourceFile) {
        fclose(_sourceFile);
    }

    if (_refEncFrame) {
        delete [] _refEncFrame;
    }
}

int32_t
UnitTestEncodeCompleteCallback::Encoded(EncodedImage& encodedImage,
                                        const webrtc::CodecSpecificInfo* codecSpecificInfo,
                                        const webrtc::RTPFragmentationHeader*
                                        fragmentation)
{
    _encodedVideoBuffer->VerifyAndAllocate(encodedImage._size);
    _encodedVideoBuffer->CopyFrame(encodedImage._size, encodedImage._buffer);
    _encodedVideoBuffer->SetLength(encodedImage._length);
    // TODO(mikhal): Update frame type API.
    // _encodedVideoBuffer->SetFrameType(encodedImage._frameType);
    _encodedVideoBuffer->SetWidth(
        (uint16_t)encodedImage._encodedWidth);
    _encodedVideoBuffer->SetHeight(
        (uint16_t)encodedImage._encodedHeight);
    _encodedVideoBuffer->SetTimeStamp(encodedImage._timeStamp);
    _encodeComplete = true;
    _encodedFrameType = encodedImage._frameType;
    return 0;
}

int32_t UnitTestDecodeCompleteCallback::Decoded(I420VideoFrame& image)
{
    _decodedVideoBuffer->CopyFrame(image);
    _decodeComplete = true;
    return 0;
}

bool
UnitTestEncodeCompleteCallback::EncodeComplete()
{
    if (_encodeComplete)
    {
        _encodeComplete = false;
        return true;
    }
    return false;
}

VideoFrameType
UnitTestEncodeCompleteCallback::EncodedFrameType() const
{
    return _encodedFrameType;
}

bool
UnitTestDecodeCompleteCallback::DecodeComplete()
{
    if (_decodeComplete)
    {
        _decodeComplete = false;
        return true;
    }
    return false;
}

uint32_t
UnitTest::WaitForEncodedFrame() const
{
    int64_t startTime = TickTime::MillisecondTimestamp();
    while (TickTime::MillisecondTimestamp() - startTime < kMaxWaitEncTimeMs)
    {
        if (_encodeCompleteCallback->EncodeComplete())
        {
          return _encodedVideoBuffer.Length();
        }
    }
    return 0;
}

uint32_t
UnitTest::WaitForDecodedFrame() const
{
    int64_t startTime = TickTime::MillisecondTimestamp();
    while (TickTime::MillisecondTimestamp() - startTime < kMaxWaitDecTimeMs)
    {
        if (_decodeCompleteCallback->DecodeComplete())
        {
          return webrtc::CalcBufferSize(kI420, _decodedVideoBuffer.width(),
                                        _decodedVideoBuffer.height());
        }
    }
    return 0;
}

uint32_t
UnitTest::CodecSpecific_SetBitrate(uint32_t bitRate,
                                   uint32_t /* frameRate */)
{
    return _encoder->SetRates(bitRate, _inst.maxFramerate);
}

void
UnitTest::Setup()
{
    // Use _sourceFile as a check to prevent multiple Setup() calls.
    if (_sourceFile != NULL)
    {
        return;
    }

    if (_encodeCompleteCallback == NULL)
    {
        _encodeCompleteCallback =
            new UnitTestEncodeCompleteCallback(&_encodedVideoBuffer);
    }
    if (_decodeCompleteCallback == NULL)
    {
        _decodeCompleteCallback =
            new UnitTestDecodeCompleteCallback(&_decodedVideoBuffer);
    }

    _encoder->RegisterEncodeCompleteCallback(_encodeCompleteCallback);
    _decoder->RegisterDecodeCompleteCallback(_decodeCompleteCallback);

    _source = new VideoSource(webrtc::test::ProjectRootPath() +
                              "resources/foreman_cif.yuv", kCIF);

    _lengthSourceFrame = _source->GetFrameLength();
    _refFrame = new unsigned char[_lengthSourceFrame];
    _refDecFrame = new unsigned char[_lengthSourceFrame];
    _sourceBuffer = new unsigned char [_lengthSourceFrame];
    _sourceFile = fopen(_source->GetFileName().c_str(), "rb");
    ASSERT_TRUE(_sourceFile != NULL);

    _inst.maxFramerate = _source->GetFrameRate();
    _bitRate = 300;
    _inst.startBitrate = 300;
    _inst.maxBitrate = 4000;
    _inst.width = _source->GetWidth();
    _inst.height = _source->GetHeight();
    _inst.qpMax = 56;
    _inst.codecSpecific.VP8.denoisingOn = true;

    // Get input frame.
    ASSERT_TRUE(fread(_refFrame, 1, _lengthSourceFrame, _sourceFile)
                           == _lengthSourceFrame);
    int size_y = _inst.width * _inst.height;
    int size_uv = ((_inst.width + 1) / 2)  * ((_inst.height + 1) / 2);
    _inputVideoBuffer.CreateFrame(size_y, _refFrame,
                                  size_uv, _refFrame + size_y,
                                  size_uv, _refFrame + size_y + size_uv,
                                  _inst.width, _inst.height,
                                  _inst.width,
                                  (_inst.width + 1) / 2, (_inst.width + 1) / 2);
    rewind(_sourceFile);

    // Get a reference encoded frame.
    _encodedVideoBuffer.VerifyAndAllocate(_lengthSourceFrame);

    // Ensures our initial parameters are valid.
    EXPECT_TRUE(_encoder->InitEncode(&_inst, 1, 1440) == WEBRTC_VIDEO_CODEC_OK);
    _encoder->Encode(_inputVideoBuffer, NULL, NULL);
    _refEncFrameLength = WaitForEncodedFrame();
    ASSERT_TRUE(_refEncFrameLength > 0);
    _refEncFrame = new unsigned char[_refEncFrameLength];
    memcpy(_refEncFrame, _encodedVideoBuffer.Buffer(), _refEncFrameLength);

    // Get a reference decoded frame.
    _decodedVideoBuffer.CreateEmptyFrame(_inst.width, _inst.height, _inst.width,
                                         (_inst.width + 1) / 2,
                                         (_inst.width + 1) / 2);
    EXPECT_TRUE(_decoder->InitDecode(&_inst, 1) == WEBRTC_VIDEO_CODEC_OK);
    ASSERT_FALSE(SetCodecSpecificParameters() != WEBRTC_VIDEO_CODEC_OK);

    unsigned int frameLength = 0;
    int i=0;
    _inputVideoBuffer.CreateEmptyFrame(_inst.width, _inst.height, _inst.width,
                                       (_inst.width + 1) / 2,
                                       (_inst.width + 1) / 2);
    while (frameLength == 0)
    {
        if (i > 0)
        {
            // Insert yet another frame
            ASSERT_TRUE(fread(_refFrame, 1, _lengthSourceFrame,
                _sourceFile) == _lengthSourceFrame);
            EXPECT_EQ(0, ConvertToI420(kI420, _refFrame, 0, 0, _width, _height,
                          0, kRotateNone, &_inputVideoBuffer));
            _encoder->Encode(_inputVideoBuffer, NULL, NULL);
            ASSERT_TRUE(WaitForEncodedFrame() > 0);
        }
        EncodedImage encodedImage;
        VideoEncodedBufferToEncodedImage(_encodedVideoBuffer, encodedImage);
        ASSERT_TRUE(_decoder->Decode(encodedImage, 0, NULL)
                               == WEBRTC_VIDEO_CODEC_OK);
        frameLength = WaitForDecodedFrame();
        _encodedVideoBuffer.SetLength(0);
        i++;
    }
    rewind(_sourceFile);
    EXPECT_TRUE(frameLength == _lengthSourceFrame);
    ExtractBuffer(_decodedVideoBuffer, _lengthSourceFrame, _refDecFrame);
}

void
UnitTest::Teardown()
{
    // Use _sourceFile as a check to prevent multiple Teardown() calls.
    if (_sourceFile == NULL)
    {
        return;
    }

    _encoder->Release();
    _decoder->Release();

    fclose(_sourceFile);
    _sourceFile = NULL;
    delete [] _refFrame;
    _refFrame = NULL;
    delete [] _refEncFrame;
    _refEncFrame = NULL;
    delete [] _refDecFrame;
    _refDecFrame = NULL;
    delete [] _sourceBuffer;
    _sourceBuffer = NULL;
}

void
UnitTest::Print()
{
}

int
UnitTest::DecodeWithoutAssert()
{
    EncodedImage encodedImage;
    VideoEncodedBufferToEncodedImage(_encodedVideoBuffer, encodedImage);
    int ret = _decoder->Decode(encodedImage, 0, NULL);
    int frameLength = WaitForDecodedFrame();
    _encodedVideoBuffer.SetLength(0);
    return ret == WEBRTC_VIDEO_CODEC_OK ? frameLength : ret;
}

int
UnitTest::Decode()
{
    EncodedImage encodedImage;
    VideoEncodedBufferToEncodedImage(_encodedVideoBuffer, encodedImage);
    if (encodedImage._length == 0)
    {
        return WEBRTC_VIDEO_CODEC_OK;
    }
    int ret = _decoder->Decode(encodedImage, 0, NULL);
    unsigned int frameLength = WaitForDecodedFrame();
    assert(ret == WEBRTC_VIDEO_CODEC_OK && (frameLength == 0 || frameLength
        == _lengthSourceFrame));
    EXPECT_TRUE(ret == WEBRTC_VIDEO_CODEC_OK && (frameLength == 0 || frameLength
        == _lengthSourceFrame));
    _encodedVideoBuffer.SetLength(0);
    return ret == WEBRTC_VIDEO_CODEC_OK ? frameLength : ret;
}

// Test pure virtual VideoEncoder and VideoDecoder APIs.
void
UnitTest::Perform()
{
    UnitTest::Setup();
    int frameLength;
    I420VideoFrame inputImage;
    EncodedImage encodedImage;

    //----- Encoder parameter tests -----

    //-- Calls before InitEncode() --
    // We want to revert the initialization done in Setup().
    EXPECT_TRUE(_encoder->Release() == WEBRTC_VIDEO_CODEC_OK);
    EXPECT_TRUE(_encoder->Encode(_inputVideoBuffer, NULL, NULL)
               == WEBRTC_VIDEO_CODEC_UNINITIALIZED);

    //-- InitEncode() errors --
    // Null pointer.
    EXPECT_TRUE(_encoder->InitEncode(NULL, 1, 1440) ==
        WEBRTC_VIDEO_CODEC_ERR_PARAMETER);
    // bit rate exceeds max bit rate
    int32_t tmpBitRate = _inst.startBitrate;
    int32_t tmpMaxBitRate = _inst.maxBitrate;
    _inst.startBitrate = 4000;
    _inst.maxBitrate = 3000;
    EXPECT_TRUE(_encoder->InitEncode(&_inst, 1, 1440)  ==
        WEBRTC_VIDEO_CODEC_ERR_PARAMETER);
    _inst.startBitrate = tmpBitRate;
    _inst.maxBitrate = tmpMaxBitRate; //unspecified value

    // Bad framerate.
    _inst.maxFramerate = 0;
    EXPECT_TRUE(_encoder->InitEncode(&_inst, 1, 1440) ==
        WEBRTC_VIDEO_CODEC_ERR_PARAMETER);
    // Seems like we should allow any framerate in range [0, 255].
    //_inst.frameRate = 100;
    //EXPECT_TRUE(_encoder->InitEncode(&_inst, 1) == -1); // FAILS
    _inst.maxFramerate = 30;

    // Bad bitrate.
    _inst.startBitrate = -1;
    EXPECT_TRUE(_encoder->InitEncode(&_inst, 1, 1440) ==
        WEBRTC_VIDEO_CODEC_ERR_PARAMETER);
    _inst.maxBitrate = _inst.startBitrate - 1;
    EXPECT_TRUE(_encoder->InitEncode(&_inst, 1, 1440) ==
        WEBRTC_VIDEO_CODEC_ERR_PARAMETER);
    _inst.maxBitrate = 0;
    _inst.startBitrate = 300;

    // Bad maxBitRate.
    _inst.maxBitrate = 200;
    EXPECT_TRUE(_encoder->InitEncode(&_inst, 1, 1440) ==
        WEBRTC_VIDEO_CODEC_ERR_PARAMETER);
    _inst.maxBitrate = 4000;

    // Bad width.
    _inst.width = 0;
    EXPECT_TRUE(_encoder->InitEncode(&_inst, 1, 1440) < 0);
    _inst.width = _source->GetWidth();

    // Bad height.
    _inst.height = 0;
    EXPECT_TRUE(_encoder->InitEncode(&_inst, 1, 1440) < 0);
    _inst.height = _source->GetHeight();

    // Bad number of cores.
    EXPECT_TRUE(_encoder->InitEncode(&_inst, -1, 1440) ==
        WEBRTC_VIDEO_CODEC_ERR_PARAMETER);

    EXPECT_TRUE(_encoder->InitEncode(&_inst, 1, 1440) == WEBRTC_VIDEO_CODEC_OK);

    //-- Encode() errors --
    inputImage.ResetSize();
    EXPECT_TRUE(_encoder->Encode(inputImage, NULL, NULL) ==
        WEBRTC_VIDEO_CODEC_ERR_PARAMETER);
    int width = _source->GetWidth();
    int half_width = (width + 1) / 2;
    int height = _source->GetHeight();
    int half_height = (height + 1) / 2;
    int size_y = width * height;
    int size_uv = half_width * half_height;
    _inputVideoBuffer.CreateFrame(size_y, _refFrame,
                                  size_uv, _refFrame + size_y,
                                  size_uv, _refFrame + size_y + size_uv,
                                  width, height,
                                  width, half_width, half_width);
    //----- Encoder stress tests -----

    // Vary frame rate and I-frame request.
    for (int i = 1; i <= 60; i++)
    {
        VideoFrameType frame_type = !(i % 2) ? kKeyFrame : kDeltaFrame;
        std::vector<VideoFrameType> frame_types(1, frame_type);
        EXPECT_TRUE(_encoder->Encode(_inputVideoBuffer, NULL, &frame_types) ==
            WEBRTC_VIDEO_CODEC_OK);
        EXPECT_TRUE(WaitForEncodedFrame() > 0);
    }

    // Init then encode.
    _encodedVideoBuffer.SetLength(0);
    EXPECT_TRUE(_encoder->Encode(_inputVideoBuffer, NULL, NULL) ==
        WEBRTC_VIDEO_CODEC_OK);
    EXPECT_TRUE(WaitForEncodedFrame() > 0);

    EXPECT_TRUE(_encoder->InitEncode(&_inst, 1, 1440) == WEBRTC_VIDEO_CODEC_OK);
    _encoder->Encode(_inputVideoBuffer, NULL, NULL);
    frameLength = WaitForEncodedFrame();
    EXPECT_TRUE(frameLength > 0);
    EXPECT_TRUE(CheckIfBitExact(_refEncFrame, _refEncFrameLength,
            _encodedVideoBuffer.Buffer(), frameLength) == true);

    // Reset then encode.
    _encodedVideoBuffer.SetLength(0);
    EXPECT_TRUE(_encoder->Encode(_inputVideoBuffer, NULL, NULL) ==
        WEBRTC_VIDEO_CODEC_OK);
    WaitForEncodedFrame();
    EXPECT_TRUE(_encoder->InitEncode(&_inst, 1, 1440) == WEBRTC_VIDEO_CODEC_OK);
    _encoder->Encode(_inputVideoBuffer, NULL, NULL);
    frameLength = WaitForEncodedFrame();
    EXPECT_TRUE(frameLength > 0);
    EXPECT_TRUE(CheckIfBitExact(_refEncFrame, _refEncFrameLength,
        _encodedVideoBuffer.Buffer(), frameLength) == true);

    // Release then encode.
    _encodedVideoBuffer.SetLength(0);
    EXPECT_TRUE(_encoder->Encode(_inputVideoBuffer, NULL, NULL) ==
        WEBRTC_VIDEO_CODEC_OK);
    WaitForEncodedFrame();
    EXPECT_TRUE(_encoder->Release() == WEBRTC_VIDEO_CODEC_OK);
    EXPECT_TRUE(_encoder->InitEncode(&_inst, 1, 1440) == WEBRTC_VIDEO_CODEC_OK);
    _encoder->Encode(_inputVideoBuffer, NULL, NULL);
    frameLength = WaitForEncodedFrame();
    EXPECT_TRUE(frameLength > 0);
    EXPECT_TRUE(CheckIfBitExact(_refEncFrame, _refEncFrameLength,
        _encodedVideoBuffer.Buffer(), frameLength) == true);

    //----- Decoder parameter tests -----

    //-- Calls before InitDecode() --
    // We want to revert the initialization done in Setup().
    EXPECT_TRUE(_decoder->Release() == WEBRTC_VIDEO_CODEC_OK);
    VideoEncodedBufferToEncodedImage(_encodedVideoBuffer, encodedImage);
    EXPECT_TRUE(_decoder->Decode(encodedImage, false, NULL) ==
        WEBRTC_VIDEO_CODEC_UNINITIALIZED);
    WaitForDecodedFrame();
    EXPECT_TRUE(_decoder->Reset() == WEBRTC_VIDEO_CODEC_UNINITIALIZED);
    EXPECT_TRUE(_decoder->InitDecode(&_inst, 1) == WEBRTC_VIDEO_CODEC_OK);
    ASSERT_FALSE(SetCodecSpecificParameters() != WEBRTC_VIDEO_CODEC_OK);

    //-- Decode() errors --
    // Unallocated encodedVideoBuffer.
    _encodedVideoBuffer.Free();
    VideoEncodedBufferToEncodedImage(_encodedVideoBuffer, encodedImage);
    encodedImage._length = 10;  // Buffer NULL but length > 0
    EXPECT_EQ(_decoder->Decode(encodedImage, false, NULL),
              WEBRTC_VIDEO_CODEC_ERR_PARAMETER);
    _encodedVideoBuffer.VerifyAndAllocate(_lengthSourceFrame);

    //----- Decoder stress tests -----
    unsigned char* tmpBuf = new unsigned char[_lengthSourceFrame];

    // "Random" and zero data.
    // We either expect an error, or at the least, no output.
    // This relies on the codec's ability to detect an erroneous bitstream.
    EXPECT_TRUE(_decoder->Reset() == WEBRTC_VIDEO_CODEC_OK);
    EXPECT_TRUE(_decoder->InitDecode(&_inst, 1) == WEBRTC_VIDEO_CODEC_OK);
    ASSERT_FALSE(SetCodecSpecificParameters() != WEBRTC_VIDEO_CODEC_OK);
    for (int i = 0; i < 100; i++)
    {
        ASSERT_TRUE(fread(tmpBuf, 1, _refEncFrameLength, _sourceFile)
            == _refEncFrameLength);
        _encodedVideoBuffer.CopyFrame(_refEncFrameLength, tmpBuf);
        VideoEncodedBufferToEncodedImage(_encodedVideoBuffer, encodedImage);
        int ret = _decoder->Decode(encodedImage, false, NULL);
        EXPECT_TRUE(ret <= 0);
        if (ret == 0)
        {
            EXPECT_TRUE(WaitForDecodedFrame() == 0);
        }

        memset(tmpBuf, 0, _refEncFrameLength);
        _encodedVideoBuffer.CopyFrame(_refEncFrameLength, tmpBuf);
        VideoEncodedBufferToEncodedImage(_encodedVideoBuffer, encodedImage);
        ret = _decoder->Decode(encodedImage, false, NULL);
        EXPECT_TRUE(ret <= 0);
        if (ret == 0)
        {
            EXPECT_TRUE(WaitForDecodedFrame() == 0);
        }
    }
    rewind(_sourceFile);

    _encodedVideoBuffer.SetLength(_refEncFrameLength);
    _encodedVideoBuffer.CopyFrame(_refEncFrameLength, _refEncFrame);

    // Init then decode.
    EXPECT_TRUE(_decoder->InitDecode(&_inst, 1) == WEBRTC_VIDEO_CODEC_OK);
    ASSERT_FALSE(SetCodecSpecificParameters() != WEBRTC_VIDEO_CODEC_OK);
    frameLength = 0;
    VideoEncodedBufferToEncodedImage(_encodedVideoBuffer, encodedImage);
    while (frameLength == 0)
    {
        _decoder->Decode(encodedImage, false, NULL);
        frameLength = WaitForDecodedFrame();
    }
    unsigned int length = CalcBufferSize(kI420, width, height);
    scoped_array<uint8_t> decoded_buffer(new uint8_t[length]);
    ExtractBuffer(_decodedVideoBuffer, _lengthSourceFrame,
                  decoded_buffer.get());
    EXPECT_TRUE(CheckIfBitExact(decoded_buffer.get(), frameLength, _refDecFrame,
                                _lengthSourceFrame) == true);

    // Reset then decode.
    EXPECT_TRUE(_decoder->Reset() == WEBRTC_VIDEO_CODEC_OK);
    frameLength = 0;
    VideoEncodedBufferToEncodedImage(_encodedVideoBuffer, encodedImage);
    while (frameLength == 0)
    {
        _decoder->Decode(encodedImage, false, NULL);
        frameLength = WaitForDecodedFrame();
    }
    ExtractBuffer(_decodedVideoBuffer, _lengthSourceFrame,
                  decoded_buffer.get());
    EXPECT_TRUE(CheckIfBitExact(decoded_buffer.get(), frameLength,
                                _refDecFrame, _lengthSourceFrame) == true);

    // Decode with other size, reset, then decode with original size again
    // to verify that decoder is reset to a "fresh" state upon Reset().
    {
        // Assert that input frame size is a factor of two, so that we can use
        // quarter size below.
        EXPECT_TRUE((_inst.width % 2 == 0) && (_inst.height % 2 == 0));

        VideoCodec tempInst;
        memcpy(&tempInst, &_inst, sizeof(VideoCodec));
        tempInst.width /= 2;
        tempInst.height /= 2;
        int tmpHalfWidth = (tempInst.width + 1) / 2;
        int tmpHalfHeight = (tempInst.height + 1) / 2;

        int tmpSizeY = tempInst.width * tempInst.height;
        int tmpSizeUv = tmpHalfWidth * tmpHalfHeight;

        // Encode reduced (quarter) frame size.
        EXPECT_TRUE(_encoder->Release() == WEBRTC_VIDEO_CODEC_OK);
        EXPECT_TRUE(_encoder->InitEncode(&tempInst, 1, 1440) ==
            WEBRTC_VIDEO_CODEC_OK);
        webrtc::I420VideoFrame tempInput;
        tempInput.CreateFrame(tmpSizeY, _inputVideoBuffer.buffer(kYPlane),
                              tmpSizeUv, _inputVideoBuffer.buffer(kUPlane),
                              tmpSizeUv, _inputVideoBuffer.buffer(kVPlane),
                              tempInst.width, tempInst.height,
                              tempInst.width, tmpHalfWidth, tmpHalfWidth);
        _encoder->Encode(tempInput, NULL, NULL);
        frameLength = WaitForEncodedFrame();
        EXPECT_TRUE(frameLength > 0);
        // Reset then decode.
        EXPECT_TRUE(_decoder->Reset() == WEBRTC_VIDEO_CODEC_OK);
        frameLength = 0;
        VideoEncodedBufferToEncodedImage(_encodedVideoBuffer, encodedImage);
        while (frameLength == 0)
        {
            _decoder->Decode(encodedImage, false, NULL);
            frameLength = WaitForDecodedFrame();
        }

        // Encode original frame again
        EXPECT_TRUE(_encoder->Release() == WEBRTC_VIDEO_CODEC_OK);
        EXPECT_TRUE(_encoder->InitEncode(&_inst, 1, 1440) ==
            WEBRTC_VIDEO_CODEC_OK);
        _encoder->Encode(_inputVideoBuffer, NULL, NULL);
        frameLength = WaitForEncodedFrame();
        EXPECT_TRUE(frameLength > 0);

        // Reset then decode original frame again.
        EXPECT_TRUE(_decoder->Reset() == WEBRTC_VIDEO_CODEC_OK);
        frameLength = 0;
        VideoEncodedBufferToEncodedImage(_encodedVideoBuffer, encodedImage);
        while (frameLength == 0)
        {
            _decoder->Decode(encodedImage, false, NULL);
            frameLength = WaitForDecodedFrame();
        }

        // check that decoded frame matches with reference
        unsigned int length = CalcBufferSize(kI420, width, height);
        scoped_array<uint8_t> decoded_buffer(new uint8_t[length]);
        ExtractBuffer(_decodedVideoBuffer, length, decoded_buffer.get());
        EXPECT_TRUE(CheckIfBitExact(decoded_buffer.get(), length,
                                    _refDecFrame, _lengthSourceFrame) == true);
    }

    // Release then decode.
    EXPECT_TRUE(_decoder->Release() == WEBRTC_VIDEO_CODEC_OK);
    EXPECT_TRUE(_decoder->InitDecode(&_inst, 1) == WEBRTC_VIDEO_CODEC_OK);
    ASSERT_FALSE(SetCodecSpecificParameters() != WEBRTC_VIDEO_CODEC_OK);
    frameLength = 0;
    VideoEncodedBufferToEncodedImage(_encodedVideoBuffer, encodedImage);
    while (frameLength == 0)
    {
        _decoder->Decode(encodedImage, false, NULL);
        frameLength = WaitForDecodedFrame();
    }
    ExtractBuffer(_decodedVideoBuffer, length, decoded_buffer.get());
    EXPECT_TRUE(CheckIfBitExact(decoded_buffer.get(), frameLength,
                                _refDecFrame, _lengthSourceFrame) == true);
    _encodedVideoBuffer.SetLength(0);

    delete [] tmpBuf;

    //----- Function tests -----
    int frames = 0;
    // Do not specify maxBitRate (as in ViE).
    _inst.maxBitrate = 0;

    //-- Timestamp propagation --
    EXPECT_TRUE(_encoder->InitEncode(&_inst, 1, 1440) == WEBRTC_VIDEO_CODEC_OK);
    EXPECT_TRUE(_decoder->Reset() == WEBRTC_VIDEO_CODEC_OK);
    EXPECT_TRUE(_decoder->InitDecode(&_inst, 1) == WEBRTC_VIDEO_CODEC_OK);
    ASSERT_FALSE(SetCodecSpecificParameters() != WEBRTC_VIDEO_CODEC_OK);

    frames = 0;
    int frameDelay = 0;
    int encTimeStamp;
    _decodedVideoBuffer.set_timestamp(0);
    while (fread(_sourceBuffer, 1, _lengthSourceFrame, _sourceFile) ==
        _lengthSourceFrame)
    {
      _inputVideoBuffer.CreateFrame(size_y, _sourceBuffer,
                                    size_uv, _sourceBuffer + size_y,
                                    size_uv, _sourceBuffer + size_y + size_uv,
                                    width, height,
                                    width, half_width, half_width);

        _inputVideoBuffer.set_timestamp(frames);
        ASSERT_TRUE(_encoder->Encode(_inputVideoBuffer, NULL, NULL) ==
            WEBRTC_VIDEO_CODEC_OK);
        frameLength = WaitForEncodedFrame();
        //ASSERT_TRUE(frameLength);
        EXPECT_TRUE(frameLength > 0);
        encTimeStamp = _encodedVideoBuffer.TimeStamp();
        EXPECT_TRUE(_inputVideoBuffer.timestamp() ==
                static_cast<unsigned>(encTimeStamp));

        frameLength = Decode();
        if (frameLength == 0)
        {
            frameDelay++;
        }

        encTimeStamp -= frameDelay;
        if (encTimeStamp < 0)
        {
            encTimeStamp = 0;
        }
        EXPECT_TRUE(_decodedVideoBuffer.timestamp() ==
                static_cast<unsigned>(encTimeStamp));
        frames++;
    }
    ASSERT_TRUE(feof(_sourceFile) != 0);
    rewind(_sourceFile);

    RateControlTests();

    Teardown();
}

void
UnitTest::RateControlTests()
{
    int frames = 0;
    VideoFrame inputImage;
    uint32_t frameLength;

    // Do not specify maxBitRate (as in ViE).
    _inst.maxBitrate = 0;
    // Verify rate control. For this test turn on codec frame dropper.
    // At least one other test (BasicUnitTest) assumes frame dropper off, so
    // for now we only set frame dropper on for this (rate control) test.
    _inst.codecSpecific.VP8.frameDroppingOn = true;
    EXPECT_TRUE(_encoder->InitEncode(&_inst, 1, 1440) == WEBRTC_VIDEO_CODEC_OK);
    EXPECT_TRUE(_decoder->Reset() == WEBRTC_VIDEO_CODEC_OK);
    EXPECT_TRUE(_decoder->InitDecode(&_inst, 1) == WEBRTC_VIDEO_CODEC_OK);
    // add: should also be 0, and 1
    const int bitRate[] = {100, 500};
    const int nBitrates = sizeof(bitRate)/sizeof(*bitRate);

    printf("\nRate control test\n");
    for (int i = 0; i < nBitrates; i++)
    {
        _bitRate = bitRate[i];
        int totalBytes = 0;
        _inst.startBitrate = _bitRate;
        _encoder->InitEncode(&_inst, 4, 1440);
        _decoder->Reset();
        _decoder->InitDecode(&_inst, 1);
        frames = 0;

        if (_bitRate > _inst.maxBitrate)
        {
            CodecSpecific_SetBitrate(_bitRate, _inst.maxFramerate);
        }
        else
        {
            CodecSpecific_SetBitrate(_bitRate, _inst.maxFramerate);
        }
        int width = _source->GetWidth();
        int half_width = (width + 1) / 2;
        int height = _source->GetHeight();
        int half_height = (height + 1) / 2;
        int size_y = width * height;
        int size_uv = half_width * half_height;
        while (fread(_sourceBuffer, 1, _lengthSourceFrame, _sourceFile) ==
            _lengthSourceFrame)
        {
            _inputVideoBuffer.CreateFrame(size_y, _sourceBuffer,
                                           size_uv, _sourceBuffer + size_y,
                                           size_uv, _sourceBuffer + size_y +
                                           size_uv,
                                           width, height,
                                           width, half_width, half_width);
            _inputVideoBuffer.set_timestamp(static_cast<uint32_t>(9e4 /
                    static_cast<float>(_inst.maxFramerate)));
            ASSERT_EQ(_encoder->Encode(_inputVideoBuffer, NULL, NULL),
                      WEBRTC_VIDEO_CODEC_OK);
            frameLength = WaitForEncodedFrame();
            ASSERT_GE(frameLength, 0u);
            totalBytes += frameLength;
            frames++;

            _encodedVideoBuffer.SetLength(0);
        }
        uint32_t actualBitrate =
            (totalBytes  / frames * _inst.maxFramerate * 8)/1000;
        printf("Target bitrate: %d kbps, actual bitrate: %d kbps\n", _bitRate,
            actualBitrate);
        // Test for close match over reasonable range.
          EXPECT_TRUE(abs(int32_t(actualBitrate - _bitRate)) <
                      0.12 * _bitRate);
        ASSERT_TRUE(feof(_sourceFile) != 0);
        rewind(_sourceFile);
    }
}

bool
UnitTest::CheckIfBitExact(const void* ptrA, unsigned int aLengthBytes,
                          const void* ptrB, unsigned int bLengthBytes)
{
    if (aLengthBytes != bLengthBytes)
    {
        return false;
    }

    return memcmp(ptrA, ptrB, aLengthBytes) == 0;
}
