/*
 *  Copyright (c) 2011 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

//
//  video_capture_recursive_lock.h
//
//

#ifndef WEBRTC_MODULES_VIDEO_CAPTURE_MAIN_SOURCE_MAC_QTKIT_VIDEO_CAPTURE_RECURSIVE_LOCK_H_
#define WEBRTC_MODULES_VIDEO_CAPTURE_MAIN_SOURCE_MAC_QTKIT_VIDEO_CAPTURE_RECURSIVE_LOCK_H_

#import <Foundation/Foundation.h>

@interface VideoCaptureRecursiveLock : NSRecursiveLock <NSLocking> {
    BOOL _locked;
}

@property BOOL locked;

- (void)lock;
- (void)unlock;

@end

#endif // WEBRTC_MODULES_VIDEO_CAPTURE_MAIN_SOURCE_MAC_QTKIT_VIDEO_CAPTURE_RECURSIVE_LOCK_H_
