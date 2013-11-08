/*
 *  Copyright (c) 2013 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef WEBRTC_MODULES_VIDEO_CAPTURE_IOS_DEVICE_INFO_IOS_OBJC_H_
#define WEBRTC_MODULES_VIDEO_CAPTURE_IOS_DEVICE_INFO_IOS_OBJC_H_

#import <AVFoundation/AVFoundation.h>

@interface DeviceInfoIosObjC : NSObject
+ (int)captureDeviceCount;
+ (AVCaptureDevice*)captureDeviceForIndex:(int)index;
+ (AVCaptureDevice*)captureDeviceForUniqueId:(NSString*)uniqueId;
+ (NSString*)deviceNameForIndex:(int)index;
+ (NSString*)deviceUniqueIdForIndex:(int)index;
+ (NSString*)deviceNameForUniqueId:(NSString*)uniqueId;
@end

#endif  // WEBRTC_MODULES_VIDEO_CAPTURE_IOS_DEVICE_INFO_IOS_OBJC_H_
