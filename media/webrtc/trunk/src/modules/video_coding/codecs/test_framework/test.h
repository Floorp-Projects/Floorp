/*
 *  Copyright (c) 2012 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef WEBRTC_MODULES_VIDEO_CODING_CODECS_TEST_FRAWEWORK_TEST_H_
#define WEBRTC_MODULES_VIDEO_CODING_CODECS_TEST_FRAWEWORK_TEST_H_

#include "video_codec_interface.h"
#include "video_buffer.h"
#include <string>
#include <fstream>
#include <cstdlib>

class CodecTest
{
public:
    CodecTest(std::string name, std::string description);
    CodecTest(std::string name, std::string description,
              WebRtc_UWord32 bitRate);
    virtual ~CodecTest() {};
    virtual void Perform()=0;
    virtual void Print();
    void SetEncoder(webrtc::VideoEncoder *encoder);
    void SetDecoder(webrtc::VideoDecoder *decoder);
    void SetLog(std::fstream* log);

protected:
    virtual void Setup();
    virtual void CodecSettings(int width,
                               int height,
                               WebRtc_UWord32 frameRate=30,
                               WebRtc_UWord32 bitRate=0);
    virtual void Teardown();
    double ActualBitRate(int nFrames);
    virtual bool PacketLoss(double lossRate, int /*thrown*/);
    static double RandUniform() { return (std::rand() + 1.0)/(RAND_MAX + 1.0); }
    static void VideoBufferToRawImage(TestVideoBuffer& videoBuffer,
                                      webrtc::VideoFrame &image);
    static void VideoEncodedBufferToEncodedImage(
        TestVideoEncodedBuffer& videoBuffer,
        webrtc::EncodedImage &image);

    webrtc::VideoEncoder*   _encoder;
    webrtc::VideoDecoder*   _decoder;
    WebRtc_UWord32          _bitRate;
    unsigned int            _lengthSourceFrame;
    unsigned char*          _sourceBuffer;
    TestVideoBuffer         _inputVideoBuffer;
    TestVideoEncodedBuffer  _encodedVideoBuffer;
    TestVideoBuffer         _decodedVideoBuffer;
    webrtc::VideoCodec      _inst;
    std::fstream*           _log;
    std::string             _inname;
    std::string             _outname;
    std::string             _encodedName;
    int                     _sumEncBytes;

private:
    std::string             _name;
    std::string             _description;

};

#endif // WEBRTC_MODULES_VIDEO_CODING_CODECS_TEST_FRAWEWORK_TEST_H_
