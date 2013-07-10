/*
 *  Copyright (c) 2012 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "normal_test.h"

#include <time.h>
#include <sstream>
#include <string.h>

#include "common_video/libyuv/include/webrtc_libyuv.h"
#include "gtest/gtest.h"
#include "testsupport/fileutils.h"

NormalTest::NormalTest()
:
CodecTest("Normal Test 1", "A test of normal execution of the codec"),
_testNo(1),
_lengthEncFrame(0),
_appendNext(false)
{
}

NormalTest::NormalTest(std::string name, std::string description,
                       unsigned int testNo)
:
CodecTest(name, description),
_requestKeyFrame(false),
_testNo(testNo),
_lengthEncFrame(0),
_appendNext(false)
{
}

NormalTest::NormalTest(std::string name, std::string description,
                       uint32_t bitRate, unsigned int testNo)
:
CodecTest(name, description, bitRate),
_requestKeyFrame(false),
_testNo(testNo),
_lengthEncFrame(0),
_appendNext(false)
{
}

void
NormalTest::Setup()
{
    CodecTest::Setup();
    std::stringstream ss;
    std::string strTestNo;
    ss << _testNo;
    ss >> strTestNo;

    // Check if settings exist. Otherwise use defaults.
    if (_outname == "")
    {
        _outname = webrtc::test::OutputPath() + "out_normaltest" + strTestNo +
            ".yuv";
    }

    if (_encodedName == "")
    {
        _encodedName = webrtc::test::OutputPath() + "encoded_normaltest" +
            strTestNo + ".yuv";
    }
    
    if ((_sourceFile = fopen(_inname.c_str(), "rb")) == NULL)
    {
        printf("Cannot read file %s.\n", _inname.c_str());
        exit(1);
    }

    if ((_encodedFile = fopen(_encodedName.c_str(), "wb")) == NULL)
    {
        printf("Cannot write encoded file.\n");
        exit(1);
    }

    char mode[3] = "wb";
    if (_appendNext)
    {
        strncpy(mode, "ab", 3);
    }

    if ((_decodedFile = fopen(_outname.c_str(), mode)) == NULL)
    {
        printf("Cannot write file %s.\n", _outname.c_str());
        exit(1);
    }

    _appendNext = true;
}

void
NormalTest::Teardown()
{
    CodecTest::Teardown();
    fclose(_sourceFile);
    fclose(_decodedFile);
}

void
NormalTest::Perform()
{
    _width = 352;
    _halfWidth = (_width + 1) / 2;
    _height = 288;
    _halfHeight = (_height + 1) / 2;
    _sizeY = _width * _height;
    _sizeUv = _halfWidth * _halfHeight;
    _inname = webrtc::test::ProjectRootPath() + "resources/foreman_cif.yuv";
    CodecSettings(_width, _height, 30, _bitRate);
    Setup();

    _inputVideoBuffer.CreateEmptyFrame(_width, _height,
                                       _width, _halfWidth, _halfWidth);
    _decodedVideoBuffer.CreateEmptyFrame(_width, _height,
                                         _width, _halfWidth, _halfWidth);
    _encodedVideoBuffer.VerifyAndAllocate(_lengthSourceFrame);

    _encoder->InitEncode(&_inst, 1, 1460);
    CodecSpecific_InitBitrate();
    _decoder->InitDecode(&_inst,1);

    _totalEncodeTime = _totalDecodeTime = 0;
    _framecnt = 0;
    _sumEncBytes = 0;
    _lengthEncFrame = 0;
    int decodeLength = 0;
    while (!Encode())
    {
        DoPacketLoss();
        _encodedVideoBuffer.SetLength(_encodedVideoBuffer.Length());
        if (fwrite(_encodedVideoBuffer.Buffer(), 1,
                   _encodedVideoBuffer.Length(),
                   _encodedFile) !=  _encodedVideoBuffer.Length()) {
          return;
        }
        decodeLength = Decode();
        if (decodeLength < 0)
        {
            fprintf(stderr,"\n\nError in decoder: %d\n\n", decodeLength);
            exit(EXIT_FAILURE);
        }
        if (PrintI420VideoFrame(_decodedVideoBuffer, _decodedFile) < 0) {
          return;
        }
        CodecSpecific_InitBitrate();
        _framecnt++;
    }

    // Ensure we empty the decoding queue.
    while (decodeLength > 0)
    {
        decodeLength = Decode();
        if (decodeLength < 0)
        {
            fprintf(stderr,"\n\nError in decoder: %d\n\n", decodeLength);
            exit(EXIT_FAILURE);
        }
        if (PrintI420VideoFrame(_decodedVideoBuffer, _decodedFile) < 0) {
          return;
        }
    }

    double actualBitRate = ActualBitRate(_framecnt) / 1000.0;
    double avgEncTime = _totalEncodeTime / _framecnt;
    double avgDecTime = _totalDecodeTime / _framecnt;
    printf("Actual bitrate: %f kbps\n", actualBitRate);
    printf("Average encode time: %f s\n", avgEncTime);
    printf("Average decode time: %f s\n", avgDecTime);
    (*_log) << "Actual bitrate: " << actualBitRate << " kbps\tTarget: " << _bitRate << " kbps" << std::endl;
    (*_log) << "Average encode time: " << avgEncTime << " s" << std::endl;
    (*_log) << "Average decode time: " << avgDecTime << " s" << std::endl;

    _encoder->Release();
    _decoder->Release();

    Teardown();
}

bool
NormalTest::Encode()
{
    _lengthEncFrame = 0;
    EXPECT_GT(fread(_sourceBuffer, 1, _lengthSourceFrame, _sourceFile), 0u);
    if (feof(_sourceFile) != 0)
    {
        return true;
    }
        _inputVideoBuffer.CreateFrame(_sizeY, _sourceBuffer,
                                      _sizeUv, _sourceBuffer + _sizeY,
                                      _sizeUv, _sourceBuffer + _sizeY +
                                      _sizeUv,
                                      _width, _height,
                                      _width, _halfWidth, _halfWidth);
    _inputVideoBuffer.set_timestamp(_framecnt);

    // This multiple attempt ridiculousness is to accomodate VP7:
    // 1. The wrapper can unilaterally reduce the framerate for low bitrates.
    // 2. The codec inexplicably likes to reject some frames. Perhaps there
    //    is a good reason for this...
    int encodingAttempts = 0;
    double starttime = 0;
    double endtime = 0;
    while (_lengthEncFrame == 0)
    {
        starttime = clock()/(double)CLOCKS_PER_SEC;

        _inputVideoBuffer.set_width(_inst.width);
        _inputVideoBuffer.set_height(_inst.height);
        endtime = clock()/(double)CLOCKS_PER_SEC;

        _encodedVideoBuffer.SetHeight(_inst.height);
        _encodedVideoBuffer.SetWidth(_inst.width);
        if (_lengthEncFrame < 0)
        {
            (*_log) << "Error in encoder: " << _lengthEncFrame << std::endl;
            fprintf(stderr,"\n\nError in encoder: %d\n\n", _lengthEncFrame);
            exit(EXIT_FAILURE);
        }
        _sumEncBytes += _lengthEncFrame;

        encodingAttempts++;
        if (encodingAttempts > 50)
        {
            (*_log) << "Unable to encode frame: " << _framecnt << std::endl;
            fprintf(stderr,"\n\nUnable to encode frame: %d\n\n", _framecnt);
            exit(EXIT_FAILURE);
        }
    }
    _totalEncodeTime += endtime - starttime;

    if (encodingAttempts > 1)
    {
        (*_log) << encodingAttempts << " attempts required to encode frame: " <<
            _framecnt + 1 << std::endl;
        fprintf(stderr,"\n%d attempts required to encode frame: %d\n", encodingAttempts,
            _framecnt + 1);
    }
        
    return false;
}

int
NormalTest::Decode(int lossValue)
{
    _encodedVideoBuffer.SetWidth(_inst.width);
    _encodedVideoBuffer.SetHeight(_inst.height);
    int lengthDecFrame = 0;
    if (lengthDecFrame < 0)
    {
        return lengthDecFrame;
    }
    _encodedVideoBuffer.SetLength(0);
    return lengthDecFrame;
}
