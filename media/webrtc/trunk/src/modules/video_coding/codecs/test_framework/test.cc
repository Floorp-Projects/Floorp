/*
 *  Copyright (c) 2012 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "test.h"

#include <cstring>
#include <iostream>

#include "testsupport/metrics/video_metrics.h"

using namespace webrtc;

long filesize(const char *filename); // local function defined at end of file

CodecTest::CodecTest(std::string name, std::string description)
:
_bitRate(0),
_inname(""),
_outname(""),
_encodedName(""),
_name(name),
_description(description)
{
    memset(&_inst, 0, sizeof(_inst));
    unsigned int seed = static_cast<unsigned int>(0);
    std::srand(seed);
}

CodecTest::CodecTest(std::string name, std::string description,
                     WebRtc_UWord32 bitRate)
:
_bitRate(bitRate),
_inname(""),
_outname(""),
_encodedName(""),
_name(name),
_description(description)
{
    memset(&_inst, 0, sizeof(_inst));
    unsigned int seed = static_cast<unsigned int>(0);
    std::srand(seed);
}

void
CodecTest::Print()
{
    std::cout << _name << " completed!" << std::endl;
    (*_log) << _name << std::endl;
    (*_log) << _description << std::endl;
    (*_log) << "Input file: " << _inname << std::endl;
    (*_log) << "Output file: " << _outname << std::endl;
    webrtc::test::QualityMetricsResult psnr;
    webrtc::test::QualityMetricsResult ssim;
    I420PSNRFromFiles(_inname.c_str(), _outname.c_str(), _inst.width,
                      _inst.height, &psnr);
    I420SSIMFromFiles(_inname.c_str(), _outname.c_str(), _inst.width,
                      _inst.height, &ssim);

    (*_log) << "PSNR: " << psnr.average << std::endl;
    std::cout << "PSNR: " << psnr.average << std::endl << std::endl;
    (*_log) << "SSIM: " << ssim.average << std::endl;
    std::cout << "SSIM: " << ssim.average << std::endl << std::endl;
    (*_log) << std::endl;
}

void
CodecTest::Setup()
{
    int widhei          = _inst.width*_inst.height;
    _lengthSourceFrame  = 3*widhei/2;
    _sourceBuffer       = new unsigned char[_lengthSourceFrame];
}

void
CodecTest::CodecSettings(int width, int height,
                         WebRtc_UWord32 frameRate /*=30*/,
                         WebRtc_UWord32 bitRate /*=0*/)
{
    if (bitRate > 0)
    {
        _bitRate = bitRate;
    }
    else if (_bitRate == 0)
    {
        _bitRate = 600;
    }
    _inst.codecType = kVideoCodecVP8;
    _inst.codecSpecific.VP8.feedbackModeOn = true;
    _inst.maxFramerate = (unsigned char)frameRate;
    _inst.startBitrate = (int)_bitRate;
    _inst.maxBitrate = 8000;
    _inst.width = width;
    _inst.height = height;
}

void
CodecTest::Teardown()
{
    delete [] _sourceBuffer;
}

void
CodecTest::SetEncoder(webrtc::VideoEncoder*encoder)
{
    _encoder = encoder;
}

void
CodecTest::SetDecoder(VideoDecoder*decoder)
{
    _decoder = decoder;
}

void
CodecTest::SetLog(std::fstream* log)
{
    _log = log;
}

double CodecTest::ActualBitRate(int nFrames)
{
    return 8.0 * _sumEncBytes / (nFrames / _inst.maxFramerate);
}

bool CodecTest::PacketLoss(double lossRate, int /*thrown*/)
{
    return RandUniform() < lossRate;
}

void
CodecTest::VideoBufferToRawImage(TestVideoBuffer& videoBuffer,
                                 VideoFrame &image)
{
  // TODO(mikhal): Use videoBuffer in lieu of TestVideoBuffer.
  image.CopyFrame(videoBuffer.GetLength(), videoBuffer.GetBuffer());
  image.SetWidth(videoBuffer.GetWidth());
  image.SetHeight(videoBuffer.GetHeight());
  image.SetTimeStamp(videoBuffer.GetTimeStamp());
}
void
CodecTest::VideoEncodedBufferToEncodedImage(TestVideoEncodedBuffer& videoBuffer,
                                            EncodedImage &image)
{
    image._buffer = videoBuffer.GetBuffer();
    image._length = videoBuffer.GetLength();
    image._size = videoBuffer.GetSize();
    image._frameType = static_cast<VideoFrameType>(videoBuffer.GetFrameType());
    image._timeStamp = videoBuffer.GetTimeStamp();
    image._encodedWidth = videoBuffer.GetCaptureWidth();
    image._encodedHeight = videoBuffer.GetCaptureHeight();
    image._completeFrame = true;
}

long filesize(const char *filename)
{
    FILE *f = fopen(filename,"rb");  /* open the file in read only */
    long size = 0;
    if (fseek(f,0,SEEK_END)==0) /* seek was successful */
        size = ftell(f);
    fclose(f);
    return size;
}
