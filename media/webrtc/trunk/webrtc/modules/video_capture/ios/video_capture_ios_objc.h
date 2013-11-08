/*
 *  Copyright (c) 2013 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef WEBRTC_MODULES_VIDEO_CAPTURE_IOS_VIDEO_CAPTURE_IOS_OBJC_H_
#define WEBRTC_MODULES_VIDEO_CAPTURE_IOS_VIDEO_CAPTURE_IOS_OBJC_H_

#import <UIKit/UIKit.h>

#include "webrtc/modules/video_capture/ios/video_capture_ios.h"

@interface VideoCaptureIosObjC
    : UIViewController<AVCaptureVideoDataOutputSampleBufferDelegate> {
 @private
  webrtc::videocapturemodule::VideoCaptureIos* _owner;
  webrtc::VideoCaptureCapability _capability;
  AVCaptureSession* _captureSession;
  int _captureId;
}

@property webrtc::VideoCaptureRotation frameRotation;

// custom initializer. Instance of VideoCaptureIos is needed
// for callback purposes.
// default init methods have been overridden to return nil.
- (id)initWithOwner:(webrtc::videocapturemodule::VideoCaptureIos*)owner
          captureId:(int)captureId;
- (BOOL)setCaptureDeviceByUniqueId:(NSString*)uniequeId;
- (BOOL)startCaptureWithCapability:
        (const webrtc::VideoCaptureCapability&)capability;
- (BOOL)stopCapture;

@end
#endif  // WEBRTC_MODULES_VIDEO_CAPTURE_IOS_VIDEO_CAPTURE_IOS_OBJC_H_
