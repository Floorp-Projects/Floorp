/*
 *  Copyright (c) 2011 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "video_source.h"

#include <cassert>

#include "testsupport/fileutils.h"

VideoSource::VideoSource()
:
_fileName(webrtc::test::ProjectRootPath() + "resources/foreman_cif.yuv"),
_width(352),
_height(288),
_type(webrtc::kI420),
_frameRate(30)
{
   //
}

VideoSource::VideoSource(std::string fileName, VideoSize size,
    float frameRate, webrtc::VideoType type /*= webrtc::kI420*/)
:
_fileName(fileName),
_width(0),
_height(0),
_type(type),
_frameRate(frameRate)
{
    assert(size != kUndefined && size != kNumberOfVideoSizes);
    assert(type != webrtc::kUnknown);
    assert(frameRate > 0);
    GetWidthHeight(size);
}

VideoSource::VideoSource(std::string fileName, uint16_t width, uint16_t height,
    float frameRate /*= 30*/, webrtc::VideoType type /*= webrtc::kI420*/)
:
_fileName(fileName),
_width(width),
_height(height),
_type(type),
_frameRate(frameRate)
{
    assert(width > 0);
    assert(height > 0);
    assert(type != webrtc::kUnknown);
    assert(frameRate > 0);
}

int32_t
VideoSource::GetFrameLength() const
{
    return webrtc::CalcBufferSize(_type, _width, _height);
}

std::string
VideoSource::GetName() const
{
    // Remove path.
    size_t slashPos = _fileName.find_last_of("/\\");
    if (slashPos == std::string::npos)
    {
        slashPos = 0;
    }
    else
    {
        slashPos++;
    }

    // Remove extension and underscored suffix if it exists.
    //return _fileName.substr(slashPos, std::min(_fileName.find_last_of("_"),
    //    _fileName.find_last_of(".")) - slashPos);
    // MS: Removing suffix, not underscore....keeping full file name
    return _fileName.substr(slashPos, _fileName.find_last_of(".") - slashPos);

}

int
VideoSource::GetWidthHeight( VideoSize size)
{
    switch(size)
    {
    case kSQCIF:
        _width = 128;
        _height = 96;
        return 0;
    case kQQVGA:
        _width = 160;
        _height = 120;
        return 0;
    case kQCIF:
        _width = 176;
        _height = 144;
        return 0;
    case kCGA:
        _width = 320;
        _height = 200;
        return 0;
    case kQVGA:
        _width = 320;
        _height = 240;
        return 0;
    case kSIF:
        _width = 352;
        _height = 240;
        return 0;
    case kWQVGA:
        _width = 400;
        _height = 240;
        return 0;
    case kCIF:
        _width = 352;
        _height = 288;
        return 0;
    case kW288p:
        _width = 512;
        _height = 288;
        return 0;
    case k448p:
        _width = 576;
        _height = 448;
        return 0;
    case kVGA:
        _width = 640;
        _height = 480;
        return 0;
    case k432p:
        _width = 720;
        _height = 432;
        return 0;
    case kW432p:
        _width = 768;
        _height = 432;
        return 0;
    case k4SIF:
        _width = 704;
        _height = 480;
        return 0;
    case kW448p:
        _width = 768;
        _height = 448;
        return 0;
    case kNTSC:
        _width = 720;
        _height = 480;
        return 0;
    case kFW448p:
        _width = 800;
        _height = 448;
        return 0;
    case kWVGA:
        _width = 800;
        _height = 480;
        return 0;
    case k4CIF:
        _width = 704;
        _height = 576;
        return 0;
    case kSVGA:
        _width = 800;
        _height = 600;
        return 0;
    case kW544p:
        _width = 960;
        _height = 544;
        return 0;
    case kW576p:
        _width = 1024;
        _height = 576;
        return 0;
    case kHD:
        _width = 960;
        _height = 720;
        return 0;
    case kXGA:
        _width = 1024;
        _height = 768;
        return 0;
    case kFullHD:
        _width = 1440;
        _height = 1080;
        return 0;
    case kWHD:
        _width = 1280;
        _height = 720;
        return 0;
    case kWFullHD:
        _width = 1920;
        _height = 1080;
        return 0;
    default:
        return -1;
    }
}
