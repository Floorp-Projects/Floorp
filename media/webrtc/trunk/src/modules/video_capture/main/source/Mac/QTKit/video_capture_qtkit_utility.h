/*
 *  Copyright (c) 2011 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

/*
 *  video_capture_qtkit_utility.h
 *
 */


#ifndef WEBRTC_MODULES_VIDEO_CAPTURE_MAIN_SOURCE_MAC_QTKIT_VIDEO_CAPTURE_QTKIT_UTILITY_H_
#define WEBRTC_MODULES_VIDEO_CAPTURE_MAIN_SOURCE_MAC_QTKIT_VIDEO_CAPTURE_QTKIT_UTILITY_H_

#define MAX_NAME_LENGTH                1024

#define QTKIT_MIN_WIDTH                0
#define QTKIT_MAX_WIDTH                2560
#define QTKIT_DEFAULT_WIDTH            352

#define QTKIT_MIN_HEIGHT            0
#define QTKIT_MAX_HEIGHT            1440
#define QTKIT_DEFAULT_HEIGHT        288

#define QTKIT_MIN_FRAME_RATE        1
#define QTKIT_MAX_FRAME_RATE        60
#define QTKIT_DEFAULT_FRAME_RATE    30

#define RELEASE_AND_CLEAR(p)        if (p) { (p) -> Release () ; (p) = NULL ; }

#endif  // WEBRTC_MODULES_VIDEO_CAPTURE_MAIN_SOURCE_MAC_QTKIT_VIDEO_CAPTURE_QTKIT_UTILITY_H_
