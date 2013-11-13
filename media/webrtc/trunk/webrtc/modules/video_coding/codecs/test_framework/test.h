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

#include <stdlib.h>

#include <fstream>
#include <string>

#include "webrtc/modules/interface/module_common_types.h"
#include "webrtc/modules/video_coding/codecs/interface/video_codec_interface.h"

class CodecTest
{
public:
    CodecTest(std::string name, std::string description);
    CodecTest(std::string name, std::string description,
              uint32_t bitRate);
    virtual ~CodecTest() {};
    virtual void Setup();
    virtual void Perform()=0;
    virtual void Print();
    void SetEncoder(webrtc::VideoEncoder *encoder);
    void SetDecoder(webrtc::VideoDecoder *decoder);
    void SetLog(std::fstream* log);

protected:
    virtual void CodecSettings(int width,
                               int height,
                               uint32_t frameRate=30,
                               uint32_t bitRate=0);
    virtual void Teardown();
    double ActualBitRate(int nFrames);
    virtual bool PacketLoss(double lossRate, int /*thrown*/);
    static double RandUniform() { return (std::rand() + 1.0)/(RAND_MAX + 1.0); }
    static void VideoEncodedBufferToEncodedImage(
        webrtc::VideoFrame& videoBuffer,
        webrtc::EncodedImage &image);

    webrtc::VideoEncoder*   _encoder;
    webrtc::VideoDecoder*   _decoder;
    uint32_t          _bitRate;
    unsigned int            _lengthSourceFrame;
    unsigned char*          _sourceBuffer;
    webrtc::I420VideoFrame  _inputVideoBuffer;
    // TODO(mikhal): For now using VideoFrame for encodedBuffer, should use a
    // designated class.
    webrtc::VideoFrame      _encodedVideoBuffer;
    webrtc::I420VideoFrame  _decodedVideoBuffer;
    webrtc::VideoCodec      _inst;
    std::fstream*           _log;
    std::string             _inname;
    std::string             _outname;
    std::string             _encodedName;
    int                     _sumEncBytes;
    int                     _width;
    int                     _halfWidth;
    int                     _height;
    int                     _halfHeight;
    int                     _sizeY;
    int                     _sizeUv;
private:
    std::string             _name;
    std::string             _description;

};

#endif // WEBRTC_MODULES_VIDEO_CODING_CODECS_TEST_FRAWEWORK_TEST_H_
