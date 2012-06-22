/*
 *  Copyright (c) 2011 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef WEBRTC_MODULES_VIDEO_CODING_TEST_VIDEO_SOURCE_H_
#define WEBRTC_MODULES_VIDEO_CODING_TEST_VIDEO_SOURCE_H_

#include "common_video/libyuv/include/libyuv.h"
#include "typedefs.h"

#include <string>

enum VideoSize
    {
        kUndefined,
        kSQCIF,     // 128*96       = 12 288
        kQQVGA,     // 160*120      = 19 200
        kQCIF,      // 176*144      = 25 344
        kCGA,       // 320*200      = 64 000
        kQVGA,      // 320*240      = 76 800
        kSIF,       // 352*240      = 84 480
        kWQVGA,     // 400*240      = 96 000
        kCIF,       // 352*288      = 101 376
        kW288p,     // 512*288      = 147 456 (WCIF)
        k448p,      // 576*448      = 281 088
        kVGA,       // 640*480      = 307 200
        k432p,      // 720*432      = 311 040
        kW432p,     // 768*432      = 331 776
        k4SIF,      // 704*480      = 337 920
        kW448p,     // 768*448      = 344 064
        kNTSC,		// 720*480      = 345 600
        kFW448p,    // 800*448      = 358 400
        kWVGA,      // 800*480      = 384 000
        k4CIF,      // 704*576      = 405 504
        kSVGA,      // 800*600      = 480 000
        kW544p,     // 960*544      = 522 240
        kW576p,     // 1024*576     = 589 824 (W4CIF)
        kHD,        // 960*720      = 691 200
        kXGA,       // 1024*768     = 786 432
        kWHD,       // 1280*720     = 921 600
        kFullHD,   // 1440*1080    = 1 555 200
        kWFullHD,  // 1920*1080    = 2 073 600

        kNumberOfVideoSizes
    };


class VideoSource
{
public:
  VideoSource();
  VideoSource(std::string fileName, VideoSize size, float frameRate, webrtc::VideoType type = webrtc::kI420);
  VideoSource(std::string fileName, WebRtc_UWord16 width, WebRtc_UWord16 height,
      float frameRate = 30, webrtc::VideoType type = webrtc::kI420);

    std::string GetFileName() const { return _fileName; }
    WebRtc_UWord16  GetWidth() const { return _width; }
    WebRtc_UWord16 GetHeight() const { return _height; }
    webrtc::VideoType GetType() const { return _type; }
    float GetFrameRate() const { return _frameRate; }
    int GetWidthHeight( VideoSize size);

    // Returns the filename with the path (including the leading slash) removed.
    std::string GetName() const;

    WebRtc_Word32 GetFrameLength() const;

private:
    std::string         _fileName;
    WebRtc_UWord16      _width;
    WebRtc_UWord16      _height;
    webrtc::VideoType   _type;
    float               _frameRate;
};

#endif // WEBRTC_MODULES_VIDEO_CODING_TEST_VIDEO_SOURCE_H_

