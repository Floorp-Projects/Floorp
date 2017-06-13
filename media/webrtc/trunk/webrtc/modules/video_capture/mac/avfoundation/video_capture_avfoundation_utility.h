/*
 *  Copyright (c) 2016 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

/*
 *  video_capture_avfoundation_utility.h
 *
 */


#ifndef WEBRTC_MODULES_VIDEO_CAPTURE_MAIN_SOURCE_MAC_AVFOUNDATION_VIDEO_CAPTURE_AVFOUNDATION_UTILITY_H_
#define WEBRTC_MODULES_VIDEO_CAPTURE_MAIN_SOURCE_MAC_AVFOUNDATION_VIDEO_CAPTURE_AVFOUNDATION_UTILITY_H_

#define MAX_NAME_LENGTH                1024

#define AVFOUNDATION_MIN_WIDTH                0
#define AVFOUNDATION_MAX_WIDTH                2560
#define AVFOUNDATION_DEFAULT_WIDTH            352

#define AVFOUNDATION_MIN_HEIGHT            0
#define AVFOUNDATION_MAX_HEIGHT            1440
#define AVFOUNDATION_DEFAULT_HEIGHT        288

#define AVFOUNDATION_MIN_FRAME_RATE        1
#define AVFOUNDATION_MAX_FRAME_RATE        60
#define AVFOUNDATION_DEFAULT_FRAME_RATE    30

#define RELEASE_AND_CLEAR(p)        if (p) { (p) -> Release () ; (p) = NULL ; }

@interface VideoCaptureMacAVFoundationUtility : NSObject {}
+ (webrtc::RawVideoType)fourCCToRawVideoType:(FourCharCode)fourcc;
@end

@implementation VideoCaptureMacAVFoundationUtility
+ (webrtc::RawVideoType)fourCCToRawVideoType:(FourCharCode)fourcc {
    switch (fourcc) {
        case kCMPixelFormat_32ARGB:
            return webrtc::kVideoBGRA;
        case kCMPixelFormat_32BGRA:
            return webrtc::kVideoARGB;
        case kCMPixelFormat_24RGB:
            return webrtc::kVideoRGB24;
        case kCMPixelFormat_16LE565:
            return webrtc::kVideoRGB565;
        case kCMPixelFormat_16LE5551:
            return webrtc::kVideoARGB1555;
        case kCMPixelFormat_422YpCbCr8:
            return webrtc::kVideoUYVY;
        case kCMPixelFormat_422YpCbCr8_yuvs:
            return webrtc::kVideoYUY2;
        case kCMVideoCodecType_JPEG_OpenDML:
            return webrtc::kVideoMJPEG;
        case kCVPixelFormatType_420YpCbCr8BiPlanarVideoRange:
        case kCVPixelFormatType_420YpCbCr8BiPlanarFullRange:
            return webrtc::kVideoNV12;
        case kCVPixelFormatType_420YpCbCr8Planar:
        case kCVPixelFormatType_420YpCbCr8PlanarFullRange:
            return webrtc::kVideoI420;
        default:
            return webrtc::kVideoUnknown;
    }
}
@end

#endif  // WEBRTC_MODULES_VIDEO_CAPTURE_MAIN_SOURCE_MAC_AVFOUNDATION_VIDEO_CAPTURE_AVFOUNDATION_UTILITY_H_
